#include <lore/math/math.hpp>

#include <immintrin.h>
#include <random>
#include <cmath>
#include <algorithm>

namespace lore::math {

// Global constants
static PhysicsConstants g_physics_constants;

// Random number generator
static thread_local std::random_device rd;
static thread_local std::mt19937 rng(rd());

// Transform implementation
Vec3 Transform::get_forward() const noexcept {
    return glm::normalize(rotation * Vec3(0.0f, 0.0f, -1.0f));
}

Vec3 Transform::get_right() const noexcept {
    return glm::normalize(rotation * Vec3(1.0f, 0.0f, 0.0f));
}

Vec3 Transform::get_up() const noexcept {
    return glm::normalize(rotation * Vec3(0.0f, 1.0f, 0.0f));
}

Mat4 Transform::get_matrix() const noexcept {
    Mat4 translation_matrix = glm::translate(Mat4(1.0f), position);
    Mat4 rotation_matrix = glm::mat4_cast(rotation);
    Mat4 scale_matrix = glm::scale(Mat4(1.0f), scale);

    return translation_matrix * rotation_matrix * scale_matrix;
}

void Transform::set_rotation_euler(const Vec3& euler_angles) noexcept {
    rotation = glm::quat(euler_angles);
}

void Transform::translate(const Vec3& delta) noexcept {
    position += delta;
}

void Transform::rotate(const Quat& delta_rotation) noexcept {
    rotation = glm::normalize(rotation * delta_rotation);
}

void Transform::rotate_around_axis(const Vec3& axis, float angle) noexcept {
    Quat axis_rotation = glm::angleAxis(angle, glm::normalize(axis));
    rotate(axis_rotation);
}

void Transform::look_at(const Vec3& target, const Vec3& up) noexcept {
    Vec3 forward = glm::normalize(target - position);
    Vec3 right = glm::normalize(glm::cross(forward, up));
    Vec3 corrected_up = glm::cross(right, forward);

    Mat3 rotation_matrix;
    rotation_matrix[0] = right;
    rotation_matrix[1] = corrected_up;
    rotation_matrix[2] = -forward;

    rotation = glm::quat_cast(rotation_matrix);
}

// SIMD utility functions
namespace simd {

Vec3SIMD load_vec3(const Vec3& v) noexcept {
    return _mm_set_ps(0.0f, v.z, v.y, v.x);
}

Vec4SIMD load_vec4(const Vec4& v) noexcept {
    return _mm_set_ps(v.w, v.z, v.y, v.x);
}

void store_vec3(Vec3& v, Vec3SIMD simd) noexcept {
    alignas(16) float temp[4];
    _mm_store_ps(temp, simd);
    v.x = temp[0];
    v.y = temp[1];
    v.z = temp[2];
}

void store_vec4(Vec4& v, Vec4SIMD simd) noexcept {
    alignas(16) float temp[4];
    _mm_store_ps(temp, simd);
    v.x = temp[0];
    v.y = temp[1];
    v.z = temp[2];
    v.w = temp[3];
}

Vec3SIMD add(Vec3SIMD a, Vec3SIMD b) noexcept {
    return _mm_add_ps(a, b);
}

Vec3SIMD sub(Vec3SIMD a, Vec3SIMD b) noexcept {
    return _mm_sub_ps(a, b);
}

Vec3SIMD mul(Vec3SIMD a, Vec3SIMD b) noexcept {
    return _mm_mul_ps(a, b);
}

Vec3SIMD mul_scalar(Vec3SIMD v, float scalar) noexcept {
    Vec3SIMD scalar_vec = _mm_set1_ps(scalar);
    return _mm_mul_ps(v, scalar_vec);
}

Vec3SIMD dot(Vec3SIMD a, Vec3SIMD b) noexcept {
    Vec3SIMD mul_result = _mm_mul_ps(a, b);

    // Sum the first 3 components
    Vec3SIMD x = _mm_shuffle_ps(mul_result, mul_result, _MM_SHUFFLE(0, 0, 0, 0));
    Vec3SIMD y = _mm_shuffle_ps(mul_result, mul_result, _MM_SHUFFLE(1, 1, 1, 1));
    Vec3SIMD z = _mm_shuffle_ps(mul_result, mul_result, _MM_SHUFFLE(2, 2, 2, 2));

    Vec3SIMD sum = _mm_add_ps(_mm_add_ps(x, y), z);
    return sum;
}

Vec3SIMD cross(Vec3SIMD a, Vec3SIMD b) noexcept {
    // a.yzx
    Vec3SIMD a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
    // b.zxy
    Vec3SIMD b_zxy = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2));
    // a.zxy
    Vec3SIMD a_zxy = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));
    // b.yzx
    Vec3SIMD b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));

    Vec3SIMD result = _mm_sub_ps(_mm_mul_ps(a_yzx, b_zxy), _mm_mul_ps(a_zxy, b_yzx));
    return result;
}

Vec3SIMD normalize(Vec3SIMD v) noexcept {
    Vec3SIMD dot_result = dot(v, v);
    Vec3SIMD length = _mm_sqrt_ps(dot_result);

    // Avoid division by zero
    Vec3SIMD epsilon = _mm_set1_ps(1e-8f);
    Vec3SIMD safe_length = _mm_max_ps(length, epsilon);

    return _mm_div_ps(v, safe_length);
}

Vec3SIMD length(Vec3SIMD v) noexcept {
    return _mm_sqrt_ps(dot(v, v));
}

Vec3SIMD length_squared(Vec3SIMD v) noexcept {
    return dot(v, v);
}

Mat4SIMD load_mat4(const Mat4& m) noexcept {
    Mat4SIMD result;
    result[0] = _mm_load_ps(&m[0][0]);
    result[1] = _mm_load_ps(&m[1][0]);
    result[2] = _mm_load_ps(&m[2][0]);
    result[3] = _mm_load_ps(&m[3][0]);
    return result;
}

void store_mat4(Mat4& m, const Mat4SIMD& simd) noexcept {
    _mm_store_ps(&m[0][0], simd[0]);
    _mm_store_ps(&m[1][0], simd[1]);
    _mm_store_ps(&m[2][0], simd[2]);
    _mm_store_ps(&m[3][0], simd[3]);
}

Mat4SIMD multiply(const Mat4SIMD& a, const Mat4SIMD& b) noexcept {
    Mat4SIMD result;

    for (std::size_t i = 0; i < 4; ++i) {
        Vec4SIMD x = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(0, 0, 0, 0));
        Vec4SIMD y = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(1, 1, 1, 1));
        Vec4SIMD z = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(2, 2, 2, 2));
        Vec4SIMD w = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(3, 3, 3, 3));

        result[i] = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(x, b[0]), _mm_mul_ps(y, b[1])),
            _mm_add_ps(_mm_mul_ps(z, b[2]), _mm_mul_ps(w, b[3]))
        );
    }

    return result;
}

Vec4SIMD transform(const Mat4SIMD& m, Vec4SIMD v) noexcept {
    Vec4SIMD x = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0));
    Vec4SIMD y = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1));
    Vec4SIMD z = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2));
    Vec4SIMD w = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));

    return _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(x, m[0]), _mm_mul_ps(y, m[1])),
        _mm_add_ps(_mm_mul_ps(z, m[2]), _mm_mul_ps(w, m[3]))
    );
}

} // namespace simd

// Utility functions
namespace utils {

float lerp(float a, float b, float t) noexcept {
    return a + t * (b - a);
}

Vec3 lerp(const Vec3& a, const Vec3& b, float t) noexcept {
    return a + t * (b - a);
}

Quat slerp(const Quat& a, const Quat& b, float t) noexcept {
    return glm::slerp(a, b, t);
}

float clamp(float value, float min, float max) noexcept {
    return std::clamp(value, min, max);
}

Vec3 clamp(const Vec3& value, const Vec3& min, const Vec3& max) noexcept {
    return Vec3(
        std::clamp(value.x, min.x, max.x),
        std::clamp(value.y, min.y, max.y),
        std::clamp(value.z, min.z, max.z)
    );
}

bool approximately_equal(float a, float b, float epsilon) noexcept {
    return std::abs(a - b) <= epsilon;
}

bool approximately_equal(const Vec3& a, const Vec3& b, float epsilon) noexcept {
    return approximately_equal(a.x, b.x, epsilon) &&
           approximately_equal(a.y, b.y, epsilon) &&
           approximately_equal(a.z, b.z, epsilon);
}

float random_float(float min, float max) noexcept {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

Vec3 random_vec3(const Vec3& min, const Vec3& max) noexcept {
    return Vec3(
        random_float(min.x, max.x),
        random_float(min.y, max.y),
        random_float(min.z, max.z)
    );
}

Vec3 random_unit_vector() noexcept {
    // Use rejection sampling for uniform distribution on sphere
    float x, y, z, norm_sq;
    do {
        x = random_float(-1.0f, 1.0f);
        y = random_float(-1.0f, 1.0f);
        z = random_float(-1.0f, 1.0f);
        norm_sq = x * x + y * y + z * z;
    } while (norm_sq > 1.0f || norm_sq < 1e-8f);

    float inv_norm = 1.0f / std::sqrt(norm_sq);
    return Vec3(x * inv_norm, y * inv_norm, z * inv_norm);
}

} // namespace utils

// Geometric shapes
namespace geometry {

// AABB implementation
Vec3 AABB::get_center() const noexcept {
    return (min + max) * 0.5f;
}

Vec3 AABB::get_size() const noexcept {
    return max - min;
}

Vec3 AABB::get_half_size() const noexcept {
    return get_size() * 0.5f;
}

void AABB::set_center(const Vec3& center) noexcept {
    Vec3 half_size = get_half_size();
    min = center - half_size;
    max = center + half_size;
}

void AABB::set_size(const Vec3& size) noexcept {
    Vec3 center = get_center();
    Vec3 half_size = size * 0.5f;
    min = center - half_size;
    max = center + half_size;
}

bool AABB::contains(const Vec3& point) const noexcept {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

bool AABB::intersects(const AABB& other) const noexcept {
    return max.x >= other.min.x && min.x <= other.max.x &&
           max.y >= other.min.y && min.y <= other.max.y &&
           max.z >= other.min.z && min.z <= other.max.z;
}

void AABB::expand_to_include(const Vec3& point) noexcept {
    min = glm::min(min, point);
    max = glm::max(max, point);
}

void AABB::expand_to_include(const AABB& other) noexcept {
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
}

// Sphere implementation
bool Sphere::contains(const Vec3& point) const noexcept {
    Vec3 diff = point - center;
    float distance_sq = glm::dot(diff, diff);
    return distance_sq <= radius * radius;
}

bool Sphere::intersects(const Sphere& other) const noexcept {
    Vec3 diff = center - other.center;
    float distance_sq = glm::dot(diff, diff);
    float radius_sum = radius + other.radius;
    return distance_sq <= radius_sum * radius_sum;
}

bool Sphere::intersects(const AABB& aabb) const noexcept {
    Vec3 closest_point = glm::clamp(center, aabb.min, aabb.max);
    Vec3 diff = center - closest_point;
    float distance_sq = glm::dot(diff, diff);
    return distance_sq <= radius * radius;
}

// Plane implementation
float Plane::distance_to_point(const Vec3& point) const noexcept {
    return glm::dot(normal, point) + distance;
}

Vec3 Plane::closest_point(const Vec3& point) const noexcept {
    float dist = distance_to_point(point);
    return point - dist * normal;
}

// Ray implementation
Vec3 Ray::point_at(float t) const noexcept {
    return origin + t * direction;
}

bool Ray::intersects_sphere(const Sphere& sphere, float& t) const noexcept {
    Vec3 oc = origin - sphere.center;
    float a = glm::dot(direction, direction);
    float b = 2.0f * glm::dot(oc, direction);
    float c = glm::dot(oc, oc) - sphere.radius * sphere.radius;

    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0) {
        return false;
    }

    float sqrt_discriminant = std::sqrt(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);

    if (t1 >= 0) {
        t = t1;
        return true;
    } else if (t2 >= 0) {
        t = t2;
        return true;
    }

    return false;
}

bool Ray::intersects_aabb(const AABB& aabb, float& t) const noexcept {
    Vec3 inv_dir = 1.0f / direction;
    Vec3 t_min = (aabb.min - origin) * inv_dir;
    Vec3 t_max = (aabb.max - origin) * inv_dir;

    // Ensure t_min contains the smaller values
    Vec3 t1 = glm::min(t_min, t_max);
    Vec3 t2 = glm::max(t_min, t_max);

    float t_near = std::max({t1.x, t1.y, t1.z});
    float t_far = std::min({t2.x, t2.y, t2.z});

    if (t_near <= t_far && t_far >= 0) {
        t = t_near >= 0 ? t_near : t_far;
        return true;
    }

    return false;
}

bool Ray::intersects_plane(const Plane& plane, float& t) const noexcept {
    float denom = glm::dot(plane.normal, direction);

    if (std::abs(denom) < 1e-6f) {
        return false; // Ray is parallel to plane
    }

    t = -(glm::dot(plane.normal, origin) + plane.distance) / denom;
    return t >= 0;
}

} // namespace geometry

} // namespace lore::math