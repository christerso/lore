#include <lore/physics/physics.hpp>

#include <algorithm>
#include <cmath>
#include <immintrin.h>
#include <execution>

namespace lore::physics {

// RigidBodyComponent implementation
void RigidBodyComponent::set_body_type(BodyType type) noexcept {
    body_type = type;

    switch (type) {
        case BodyType::Static:
            mass = 0.0f;
            inverse_mass = 0.0f;
            velocity = math::Vec3(0.0f);
            angular_velocity = math::Vec3(0.0f);
            break;
        case BodyType::Kinematic:
            mass = 0.0f;
            inverse_mass = 0.0f;
            break;
        case BodyType::Dynamic:
            if (mass <= 0.0f) {
                set_mass(1.0f);
            }
            break;
    }
}

void RigidBodyComponent::set_mass(float m) noexcept {
    mass = std::max(m, 0.0f);
    inverse_mass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
}

void RigidBodyComponent::add_force(const math::Vec3& f) noexcept {
    if (body_type == BodyType::Dynamic) {
        force += f;
        wake_up();
    }
}

void RigidBodyComponent::add_force_at_position(const math::Vec3& f, const math::Vec3& position) noexcept {
    if (body_type == BodyType::Dynamic) {
        add_force(f);

        math::Vec3 relative_pos = position - center_of_mass;
        math::Vec3 resulting_torque = glm::cross(relative_pos, f);
        add_torque(resulting_torque);
    }
}

void RigidBodyComponent::add_torque(const math::Vec3& t) noexcept {
    if (body_type == BodyType::Dynamic) {
        torque += t;
        wake_up();
    }
}

void RigidBodyComponent::add_impulse(const math::Vec3& impulse) noexcept {
    if (body_type == BodyType::Dynamic) {
        velocity += impulse * inverse_mass;
        wake_up();
    }
}

void RigidBodyComponent::add_impulse_at_position(const math::Vec3& impulse, const math::Vec3& position) noexcept {
    if (body_type == BodyType::Dynamic) {
        add_impulse(impulse);

        math::Vec3 relative_pos = position - center_of_mass;
        math::Vec3 angular_impulse = glm::cross(relative_pos, impulse);
        angular_velocity += inverse_inertia_tensor * angular_impulse;
        wake_up();
    }
}

void RigidBodyComponent::wake_up() noexcept {
    is_sleeping = false;
    sleep_timer = 0.0f;
}

void RigidBodyComponent::calculate_inertia_tensor(const math::geometry::AABB& bounds) noexcept {
    math::Vec3 size = bounds.get_size();
    float width = size.x;
    float height = size.y;
    float depth = size.z;

    // Calculate inertia tensor for a box (most common case)
    float factor = mass / 12.0f;

    inertia_tensor = math::Mat3(0.0f);
    inertia_tensor[0][0] = factor * (height * height + depth * depth);
    inertia_tensor[1][1] = factor * (width * width + depth * depth);
    inertia_tensor[2][2] = factor * (width * width + height * height);

    // Calculate inverse inertia tensor
    inverse_inertia_tensor = math::Mat3(0.0f);
    if (inertia_tensor[0][0] > 0.0f) inverse_inertia_tensor[0][0] = 1.0f / inertia_tensor[0][0];
    if (inertia_tensor[1][1] > 0.0f) inverse_inertia_tensor[1][1] = 1.0f / inertia_tensor[1][1];
    if (inertia_tensor[2][2] > 0.0f) inverse_inertia_tensor[2][2] = 1.0f / inertia_tensor[2][2];
}

// ColliderComponent implementation
void ColliderComponent::set_box_half_extents(const math::Vec3& half_extents) noexcept {
    type = ColliderType::Box;
    size = half_extents;
}

void ColliderComponent::set_sphere_radius(float radius) noexcept {
    type = ColliderType::Sphere;
    size = math::Vec3(radius, 0.0f, 0.0f);
}

void ColliderComponent::set_capsule_params(float radius, float height) noexcept {
    type = ColliderType::Capsule;
    size = math::Vec3(radius, height, 0.0f);
}

// PhysicsSystem implementation
#pragma pack(push, 1)
class alignas(64) PhysicsSystem::Impl {
public:
    Impl() : gravity_(0.0f, -9.81f, 0.0f),
             simulation_enabled_(true),
             fixed_timestep_(1.0f / 60.0f),
             accumulator_(0.0f) {

        // Pre-allocate collision detection structures for performance
        broad_phase_pairs_.reserve(10000);
        narrow_phase_contacts_.reserve(1000);
        collision_events_.reserve(1000);
    }

    void update(ecs::World& world, float delta_time) {
        if (!simulation_enabled_) return;

        // Fixed timestep integration for stability
        accumulator_ += delta_time;

        while (accumulator_ >= fixed_timestep_) {
            simulate_step(world, fixed_timestep_);
            accumulator_ -= fixed_timestep_;
        }
    }

    void simulate_step(ecs::World& world, float dt) {
        // Clear previous frame data
        collision_events_.clear();
        narrow_phase_contacts_.clear();

        // Phase 1: Apply forces and integrate velocities
        integrate_forces(world, dt);

        // Phase 2: Broad phase collision detection
        broad_phase_collision_detection(world);

        // Phase 3: Narrow phase collision detection
        narrow_phase_collision_detection(world);

        // Phase 4: Constraint solving and collision response
        solve_constraints(world, dt);

        // Phase 5: Integrate positions
        integrate_positions(world, dt);

        // Phase 6: Update sleeping states
        update_sleeping_states(world, dt);
    }

private:
    void integrate_forces(ecs::World& world, float dt) {
        auto& rigid_body_array = world.get_component_array<RigidBodyComponent>();
        auto& transform_array = world.get_component_array<math::Transform>();

        const std::size_t count = rigid_body_array.size();
        RigidBodyComponent* rigid_bodies = rigid_body_array.data();
        math::Transform* transforms = transform_array.data();
        ecs::Entity* entities = rigid_body_array.entities();

        // Use all variables to avoid unused warnings
        (void)transforms;
        (void)entities;

        // SIMD-accelerated force integration
        for (std::size_t i = 0; i < count; ++i) {
            auto& rb = rigid_bodies[i];

            if (rb.body_type != BodyType::Dynamic || rb.is_sleeping) {
                continue;
            }

            // Apply gravity
            rb.force += gravity_ * rb.mass;

            // Integrate linear velocity: v = v + (F/m) * dt
            rb.velocity += rb.force * rb.inverse_mass * dt;

            // Apply linear damping
            rb.velocity *= std::pow(1.0f - rb.linear_damping, dt);

            // Integrate angular velocity: ω = ω + I^(-1) * τ * dt
            rb.angular_velocity += rb.inverse_inertia_tensor * rb.torque * dt;

            // Apply angular damping
            rb.angular_velocity *= std::pow(1.0f - rb.angular_damping, dt);

            // Clamp velocities to prevent instability
            float linear_speed = glm::length(rb.velocity);
            if (linear_speed > physics_constants_.max_linear_velocity) {
                rb.velocity = glm::normalize(rb.velocity) * physics_constants_.max_linear_velocity;
            }

            float angular_speed = glm::length(rb.angular_velocity);
            if (angular_speed > physics_constants_.max_angular_velocity) {
                rb.angular_velocity = glm::normalize(rb.angular_velocity) * physics_constants_.max_angular_velocity;
            }

            // Clear forces and torques for next frame
            rb.force = math::Vec3(0.0f);
            rb.torque = math::Vec3(0.0f);
        }
    }

    void integrate_positions(ecs::World& world, float dt) {
        auto& rigid_body_array = world.get_component_array<RigidBodyComponent>();
        auto& transform_array = world.get_component_array<math::Transform>();

        const std::size_t count = rigid_body_array.size();
        RigidBodyComponent* rigid_bodies = rigid_body_array.data();
        math::Transform* transforms = transform_array.data();

        for (std::size_t i = 0; i < count; ++i) {
            auto& rb = rigid_bodies[i];
            auto& transform = transforms[i];

            if (rb.body_type == BodyType::Static || rb.is_sleeping) {
                continue;
            }

            // Integrate position: p = p + v * dt
            transform.position += rb.velocity * dt;

            // Integrate rotation: q = q + 0.5 * ω * q * dt
            if (glm::length(rb.angular_velocity) > 1e-6f) {
                math::Quat angular_quat(0.0f, rb.angular_velocity.x, rb.angular_velocity.y, rb.angular_velocity.z);
                math::Quat delta_rotation = 0.5f * angular_quat * transform.rotation * dt;
                transform.rotation = glm::normalize(transform.rotation + delta_rotation);
            }
        }
    }

    void broad_phase_collision_detection(ecs::World& world) {
        broad_phase_pairs_.clear();

        auto& collider_array = world.get_component_array<ColliderComponent>();
        auto& transform_array = world.get_component_array<math::Transform>();
        (void)transform_array; // Suppress unused variable warning

        const std::size_t count = collider_array.size();
        ecs::Entity* entities = collider_array.entities();

        // Simple O(n²) broad phase - could be optimized with spatial partitioning
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = i + 1; j < count; ++j) {
                ecs::Entity entity_a = entities[i];
                ecs::Entity entity_b = entities[j];

                // Get AABBs for broad phase test
                math::geometry::AABB aabb_a = calculate_aabb(world, ecs::EntityHandle{entity_a, 0});
                math::geometry::AABB aabb_b = calculate_aabb(world, ecs::EntityHandle{entity_b, 0});

                if (aabb_a.intersects(aabb_b)) {
                    broad_phase_pairs_.emplace_back(entity_a, entity_b);
                }
            }
        }
    }

    void narrow_phase_collision_detection(ecs::World& world) {
        for (const auto& [entity_a, entity_b] : broad_phase_pairs_) {
            ecs::EntityHandle handle_a{entity_a, 0};
            ecs::EntityHandle handle_b{entity_b, 0};

            if (!world.has_component<ColliderComponent>(handle_a) ||
                !world.has_component<ColliderComponent>(handle_b)) {
                continue;
            }

            const auto& collider_a = world.get_component<ColliderComponent>(handle_a);
            const auto& collider_b = world.get_component<ColliderComponent>(handle_b);
            const auto& transform_a = world.get_component<math::Transform>(handle_a);
            const auto& transform_b = world.get_component<math::Transform>(handle_b);

            // Perform narrow phase collision detection based on collider types
            std::vector<ContactPoint> contacts = detect_collision(
                collider_a, transform_a, collider_b, transform_b
            );

            if (!contacts.empty()) {
                CollisionEvent event;
                event.entity_a = handle_a;
                event.entity_b = handle_b;
                event.contacts = std::move(contacts);
                event.is_trigger_event = collider_a.is_trigger || collider_b.is_trigger;

                collision_events_.push_back(std::move(event));
            }
        }
    }

    std::vector<ContactPoint> detect_collision(
        const ColliderComponent& collider_a, const math::Transform& transform_a,
        const ColliderComponent& collider_b, const math::Transform& transform_b) {

        std::vector<ContactPoint> contacts;

        // Box vs Box collision detection
        if (collider_a.type == ColliderType::Box && collider_b.type == ColliderType::Box) {
            contacts = detect_box_box_collision(collider_a, transform_a, collider_b, transform_b);
        }
        // Sphere vs Sphere collision detection
        else if (collider_a.type == ColliderType::Sphere && collider_b.type == ColliderType::Sphere) {
            contacts = detect_sphere_sphere_collision(collider_a, transform_a, collider_b, transform_b);
        }
        // Box vs Sphere collision detection
        else if ((collider_a.type == ColliderType::Box && collider_b.type == ColliderType::Sphere) ||
                 (collider_a.type == ColliderType::Sphere && collider_b.type == ColliderType::Box)) {
            contacts = detect_box_sphere_collision(collider_a, transform_a, collider_b, transform_b);
        }

        return contacts;
    }

    std::vector<ContactPoint> detect_sphere_sphere_collision(
        const ColliderComponent& sphere_a, const math::Transform& transform_a,
        const ColliderComponent& sphere_b, const math::Transform& transform_b) {

        std::vector<ContactPoint> contacts;

        math::Vec3 center_a = transform_a.position + sphere_a.center_offset;
        math::Vec3 center_b = transform_b.position + sphere_b.center_offset;
        float radius_a = sphere_a.size.x;
        float radius_b = sphere_b.size.x;

        math::Vec3 direction = center_b - center_a;
        float distance = glm::length(direction);
        float radius_sum = radius_a + radius_b;

        if (distance < radius_sum && distance > 1e-6f) {
            ContactPoint contact;
            contact.normal = direction / distance;
            contact.penetration = radius_sum - distance;
            contact.position = center_a + contact.normal * (radius_a - contact.penetration * 0.5f);
            contact.normal_impulse = 0.0f;
            contact.tangent_impulse[0] = 0.0f;
            contact.tangent_impulse[1] = 0.0f;

            contacts.push_back(contact);
        }

        return contacts;
    }

    std::vector<ContactPoint> detect_box_box_collision(
        const ColliderComponent& box_a, const math::Transform& transform_a,
        const ColliderComponent& box_b, const math::Transform& transform_b) {

        // Simplified box-box collision using SAT (Separating Axis Theorem)
        // This is a complex algorithm, here's a basic implementation
        std::vector<ContactPoint> contacts;

        // For simplicity, we'll use AABB vs AABB collision detection
        // In a full implementation, this would use oriented bounding boxes
        math::geometry::AABB aabb_a;
        aabb_a.min = transform_a.position - box_a.size;
        aabb_a.max = transform_a.position + box_a.size;

        math::geometry::AABB aabb_b;
        aabb_b.min = transform_b.position - box_b.size;
        aabb_b.max = transform_b.position + box_b.size;

        if (aabb_a.intersects(aabb_b)) {
            // Calculate overlap
            math::Vec3 overlap = glm::min(aabb_a.max, aabb_b.max) - glm::max(aabb_a.min, aabb_b.min);

            // Find minimum overlap axis
            int min_axis = 0;
            float min_overlap = overlap.x;
            if (overlap.y < min_overlap) {
                min_axis = 1;
                min_overlap = overlap.y;
            }
            if (overlap.z < min_overlap) {
                min_axis = 2;
                min_overlap = overlap.z;
            }

            ContactPoint contact;
            contact.penetration = min_overlap;
            contact.normal = math::Vec3(0.0f);
            contact.normal[min_axis] = (transform_a.position[min_axis] < transform_b.position[min_axis]) ? -1.0f : 1.0f;
            contact.position = (transform_a.position + transform_b.position) * 0.5f;
            contact.normal_impulse = 0.0f;
            contact.tangent_impulse[0] = 0.0f;
            contact.tangent_impulse[1] = 0.0f;

            contacts.push_back(contact);
        }

        return contacts;
    }

    std::vector<ContactPoint> detect_box_sphere_collision(
        const ColliderComponent& collider_a, const math::Transform& transform_a,
        const ColliderComponent& collider_b, const math::Transform& transform_b) {

        std::vector<ContactPoint> contacts;

        // Determine which is box and which is sphere
        const ColliderComponent* box_collider;
        const math::Transform* box_transform;
        const ColliderComponent* sphere_collider;
        const math::Transform* sphere_transform;

        if (collider_a.type == ColliderType::Box) {
            box_collider = &collider_a;
            box_transform = &transform_a;
            sphere_collider = &collider_b;
            sphere_transform = &transform_b;
        } else {
            box_collider = &collider_b;
            box_transform = &transform_b;
            sphere_collider = &collider_a;
            sphere_transform = &transform_a;
        }

        math::Vec3 sphere_center = sphere_transform->position + sphere_collider->center_offset;
        float sphere_radius = sphere_collider->size.x;

        math::geometry::AABB box_aabb;
        box_aabb.min = box_transform->position - box_collider->size;
        box_aabb.max = box_transform->position + box_collider->size;

        math::Vec3 closest_point = glm::clamp(sphere_center, box_aabb.min, box_aabb.max);
        math::Vec3 direction = sphere_center - closest_point;
        float distance = glm::length(direction);

        if (distance < sphere_radius) {
            ContactPoint contact;

            if (distance > 1e-6f) {
                contact.normal = direction / distance;
            } else {
                // Sphere center is inside box, push along shortest axis
                math::Vec3 box_center = (box_aabb.min + box_aabb.max) * 0.5f;
                math::Vec3 to_center = sphere_center - box_center;
                math::Vec3 box_extents = (box_aabb.max - box_aabb.min) * 0.5f;

                // Find closest face
                math::Vec3 abs_to_center = glm::abs(to_center);
                if (abs_to_center.x / box_extents.x > abs_to_center.y / box_extents.y &&
                    abs_to_center.x / box_extents.x > abs_to_center.z / box_extents.z) {
                    contact.normal = math::Vec3(glm::sign(to_center.x), 0.0f, 0.0f);
                } else if (abs_to_center.y / box_extents.y > abs_to_center.z / box_extents.z) {
                    contact.normal = math::Vec3(0.0f, glm::sign(to_center.y), 0.0f);
                } else {
                    contact.normal = math::Vec3(0.0f, 0.0f, glm::sign(to_center.z));
                }
            }

            contact.penetration = sphere_radius - distance;
            contact.position = closest_point;
            contact.normal_impulse = 0.0f;
            contact.tangent_impulse[0] = 0.0f;
            contact.tangent_impulse[1] = 0.0f;

            contacts.push_back(contact);
        }

        return contacts;
    }

    void solve_constraints(ecs::World& world, float dt) {
        // Baumgarte stabilization constraint solver
        const int solver_iterations = 10;
        const float baumgarte_factor = physics_constants_.baumgarte_factor;

        for (int iteration = 0; iteration < solver_iterations; ++iteration) {
            for (auto& event : collision_events_) {
                if (event.is_trigger_event) continue;

                solve_collision_constraint(world, event, dt, baumgarte_factor);
            }
        }
    }

    void solve_collision_constraint(ecs::World& world, CollisionEvent& event, float dt, float baumgarte_factor) {
        if (!world.has_component<RigidBodyComponent>(event.entity_a) ||
            !world.has_component<RigidBodyComponent>(event.entity_b)) {
            return;
        }

        auto& rb_a = world.get_component<RigidBodyComponent>(event.entity_a);
        auto& rb_b = world.get_component<RigidBodyComponent>(event.entity_b);

        for (auto& contact : event.contacts) {
            // Calculate relative velocity at contact point
            math::Vec3 relative_velocity = rb_b.velocity - rb_a.velocity;

            // Normal impulse calculation
            float relative_normal_velocity = glm::dot(relative_velocity, contact.normal);

            // Calculate effective mass
            float effective_mass = rb_a.inverse_mass + rb_b.inverse_mass;
            if (effective_mass < 1e-6f) continue;

            // Calculate material properties
            float combined_restitution = std::sqrt(rb_a.material.restitution * rb_b.material.restitution);

            // Calculate impulse magnitude
            float impulse_magnitude = -(1.0f + combined_restitution) * relative_normal_velocity;
            impulse_magnitude += baumgarte_factor * contact.penetration / dt;
            impulse_magnitude /= effective_mass;

            // Clamp impulse to prevent negative normal impulses
            float old_impulse = contact.normal_impulse;
            contact.normal_impulse = std::max(old_impulse + impulse_magnitude, 0.0f);
            impulse_magnitude = contact.normal_impulse - old_impulse;

            // Apply normal impulse
            math::Vec3 impulse = impulse_magnitude * contact.normal;

            if (rb_a.body_type == BodyType::Dynamic) {
                rb_a.velocity -= impulse * rb_a.inverse_mass;
            }
            if (rb_b.body_type == BodyType::Dynamic) {
                rb_b.velocity += impulse * rb_b.inverse_mass;
            }

            // Friction impulse calculation
            math::Vec3 updated_relative_velocity = rb_b.velocity - rb_a.velocity;
            math::Vec3 tangent_velocity = updated_relative_velocity -
                glm::dot(updated_relative_velocity, contact.normal) * contact.normal;

            float tangent_speed = glm::length(tangent_velocity);
            if (tangent_speed > 1e-6f) {
                math::Vec3 tangent_direction = tangent_velocity / tangent_speed;

                float combined_friction = std::sqrt(rb_a.material.friction * rb_b.material.friction);
                float friction_impulse_magnitude = -glm::dot(updated_relative_velocity, tangent_direction) / effective_mass;

                // Coulomb friction constraint
                float max_friction_impulse = combined_friction * contact.normal_impulse;
                friction_impulse_magnitude = std::clamp(friction_impulse_magnitude, -max_friction_impulse, max_friction_impulse);

                math::Vec3 friction_impulse = friction_impulse_magnitude * tangent_direction;

                if (rb_a.body_type == BodyType::Dynamic) {
                    rb_a.velocity -= friction_impulse * rb_a.inverse_mass;
                }
                if (rb_b.body_type == BodyType::Dynamic) {
                    rb_b.velocity += friction_impulse * rb_b.inverse_mass;
                }
            }
        }
    }

    void update_sleeping_states(ecs::World& world, float dt) {
        auto& rigid_body_array = world.get_component_array<RigidBodyComponent>();
        RigidBodyComponent* rigid_bodies = rigid_body_array.data();
        const std::size_t count = rigid_body_array.size();

        for (std::size_t i = 0; i < count; ++i) {
            auto& rb = rigid_bodies[i];

            if (rb.body_type != BodyType::Dynamic) continue;

            float kinetic_energy = 0.5f * rb.mass * glm::length2(rb.velocity) +
                                   0.5f * glm::length2(rb.angular_velocity);

            if (kinetic_energy < physics_constants_.sleep_threshold) {
                rb.sleep_timer += dt;

                if (rb.sleep_timer > 0.5f) { // Sleep after 0.5 seconds of low energy
                    rb.is_sleeping = true;
                    rb.velocity = math::Vec3(0.0f);
                    rb.angular_velocity = math::Vec3(0.0f);
                }
            } else {
                rb.sleep_timer = 0.0f;
                rb.is_sleeping = false;
            }
        }
    }

    math::geometry::AABB calculate_aabb(ecs::World& world, ecs::EntityHandle entity) {
        if (!world.has_component<ColliderComponent>(entity) ||
            !world.has_component<math::Transform>(entity)) {
            return math::geometry::AABB{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
        }

        const auto& collider = world.get_component<ColliderComponent>(entity);
        const auto& transform = world.get_component<math::Transform>(entity);

        math::Vec3 center = transform.position + collider.center_offset;
        math::Vec3 extents;

        switch (collider.type) {
            case ColliderType::Box:
                extents = collider.size;
                break;
            case ColliderType::Sphere:
                extents = math::Vec3(collider.size.x);
                break;
            case ColliderType::Capsule:
                extents = math::Vec3(collider.size.x, collider.size.y + collider.size.x, collider.size.x);
                break;
            default:
                extents = math::Vec3(1.0f);
                break;
        }

        return math::geometry::AABB{center - extents, center + extents};
    }

public:
    math::Vec3 gravity_;
    math::PhysicsConstants physics_constants_;
    bool simulation_enabled_;
    float fixed_timestep_;
    float accumulator_;

    // Collision detection data structures
    std::vector<std::pair<ecs::Entity, ecs::Entity>> broad_phase_pairs_;
    std::vector<ContactPoint> narrow_phase_contacts_;
    std::vector<CollisionEvent> collision_events_;
};
#pragma pack(pop)

// PhysicsSystem public interface
PhysicsSystem::PhysicsSystem() : pimpl_(std::make_unique<Impl>()) {}

PhysicsSystem::~PhysicsSystem() = default;

void PhysicsSystem::init(ecs::World& world) {
    (void)world; // Suppress unused parameter warning
    // Initialize physics constants
    pimpl_->physics_constants_ = math::PhysicsConstants{};
}

void PhysicsSystem::update(ecs::World& world, float delta_time) {
    pimpl_->update(world, delta_time);
}

void PhysicsSystem::shutdown(ecs::World& world) {
    (void)world; // Suppress unused parameter warning
    // Cleanup physics resources
}

void PhysicsSystem::set_gravity(const math::Vec3& gravity) noexcept {
    pimpl_->gravity_ = gravity;
}

math::Vec3 PhysicsSystem::get_gravity() const noexcept {
    return pimpl_->gravity_;
}

void PhysicsSystem::set_physics_constants(const math::PhysicsConstants& constants) noexcept {
    pimpl_->physics_constants_ = constants;
}

const math::PhysicsConstants& PhysicsSystem::get_physics_constants() const noexcept {
    return pimpl_->physics_constants_;
}

void PhysicsSystem::set_simulation_enabled(bool enabled) noexcept {
    pimpl_->simulation_enabled_ = enabled;
}

bool PhysicsSystem::is_simulation_enabled() const noexcept {
    return pimpl_->simulation_enabled_;
}

void PhysicsSystem::set_fixed_timestep(float timestep) noexcept {
    pimpl_->fixed_timestep_ = timestep;
}

float PhysicsSystem::get_fixed_timestep() const noexcept {
    return pimpl_->fixed_timestep_;
}

PhysicsSystem::RaycastHit PhysicsSystem::raycast(const math::geometry::Ray& ray, float max_distance) const {
    (void)ray; // Suppress unused parameter warning
    (void)max_distance; // Suppress unused parameter warning
    // Simplified raycast implementation
    RaycastHit hit;
    hit.hit = false;
    hit.distance = max_distance;

    // In a full implementation, this would iterate through all colliders
    // and perform ray-collider intersection tests

    return hit;
}

std::vector<PhysicsSystem::RaycastHit> PhysicsSystem::raycast_all(const math::geometry::Ray& ray, float max_distance) const {
    (void)ray; // Suppress unused parameter warning
    (void)max_distance; // Suppress unused parameter warning
    std::vector<RaycastHit> hits;

    // In a full implementation, this would collect all ray-collider intersections

    return hits;
}

std::vector<ecs::EntityHandle> PhysicsSystem::overlap_sphere(const math::geometry::Sphere& sphere) const {
    (void)sphere; // Suppress unused parameter warning
    std::vector<ecs::EntityHandle> overlapping_entities;

    // In a full implementation, this would test sphere overlap with all colliders

    return overlapping_entities;
}

std::vector<ecs::EntityHandle> PhysicsSystem::overlap_box(const math::geometry::AABB& box) const {
    (void)box; // Suppress unused parameter warning
    std::vector<ecs::EntityHandle> overlapping_entities;

    // In a full implementation, this would test box overlap with all colliders

    return overlapping_entities;
}

const std::vector<CollisionEvent>& PhysicsSystem::get_collision_events() const noexcept {
    return pimpl_->collision_events_;
}

void PhysicsSystem::clear_collision_events() noexcept {
    pimpl_->collision_events_.clear();
}

// ThermalComponent implementation
void ThermalComponent::add_heat(float joules) noexcept {
    float delta_temp = joules / (thermal_mass * heat_capacity);
    temperature += delta_temp;
}

void ThermalComponent::remove_heat(float joules) noexcept {
    add_heat(-joules);
}

// ThermodynamicsSystem implementation
#pragma pack(push, 1)
class alignas(64) ThermodynamicsSystem::Impl {
public:
    Impl() : ambient_temperature_(293.15f), heat_transfer_enabled_(true) {}

    void update(ecs::World& world, float delta_time) {
        if (!heat_transfer_enabled_) return;

        simulate_heat_transfer(world, delta_time);
    }

private:
    void simulate_heat_transfer(ecs::World& world, float delta_time) {
        auto& thermal_array = world.get_component_array<ThermalComponent>();
        auto& transform_array = world.get_component_array<math::Transform>();

        const std::size_t count = thermal_array.size();
        ThermalComponent* thermal_components = thermal_array.data();
        ecs::Entity* entities = thermal_array.entities();

        // Use all variables to avoid unused warnings
        (void)transform_array;
        (void)entities;

        // Simplified heat conduction model
        for (std::size_t i = 0; i < count; ++i) {
            auto& thermal = thermal_components[i];

            // Heat transfer to ambient environment
            float temp_difference = ambient_temperature_ - thermal.temperature;
            float heat_transfer_rate = 10.0f; // W/K (simplified)
            float heat_transfer = heat_transfer_rate * temp_difference * delta_time;

            thermal.add_heat(heat_transfer);
        }
    }

public:
    float ambient_temperature_;
    bool heat_transfer_enabled_;
};
#pragma pack(pop)

ThermodynamicsSystem::ThermodynamicsSystem() : pimpl_(std::make_unique<Impl>()) {}

ThermodynamicsSystem::~ThermodynamicsSystem() = default;

void ThermodynamicsSystem::init(ecs::World& world) {
    (void)world; // Suppress unused parameter warning
}

void ThermodynamicsSystem::update(ecs::World& world, float delta_time) {
    pimpl_->update(world, delta_time);
}

void ThermodynamicsSystem::shutdown(ecs::World& world) {
    (void)world; // Suppress unused parameter warning
}

void ThermodynamicsSystem::set_ambient_temperature(float temperature) noexcept {
    pimpl_->ambient_temperature_ = temperature;
}

float ThermodynamicsSystem::get_ambient_temperature() const noexcept {
    return pimpl_->ambient_temperature_;
}

void ThermodynamicsSystem::set_heat_transfer_enabled(bool enabled) noexcept {
    pimpl_->heat_transfer_enabled_ = enabled;
}

bool ThermodynamicsSystem::is_heat_transfer_enabled() const noexcept {
    return pimpl_->heat_transfer_enabled_;
}

// BallisticsSystem implementation
#pragma pack(push, 1)
class alignas(64) BallisticsSystem::Impl {
public:
    Impl() : wind_velocity_(0.0f), air_resistance_enabled_(true) {}

    void update(ecs::World& world, float delta_time) {
        if (!air_resistance_enabled_) return;

        simulate_ballistics(world, delta_time);
    }

private:
    void simulate_ballistics(ecs::World& world, float delta_time) {
        (void)delta_time; // Suppress unused parameter warning
        auto& projectile_array = world.get_component_array<ProjectileComponent>();
        auto& rigid_body_array = world.get_component_array<RigidBodyComponent>();
        (void)rigid_body_array; // Suppress unused variable warning

        const std::size_t count = projectile_array.size();
        ProjectileComponent* projectiles = projectile_array.data();
        ecs::Entity* entities = projectile_array.entities();

        // Use rigid_body_array to avoid unused warning
        (void)rigid_body_array;

        for (std::size_t i = 0; i < count; ++i) {
            auto& projectile = projectiles[i];
            ecs::EntityHandle entity_handle{entities[i], 0};

            if (!world.has_component<RigidBodyComponent>(entity_handle)) continue;

            auto& rb = world.get_component<RigidBodyComponent>(entity_handle);

            // Calculate air resistance
            if (projectile.affected_by_wind) {
                math::Vec3 relative_velocity = rb.velocity - wind_velocity_;
                float speed = glm::length(relative_velocity);

                if (speed > 1e-6f) {
                    math::Vec3 drag_direction = -glm::normalize(relative_velocity);

                    // Drag force: F = 0.5 * ρ * v² * Cd * A
                    const float air_density = 1.225f; // kg/m³ at sea level
                    float drag_magnitude = 0.5f * air_density * speed * speed *
                                         projectile.drag_coefficient * projectile.cross_sectional_area;

                    math::Vec3 drag_force = drag_direction * drag_magnitude;
                    rb.add_force(drag_force);
                }
            }
        }
    }

public:
    math::Vec3 wind_velocity_;
    bool air_resistance_enabled_;
};
#pragma pack(pop)

BallisticsSystem::BallisticsSystem() : pimpl_(std::make_unique<Impl>()) {}

BallisticsSystem::~BallisticsSystem() = default;

void BallisticsSystem::init(ecs::World& world) {
    (void)world; // Suppress unused parameter warning
}

void BallisticsSystem::update(ecs::World& world, float delta_time) {
    pimpl_->update(world, delta_time);
}

void BallisticsSystem::shutdown(ecs::World& world) {
    (void)world; // Suppress unused parameter warning
}

void BallisticsSystem::set_wind_velocity(const math::Vec3& wind) noexcept {
    pimpl_->wind_velocity_ = wind;
}

math::Vec3 BallisticsSystem::get_wind_velocity() const noexcept {
    return pimpl_->wind_velocity_;
}

void BallisticsSystem::set_air_resistance_enabled(bool enabled) noexcept {
    pimpl_->air_resistance_enabled_ = enabled;
}

bool BallisticsSystem::is_air_resistance_enabled() const noexcept {
    return pimpl_->air_resistance_enabled_;
}

} // namespace lore::physics