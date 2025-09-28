#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/constants.hpp>
#include <immintrin.h>
#include <cstdint>
#include <array>

namespace lore::math {

// SIMD-optimized math types inspired by Aeon's physics engine
// Designed for high-performance parallel processing

// SIMD vector types
using Vec3SIMD = __m128;
using Vec4SIMD = __m128;
using Mat4SIMD = std::array<__m128, 4>;

// Standard GLM types
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;
using Quat = glm::quat;

// Physics constants structure
struct PhysicsConstants {
    float gravity = 9.81f;
    float air_density = 1.225f;
    float water_density = 1000.0f;
    float default_friction = 0.5f;
    float default_restitution = 0.3f;
    float sleep_threshold = 0.01f;
    float penetration_slop = 0.01f;
    float baumgarte_factor = 0.2f;
    float max_linear_velocity = 100.0f;
    float max_angular_velocity = 100.0f;

    // Getters
    float get_gravity() const noexcept { return gravity; }
    float get_air_density() const noexcept { return air_density; }
    float get_water_density() const noexcept { return water_density; }
    float get_default_friction() const noexcept { return default_friction; }
    float get_default_restitution() const noexcept { return default_restitution; }

    // Setters
    void set_gravity(float value) noexcept { gravity = value; }
    void set_air_density(float value) noexcept { air_density = value; }
    void set_water_density(float value) noexcept { water_density = value; }
    void set_default_friction(float value) noexcept { default_friction = value; }
    void set_default_restitution(float value) noexcept { default_restitution = value; }
};

// Transform component
struct Transform {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f};

    // Getters
    const Vec3& get_position() const noexcept { return position; }
    const Quat& get_rotation() const noexcept { return rotation; }
    const Vec3& get_scale() const noexcept { return scale; }
    Vec3 get_forward() const noexcept;
    Vec3 get_right() const noexcept;
    Vec3 get_up() const noexcept;
    Mat4 get_matrix() const noexcept;

    // Setters
    void set_position(const Vec3& pos) noexcept { position = pos; }
    void set_rotation(const Quat& rot) noexcept { rotation = rot; }
    void set_scale(const Vec3& scl) noexcept { scale = scl; }
    void set_rotation_euler(const Vec3& euler_angles) noexcept;

    // Transform operations
    void translate(const Vec3& delta) noexcept;
    void rotate(const Quat& delta_rotation) noexcept;
    void rotate_around_axis(const Vec3& axis, float angle) noexcept;
    void look_at(const Vec3& target, const Vec3& up = Vec3(0, 1, 0)) noexcept;
};

// SIMD utility functions
namespace simd {
    // Load/store operations
    Vec3SIMD load_vec3(const Vec3& v) noexcept;
    Vec4SIMD load_vec4(const Vec4& v) noexcept;
    void store_vec3(Vec3& v, Vec3SIMD simd) noexcept;
    void store_vec4(Vec4& v, Vec4SIMD simd) noexcept;

    // Vector operations
    Vec3SIMD add(Vec3SIMD a, Vec3SIMD b) noexcept;
    Vec3SIMD sub(Vec3SIMD a, Vec3SIMD b) noexcept;
    Vec3SIMD mul(Vec3SIMD a, Vec3SIMD b) noexcept;
    Vec3SIMD mul_scalar(Vec3SIMD v, float scalar) noexcept;
    Vec3SIMD dot(Vec3SIMD a, Vec3SIMD b) noexcept;
    Vec3SIMD cross(Vec3SIMD a, Vec3SIMD b) noexcept;
    Vec3SIMD normalize(Vec3SIMD v) noexcept;
    Vec3SIMD length(Vec3SIMD v) noexcept;
    Vec3SIMD length_squared(Vec3SIMD v) noexcept;

    // Matrix operations
    Mat4SIMD load_mat4(const Mat4& m) noexcept;
    void store_mat4(Mat4& m, const Mat4SIMD& simd) noexcept;
    Mat4SIMD multiply(const Mat4SIMD& a, const Mat4SIMD& b) noexcept;
    Vec4SIMD transform(const Mat4SIMD& m, Vec4SIMD v) noexcept;
}

// Utility functions
namespace utils {
    // Constants
    constexpr float PI = 3.14159265359f;
    constexpr float TWO_PI = 2.0f * PI;
    constexpr float HALF_PI = 0.5f * PI;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / PI;

    // Angle conversions
    constexpr float degrees_to_radians(float degrees) noexcept { return degrees * DEG_TO_RAD; }
    constexpr float radians_to_degrees(float radians) noexcept { return radians * RAD_TO_DEG; }

    // Interpolation
    float lerp(float a, float b, float t) noexcept;
    Vec3 lerp(const Vec3& a, const Vec3& b, float t) noexcept;
    Quat slerp(const Quat& a, const Quat& b, float t) noexcept;

    // Clamping
    float clamp(float value, float min, float max) noexcept;
    Vec3 clamp(const Vec3& value, const Vec3& min, const Vec3& max) noexcept;

    // Comparison with epsilon
    bool approximately_equal(float a, float b, float epsilon = 1e-6f) noexcept;
    bool approximately_equal(const Vec3& a, const Vec3& b, float epsilon = 1e-6f) noexcept;

    // Random number generation
    float random_float(float min = 0.0f, float max = 1.0f) noexcept;
    Vec3 random_vec3(const Vec3& min, const Vec3& max) noexcept;
    Vec3 random_unit_vector() noexcept;
}

// Geometric shapes for collision/physics
namespace geometry {
    struct AABB {
        Vec3 min;
        Vec3 max;

        // Getters
        Vec3 get_center() const noexcept;
        Vec3 get_size() const noexcept;
        Vec3 get_half_size() const noexcept;

        // Setters
        void set_center(const Vec3& center) noexcept;
        void set_size(const Vec3& size) noexcept;

        // Operations
        bool contains(const Vec3& point) const noexcept;
        bool intersects(const AABB& other) const noexcept;
        void expand_to_include(const Vec3& point) noexcept;
        void expand_to_include(const AABB& other) noexcept;
    };

    struct Sphere {
        Vec3 center;
        float radius;

        // Getters
        const Vec3& get_center() const noexcept { return center; }
        float get_radius() const noexcept { return radius; }

        // Setters
        void set_center(const Vec3& c) noexcept { center = c; }
        void set_radius(float r) noexcept { radius = r; }

        // Operations
        bool contains(const Vec3& point) const noexcept;
        bool intersects(const Sphere& other) const noexcept;
        bool intersects(const AABB& aabb) const noexcept;
    };

    struct Plane {
        Vec3 normal;
        float distance;

        // Getters
        const Vec3& get_normal() const noexcept { return normal; }
        float get_distance() const noexcept { return distance; }

        // Setters
        void set_normal(const Vec3& n) noexcept { normal = n; }
        void set_distance(float d) noexcept { distance = d; }

        // Operations
        float distance_to_point(const Vec3& point) const noexcept;
        Vec3 closest_point(const Vec3& point) const noexcept;
    };

    struct Ray {
        Vec3 origin;
        Vec3 direction;

        // Getters
        const Vec3& get_origin() const noexcept { return origin; }
        const Vec3& get_direction() const noexcept { return direction; }

        // Setters
        void set_origin(const Vec3& o) noexcept { origin = o; }
        void set_direction(const Vec3& d) noexcept { direction = glm::normalize(d); }

        // Operations
        Vec3 point_at(float t) const noexcept;
        bool intersects_sphere(const Sphere& sphere, float& t) const noexcept;
        bool intersects_aabb(const AABB& aabb, float& t) const noexcept;
        bool intersects_plane(const Plane& plane, float& t) const noexcept;
    };
}

} // namespace lore::math