#pragma once

#include <lore/math/math.hpp>
#include <lore/ecs/ecs.hpp>
#include <vector>
#include <memory>

namespace lore::physics {

// Advanced physics engine inspired by Aeon's SIMD-accelerated design
// High-performance rigid body dynamics with scientific accuracy

// Forward declarations
class PhysicsWorld;
class RigidBody;
class Collider;

// Body types
enum class BodyType : std::uint8_t {
    Static,    // Never moves, infinite mass
    Kinematic, // Moves but not affected by forces
    Dynamic    // Affected by forces and gravity
};

// Material properties for physics interactions
struct Material {
    float density = 1.0f;
    float friction = 0.5f;
    float restitution = 0.3f;
    float hardness = 1.0f;
    float thermal_conductivity = 1.0f;
    float electrical_conductivity = 0.0f;

    // Getters
    float get_density() const noexcept { return density; }
    float get_friction() const noexcept { return friction; }
    float get_restitution() const noexcept { return restitution; }
    float get_hardness() const noexcept { return hardness; }
    float get_thermal_conductivity() const noexcept { return thermal_conductivity; }
    float get_electrical_conductivity() const noexcept { return electrical_conductivity; }

    // Setters
    void set_density(float value) noexcept { density = value; }
    void set_friction(float value) noexcept { friction = value; }
    void set_restitution(float value) noexcept { restitution = value; }
    void set_hardness(float value) noexcept { hardness = value; }
    void set_thermal_conductivity(float value) noexcept { thermal_conductivity = value; }
    void set_electrical_conductivity(float value) noexcept { electrical_conductivity = value; }
};

// Rigid body component for ECS
struct RigidBodyComponent {
    BodyType body_type = BodyType::Dynamic;
    float mass = 1.0f;
    float inverse_mass = 1.0f;
    math::Vec3 velocity{0.0f};
    math::Vec3 angular_velocity{0.0f};
    math::Vec3 force{0.0f};
    math::Vec3 torque{0.0f};
    math::Vec3 center_of_mass{0.0f};
    math::Mat3 inertia_tensor{1.0f};
    math::Mat3 inverse_inertia_tensor{1.0f};
    Material material;
    bool is_sleeping = false;
    float sleep_timer = 0.0f;
    float linear_damping = 0.1f;
    float angular_damping = 0.1f;

    // Getters
    BodyType get_body_type() const noexcept { return body_type; }
    float get_mass() const noexcept { return mass; }
    const math::Vec3& get_velocity() const noexcept { return velocity; }
    const math::Vec3& get_angular_velocity() const noexcept { return angular_velocity; }
    const math::Vec3& get_force() const noexcept { return force; }
    const math::Vec3& get_torque() const noexcept { return torque; }
    const Material& get_material() const noexcept { return material; }
    bool get_is_sleeping() const noexcept { return is_sleeping; }
    float get_linear_damping() const noexcept { return linear_damping; }
    float get_angular_damping() const noexcept { return angular_damping; }

    // Setters
    void set_body_type(BodyType type) noexcept;
    void set_mass(float m) noexcept;
    void set_velocity(const math::Vec3& vel) noexcept { velocity = vel; }
    void set_angular_velocity(const math::Vec3& ang_vel) noexcept { angular_velocity = ang_vel; }
    void set_material(const Material& mat) noexcept { material = mat; }
    void set_linear_damping(float damping) noexcept { linear_damping = damping; }
    void set_angular_damping(float damping) noexcept { angular_damping = damping; }

    // Force application
    void add_force(const math::Vec3& f) noexcept;
    void add_force_at_position(const math::Vec3& f, const math::Vec3& position) noexcept;
    void add_torque(const math::Vec3& t) noexcept;
    void add_impulse(const math::Vec3& impulse) noexcept;
    void add_impulse_at_position(const math::Vec3& impulse, const math::Vec3& position) noexcept;

    // Helpers
    void wake_up() noexcept;
    void calculate_inertia_tensor(const math::geometry::AABB& bounds) noexcept;
};

// Collision shapes
enum class ColliderType : std::uint8_t {
    Box,
    Sphere,
    Capsule,
    Mesh,
    Heightfield
};

struct ColliderComponent {
    ColliderType type = ColliderType::Box;
    math::Vec3 size{1.0f};  // Box: half-extents, Sphere: radius in x, Capsule: radius in x, height in y
    bool is_trigger = false;
    bool is_sensor = false;
    math::Vec3 center_offset{0.0f};

    // Getters
    ColliderType get_type() const noexcept { return type; }
    const math::Vec3& get_size() const noexcept { return size; }
    bool get_is_trigger() const noexcept { return is_trigger; }
    bool get_is_sensor() const noexcept { return is_sensor; }
    const math::Vec3& get_center_offset() const noexcept { return center_offset; }

    // Setters
    void set_type(ColliderType t) noexcept { type = t; }
    void set_size(const math::Vec3& s) noexcept { size = s; }
    void set_is_trigger(bool trigger) noexcept { is_trigger = trigger; }
    void set_is_sensor(bool sensor) noexcept { is_sensor = sensor; }
    void set_center_offset(const math::Vec3& offset) noexcept { center_offset = offset; }

    // Shape-specific setters
    void set_box_half_extents(const math::Vec3& half_extents) noexcept;
    void set_sphere_radius(float radius) noexcept;
    void set_capsule_params(float radius, float height) noexcept;
};

// Contact point for collision resolution
struct ContactPoint {
    math::Vec3 position;
    math::Vec3 normal;
    float penetration;
    float normal_impulse;
    float tangent_impulse[2];
};

// Collision event data
struct CollisionEvent {
    ecs::EntityHandle entity_a;
    ecs::EntityHandle entity_b;
    std::vector<ContactPoint> contacts;
    bool is_trigger_event;
};

// Physics system for ECS integration
class PhysicsSystem : public ecs::System {
public:
    PhysicsSystem();
    ~PhysicsSystem() override;

    void init(ecs::World& world) override;
    void update(ecs::World& world, float delta_time) override;
    void shutdown(ecs::World& world) override;

    // Configuration
    void set_gravity(const math::Vec3& gravity) noexcept;
    math::Vec3 get_gravity() const noexcept;
    void set_physics_constants(const math::PhysicsConstants& constants) noexcept;
    const math::PhysicsConstants& get_physics_constants() const noexcept;

    // Simulation control
    void set_simulation_enabled(bool enabled) noexcept;
    bool is_simulation_enabled() const noexcept;
    void set_fixed_timestep(float timestep) noexcept;
    float get_fixed_timestep() const noexcept;

    // Raycasting
    struct RaycastHit {
        ecs::EntityHandle entity;
        math::Vec3 point;
        math::Vec3 normal;
        float distance;
        bool hit;
    };

    RaycastHit raycast(const math::geometry::Ray& ray, float max_distance = 1000.0f) const;
    std::vector<RaycastHit> raycast_all(const math::geometry::Ray& ray, float max_distance = 1000.0f) const;

    // Overlap queries
    std::vector<ecs::EntityHandle> overlap_sphere(const math::geometry::Sphere& sphere) const;
    std::vector<ecs::EntityHandle> overlap_box(const math::geometry::AABB& box) const;

    // Collision events
    const std::vector<CollisionEvent>& get_collision_events() const noexcept;
    void clear_collision_events() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Thermodynamics system for heat simulation
class ThermodynamicsSystem : public ecs::System {
public:
    ThermodynamicsSystem();
    ~ThermodynamicsSystem() override;

    void init(ecs::World& world) override;
    void update(ecs::World& world, float delta_time) override;
    void shutdown(ecs::World& world) override;

    // Temperature control
    void set_ambient_temperature(float temperature) noexcept;
    float get_ambient_temperature() const noexcept;
    void set_heat_transfer_enabled(bool enabled) noexcept;
    bool is_heat_transfer_enabled() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

struct ThermalComponent {
    float temperature = 293.15f;  // Room temperature in Kelvin
    float heat_capacity = 1000.0f;
    float thermal_mass = 1.0f;
    float surface_area = 1.0f;

    // Getters
    float get_temperature() const noexcept { return temperature; }
    float get_celsius() const noexcept { return temperature - 273.15f; }
    float get_fahrenheit() const noexcept { return (temperature - 273.15f) * 9.0f / 5.0f + 32.0f; }
    float get_heat_capacity() const noexcept { return heat_capacity; }
    float get_thermal_mass() const noexcept { return thermal_mass; }
    float get_surface_area() const noexcept { return surface_area; }

    // Setters
    void set_temperature(float temp) noexcept { temperature = temp; }
    void set_celsius(float celsius) noexcept { temperature = celsius + 273.15f; }
    void set_fahrenheit(float fahrenheit) noexcept { temperature = (fahrenheit - 32.0f) * 5.0f / 9.0f + 273.15f; }
    void set_heat_capacity(float capacity) noexcept { heat_capacity = capacity; }
    void set_thermal_mass(float mass) noexcept { thermal_mass = mass; }
    void set_surface_area(float area) noexcept { surface_area = area; }

    // Heat transfer
    void add_heat(float joules) noexcept;
    void remove_heat(float joules) noexcept;
};

// Ballistics system for projectile physics
class BallisticsSystem : public ecs::System {
public:
    BallisticsSystem();
    ~BallisticsSystem() override;

    void init(ecs::World& world) override;
    void update(ecs::World& world, float delta_time) override;
    void shutdown(ecs::World& world) override;

    // Wind configuration
    void set_wind_velocity(const math::Vec3& wind) noexcept;
    math::Vec3 get_wind_velocity() const noexcept;
    void set_air_resistance_enabled(bool enabled) noexcept;
    bool is_air_resistance_enabled() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

struct ProjectileComponent {
    float drag_coefficient = 0.47f;  // Sphere
    float cross_sectional_area = 0.01f;  // m²
    float ballistic_coefficient = 1.0f;
    math::Vec3 wind_resistance{0.0f};
    bool affected_by_wind = true;

    // Getters
    float get_drag_coefficient() const noexcept { return drag_coefficient; }
    float get_cross_sectional_area() const noexcept { return cross_sectional_area; }
    float get_ballistic_coefficient() const noexcept { return ballistic_coefficient; }
    bool is_affected_by_wind() const noexcept { return affected_by_wind; }

    // Setters
    void set_drag_coefficient(float drag) noexcept { drag_coefficient = drag; }
    void set_cross_sectional_area(float area) noexcept { cross_sectional_area = area; }
    void set_ballistic_coefficient(float bc) noexcept { ballistic_coefficient = bc; }
    void set_affected_by_wind(bool affected) noexcept { affected_by_wind = affected; }
};

} // namespace lore::physics