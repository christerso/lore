#include <lore/world/fluid_debris_interaction.hpp>
#include <lore/core/log.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>

using namespace lore::core;

namespace lore::world {

// FluidGrid implementation
FluidCell FluidGrid::sample_at_position(const math::Vec3& world_pos) const {
    uint32_t x, y, z;
    if (world_to_grid(world_pos, x, y, z)) {
        const FluidCell* cell = get_cell(x, y, z);
        if (cell) {
            return *cell;
        }
    }

    // Return air cell if outside grid
    FluidCell air_cell;
    air_cell.type = FluidCell::Type::Air;
    air_cell.density = 0.0f;
    return air_cell;
}

bool FluidGrid::world_to_grid(const math::Vec3& world_pos, uint32_t& x, uint32_t& y, uint32_t& z) const {
    // Convert world position to grid coordinates
    float fx = (world_pos.x - origin.x) / cell_size.x;
    float fy = (world_pos.y - origin.y) / cell_size.y;
    float fz = (world_pos.z - origin.z) / cell_size.z;

    // Check bounds
    if (fx < 0.0f || fy < 0.0f || fz < 0.0f) return false;

    x = static_cast<uint32_t>(fx);
    y = static_cast<uint32_t>(fy);
    z = static_cast<uint32_t>(fz);

    return x < width && y < height && z < depth;
}

const FluidCell* FluidGrid::get_cell(uint32_t x, uint32_t y, uint32_t z) const {
    if (x >= width || y >= height || z >= depth) return nullptr;

    uint32_t index = z * (width * height) + y * width + x;
    if (index >= cells.size()) return nullptr;

    return &cells[index];
}

// FluidDebrisInteraction implementation
FluidDebrisInteraction::FluidDebrisInteraction(const FluidDebrisConfig& config)
    : config_(config)
{
    LOG_INFO(Game, "FluidDebrisInteraction initialized (buoyancy={}, drag={})",
            config_.enable_buoyancy, config_.enable_drag);
}

// Rotate vector by quaternion
math::Vec3 FluidDebrisInteraction::rotate_vector(const math::Vec3& v, const math::Quat& q) {
    // q * v * q^-1 (Hamilton product)
    // Optimized formula: v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)

    math::Vec3 qxyz = {q.x, q.y, q.z};

    // cross(q.xyz, v)
    math::Vec3 cross1 = {
        qxyz.y * v.z - qxyz.z * v.y,
        qxyz.z * v.x - qxyz.x * v.z,
        qxyz.x * v.y - qxyz.y * v.x
    };

    // cross1 + q.w * v
    math::Vec3 temp = {
        cross1.x + q.w * v.x,
        cross1.y + q.w * v.y,
        cross1.z + q.w * v.z
    };

    // cross(q.xyz, temp)
    math::Vec3 cross2 = {
        qxyz.y * temp.z - qxyz.z * temp.y,
        qxyz.z * temp.x - qxyz.x * temp.z,
        qxyz.x * temp.y - qxyz.y * temp.x
    };

    // v + 2 * cross2
    return {
        v.x + 2.0f * cross2.x,
        v.y + 2.0f * cross2.y,
        v.z + 2.0f * cross2.z
    };
}

// Update voxel positions after debris movement
void FluidDebrisInteraction::update_voxel_positions(DebrisPiece& piece) const {
    for (auto& voxel : piece.voxel_approximation) {
        // Transform local voxel position to world space
        // world_pos = debris_position + rotate(local_pos - centroid, debris_rotation)

        math::Vec3 local_relative = {
            voxel.position.x - piece.centroid.x,
            voxel.position.y - piece.centroid.y,
            voxel.position.z - piece.centroid.z
        };

        math::Vec3 rotated = rotate_vector(local_relative, piece.rotation);

        // Note: voxel.position is stored in local space
        // For world space queries, we transform on-the-fly in other functions
        // This function updates nothing because voxels stay in local space
        // The transformation happens during fluid sampling
    }
}

// Calculate submerged volume
float FluidDebrisInteraction::calculate_submerged_volume(
    const DebrisPiece& piece,
    const FluidGrid& fluid_grid) const
{
    if (piece.voxel_approximation.empty()) return 0.0f;

    // Voxel grid is 4×4×4 = 64 voxels
    // Each voxel represents a portion of the debris piece
    float voxel_volume = ((piece.aabb_max.x - piece.aabb_min.x) / 4.0f) *
                        ((piece.aabb_max.y - piece.aabb_min.y) / 4.0f) *
                        ((piece.aabb_max.z - piece.aabb_min.z) / 4.0f);

    uint32_t submerged_count = 0;

    for (const auto& voxel : piece.voxel_approximation) {
        if (!voxel.is_occupied) continue;  // Skip empty voxels

        // Transform voxel to world space
        math::Vec3 local_relative = {
            voxel.position.x - piece.centroid.x,
            voxel.position.y - piece.centroid.y,
            voxel.position.z - piece.centroid.z
        };

        math::Vec3 rotated = rotate_vector(local_relative, piece.rotation);

        math::Vec3 world_pos = {
            piece.position.x + rotated.x,
            piece.position.y + rotated.y,
            piece.position.z + rotated.z
        };

        // Sample fluid at voxel position
        FluidCell fluid = fluid_grid.sample_at_position(world_pos);

        // Check if voxel is in fluid
        if (fluid.density >= config_.voxel_collision_threshold) {
            submerged_count++;
        }
    }

    return static_cast<float>(submerged_count) * voxel_volume;
}

// Calculate buoyancy force
math::Vec3 FluidDebrisInteraction::calculate_buoyancy(
    const DebrisPiece& piece,
    const FluidGrid& fluid_grid) const
{
    if (!config_.enable_buoyancy) {
        return {0, 0, 0};
    }

    // Archimedes principle: F_buoyancy = ρ_fluid × V_displaced × g
    float submerged_volume = calculate_submerged_volume(piece, fluid_grid);

    if (submerged_volume < 1e-6f) {
        return {0, 0, 0};  // No buoyancy if not submerged
    }

    float buoyancy_force = config_.fluid_density * submerged_volume * config_.gravity_magnitude;

    // Buoyancy acts upward
    return {0, buoyancy_force, 0};
}

// Calculate drag area
float FluidDebrisInteraction::calculate_drag_area(
    const DebrisPiece& piece,
    const math::Vec3& velocity_direction) const
{
    // Approximate cross-sectional area from AABB
    // Project AABB onto plane perpendicular to velocity

    float dx = piece.aabb_max.x - piece.aabb_min.x;
    float dy = piece.aabb_max.y - piece.aabb_min.y;
    float dz = piece.aabb_max.z - piece.aabb_min.z;

    // Find dominant velocity component
    float abs_vx = std::abs(velocity_direction.x);
    float abs_vy = std::abs(velocity_direction.y);
    float abs_vz = std::abs(velocity_direction.z);

    // Approximate area based on dominant direction
    if (abs_vx > abs_vy && abs_vx > abs_vz) {
        // Moving primarily in X: use YZ plane
        return dy * dz;
    } else if (abs_vy > abs_vz) {
        // Moving primarily in Y: use XZ plane
        return dx * dz;
    } else {
        // Moving primarily in Z: use XY plane
        return dx * dy;
    }
}

// Calculate drag force
math::Vec3 FluidDebrisInteraction::calculate_drag(
    const DebrisPiece& piece,
    const FluidGrid& fluid_grid) const
{
    if (!config_.enable_drag) {
        return {0, 0, 0};
    }

    // Sample fluid at debris center
    FluidCell fluid = fluid_grid.sample_at_position(piece.position);

    if (fluid.density < config_.voxel_collision_threshold) {
        return {0, 0, 0};  // No drag in air
    }

    // Relative velocity (debris - fluid)
    math::Vec3 relative_velocity = {
        piece.velocity.x - fluid.velocity.x,
        piece.velocity.y - fluid.velocity.y,
        piece.velocity.z - fluid.velocity.z
    };

    float speed = std::sqrt(
        relative_velocity.x * relative_velocity.x +
        relative_velocity.y * relative_velocity.y +
        relative_velocity.z * relative_velocity.z
    );

    if (speed < 1e-6f) {
        return {0, 0, 0};  // No drag if no relative motion
    }

    // Velocity direction
    math::Vec3 velocity_dir = {
        relative_velocity.x / speed,
        relative_velocity.y / speed,
        relative_velocity.z / speed
    };

    // Drag formula: F_drag = 0.5 × ρ × v² × C_d × A
    float drag_area = calculate_drag_area(piece, velocity_dir);
    float drag_magnitude = 0.5f * fluid.density * config_.fluid_density * speed * speed *
                          config_.linear_drag_coefficient * drag_area;

    // Drag opposes motion
    return {
        -velocity_dir.x * drag_magnitude,
        -velocity_dir.y * drag_magnitude,
        -velocity_dir.z * drag_magnitude
    };
}

// Calculate angular drag
math::Vec3 FluidDebrisInteraction::calculate_angular_drag(
    const DebrisPiece& piece,
    const FluidGrid& fluid_grid) const
{
    if (!config_.enable_drag) {
        return {0, 0, 0};
    }

    // Sample fluid at debris center
    FluidCell fluid = fluid_grid.sample_at_position(piece.position);

    if (fluid.density < config_.voxel_collision_threshold) {
        return {0, 0, 0};
    }

    // Angular drag opposes rotation
    float angular_speed = std::sqrt(
        piece.angular_velocity.x * piece.angular_velocity.x +
        piece.angular_velocity.y * piece.angular_velocity.y +
        piece.angular_velocity.z * piece.angular_velocity.z
    );

    if (angular_speed < 1e-6f) {
        return {0, 0, 0};
    }

    // Simplified angular drag: τ = -k × ω
    float angular_drag_strength = fluid.density * config_.fluid_density *
                                 config_.angular_drag_coefficient * 0.01f;

    return {
        -piece.angular_velocity.x * angular_drag_strength,
        -piece.angular_velocity.y * angular_drag_strength,
        -piece.angular_velocity.z * angular_drag_strength
    };
}

// Calculate flow force
math::Vec3 FluidDebrisInteraction::calculate_flow_force(
    const DebrisPiece& piece,
    const FluidGrid& fluid_grid) const
{
    if (!config_.enable_flow_influence) {
        return {0, 0, 0};
    }

    if (piece.voxel_approximation.empty()) {
        return {0, 0, 0};
    }

    // Accumulate flow forces from all submerged voxels
    math::Vec3 total_flow_force{0, 0, 0};
    uint32_t submerged_count = 0;

    for (const auto& voxel : piece.voxel_approximation) {
        if (!voxel.is_occupied) continue;

        // Transform voxel to world space
        math::Vec3 local_relative = {
            voxel.position.x - piece.centroid.x,
            voxel.position.y - piece.centroid.y,
            voxel.position.z - piece.centroid.z
        };

        math::Vec3 rotated = rotate_vector(local_relative, piece.rotation);

        math::Vec3 world_pos = {
            piece.position.x + rotated.x,
            piece.position.y + rotated.y,
            piece.position.z + rotated.z
        };

        // Sample fluid at voxel
        FluidCell fluid = fluid_grid.sample_at_position(world_pos);

        if (fluid.density >= config_.voxel_collision_threshold) {
            // Add flow velocity weighted by fluid density
            total_flow_force.x += fluid.velocity.x * fluid.density;
            total_flow_force.y += fluid.velocity.y * fluid.density;
            total_flow_force.z += fluid.velocity.z * fluid.density;
            submerged_count++;
        }
    }

    if (submerged_count == 0) {
        return {0, 0, 0};
    }

    // Average flow force
    float inv_count = 1.0f / static_cast<float>(submerged_count);
    total_flow_force.x *= inv_count * piece.mass * config_.flow_influence_strength;
    total_flow_force.y *= inv_count * piece.mass * config_.flow_influence_strength;
    total_flow_force.z *= inv_count * piece.mass * config_.flow_influence_strength;

    return total_flow_force;
}

// Check if debris is submerged
bool FluidDebrisInteraction::is_submerged(
    const DebrisPiece& piece,
    const FluidGrid& fluid_grid,
    float* out_submerged_ratio) const
{
    if (piece.voxel_approximation.empty()) {
        if (out_submerged_ratio) *out_submerged_ratio = 0.0f;
        return false;
    }

    uint32_t occupied_count = 0;
    uint32_t submerged_count = 0;

    for (const auto& voxel : piece.voxel_approximation) {
        if (!voxel.is_occupied) continue;

        occupied_count++;

        // Transform voxel to world space
        math::Vec3 local_relative = {
            voxel.position.x - piece.centroid.x,
            voxel.position.y - piece.centroid.y,
            voxel.position.z - piece.centroid.z
        };

        math::Vec3 rotated = rotate_vector(local_relative, piece.rotation);

        math::Vec3 world_pos = {
            piece.position.x + rotated.x,
            piece.position.y + rotated.y,
            piece.position.z + rotated.z
        };

        // Sample fluid
        FluidCell fluid = fluid_grid.sample_at_position(world_pos);

        if (fluid.density >= config_.voxel_collision_threshold) {
            submerged_count++;
        }
    }

    if (out_submerged_ratio && occupied_count > 0) {
        *out_submerged_ratio = static_cast<float>(submerged_count) /
                               static_cast<float>(occupied_count);
    }

    return submerged_count > 0;
}

// Update system
void FluidDebrisInteraction::update(
    std::vector<DebrisPiece>& debris_pieces,
    const FluidGrid& fluid_grid,
    float delta_time)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    stats_ = Statistics{};  // Reset stats

    uint32_t processed = 0;
    float total_buoyancy = 0.0f;
    float total_drag = 0.0f;

    for (auto& piece : debris_pieces) {
        // Skip sleeping debris for performance
        if (config_.skip_sleeping_debris && piece.is_sleeping) {
            continue;
        }

        // Limit processing for performance
        if (processed >= config_.max_debris_to_process) {
            break;
        }

        processed++;

        // Check if debris is in fluid
        float submerged_ratio = 0.0f;
        bool in_fluid = is_submerged(piece, fluid_grid, &submerged_ratio);

        if (!in_fluid) {
            continue;  // Not in fluid, skip
        }

        stats_.debris_submerged++;
        stats_.voxels_in_fluid += static_cast<uint32_t>(
            submerged_ratio * piece.voxel_approximation.size()
        );

        // Calculate forces
        math::Vec3 buoyancy = calculate_buoyancy(piece, fluid_grid);
        math::Vec3 drag = calculate_drag(piece, fluid_grid);
        math::Vec3 angular_drag = calculate_angular_drag(piece, fluid_grid);
        math::Vec3 flow = calculate_flow_force(piece, fluid_grid);

        // Apply buoyancy
        if (config_.enable_buoyancy) {
            piece.velocity.x += (buoyancy.x / piece.mass) * delta_time;
            piece.velocity.y += (buoyancy.y / piece.mass) * delta_time;
            piece.velocity.z += (buoyancy.z / piece.mass) * delta_time;

            float buoyancy_mag = std::sqrt(buoyancy.x * buoyancy.x +
                                          buoyancy.y * buoyancy.y +
                                          buoyancy.z * buoyancy.z);
            total_buoyancy += buoyancy_mag;
        }

        // Apply drag
        if (config_.enable_drag) {
            piece.velocity.x += (drag.x / piece.mass) * delta_time;
            piece.velocity.y += (drag.y / piece.mass) * delta_time;
            piece.velocity.z += (drag.z / piece.mass) * delta_time;

            // Apply angular drag
            piece.angular_velocity.x += (angular_drag.x / piece.inertia_tensor.x) * delta_time;
            piece.angular_velocity.y += (angular_drag.y / piece.inertia_tensor.y) * delta_time;
            piece.angular_velocity.z += (angular_drag.z / piece.inertia_tensor.z) * delta_time;

            float drag_mag = std::sqrt(drag.x * drag.x + drag.y * drag.y + drag.z * drag.z);
            total_drag += drag_mag;
        }

        // Apply flow influence
        if (config_.enable_flow_influence) {
            piece.velocity.x += (flow.x / piece.mass) * delta_time;
            piece.velocity.y += (flow.y / piece.mass) * delta_time;
            piece.velocity.z += (flow.z / piece.mass) * delta_time;
        }

        // Wake up debris if significant fluid interaction
        if (piece.is_sleeping) {
            float force_mag = std::sqrt(
                (buoyancy.x + drag.x + flow.x) * (buoyancy.x + drag.x + flow.x) +
                (buoyancy.y + drag.y + flow.y) * (buoyancy.y + drag.y + flow.y) +
                (buoyancy.z + drag.z + flow.z) * (buoyancy.z + drag.z + flow.z)
            );

            if (force_mag > 1.0f) {  // Threshold for waking
                piece.is_sleeping = false;
            }
        }
    }

    stats_.debris_processed = processed;
    stats_.average_buoyancy = stats_.debris_submerged > 0 ?
        total_buoyancy / static_cast<float>(stats_.debris_submerged) : 0.0f;
    stats_.average_drag = stats_.debris_submerged > 0 ?
        total_drag / static_cast<float>(stats_.debris_submerged) : 0.0f;

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.update_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    if (stats_.debris_submerged > 0) {
        LOG_DEBUG(Game, "Fluid-debris: {}/{} submerged, avg buoyancy={:.1f}N, avg drag={:.1f}N, {:.2f}ms",
                 stats_.debris_submerged, stats_.debris_processed,
                 stats_.average_buoyancy, stats_.average_drag, stats_.update_time_ms);
    }
}

} // namespace lore::world
