#pragma once

#include <lore/world/voronoi_fracture.hpp>
#include <lore/math/math.hpp>
#include <vector>
#include <cstdint>

namespace lore::world {

/**
 * @brief Physics configuration for debris simulation
 */
struct DebrisPhysicsConfig {
    math::Vec3 gravity{0.0f, -9.81f, 0.0f};  // Gravity acceleration (m/s²)
    float air_drag = 0.1f;                    // Air resistance coefficient (0-1)
    float angular_drag = 0.05f;               // Rotational drag coefficient (0-1)
    float restitution = 0.3f;                 // Bounce coefficient (0-1)
    float friction = 0.5f;                    // Surface friction coefficient (0-1)
    float sleep_velocity_threshold = 0.01f;   // Sleep when |velocity| < threshold
    float sleep_angular_threshold = 0.01f;    // Sleep when |angular_vel| < threshold
    float sleep_time_required = 0.5f;         // Seconds below threshold before sleep
    float collision_margin = 0.01f;           // AABB expansion for collision detection
    uint32_t max_collision_iterations = 4;    // Maximum collision resolution passes
};

/**
 * @brief Collision contact information
 */
struct CollisionContact {
    math::Vec3 point;           // Contact point in world space
    math::Vec3 normal;          // Contact normal (from A to B)
    float penetration_depth;    // How deep objects are overlapping
    uint32_t debris_a_index;    // Index of first debris piece
    uint32_t debris_b_index;    // Index of second debris piece
};

/**
 * @brief Debris physics simulator
 *
 * Handles realistic physics simulation for fractured debris pieces:
 * - Gravity and air resistance
 * - Realistic tumbling and rotation
 * - Debris-to-debris collision detection and response
 * - Ground collision and settling
 * - Sleeping state when velocity drops below threshold
 *
 * Performance: Optimized for 20-100 active debris pieces per frame
 */
class DebrisPhysics {
public:
    explicit DebrisPhysics(const DebrisPhysicsConfig& config = {});

    /**
     * @brief Update debris physics for one timestep
     *
     * Performs:
     * 1. Apply forces (gravity, drag)
     * 2. Integrate velocity and position
     * 3. Update rotation via angular velocity
     * 4. Detect and resolve collisions
     * 5. Check for sleeping state
     *
     * @param debris_pieces Vector of debris to simulate (modified in-place)
     * @param delta_time Time step in seconds (typically 1/60)
     */
    void update(std::vector<DebrisPiece>& debris_pieces, float delta_time);

    /**
     * @brief Apply impulse to debris piece at specific point
     *
     * Used for impacts, explosions, or player interactions.
     *
     * @param piece Debris piece to apply impulse to
     * @param impulse Impulse vector (kg⋅m/s)
     * @param world_point Point where impulse is applied (world space)
     */
    void apply_impulse(
        DebrisPiece& piece,
        const math::Vec3& impulse,
        const math::Vec3& world_point
    );

    /**
     * @brief Apply explosive force to debris pieces
     *
     * Applies radial force from explosion center.
     *
     * @param debris_pieces Vector of debris pieces
     * @param explosion_center Center of explosion (world space)
     * @param explosion_force Force magnitude (Newtons)
     * @param explosion_radius Radius of effect (meters)
     */
    void apply_explosion(
        std::vector<DebrisPiece>& debris_pieces,
        const math::Vec3& explosion_center,
        float explosion_force,
        float explosion_radius
    );

    /**
     * @brief Wake up sleeping debris piece
     *
     * Forces piece to become active again.
     *
     * @param piece Debris piece to wake
     */
    void wake_up(DebrisPiece& piece);

    /**
     * @brief Get/set physics configuration
     */
    const DebrisPhysicsConfig& get_config() const { return config_; }
    void set_config(const DebrisPhysicsConfig& config) { config_ = config; }

    /**
     * @brief Get collision contacts from last update
     *
     * Useful for audio/particle effects at collision points.
     */
    const std::vector<CollisionContact>& get_collision_contacts() const {
        return collision_contacts_;
    }

    /**
     * @brief Statistics from last update
     */
    struct Statistics {
        uint32_t active_pieces = 0;
        uint32_t sleeping_pieces = 0;
        uint32_t collision_checks = 0;
        uint32_t collisions_resolved = 0;
        float update_time_ms = 0.0f;
    };
    const Statistics& get_statistics() const { return stats_; }

private:
    DebrisPhysicsConfig config_;
    std::vector<CollisionContact> collision_contacts_;
    Statistics stats_;

    // Sleeping state tracking (time below threshold per piece)
    std::vector<float> time_below_threshold_;

    /**
     * @brief Integrate physics for one piece
     */
    void integrate_piece(DebrisPiece& piece, float delta_time);

    /**
     * @brief Detect collisions between all debris pieces
     *
     * Uses broad-phase AABB testing then narrow-phase SAT/GJK.
     * Returns list of collision contacts.
     */
    std::vector<CollisionContact> detect_collisions(
        const std::vector<DebrisPiece>& debris_pieces
    );

    /**
     * @brief Resolve collision between two debris pieces
     *
     * Applies impulse-based collision response with friction.
     */
    void resolve_collision(
        DebrisPiece& piece_a,
        DebrisPiece& piece_b,
        const CollisionContact& contact
    );

    /**
     * @brief Check AABB overlap (broad-phase collision)
     */
    bool aabb_overlap(
        const math::Vec3& min_a, const math::Vec3& max_a,
        const math::Vec3& min_b, const math::Vec3& max_b
    ) const;

    /**
     * @brief Narrow-phase collision detection (simplified)
     *
     * Returns contact point, normal, and penetration depth.
     * Simplified implementation uses AABB-based contact generation.
     */
    bool narrow_phase_collision(
        const DebrisPiece& piece_a,
        const DebrisPiece& piece_b,
        CollisionContact& out_contact
    ) const;

    /**
     * @brief Check if debris should transition to sleeping state
     */
    bool should_sleep(const DebrisPiece& piece, uint32_t piece_index, float delta_time);

    /**
     * @brief Rotate vector by quaternion
     */
    static math::Vec3 rotate_vector(const math::Vec3& v, const math::Quat& q);

    /**
     * @brief Normalize quaternion
     */
    static void normalize_quaternion(math::Quat& q);

    /**
     * @brief Multiply two quaternions
     */
    static math::Quat quaternion_multiply(const math::Quat& a, const math::Quat& b);
};

} // namespace lore::world
