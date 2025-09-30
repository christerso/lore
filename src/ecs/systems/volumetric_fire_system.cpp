#include "lore/ecs/systems/volumetric_fire_system.hpp"
#include <iostream>
#include <cmath>

namespace lore::ecs {

VolumetricFireSystem::VolumetricFireSystem() {
    config_ = Config{};
}

VolumetricFireSystem::~VolumetricFireSystem() {
    shutdown();
}

bool VolumetricFireSystem::initialize(
    graphics::GPUComputeContext& gpu_context,
    const Config& config
) {
    config_ = config;
    gpu_context_ = &gpu_context;

    // Create compute pipelines from shaders
    if (!create_compute_pipelines()) {
        std::cerr << "[VolumetricFireSystem] Failed to create compute pipelines\n";
        return false;
    }

    std::cout << "[VolumetricFireSystem] Initialized with max " << config_.max_active_fires << " fires\n";
    return true;
}

void VolumetricFireSystem::shutdown() {
    if (!gpu_context_) return;

    // Destroy all fire resources
    for (auto& resources : fire_resources_) {
        destroy_fire_resources(resources);
    }
    fire_resources_.clear();

    // TODO: Destroy compute pipelines, descriptor pools, layouts
    // vkDestroyPipeline(device, pipeline_advect_, nullptr);
    // vkDestroyPipeline(device, pipeline_inject_source_, nullptr);
    // ... etc

    gpu_context_ = nullptr;
}

void VolumetricFireSystem::update(World& world, float delta_time_s) {
    if (!gpu_context_ || delta_time_s <= 0.0f) return;

    // Accumulate time for fixed timestep
    accumulated_time_ += delta_time_s;
    const float fixed_dt = config_.time_step_s;

    if (accumulated_time_ < fixed_dt) {
        return;  // Not time to update yet
    }

    stats_ = Stats{};

    // Get or create GPU resources for all active fires
    std::vector<FireGPUResources*> active_resources;

    world.view<VolumetricFireComponent>([&](Entity entity, VolumetricFireComponent& fire_component) {
        stats_.active_fires++;

        // Find existing resources or create new ones
        FireGPUResources* resources = nullptr;
        for (auto& res : fire_resources_) {
            if (res.entity == entity) {
                resources = &res;
                break;
            }
        }

        if (!resources) {
            // Create new GPU resources for this fire
            if (fire_resources_.size() >= config_.max_active_fires) {
                std::cerr << "[VolumetricFireSystem] Max active fires reached\n";
                return;  // Skip this fire
            }

            FireGPUResources new_resources;
            if (create_fire_resources(entity, &fire_component, new_resources)) {
                fire_resources_.push_back(new_resources);
                resources = &fire_resources_.back();
            } else {
                return;  // Failed to create resources
            }
        }

        // Update component pointer (in case it moved)
        resources->component = &fire_component;

        // Calculate LOD based on distance to camera
        // TODO: Get actual distance from camera
        float distance = 0.0f;
        resources->current_lod_scale = calculate_lod_scale(distance);

        active_resources.push_back(resources);

        // Count cells
        uint32_t cells = fire_component.get_total_cells();
        stats_.total_cells_simulated += static_cast<uint32_t>(cells * resources->current_lod_scale);
    });

    // Get command buffer (async compute if enabled)
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    // TODO: Get command buffer from gpu_context_
    // cmd = gpu_context_->get_compute_command_buffer();

    if (cmd == VK_NULL_HANDLE) {
        // Fallback: skip this frame if no command buffer available
        return;
    }

    // Simulate all active fires on GPU
    for (auto* resources : active_resources) {
        simulate_fire(cmd, *resources, fixed_dt);
    }

    // Submit compute work
    // TODO: Submit command buffer to compute queue
    // gpu_context_->submit_compute(cmd);

    accumulated_time_ -= fixed_dt;

    // Clean up resources for destroyed entities
    fire_resources_.erase(
        std::remove_if(fire_resources_.begin(), fire_resources_.end(),
            [&world](const FireGPUResources& res) {
                return !world.has_component<VolumetricFireComponent>(res.entity);
            }),
        fire_resources_.end()
    );
}

void VolumetricFireSystem::render(
    VkCommandBuffer command_buffer,
    const float* view_matrix,
    const float* proj_matrix
) {
    // TODO: Implement volumetric raymarching rendering
    // This would:
    // 1. Bind raymarching pipeline
    // 2. For each fire, bind its density/temperature textures
    // 3. Dispatch fullscreen raymarching compute shader
    // 4. Sample 3D textures along view rays
    // 5. Accumulate density with Beer's law
    // 6. Map temperature to color
    // 7. Output to framebuffer
}

bool VolumetricFireSystem::create_fire_resources(
    Entity entity,
    VolumetricFireComponent* component,
    FireGPUResources& resources
) {
    resources.entity = entity;
    resources.component = component;
    resources.read_index = 0;
    resources.write_index = 1;

    uint32_t res_x = component->resolution_x;
    uint32_t res_y = component->resolution_y;
    uint32_t res_z = component->resolution_z;

    // Create 3D textures (double-buffered for ping-pong)
    VkFormat velocity_format = VK_FORMAT_R32G32B32A32_SFLOAT;  // vec4
    VkFormat scalar_format = VK_FORMAT_R32_SFLOAT;             // float

    VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Velocity (double-buffered)
    for (int i = 0; i < 2; ++i) {
        if (!create_3d_texture(res_x, res_y, res_z, velocity_format, usage,
                              resources.velocity_texture[i],
                              resources.velocity_view[i],
                              resources.velocity_memory[i])) {
            return false;
        }
    }

    // Density (double-buffered)
    for (int i = 0; i < 2; ++i) {
        if (!create_3d_texture(res_x, res_y, res_z, scalar_format, usage,
                              resources.density_texture[i],
                              resources.density_view[i],
                              resources.density_memory[i])) {
            return false;
        }
    }

    // Temperature (double-buffered)
    for (int i = 0; i < 2; ++i) {
        if (!create_3d_texture(res_x, res_y, res_z, scalar_format, usage,
                              resources.temperature_texture[i],
                              resources.temperature_view[i],
                              resources.temperature_memory[i])) {
            return false;
        }
    }

    // Pressure (double-buffered)
    for (int i = 0; i < 2; ++i) {
        if (!create_3d_texture(res_x, res_y, res_z, scalar_format, usage,
                              resources.pressure_texture[i],
                              resources.pressure_view[i],
                              resources.pressure_memory[i])) {
            return false;
        }
    }

    // Divergence (single buffer)
    if (!create_3d_texture(res_x, res_y, res_z, scalar_format, usage,
                          resources.divergence_texture,
                          resources.divergence_view,
                          resources.divergence_memory)) {
        return false;
    }

    // TODO: Create descriptor sets for each shader pass
    // Each descriptor set binds the appropriate input/output textures

    return true;
}

void VolumetricFireSystem::destroy_fire_resources(FireGPUResources& resources) {
    if (!gpu_context_) return;

    // TODO: Destroy Vulkan resources
    // VkDevice device = gpu_context_->get_device();

    // for (int i = 0; i < 2; ++i) {
    //     vkDestroyImageView(device, resources.velocity_view[i], nullptr);
    //     vkDestroyImage(device, resources.velocity_texture[i], nullptr);
    //     vkFreeMemory(device, resources.velocity_memory[i], nullptr);
    //     ... etc for all textures
    // }
}

bool VolumetricFireSystem::create_compute_pipelines() {
    // TODO: Load and compile compute shaders
    // 1. Load SPIR-V bytecode for each shader
    // 2. Create VkShaderModule for each
    // 3. Create VkPipelineLayout with push constants and descriptor set layout
    // 4. Create VkPipeline for each compute shader
    //
    // Example:
    // VkShaderModule advect_module = load_shader("fire_advect.comp.spv");
    // VkComputePipelineCreateInfo pipeline_info = {...};
    // vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_advect_);

    return true;  // Placeholder
}

void VolumetricFireSystem::simulate_fire(
    VkCommandBuffer cmd,
    FireGPUResources& resources,
    float delta_time
) {
    VolumetricFireComponent* fire = resources.component;
    int read = resources.read_index;
    int write = resources.write_index;

    // Apply LOD scaling to resolution
    uint32_t res_x = static_cast<uint32_t>(fire->resolution_x * resources.current_lod_scale);
    uint32_t res_y = static_cast<uint32_t>(fire->resolution_y * resources.current_lod_scale);
    uint32_t res_z = static_cast<uint32_t>(fire->resolution_z * resources.current_lod_scale);

    // Dispatch groups (8x8x8 workgroup size)
    uint32_t groups_x = (res_x + 7) / 8;
    uint32_t groups_y = (res_y + 7) / 8;
    uint32_t groups_z = (res_z + 7) / 8;

    // === STEP 1: Advection ===
    // Move velocity, density, temperature through velocity field
    {
        // TODO: Bind pipeline and descriptor set
        // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_advect_);
        // vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ...);

        // Push constants
        struct {
            float delta_time;
            float dissipation;
            float cell_size;
            int use_maccormack;
        } push_constants;

        push_constants.delta_time = delta_time;
        push_constants.dissipation = fire->advection_dissipation;
        push_constants.cell_size = fire->cell_size_m;
        push_constants.use_maccormack = config_.enable_maccormack_advection ? 1 : 0;

        // TODO: Push constants
        // vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);

        // Dispatch
        // vkCmdDispatch(cmd, groups_x, groups_y, groups_z);

        // Swap read/write indices
        std::swap(resources.read_index, resources.write_index);
    }

    // TODO: Pipeline barrier (ensure advection completes before next pass)
    // VkMemoryBarrier barrier = {...};
    // vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, ...);

    // === STEP 2: Inject Source ===
    // Add fuel, heat, velocity at source point
    {
        // Similar to advection: bind pipeline, push constants, dispatch
        // Source parameters come from fire->source_position, source_temperature, etc.
    }

    // === STEP 3: Add Forces ===
    // Apply buoyancy, gravity, wind, vorticity confinement
    {
        // Bind pipeline_add_force_
        // Push constants: gravity, buoyancy_coefficient, wind, etc.
        // Dispatch
    }

    // === STEP 4: Divergence ===
    // Calculate ∇·u for pressure solve
    {
        // Bind pipeline_divergence_
        // Read velocity, write divergence
        // Dispatch
    }

    // === STEP 5: Jacobi Pressure Solve ===
    // Iteratively solve for pressure (typically 40 iterations)
    for (uint32_t iter = 0; iter < config_.max_jacobi_iterations; ++iter) {
        // Bind pipeline_jacobi_
        // Read pressure[read], divergence
        // Write pressure[write]
        // Dispatch

        // Swap pressure buffers
        read = resources.read_index;
        write = resources.write_index;
        std::swap(read, write);

        // Pipeline barrier between iterations
    }

    // === STEP 6: Projection ===
    // Subtract ∇p from velocity to make divergence-free
    {
        // Bind pipeline_project_
        // Read velocity, pressure
        // Write velocity (divergence-free)
        // Dispatch
    }

    // Final pipeline barrier (ensure all work completes)
}

float VolumetricFireSystem::calculate_lod_scale(float distance_m) const {
    if (!config_.enable_lod) {
        return 1.0f;
    }

    if (distance_m < config_.lod_distance_full_m) {
        return 1.0f;  // Full resolution
    } else if (distance_m < config_.lod_distance_half_m) {
        return 0.5f;  // Half resolution
    } else if (distance_m < config_.lod_distance_quarter_m) {
        return 0.25f;  // Quarter resolution
    } else {
        return 0.0f;  // No simulation (too far)
    }
}

bool VolumetricFireSystem::create_3d_texture(
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImage& image,
    VkImageView& view,
    VkDeviceMemory& memory
) {
    // TODO: Implement 3D texture creation
    // 1. Create VkImage with VK_IMAGE_TYPE_3D
    // 2. Allocate VkDeviceMemory
    // 3. Bind memory to image
    // 4. Create VkImageView
    //
    // VkImageCreateInfo image_info = {
    //     .imageType = VK_IMAGE_TYPE_3D,
    //     .format = format,
    //     .extent = {width, height, depth},
    //     .usage = usage,
    //     ...
    // };
    // vkCreateImage(device, &image_info, nullptr, &image);
    // vkAllocateMemory(device, &alloc_info, nullptr, &memory);
    // vkBindImageMemory(device, image, memory, 0);
    // vkCreateImageView(device, &view_info, nullptr, &view);

    return true;  // Placeholder
}

} // namespace lore::ecs