#include "lore/ecs/systems/heat_distortion_system.hpp"
#include "lore/ecs/world_manager.hpp"
#include "lore/ecs/components/volumetric_fire_component.hpp"
#include "lore/ecs/components/explosion_component.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>

// ImGui for debug UI
#ifdef LORE_ENABLE_IMGUI
#include <imgui.h>
#endif

namespace lore::ecs {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

HeatDistortionSystem::HeatDistortionSystem(World& world, const Config& config)
    : world_(world), config_(config) {
    initialize_vulkan();
}

HeatDistortionSystem::~HeatDistortionSystem() {
    cleanup_vulkan();
}

// ============================================================================
// UPDATE
// ============================================================================

void HeatDistortionSystem::update(float delta_time_s) {
    time_seconds_ += delta_time_s;
    accumulator_s_ += delta_time_s;

    // Fixed timestep update based on update_rate_hz
    float timestep = 1.0f / config_.update_rate_hz;

    while (accumulator_s_ >= timestep) {
        accumulator_s_ -= timestep;

        // Update all HeatDistortionComponents
        auto view = world_.view<HeatDistortionComponent>();

        for (auto entity : view) {
            auto& distortion = view.get<HeatDistortionComponent>(entity);

            if (!distortion.enabled) continue;

            // Update shockwave time
            if (distortion.shockwave_time_s >= 0.0f) {
                distortion.shockwave_time_s += timestep;

                // Deactivate shockwave after duration
                if (distortion.shockwave_time_s > distortion.shockwave_duration_s) {
                    distortion.shockwave_time_s = -1.0f;
                }
            }
        }

        // Auto-create distortion for fires
        if (config_.auto_create_fire_distortion) {
            auto fire_view = world_.view<VolumetricFireComponent>();
            for (auto entity : fire_view) {
                auto& fire = fire_view.get<VolumetricFireComponent>(entity);

                // Check if distortion component exists
                if (!world_.has_component<HeatDistortionComponent>(entity)) {
                    // Create appropriate distortion based on fire size
                    HeatDistortionComponent distortion;
                    if (fire.radius_m < 1.0f) {
                        distortion = HeatDistortionComponent::create_torch();
                    } else if (fire.radius_m < 3.0f) {
                        distortion = HeatDistortionComponent::create_small_fire();
                    } else {
                        distortion = HeatDistortionComponent::create_large_fire();
                    }

                    distortion.source_position = fire.position;
                    distortion.source_temperature_k = fire.temperature_k;
                    world_.add_component(entity, std::move(distortion));
                } else {
                    // Update existing distortion
                    auto& distortion = world_.get_component<HeatDistortionComponent>(entity);
                    distortion.update_from_fire(fire.temperature_k, fire.position);
                }
            }
        }

        // Auto-create distortion for explosions
        if (config_.auto_create_explosion_distortion) {
            auto explosion_view = world_.view<ExplosionComponent>();
            for (auto entity : explosion_view) {
                auto& explosion = explosion_view.get<ExplosionComponent>(entity);

                // Create distortion if it doesn't exist and explosion is starting
                if (!world_.has_component<HeatDistortionComponent>(entity) && explosion.time_since_detonation_s < 0.1f) {
                    auto distortion = HeatDistortionComponent::create_explosion_shockwave();
                    distortion.source_position = explosion.detonation_point;
                    distortion.source_temperature_k = explosion.fireball_temperature_k;
                    distortion.trigger_shockwave(explosion.fireball_radius_m, explosion.energy_joules / 1000000.0f);
                    world_.add_component(entity, std::move(distortion));
                }
            }
        }

        // Cleanup expired shockwaves
        cleanup_expired();
    }

    stats_.active_sources = static_cast<uint32_t>(world_.view<HeatDistortionComponent>().size());
}

// ============================================================================
// RENDER
// ============================================================================

void HeatDistortionSystem::render(VkCommandBuffer cmd_buffer, VkImageView scene_texture, VkImageView output_texture) {
    if (stats_.active_sources == 0) return;

    // Update uniform buffer with current heat sources
    update_uniform_buffer();

    // Bind pipeline
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk_.pipeline);

    // Bind descriptor sets
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            vk_.pipeline_layout, 0, 1, &vk_.descriptor_set, 0, nullptr);

    // Dispatch compute shader
    // Work group size: 8×8 (defined in shader)
    // Calculate dispatch counts for full screen coverage
    uint32_t width = 1920;  // TODO: Get from scene texture
    uint32_t height = 1080;
    uint32_t group_count_x = (width + 7) / 8;
    uint32_t group_count_y = (height + 7) / 8;

    vkCmdDispatch(cmd_buffer, group_count_x, group_count_y, 1);

    stats_.total_dispatches++;
}

// ============================================================================
// INTEGRATION HELPERS
// ============================================================================

void HeatDistortionSystem::integrate_fire(uint32_t entity_id, float temperature_k, const math::Vec3& position) {
    if (!world_.has_component<HeatDistortionComponent>(entity_id)) {
        // Create new distortion
        auto distortion = HeatDistortionComponent::create_small_fire();
        distortion.source_position = position;
        distortion.source_temperature_k = temperature_k;
        world_.add_component(entity_id, std::move(distortion));
    } else {
        // Update existing
        auto& distortion = world_.get_component<HeatDistortionComponent>(entity_id);
        distortion.update_from_fire(temperature_k, position);
    }
}

uint32_t HeatDistortionSystem::create_explosion_distortion(const math::Vec3& position, float radius, float intensity) {
    auto entity = world_.create_entity();
    auto distortion = HeatDistortionComponent::create_explosion_shockwave();
    distortion.source_position = position;
    distortion.trigger_shockwave(radius, intensity);
    world_.add_component(entity, std::move(distortion));
    return entity;
}

void HeatDistortionSystem::cleanup_expired() {
    auto view = world_.view<HeatDistortionComponent>();
    std::vector<uint32_t> to_remove;

    for (auto entity : view) {
        auto& distortion = view.get<HeatDistortionComponent>(entity);

        // Remove if shockwave finished and no fire component
        if (distortion.shockwave_time_s > distortion.shockwave_duration_s &&
            !world_.has_component<VolumetricFireComponent>(entity) &&
            !world_.has_component<ExplosionComponent>(entity)) {
            to_remove.push_back(entity);
        }
    }

    for (auto entity : to_remove) {
        world_.remove_component<HeatDistortionComponent>(entity);
    }
}

// ============================================================================
// VULKAN INITIALIZATION
// ============================================================================

void HeatDistortionSystem::initialize_vulkan() {
    // TODO: Get Vulkan device from engine
    // For now, placeholder - will be connected to rendering system
    vk_.device = VK_NULL_HANDLE;

    if (vk_.device == VK_NULL_HANDLE) {
        // Deferred initialization until rendering system is available
        return;
    }

    // Create uniform buffers
    // Heat source buffer
    VkDeviceSize heat_buffer_size = sizeof(GPUHeatBuffer);

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = heat_buffer_size;
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(vk_.device, &buffer_info, nullptr, &vk_.uniform_buffer);

    // Allocate memory
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(vk_.device, vk_.uniform_buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    // TODO: Find proper memory type
    alloc_info.memoryTypeIndex = 0;

    vkAllocateMemory(vk_.device, &alloc_info, nullptr, &vk_.uniform_memory);
    vkBindBufferMemory(vk_.device, vk_.uniform_buffer, vk_.uniform_memory, 0);

    // Map memory
    vkMapMemory(vk_.device, vk_.uniform_memory, 0, heat_buffer_size, 0, &vk_.uniform_mapped);

    // Camera buffer (similar setup)
    VkDeviceSize camera_buffer_size = sizeof(GPUCameraData);
    buffer_info.size = camera_buffer_size;
    vkCreateBuffer(vk_.device, &buffer_info, nullptr, &vk_.camera_buffer);

    vkGetBufferMemoryRequirements(vk_.device, vk_.camera_buffer, &mem_requirements);
    alloc_info.allocationSize = mem_requirements.size;
    vkAllocateMemory(vk_.device, &alloc_info, nullptr, &vk_.camera_memory);
    vkBindBufferMemory(vk_.device, vk_.camera_buffer, vk_.camera_memory, 0);
    vkMapMemory(vk_.device, vk_.camera_memory, 0, camera_buffer_size, 0, &vk_.camera_mapped);

    // Create compute pipeline and descriptor sets
    create_compute_pipeline();
    create_descriptor_sets();
}

void HeatDistortionSystem::cleanup_vulkan() {
    if (vk_.device == VK_NULL_HANDLE) return;

    // Cleanup buffers
    if (vk_.uniform_mapped) {
        vkUnmapMemory(vk_.device, vk_.uniform_memory);
    }
    if (vk_.uniform_buffer) {
        vkDestroyBuffer(vk_.device, vk_.uniform_buffer, nullptr);
    }
    if (vk_.uniform_memory) {
        vkFreeMemory(vk_.device, vk_.uniform_memory, nullptr);
    }

    if (vk_.camera_mapped) {
        vkUnmapMemory(vk_.device, vk_.camera_memory);
    }
    if (vk_.camera_buffer) {
        vkDestroyBuffer(vk_.device, vk_.camera_buffer, nullptr);
    }
    if (vk_.camera_memory) {
        vkFreeMemory(vk_.device, vk_.camera_memory, nullptr);
    }

    // Cleanup pipeline
    if (vk_.pipeline) {
        vkDestroyPipeline(vk_.device, vk_.pipeline, nullptr);
    }
    if (vk_.pipeline_layout) {
        vkDestroyPipelineLayout(vk_.device, vk_.pipeline_layout, nullptr);
    }
    if (vk_.compute_shader) {
        vkDestroyShaderModule(vk_.device, vk_.compute_shader, nullptr);
    }

    // Cleanup descriptors
    if (vk_.descriptor_pool) {
        vkDestroyDescriptorPool(vk_.device, vk_.descriptor_pool, nullptr);
    }
    if (vk_.descriptor_layout) {
        vkDestroyDescriptorSetLayout(vk_.device, vk_.descriptor_layout, nullptr);
    }
}

void HeatDistortionSystem::create_compute_pipeline() {
    // Load shader SPIR-V
    auto spirv = load_shader_spirv("shaders/compiled/heat_distortion.comp.spv");

    VkShaderModuleCreateInfo shader_info = {};
    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.codeSize = spirv.size() * sizeof(uint32_t);
    shader_info.pCode = spirv.data();

    vkCreateShaderModule(vk_.device, &shader_info, nullptr, &vk_.compute_shader);

    // Pipeline layout
    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &vk_.descriptor_layout;

    vkCreatePipelineLayout(vk_.device, &layout_info, nullptr, &vk_.pipeline_layout);

    // Compute pipeline
    VkComputePipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = vk_.compute_shader;
    pipeline_info.stage.pName = "main";
    pipeline_info.layout = vk_.pipeline_layout;

    vkCreateComputePipelines(vk_.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &vk_.pipeline);
}

void HeatDistortionSystem::create_descriptor_sets() {
    // Descriptor set layout
    VkDescriptorSetLayoutBinding bindings[4] = {};

    // Binding 0: Scene texture (input)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Output texture
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Heat source uniform buffer
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: Camera uniform buffer
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 4;
    layout_info.pBindings = bindings;

    vkCreateDescriptorSetLayout(vk_.device, &layout_info, nullptr, &vk_.descriptor_layout);

    // Descriptor pool
    VkDescriptorPoolSize pool_sizes[2] = {};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    pool_sizes[0].descriptorCount = 2;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[1].descriptorCount = 2;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = pool_sizes;

    vkCreateDescriptorPool(vk_.device, &pool_info, nullptr, &vk_.descriptor_pool);

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk_.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &vk_.descriptor_layout;

    vkAllocateDescriptorSets(vk_.device, &alloc_info, &vk_.descriptor_set);

    // Update descriptor set (images will be updated per-frame)
    VkDescriptorBufferInfo heat_buffer_info = {};
    heat_buffer_info.buffer = vk_.uniform_buffer;
    heat_buffer_info.offset = 0;
    heat_buffer_info.range = sizeof(GPUHeatBuffer);

    VkDescriptorBufferInfo camera_buffer_info = {};
    camera_buffer_info.buffer = vk_.camera_buffer;
    camera_buffer_info.offset = 0;
    camera_buffer_info.range = sizeof(GPUCameraData);

    VkWriteDescriptorSet writes[2] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = vk_.descriptor_set;
    writes[0].dstBinding = 2;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &heat_buffer_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = vk_.descriptor_set;
    writes[1].dstBinding = 3;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &camera_buffer_info;

    vkUpdateDescriptorSets(vk_.device, 2, writes, 0, nullptr);
}

// ============================================================================
// UNIFORM BUFFER UPDATE
// ============================================================================

void HeatDistortionSystem::update_uniform_buffer() {
    if (!vk_.uniform_mapped) return;

    auto sources = collect_heat_sources();

    GPUHeatBuffer buffer_data = {};
    buffer_data.num_sources = std::min(static_cast<uint32_t>(sources.size()), config_.max_heat_sources);
    buffer_data.time_seconds = time_seconds_;
    buffer_data.delta_time_s = 1.0f / config_.update_rate_hz;

    // Copy sources
    std::memcpy(buffer_data.sources, sources.data(), buffer_data.num_sources * sizeof(GPUHeatSource));

    // Upload to GPU
    std::memcpy(vk_.uniform_mapped, &buffer_data, sizeof(GPUHeatBuffer));
}

std::vector<HeatDistortionSystem::GPUHeatSource> HeatDistortionSystem::collect_heat_sources() {
    std::vector<GPUHeatSource> sources;
    sources.reserve(config_.max_heat_sources);

    auto view = world_.view<HeatDistortionComponent>();

    for (auto entity : view) {
        auto& comp = view.get<HeatDistortionComponent>(entity);

        if (!comp.enabled) continue;
        if (sources.size() >= config_.max_heat_sources) break;

        GPUHeatSource gpu_source = {};
        gpu_source.position = comp.source_position;
        gpu_source.temperature_k = comp.source_temperature_k;
        gpu_source.base_strength = comp.base_strength;
        gpu_source.temperature_scale = comp.temperature_scale;
        gpu_source.max_strength = comp.max_strength;
        gpu_source.inner_radius_m = comp.inner_radius_m;
        gpu_source.outer_radius_m = comp.outer_radius_m;
        gpu_source.vertical_bias = comp.vertical_bias;
        gpu_source.height_falloff_m = comp.height_falloff_m;
        gpu_source.noise_frequency = comp.noise_frequency;
        gpu_source.noise_octaves = comp.noise_octaves;
        gpu_source.noise_amplitude = comp.noise_amplitude;
        gpu_source.vertical_speed_m_s = comp.vertical_speed_m_s;
        gpu_source.turbulence_scale = comp.turbulence_scale;
        gpu_source.shockwave_enabled = comp.shockwave_enabled ? 1.0f : 0.0f;
        gpu_source.shockwave_strength = comp.shockwave_strength;
        gpu_source.shockwave_time_s = comp.shockwave_time_s;
        gpu_source.shockwave_duration_s = comp.shockwave_duration_s;
        gpu_source.shockwave_speed_m_s = comp.shockwave_speed_m_s;
        gpu_source.shockwave_thickness_m = comp.shockwave_thickness_m;
        gpu_source.ambient_temp_k = comp.ambient_temperature_k;

        sources.push_back(gpu_source);
    }

    return sources;
}

// ============================================================================
// IMGUI DEBUG UI
// ============================================================================

void HeatDistortionSystem::render_debug_ui() {
#ifdef LORE_ENABLE_IMGUI
    ImGui::Begin("Heat Distortion System");

    ImGui::Text("Active Sources: %u / %u", stats_.active_sources, config_.max_heat_sources);
    ImGui::Text("Total Dispatches: %u", stats_.total_dispatches);
    ImGui::Text("Update Time: %.2f ms", stats_.last_update_time_ms);
    ImGui::Text("Render Time: %.2f ms", stats_.last_render_time_ms);

    ImGui::Separator();
    ImGui::Text("Configuration");

    ImGui::Checkbox("Auto Fire Distortion", &config_.auto_create_fire_distortion);
    ImGui::Checkbox("Auto Explosion Distortion", &config_.auto_create_explosion_distortion);
    ImGui::Checkbox("Async Compute", &config_.enable_async_compute);
    ImGui::Checkbox("Debug Draw", &config_.debug_draw_heat_sources);

    float update_rate = config_.update_rate_hz;
    if (ImGui::SliderFloat("Update Rate (Hz)", &update_rate, 20.0f, 60.0f)) {
        config_.update_rate_hz = update_rate;
    }

    ImGui::Separator();
    ImGui::Text("Active Heat Sources");

    auto view = world_.view<HeatDistortionComponent>();
    int source_index = 0;

    for (auto entity : view) {
        auto& distortion = view.get<HeatDistortionComponent>(entity);

        if (ImGui::TreeNode(&distortion, "Source %d (Entity %u)", source_index++, entity)) {
            ImGui::Text("Position: (%.1f, %.1f, %.1f)", distortion.source_position.x, distortion.source_position.y, distortion.source_position.z);
            ImGui::Text("Temperature: %.0f K (%.0f°C)", distortion.source_temperature_k, distortion.source_temperature_k - 273.15f);

            ImGui::Checkbox("Enabled", &distortion.enabled);

            ImGui::SliderFloat("Base Strength", &distortion.base_strength, 0.0f, 0.1f);
            ImGui::SliderFloat("Max Strength", &distortion.max_strength, 0.0f, 0.2f);
            ImGui::SliderFloat("Outer Radius (m)", &distortion.outer_radius_m, 1.0f, 20.0f);
            ImGui::SliderFloat("Vertical Bias", &distortion.vertical_bias, 1.0f, 3.0f);

            if (distortion.shockwave_time_s >= 0.0f && distortion.shockwave_time_s <= distortion.shockwave_duration_s) {
                ImGui::Text("SHOCKWAVE ACTIVE: %.2f / %.2f s", distortion.shockwave_time_s, distortion.shockwave_duration_s);
                float progress = distortion.shockwave_time_s / distortion.shockwave_duration_s;
                ImGui::ProgressBar(progress, ImVec2(-1, 0));
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
#endif
}

// ============================================================================
// SHADER LOADING
// ============================================================================

std::vector<uint32_t> HeatDistortionSystem::load_shader_spirv(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        // Return empty vector - error handling in production
        return {};
    }

    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    file.close();

    return buffer;
}

// ============================================================================
// CONFIG
// ============================================================================

void HeatDistortionSystem::set_config(const Config& config) {
    config_ = config;
}

} // namespace lore::ecs