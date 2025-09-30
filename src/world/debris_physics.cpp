#include <lore/world/debris_physics.hpp>
#include <lore/core/log.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>

using namespace lore::core;

namespace lore::world {

DebrisPhysics::DebrisPhysics(const DebrisPhysicsConfig& config)
    : config_(config)
{
    LOG_INFO(Game, "DebrisPhysics initialized: gravity=({:.2f}, {:.2f}, {:.2f}), sleep_threshold={:.3f}",
            config_.gravity.x, config_.gravity.y, config_.gravity.z, config_.sleep_velocity_threshold);
}

// Quaternion helper functions
math::Vec3 DebrisPhysics::rotate_vector(const math::Vec3& v, const math::Quat& q) {
    // v' = q * v * q^-1
    // Simplified for unit quaternions: q^-1 = q* (conjugate)

    // q * v (treating v as quaternion with w=0)
    math::Quat qv;
    qv.x = q.w * v.x + q.y * v.z - q.z * v.y;
    qv.y = q.w * v.y + q.z * v.x - q.x * v.z;
    qv.z = q.w * v.z + q.x * v.y - q.y * v.x;
    qv.w = -q.x * v.x - q.y * v.y - q.z * v.z;

    // (q * v) * q^-1
    math::Vec3 result;
    result.x = qv.w * (-q.x) + qv.x * q.w + qv.y * (-q.z) - qv.z * (-q.y);
    result.y = qv.w * (-q.y) + qv.y * q.w + qv.z * (-q.x) - qv.x * (-q.z);
    result.z = qv.w * (-q.z) + qv.z * q.w + qv.x * (-q.y) - qv.y * (-q.x);

    return result;
}

void DebrisPhysics::normalize_quaternion(math::Quat& q) {
    float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length > 1e-6f) {
        float inv_length = 1.0f / length;
        q.x *= inv_length;
        q.y *= inv_length;
        q.z *= inv_length;
        q.w *= inv_length;
    } else {
        q = {0, 0, 0, 1}; // Identity quaternion
    }
}

math::Quat DebrisPhysics::quaternion_multiply(const math::Quat& a, const math::Quat& b) {
    math::Quat result;
    result.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    result.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    result.y = a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z;
    result.z = a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x;
    return result;
}

// Integrate physics for one piece
void DebrisPhysics::integrate_piece(DebrisPiece& piece, float delta_time) {
    if (piece.is_sleeping) {
        return; // Skip sleeping pieces
    }

    // Apply gravity
    math::Vec3 acceleration = config_.gravity;

    // Apply air drag (quadratic with velocity)
    float speed = std::sqrt(
        piece.velocity.x * piece.velocity.x +
        piece.velocity.y * piece.velocity.y +
        piece.velocity.z * piece.velocity.z
    );

    if (speed > 1e-6f) {
        float drag_magnitude = config_.air_drag * speed;
        math::Vec3 drag_force = {
            -piece.velocity.x / speed * drag_magnitude,
            -piece.velocity.y / speed * drag_magnitude,
            -piece.velocity.z / speed * drag_magnitude
        };
        acceleration.x += drag_force.x / piece.mass;
        acceleration.y += drag_force.y / piece.mass;
        acceleration.z += drag_force.z / piece.mass;
    }

    // Integrate linear velocity
    piece.velocity.x += acceleration.x * delta_time;
    piece.velocity.y += acceleration.y * delta_time;
    piece.velocity.z += acceleration.z * delta_time;

    // Integrate position
    piece.position.x += piece.velocity.x * delta_time;
    piece.position.y += piece.velocity.y * delta_time;
    piece.position.z += piece.velocity.z * delta_time;

    // Apply angular drag
    float angular_speed = std::sqrt(
        piece.angular_velocity.x * piece.angular_velocity.x +
        piece.angular_velocity.y * piece.angular_velocity.y +
        piece.angular_velocity.z * piece.angular_velocity.z
    );

    if (angular_speed > 1e-6f) {
        float angular_drag = 1.0f - config_.angular_drag;
        piece.angular_velocity.x *= angular_drag;
        piece.angular_velocity.y *= angular_drag;
        piece.angular_velocity.z *= angular_drag;
    }

    // Integrate rotation using quaternion
    // dq/dt = 0.5 * ω * q
    math::Quat omega_quat;
    omega_quat.x = piece.angular_velocity.x;
    omega_quat.y = piece.angular_velocity.y;
    omega_quat.z = piece.angular_velocity.z;
    omega_quat.w = 0;

    math::Quat dq = quaternion_multiply(omega_quat, piece.rotation);
    piece.rotation.x += 0.5f * dq.x * delta_time;
    piece.rotation.y += 0.5f * dq.y * delta_time;
    piece.rotation.z += 0.5f * dq.z * delta_time;
    piece.rotation.w += 0.5f * dq.w * delta_time;

    normalize_quaternion(piece.rotation);

    // Update age
    piece.time_since_creation += delta_time;
}

// AABB overlap test
bool DebrisPhysics::aabb_overlap(
    const math::Vec3& min_a, const math::Vec3& max_a,
    const math::Vec3& min_b, const math::Vec3& max_b) const
{
    return (min_a.x <= max_b.x && max_a.x >= min_b.x) &&
           (min_a.y <= max_b.y && max_a.y >= min_b.y) &&
           (min_a.z <= max_b.z && max_a.z >= min_b.z);
}

// Narrow-phase collision (simplified AABB-based)
bool DebrisPhysics::narrow_phase_collision(
    const DebrisPiece& piece_a,
    const DebrisPiece& piece_b,
    CollisionContact& out_contact) const
{
    // Transform AABBs to world space
    math::Vec3 min_a = {
        piece_a.position.x + piece_a.aabb_min.x,
        piece_a.position.y + piece_a.aabb_min.y,
        piece_a.position.z + piece_a.aabb_min.z
    };
    math::Vec3 max_a = {
        piece_a.position.x + piece_a.aabb_max.x,
        piece_a.position.y + piece_a.aabb_max.y,
        piece_a.position.z + piece_a.aabb_max.z
    };

    math::Vec3 min_b = {
        piece_b.position.x + piece_b.aabb_min.x,
        piece_b.position.y + piece_b.aabb_min.y,
        piece_b.position.z + piece_b.aabb_min.z
    };
    math::Vec3 max_b = {
        piece_b.position.x + piece_b.aabb_max.x,
        piece_b.position.y + piece_b.aabb_max.y,
        piece_b.position.z + piece_b.aabb_max.z
    };

    // Check overlap
    if (!aabb_overlap(min_a, max_a, min_b, max_b)) {
        return false;
    }

    // Calculate contact point (center of overlap region)
    out_contact.point = {
        std::max(min_a.x, min_b.x) + std::min(max_a.x, max_b.x) * 0.5f,
        std::max(min_a.y, min_b.y) + std::min(max_a.y, max_b.y) * 0.5f,
        std::max(min_a.z, min_b.z) + std::min(max_a.z, max_b.z) * 0.5f
    };

    // Calculate separation vector (from A to B)
    math::Vec3 center_a = {
        (min_a.x + max_a.x) * 0.5f,
        (min_a.y + max_a.y) * 0.5f,
        (min_a.z + max_a.z) * 0.5f
    };
    math::Vec3 center_b = {
        (min_b.x + max_b.x) * 0.5f,
        (min_b.y + max_b.y) * 0.5f,
        (min_b.z + max_b.z) * 0.5f
    };

    out_contact.normal = {
        center_b.x - center_a.x,
        center_b.y - center_a.y,
        center_b.z - center_a.z
    };

    // Normalize
    float length = std::sqrt(
        out_contact.normal.x * out_contact.normal.x +
        out_contact.normal.y * out_contact.normal.y +
        out_contact.normal.z * out_contact.normal.z
    );

    if (length > 1e-6f) {
        out_contact.normal.x /= length;
        out_contact.normal.y /= length;
        out_contact.normal.z /= length;
    } else {
        out_contact.normal = {0, 1, 0}; // Default up
    }

    // Calculate penetration depth
    float overlap_x = std::min(max_a.x, max_b.x) - std::max(min_a.x, min_b.x);
    float overlap_y = std::min(max_a.y, max_b.y) - std::max(min_a.y, min_b.y);
    float overlap_z = std::min(max_a.z, max_b.z) - std::max(min_a.z, min_b.z);

    out_contact.penetration_depth = std::min({overlap_x, overlap_y, overlap_z});

    return true;
}

// Detect all collisions
std::vector<CollisionContact> DebrisPhysics::detect_collisions(
    const std::vector<DebrisPiece>& debris_pieces)
{
    std::vector<CollisionContact> contacts;

    // Broad-phase: O(n²) AABB test (could be optimized with spatial partitioning)
    for (size_t i = 0; i < debris_pieces.size(); ++i) {
        if (debris_pieces[i].is_sleeping) continue;

        for (size_t j = i + 1; j < debris_pieces.size(); ++j) {
            stats_.collision_checks++;

            CollisionContact contact;
            if (narrow_phase_collision(debris_pieces[i], debris_pieces[j], contact)) {
                contact.debris_a_index = static_cast<uint32_t>(i);
                contact.debris_b_index = static_cast<uint32_t>(j);
                contacts.push_back(contact);
            }
        }
    }

    return contacts;
}

// Resolve collision
void DebrisPhysics::resolve_collision(
    DebrisPiece& piece_a,
    DebrisPiece& piece_b,
    const CollisionContact& contact)
{
    // Calculate relative velocity at contact point
    math::Vec3 rel_vel = {
        piece_b.velocity.x - piece_a.velocity.x,
        piece_b.velocity.y - piece_a.velocity.y,
        piece_b.velocity.z - piece_a.velocity.z
    };

    // Velocity along normal
    float vel_along_normal =
        rel_vel.x * contact.normal.x +
        rel_vel.y * contact.normal.y +
        rel_vel.z * contact.normal.z;

    // Don't resolve if velocities are separating
    if (vel_along_normal > 0) {
        return;
    }

    // Calculate impulse magnitude
    float inv_mass_sum = (1.0f / piece_a.mass) + (1.0f / piece_b.mass);
    float j = -(1.0f + config_.restitution) * vel_along_normal;
    j /= inv_mass_sum;

    // Apply impulse
    math::Vec3 impulse = {
        contact.normal.x * j,
        contact.normal.y * j,
        contact.normal.z * j
    };

    piece_a.velocity.x -= impulse.x / piece_a.mass;
    piece_a.velocity.y -= impulse.y / piece_a.mass;
    piece_a.velocity.z -= impulse.z / piece_a.mass;

    piece_b.velocity.x += impulse.x / piece_b.mass;
    piece_b.velocity.y += impulse.y / piece_b.mass;
    piece_b.velocity.z += impulse.z / piece_b.mass;

    // Separate overlapping objects (position correction)
    const float percent = 0.8f; // Penetration percentage to correct
    const float slop = 0.01f; // Penetration allowance
    float correction_magnitude = std::max(contact.penetration_depth - slop, 0.0f) / inv_mass_sum * percent;

    math::Vec3 correction = {
        contact.normal.x * correction_magnitude,
        contact.normal.y * correction_magnitude,
        contact.normal.z * correction_magnitude
    };

    piece_a.position.x -= correction.x / piece_a.mass;
    piece_a.position.y -= correction.y / piece_a.mass;
    piece_a.position.z -= correction.z / piece_a.mass;

    piece_b.position.x += correction.x / piece_b.mass;
    piece_b.position.y += correction.y / piece_b.mass;
    piece_b.position.z += correction.z / piece_b.mass;

    // Wake up both pieces
    piece_a.is_sleeping = false;
    piece_b.is_sleeping = false;

    stats_.collisions_resolved++;
}

// Check if debris should sleep
bool DebrisPhysics::should_sleep(const DebrisPiece& piece, uint32_t piece_index, float delta_time) {
    // Ensure time tracking vector is large enough
    if (piece_index >= time_below_threshold_.size()) {
        time_below_threshold_.resize(piece_index + 1, 0.0f);
    }

    // Check velocity and angular velocity thresholds
    float speed = std::sqrt(
        piece.velocity.x * piece.velocity.x +
        piece.velocity.y * piece.velocity.y +
        piece.velocity.z * piece.velocity.z
    );

    float angular_speed = std::sqrt(
        piece.angular_velocity.x * piece.angular_velocity.x +
        piece.angular_velocity.y * piece.angular_velocity.y +
        piece.angular_velocity.z * piece.angular_velocity.z
    );

    if (speed < config_.sleep_velocity_threshold &&
        angular_speed < config_.sleep_angular_threshold) {
        time_below_threshold_[piece_index] += delta_time;

        if (time_below_threshold_[piece_index] >= config_.sleep_time_required) {
            return true;
        }
    } else {
        time_below_threshold_[piece_index] = 0.0f;
    }

    return false;
}

// Main update function
void DebrisPhysics::update(std::vector<DebrisPiece>& debris_pieces, float delta_time) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Reset stats
    stats_ = Statistics{};
    collision_contacts_.clear();

    // Integrate physics for all pieces
    for (auto& piece : debris_pieces) {
        if (piece.is_sleeping) {
            stats_.sleeping_pieces++;
        } else {
            integrate_piece(piece, delta_time);
            stats_.active_pieces++;
        }
    }

    // Detect collisions
    collision_contacts_ = detect_collisions(debris_pieces);

    // Resolve collisions (multiple iterations for stability)
    for (uint32_t iteration = 0; iteration < config_.max_collision_iterations; ++iteration) {
        for (const auto& contact : collision_contacts_) {
            resolve_collision(
                debris_pieces[contact.debris_a_index],
                debris_pieces[contact.debris_b_index],
                contact
            );
        }
    }

    // Check for sleeping state
    for (size_t i = 0; i < debris_pieces.size(); ++i) {
        if (!debris_pieces[i].is_sleeping && should_sleep(debris_pieces[i], static_cast<uint32_t>(i), delta_time)) {
            debris_pieces[i].is_sleeping = true;
            debris_pieces[i].velocity = {0, 0, 0};
            debris_pieces[i].angular_velocity = {0, 0, 0};
            stats_.sleeping_pieces++;
            stats_.active_pieces--;

            LOG_DEBUG(Game, "Debris piece {} entered sleeping state", i);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.update_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
}

// Apply impulse at point
void DebrisPhysics::apply_impulse(
    DebrisPiece& piece,
    const math::Vec3& impulse,
    const math::Vec3& world_point)
{
    // Linear impulse
    piece.velocity.x += impulse.x / piece.mass;
    piece.velocity.y += impulse.y / piece.mass;
    piece.velocity.z += impulse.z / piece.mass;

    // Angular impulse (torque = r × impulse)
    math::Vec3 r = {
        world_point.x - piece.position.x,
        world_point.y - piece.position.y,
        world_point.z - piece.position.z
    };

    math::Vec3 torque = {
        r.y * impulse.z - r.z * impulse.y,
        r.z * impulse.x - r.x * impulse.z,
        r.x * impulse.y - r.y * impulse.x
    };

    // Apply angular impulse (simplified - using diagonal inertia)
    piece.angular_velocity.x += torque.x / piece.inertia_tensor.x;
    piece.angular_velocity.y += torque.y / piece.inertia_tensor.y;
    piece.angular_velocity.z += torque.z / piece.inertia_tensor.z;

    // Wake up piece
    wake_up(piece);
}

// Apply explosion
void DebrisPhysics::apply_explosion(
    std::vector<DebrisPiece>& debris_pieces,
    const math::Vec3& explosion_center,
    float explosion_force,
    float explosion_radius)
{
    for (auto& piece : debris_pieces) {
        // Calculate direction from explosion to piece
        math::Vec3 dir = {
            piece.position.x - explosion_center.x,
            piece.position.y - explosion_center.y,
            piece.position.z - explosion_center.z
        };

        float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

        if (dist < explosion_radius && dist > 1e-6f) {
            // Normalize direction
            dir.x /= dist;
            dir.y /= dist;
            dir.z /= dist;

            // Falloff with distance (inverse square)
            float falloff = 1.0f - (dist / explosion_radius);
            falloff = falloff * falloff; // Square falloff

            // Apply impulse
            math::Vec3 impulse = {
                dir.x * explosion_force * falloff,
                dir.y * explosion_force * falloff,
                dir.z * explosion_force * falloff
            };

            apply_impulse(piece, impulse, piece.position);
        }
    }

    LOG_INFO(Game, "Explosion applied: center=({:.2f}, {:.2f}, {:.2f}), force={:.0f}N, radius={:.2f}m",
            explosion_center.x, explosion_center.y, explosion_center.z, explosion_force, explosion_radius);
}

// Wake up piece
void DebrisPhysics::wake_up(DebrisPiece& piece) {
    piece.is_sleeping = false;
}

} // namespace lore::world
