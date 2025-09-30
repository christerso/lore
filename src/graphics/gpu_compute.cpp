#include <lore/graphics/gpu_compute.hpp>
#include <lore/graphics/graphics.hpp>

#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cassert>
#include <iostream>

// Include SPIRV-Reflect for shader reflection
#include <spirv_reflect.h>

namespace lore::graphics {

// Helper functions for Vulkan operations
namespace {
    VkResult check_vk_result(VkResult result, const std::string& operation) {
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Vulkan operation failed: " + operation + " (code: " + std::to_string(result) + ")");
        }
        return result;
    }

    uint32_t find_queue_family(VkPhysicalDevice physical_device, VkQueueFlags desired_flags) {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

        for (uint32_t i = 0; i < queue_family_count; i++) {
            if (queue_families[i].queueFlags & desired_flags) {
                return i;
            }
        }

        throw std::runtime_error("Could not find suitable queue family");
    }

    std::vector<char> read_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        size_t file_size = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), file_size);
        file.close();

        return buffer;
    }
}

// VulkanGpuArenaManager Implementation
VulkanGpuArenaManager::VulkanGpuArenaManager(VkDevice device, VkPhysicalDevice physical_device, VmaAllocator allocator)
    : device_(device)
    , physical_device_(physical_device)
    , allocator_(allocator)
    , allocator_pipeline_(VK_NULL_HANDLE)
    , deallocator_pipeline_(VK_NULL_HANDLE)
    , compactor_pipeline_(VK_NULL_HANDLE)
    , pipeline_layout_(VK_NULL_HANDLE)
    , descriptor_set_layout_(VK_NULL_HANDLE)
    , descriptor_pool_(VK_NULL_HANDLE)
    , compute_command_pool_(VK_NULL_HANDLE)
    , compute_queue_(VK_NULL_HANDLE)
    , allocation_requests_buffer_(VK_NULL_HANDLE)
    , metadata_buffer_(VK_NULL_HANDLE)
    , free_list_buffer_(VK_NULL_HANDLE) {

    compute_queue_family_ = find_queue_family(physical_device_, VK_QUEUE_COMPUTE_BIT);
    vkGetDeviceQueue(device_, compute_queue_family_, 0, &compute_queue_);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = compute_queue_family_;

    check_vk_result(vkCreateCommandPool(device_, &pool_info, nullptr, &compute_command_pool_),
                    "create compute command pool");

    create_gpu_memory_buffers();
    initialize_compute_pipelines();
}

VulkanGpuArenaManager::~VulkanGpuArenaManager() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);

        // Destroy arenas
        for (auto& arena : arenas_) {
            if (arena) {
                vmaDestroyBuffer(allocator_, arena->buffer, arena->allocation);
            }
        }

        // Destroy GPU buffers
        if (allocation_requests_buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, allocation_requests_buffer_, allocation_requests_allocation_);
        }
        if (metadata_buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, metadata_buffer_, metadata_allocation_);
        }
        if (free_list_buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, free_list_buffer_, free_list_allocation_);
        }

        // Destroy compute pipelines
        if (allocator_pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, allocator_pipeline_, nullptr);
        }
        if (deallocator_pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, deallocator_pipeline_, nullptr);
        }
        if (compactor_pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, compactor_pipeline_, nullptr);
        }

        if (pipeline_layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        }
        if (descriptor_set_layout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
        }
        if (descriptor_pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        }
        if (compute_command_pool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, compute_command_pool_, nullptr);
        }
    }
}

uint32_t VulkanGpuArenaManager::create_arena(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
    std::lock_guard<std::mutex> lock(arenas_mutex_);

    auto arena = std::make_unique<ArenaBlock>();
    arena->size = size;
    arena->next_offset.store(0);
    arena->allocation_count.store(0);
    arena->usage_flags = usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // Always add storage buffer usage
    arena->memory_usage = memory_usage;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = arena->usage_flags;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = memory_usage;
    alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT; // Dedicated allocation for large arenas

    check_vk_result(vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
                                    &arena->buffer, &arena->allocation, nullptr),
                    "create arena buffer");

    uint32_t arena_id = static_cast<uint32_t>(arenas_.size());
    arenas_.push_back(std::move(arena));

    return arena_id;
}

VulkanGpuArenaManager::ArenaAllocation VulkanGpuArenaManager::allocate_on_gpu(uint32_t arena_id, uint32_t size, uint32_t alignment) {
    std::vector<uint32_t> sizes = { size };
    auto allocations = allocate_batch_on_gpu(arena_id, sizes, alignment);

    if (!allocations.empty()) {
        return allocations[0];
    }

    return { VK_NULL_HANDLE, 0, 0, 0xFFFFFFFF, false };
}

std::vector<VulkanGpuArenaManager::ArenaAllocation> VulkanGpuArenaManager::allocate_batch_on_gpu(
    uint32_t arena_id, const std::vector<uint32_t>& sizes, uint32_t alignment) {

    if (arena_id >= arenas_.size()) {
        return {};
    }

    // Prepare allocation requests
    std::vector<AllocationRequest> requests;
    requests.reserve(sizes.size());

    for (size_t i = 0; i < sizes.size(); ++i) {
        AllocationRequest req{};
        req.size = sizes[i];
        req.alignment = alignment;
        req.arena_id = arena_id;
        req.result_offset = 0;
        req.success = 0;
        requests.push_back(req);
    }

    // Dispatch GPU allocation
    VkResult result = dispatch_gpu_allocation(arena_id, requests);
    if (result != VK_SUCCESS) {
        return {};
    }

    // Process results
    std::vector<ArenaAllocation> allocations;
    allocations.reserve(requests.size());

    for (const auto& req : requests) {
        ArenaAllocation alloc{};
        alloc.buffer = arenas_[arena_id]->buffer;
        alloc.offset = req.result_offset;
        alloc.size = req.size;
        alloc.arena_id = arena_id;
        alloc.is_valid = (req.success != 0);
        allocations.push_back(alloc);
    }

    return allocations;
}

void VulkanGpuArenaManager::deallocate_on_gpu(const ArenaAllocation& allocation) {
    if (!allocation.is_valid || allocation.arena_id >= arenas_.size()) {
        return;
    }

    // In a full implementation, this would update the free list and compact memory
    // For now, we'll just track it as freed
    arenas_[allocation.arena_id]->allocation_count.fetch_sub(1);
}

void VulkanGpuArenaManager::create_gpu_memory_buffers() {
    const uint32_t max_allocation_requests = 1024;
    const uint32_t max_arenas = 64;
    const uint32_t max_free_blocks = 4096;

    // Create allocation requests buffer
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = max_allocation_requests * sizeof(AllocationRequest);
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    check_vk_result(vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
                                    &allocation_requests_buffer_, &allocation_requests_allocation_, nullptr),
                    "create allocation requests buffer");

    // Create metadata buffer
    buffer_info.size = max_arenas * sizeof(GpuArenaMetadata);
    check_vk_result(vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
                                    &metadata_buffer_, &metadata_allocation_, nullptr),
                    "create metadata buffer");

    // Create free list buffer
    buffer_info.size = max_free_blocks * sizeof(uint32_t) * 4; // FreeBlock structure
    check_vk_result(vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
                                    &free_list_buffer_, &free_list_allocation_, nullptr),
                    "create free list buffer");
}

void VulkanGpuArenaManager::initialize_compute_pipelines() {
    // Create descriptor set layout
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    check_vk_result(vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &descriptor_set_layout_),
                    "create descriptor set layout");

    // Create pipeline layout with push constants
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = 16; // 4 uint32_t values

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    check_vk_result(vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr, &pipeline_layout_),
                    "create pipeline layout");

    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 1> pool_sizes{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_sizes[0].descriptorCount = 9; // 3 buffers * 3 pipelines

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = 3;

    check_vk_result(vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_),
                    "create descriptor pool");

    // Note: Actual pipeline creation would require compiled SPIR-V shaders
    // For now, we'll create placeholder pipelines that can be updated when shaders are available
}

VkResult VulkanGpuArenaManager::dispatch_gpu_allocation(uint32_t arena_id, const std::vector<AllocationRequest>& requests) {
    // This is a simplified implementation
    // In a full implementation, this would:
    // 1. Upload allocation requests to GPU buffer
    // 2. Dispatch the allocation compute shader
    // 3. Wait for completion
    // 4. Read back results

    // For now, simulate successful allocation with sequential allocation
    VkDeviceSize current_offset = arenas_[arena_id]->next_offset.load();

    for (auto& request : requests) {
        uint32_t aligned_size = (request.size + request.alignment - 1) & ~(request.alignment - 1);
        if (current_offset + aligned_size <= arenas_[arena_id]->size) {
            const_cast<AllocationRequest&>(request).result_offset = static_cast<uint32_t>(current_offset);
            const_cast<AllocationRequest&>(request).success = 1;
            current_offset += aligned_size;
        } else {
            const_cast<AllocationRequest&>(request).success = 0;
        }
    }

    arenas_[arena_id]->next_offset.store(current_offset);
    return VK_SUCCESS;
}

VulkanGpuArenaManager::ArenaStats VulkanGpuArenaManager::get_arena_stats(uint32_t arena_id) const {
    if (arena_id >= arenas_.size()) {
        return {};
    }

    const auto& arena = arenas_[arena_id];
    ArenaStats stats{};
    stats.total_size = arena->size;
    stats.allocated_size = arena->next_offset.load();
    stats.free_size = stats.total_size - stats.allocated_size;
    stats.allocation_count = arena->allocation_count.load();
    stats.fragmentation_ratio = (stats.allocated_size > 0) ?
        static_cast<float>(stats.free_size) / static_cast<float>(stats.total_size) : 0.0f;

    return stats;
}

// CpuArenaAllocator Implementation
CpuArenaAllocator::CpuArenaAllocator(size_t size)
    : size_(size)
    , current_offset_(0)
    , alignment_(64) { // Cache line alignment

    buffer_ = _aligned_malloc(size_, alignment_); // Use Windows-specific function
    if (!buffer_) {
        throw std::bad_alloc();
    }
}

CpuArenaAllocator::~CpuArenaAllocator() {
    if (buffer_) {
        _aligned_free(buffer_); // Use Windows-specific function
    }
}

// ShaderCompiler Implementation
ShaderCompiler::ShaderCompiler(VkDevice device)
    : device_(device)
    , hot_reload_enabled_(false)
    , reload_thread_running_(false) {
}

ShaderCompiler::~ShaderCompiler() {
    if (reload_thread_running_.load()) {
        reload_thread_running_.store(false);
        if (reload_thread_.joinable()) {
            reload_thread_.join();
        }
    }

    // Destroy cached shader modules
    for (auto& [name, shader] : shader_cache_) {
        if (shader && shader->module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, shader->module, nullptr);
        }
    }
}

std::shared_ptr<ShaderCompiler::ShaderModule> ShaderCompiler::compile_compute_shader(const ComputeShaderInfo& info) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // Check cache first
    auto it = shader_cache_.find(info.source_path);
    if (it != shader_cache_.end()) {
        // Check if file has been modified
        auto file_time = std::filesystem::last_write_time(info.source_path);
        if (file_time <= it->second->last_modified) {
            return it->second; // Use cached version
        }
    }

    // Read source file
    std::string source_code;
    std::ifstream file(info.source_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + info.source_path);
    }

    source_code.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

    // Compile GLSL to SPIR-V
    auto spirv_code = compile_glsl_to_spirv(source_code, VK_SHADER_STAGE_COMPUTE_BIT, info.entry_point, info.definitions);

    // Create Vulkan shader module
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
    create_info.pCode = spirv_code.data();

    VkShaderModule shader_module;
    check_vk_result(vkCreateShaderModule(device_, &create_info, nullptr, &shader_module),
                    "create shader module");

    // Create shader module wrapper
    auto shader = std::make_shared<ShaderModule>();
    shader->module = shader_module;
    shader->spirv_code = std::move(spirv_code);
    shader->entry_point = info.entry_point;
    shader->stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader->last_modified = std::filesystem::last_write_time(info.source_path);

    shader_cache_[info.source_path] = shader;
    return shader;
}

std::vector<uint32_t> ShaderCompiler::compile_glsl_to_spirv(const std::string& source,
                                                            VkShaderStageFlagBits stage,
                                                            const std::string& entry_point,
                                                            const std::unordered_map<std::string, std::string>& definitions) {
    // SAFETY: Validate input parameters
    if (source.empty()) {
        throw std::invalid_argument("GLSL source cannot be empty");
    }
    if (entry_point.empty()) {
        throw std::invalid_argument("Entry point cannot be empty for shader compilation");
    }

    // Implementation using glslangValidator as external process
    // This provides full GLSL to SPIR-V compilation without complex dependencies

    // Generate unique temporary file names
    auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::string temp_glsl_path = "temp_shader_" + std::to_string(timestamp) + ".glsl";
    std::string temp_spv_path = temp_glsl_path + ".spv";

    // Determine shader stage for glslangValidator (needs to be in scope for cleanup)
    std::string stage_extension;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            stage_extension = ".vert";
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            stage_extension = ".frag";
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            stage_extension = ".comp";
            break;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            stage_extension = ".geom";
            break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            stage_extension = ".tesc";
            break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            stage_extension = ".tese";
            break;
        default:
            throw std::invalid_argument("Unsupported shader stage");
    }

    try {
        // Write GLSL source to temporary file with macro definitions
        std::ofstream temp_file(temp_glsl_path);
        if (!temp_file.is_open()) {
            throw std::runtime_error("Failed to create temporary GLSL file: " + temp_glsl_path);
        }

        // Add version directive and definitions at the top
        temp_file << "#version 450 core\n";
        for (const auto& [name, value] : definitions) {
            temp_file << "#define " << name << " " << value << "\n";
        }
        temp_file << "\n" << source;
        temp_file.close();

        // Rename file to have proper extension for stage detection
        std::string staged_glsl_path = temp_glsl_path + stage_extension;
        std::rename(temp_glsl_path.c_str(), staged_glsl_path.c_str());

        // Build glslangValidator command with explicit entry point
        std::string command = "glslangValidator -V --entry-point " + entry_point +
                             " " + staged_glsl_path + " -o " + temp_spv_path;

        // Execute compilation
        int result = std::system(command.c_str());
        if (result != 0) {
            // Try to read error output for better diagnostics
            std::string error_command = "glslangValidator -V --entry-point " + entry_point +
                                       " " + staged_glsl_path + " 2>&1";

            // Cleanup and throw error
            std::remove(staged_glsl_path.c_str());
            throw std::runtime_error("GLSL compilation failed with entry point '" + entry_point +
                                   "'. Make sure glslangValidator is in PATH. Command: " + command);
        }

        // Read compiled SPIR-V binary
        std::ifstream spv_file(temp_spv_path, std::ios::binary | std::ios::ate);
        if (!spv_file.is_open()) {
            std::remove(staged_glsl_path.c_str());
            throw std::runtime_error("Failed to read compiled SPIR-V file: " + temp_spv_path);
        }

        std::streamsize file_size = spv_file.tellg();
        if (file_size % 4 != 0) {
            spv_file.close();
            std::remove(staged_glsl_path.c_str());
            std::remove(temp_spv_path.c_str());
            throw std::runtime_error("Invalid SPIR-V file size (not multiple of 4 bytes)");
        }

        spv_file.seekg(0, std::ios::beg);
        std::vector<uint32_t> spirv_binary(file_size / 4);

        if (!spv_file.read(reinterpret_cast<char*>(spirv_binary.data()), file_size)) {
            spv_file.close();
            std::remove(staged_glsl_path.c_str());
            std::remove(temp_spv_path.c_str());
            throw std::runtime_error("Failed to read SPIR-V binary data");
        }

        spv_file.close();

        // Cleanup temporary files
        std::remove(staged_glsl_path.c_str());
        std::remove(temp_spv_path.c_str());

        // Validate SPIR-V magic number
        if (spirv_binary.empty() || spirv_binary[0] != 0x07230203) {
            throw std::runtime_error("Invalid SPIR-V magic number in compiled shader");
        }

        // SAFETY: Log successful compilation for debugging
        std::cout << "Successfully compiled shader with entry point '" << entry_point
                  << "' (" << spirv_binary.size() * 4 << " bytes)" << std::endl;

        return spirv_binary;

    } catch (const std::exception& e) {
        // Cleanup on any error
        std::remove(temp_glsl_path.c_str());
        std::remove((temp_glsl_path + stage_extension).c_str());
        std::remove(temp_spv_path.c_str());
        throw std::runtime_error("Shader compilation error: " + std::string(e.what()));
    }
}

void ShaderCompiler::enable_hot_reload(bool enable) {
    hot_reload_enabled_.store(enable);

    if (enable && !reload_thread_running_.load()) {
        reload_thread_running_.store(true);
        reload_thread_ = std::thread(&ShaderCompiler::hot_reload_worker, this);
    } else if (!enable && reload_thread_running_.load()) {
        reload_thread_running_.store(false);
        if (reload_thread_.joinable()) {
            reload_thread_.join();
        }
    }
}

void ShaderCompiler::hot_reload_worker() {
    while (reload_thread_running_.load()) {
        check_for_shader_updates();
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Check every 500ms
    }
}

void ShaderCompiler::check_for_shader_updates() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    for (auto& [path, shader] : shader_cache_) {
        if (!std::filesystem::exists(path)) {
            continue;
        }

        auto file_time = std::filesystem::last_write_time(path);
        if (file_time > shader->last_modified) {
            // File was modified, trigger reload callback
            if (reload_callback_) {
                reload_callback_(path);
            }
        }
    }
}

// ComputePipelineManager Implementation
ComputePipelineManager::ComputePipelineManager(VkDevice device, VkPhysicalDevice physical_device)
    : device_(device)
    , physical_device_(physical_device) {

    // Get device properties
    vkGetPhysicalDeviceProperties(physical_device_, &device_properties_);

    // Get subgroup properties
    VkPhysicalDeviceSubgroupProperties subgroup_props{};
    subgroup_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &subgroup_props;

    vkGetPhysicalDeviceProperties2(physical_device_, &props2);
    subgroup_properties_ = subgroup_props;

    // Store compute limits
    max_workgroup_size_[0] = device_properties_.limits.maxComputeWorkGroupSize[0];
    max_workgroup_size_[1] = device_properties_.limits.maxComputeWorkGroupSize[1];
    max_workgroup_size_[2] = device_properties_.limits.maxComputeWorkGroupSize[2];
    max_workgroup_invocations_ = device_properties_.limits.maxComputeWorkGroupInvocations;

    // Get compute queue
    uint32_t compute_queue_family = find_queue_family(physical_device_, VK_QUEUE_COMPUTE_BIT);
    vkGetDeviceQueue(device_, compute_queue_family, 0, &compute_queue_);

    // Create command pool
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = compute_queue_family;

    check_vk_result(vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_),
                    "create compute command pool");
}

ComputePipelineManager::~ComputePipelineManager() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);

        for (auto& [name, pipeline] : pipelines_) {
            vkDestroyPipeline(device_, pipeline, nullptr);
        }

        if (command_pool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, command_pool_, nullptr);
        }
    }
}

glm::uvec3 ComputePipelineManager::calculate_optimal_workgroups(glm::uvec3 total_work_items,
                                                                glm::uvec3 local_size,
                                                                const std::string& pipeline_name) {
    // Calculate workgroup count, rounding up
    glm::uvec3 workgroup_count;
    workgroup_count.x = (total_work_items.x + local_size.x - 1) / local_size.x;
    workgroup_count.y = (total_work_items.y + local_size.y - 1) / local_size.y;
    workgroup_count.z = (total_work_items.z + local_size.z - 1) / local_size.z;

    // Clamp to device limits
    workgroup_count.x = std::min(workgroup_count.x, device_properties_.limits.maxComputeWorkGroupCount[0]);
    workgroup_count.y = std::min(workgroup_count.y, device_properties_.limits.maxComputeWorkGroupCount[1]);
    workgroup_count.z = std::min(workgroup_count.z, device_properties_.limits.maxComputeWorkGroupCount[2]);

    return workgroup_count;
}

VkPipeline ComputePipelineManager::create_pipeline(const std::string& name, const PipelineConfig& config) {
    // Check if pipeline already exists
    auto it = pipelines_.find(name);
    if (it != pipelines_.end()) {
        return it->second;
    }

    if (!config.compute_shader || config.compute_shader->module == VK_NULL_HANDLE) {
        throw std::runtime_error("Invalid compute shader provided for pipeline: " + name);
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = static_cast<uint32_t>(config.descriptor_layouts.size());
    layout_info.pSetLayouts = config.descriptor_layouts.empty() ? nullptr : config.descriptor_layouts.data();
    layout_info.pushConstantRangeCount = static_cast<uint32_t>(config.push_constant_ranges.size());
    layout_info.pPushConstantRanges = config.push_constant_ranges.empty() ? nullptr : config.push_constant_ranges.data();

    VkPipelineLayout pipeline_layout;
    check_vk_result(vkCreatePipelineLayout(device_, &layout_info, nullptr, &pipeline_layout),
                    "create pipeline layout");

    // Create compute pipeline
    VkPipelineShaderStageCreateInfo shader_stage_info{};
    shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_info.module = config.compute_shader->module;
    shader_stage_info.pName = config.compute_shader->entry_point.c_str();

    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = shader_stage_info;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.flags = config.flags;

    VkPipeline pipeline;
    check_vk_result(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline),
                    "create compute pipeline");

    pipelines_[name] = pipeline;
    pipeline_layouts_[name] = pipeline_layout;
    return pipeline;
}

void ComputePipelineManager::destroy_pipeline(const std::string& name) {
    auto it = pipelines_.find(name);
    if (it != pipelines_.end()) {
        vkDestroyPipeline(device_, it->second, nullptr);
        pipelines_.erase(it);
    }

    auto layout_it = pipeline_layouts_.find(name);
    if (layout_it != pipeline_layouts_.end()) {
        vkDestroyPipelineLayout(device_, layout_it->second, nullptr);
        pipeline_layouts_.erase(layout_it);
    }
}

VkResult ComputePipelineManager::dispatch_compute(const DispatchInfo& info, VkFence fence) {
    // Allocate command buffer
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VkResult result = vkAllocateCommandBuffers(device_, &alloc_info, &cmd);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Begin command buffer
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = vkBeginCommandBuffer(cmd, &begin_info);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);
        return result;
    }

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, info.pipeline);

    // Bind descriptor sets if provided
    if (!info.descriptor_sets.empty()) {
        // Find pipeline layout for this pipeline
        VkPipelineLayout layout = VK_NULL_HANDLE;
        for (const auto& [name, pipeline] : pipelines_) {
            if (pipeline == info.pipeline) {
                auto layout_it = pipeline_layouts_.find(name);
                if (layout_it != pipeline_layouts_.end()) {
                    layout = layout_it->second;
                    break;
                }
            }
        }

        if (layout != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout,
                                   0, static_cast<uint32_t>(info.descriptor_sets.size()),
                                   info.descriptor_sets.data(), 0, nullptr);
        }
    }

    // Push constants if provided
    if (!info.push_constants.empty()) {
        // Find pipeline layout for this pipeline
        VkPipelineLayout layout = VK_NULL_HANDLE;
        for (const auto& [name, pipeline] : pipelines_) {
            if (pipeline == info.pipeline) {
                auto layout_it = pipeline_layouts_.find(name);
                if (layout_it != pipeline_layouts_.end()) {
                    layout = layout_it->second;
                    break;
                }
            }
        }

        if (layout != VK_NULL_HANDLE) {
            vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT,
                              0, static_cast<uint32_t>(info.push_constants.size()),
                              info.push_constants.data());
        }
    }

    // Dispatch compute shader
    vkCmdDispatch(cmd, info.workgroup_count.x, info.workgroup_count.y, info.workgroup_count.z);

    result = vkEndCommandBuffer(cmd);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);
        return result;
    }

    // Submit to compute queue
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    result = vkQueueSubmit(compute_queue_, 1, &submit_info, fence);

    // Free command buffer after submission
    vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);

    return result;
}

// Continue with remaining implementations...
// The full implementation would continue with GpuPhysicsSystem, GpuParticleSystem,
// EcsComputeIntegration, and GpuComputeSystem classes

// GpuComputeSystem Implementation (Main orchestrator)
GpuComputeSystem::GpuComputeSystem(GraphicsSystem& graphics_system)
    : graphics_system_(graphics_system)
    , device_(VK_NULL_HANDLE)
    , physical_device_(VK_NULL_HANDLE)
    , allocator_(VK_NULL_HANDLE)
    , compute_queue_(VK_NULL_HANDLE)
    , compute_queue_family_(0)
    , compute_completion_semaphore_(VK_NULL_HANDLE)
    , compute_command_pool_(VK_NULL_HANDLE)
    , current_frame_(0)
    , autonomous_execution_enabled_(false)
    , autonomous_thread_running_(false) {
}

GpuComputeSystem::~GpuComputeSystem() {
    shutdown();
}

void GpuComputeSystem::initialize() {
    initialize_vulkan_compute();

    // Create CPU arena for coordination tasks
    cpu_arena_ = std::make_unique<CpuArenaAllocator>(64 * 1024 * 1024); // 64MB

    // Initialize subsystems
    arena_manager_ = std::make_unique<VulkanGpuArenaManager>(device_, physical_device_, allocator_);
    shader_compiler_ = std::make_unique<ShaderCompiler>(device_);
    pipeline_manager_ = std::make_unique<ComputePipelineManager>(device_, physical_device_);

    // Create large arenas for different use cases
    uint32_t physics_arena = arena_manager_->create_arena(
        256 * 1024 * 1024, // 256MB for physics
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    uint32_t particle_arena = arena_manager_->create_arena(
        512 * 1024 * 1024, // 512MB for particles
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    uint32_t ecs_arena = arena_manager_->create_arena(
        128 * 1024 * 1024, // 128MB for ECS components
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    // Initialize GPU systems with their dedicated arenas
    physics_system_ = std::make_unique<GpuPhysicsSystem>(device_, physical_device_, allocator_, *arena_manager_);
    particle_system_ = std::make_unique<GpuParticleSystem>(device_, physical_device_, allocator_, *arena_manager_);
    ecs_integration_ = std::make_unique<EcsComputeIntegration>(device_, physical_device_, allocator_, *arena_manager_);

    // Initialize each subsystem
    physics_system_->initialize();
    particle_system_->initialize();
    ecs_integration_->initialize();

    create_compute_synchronization();

    // Enable hot reload for development
    shader_compiler_->enable_hot_reload(true);
}

void GpuComputeSystem::initialize_vulkan_compute() {
    // Access Vulkan context from graphics system
    // This would need to be implemented in the graphics system to expose these handles
    // For now, we'll assume they're available through the graphics system

    // Note: This is a simplified approach - in practice, you'd need to modify
    // the graphics system to expose the Vulkan context properly
}

void GpuComputeSystem::create_compute_synchronization() {
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        check_vk_result(vkCreateSemaphore(device_, &semaphore_info, nullptr, &compute_semaphores_[i]),
                        "create compute semaphore");
        check_vk_result(vkCreateFence(device_, &fence_info, nullptr, &compute_fences_[i]),
                        "create compute fence");
    }

    check_vk_result(vkCreateSemaphore(device_, &semaphore_info, nullptr, &compute_completion_semaphore_),
                    "create compute completion semaphore");

    // Allocate command buffers
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = compute_command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    check_vk_result(vkAllocateCommandBuffers(device_, &alloc_info, compute_command_buffers_.data()),
                    "allocate compute command buffers");
}

void GpuComputeSystem::begin_frame() {
    uint32_t frame_index = current_frame_;

    // Wait for previous frame
    vkWaitForFences(device_, 1, &compute_fences_[frame_index], VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &compute_fences_[frame_index]);

    // Reset command buffer
    vkResetCommandBuffer(compute_command_buffers_[frame_index], 0);
}

void GpuComputeSystem::execute_compute_frame(float delta_time) {
    uint32_t frame_index = current_frame_;
    VkCommandBuffer cmd = compute_command_buffers_[frame_index];

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &begin_info);

    record_compute_commands(cmd, delta_time);

    vkEndCommandBuffer(cmd);

    // Submit compute work
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &compute_semaphores_[frame_index];

    check_vk_result(vkQueueSubmit(compute_queue_, 1, &submit_info, compute_fences_[frame_index]),
                    "submit compute commands");
}

void GpuComputeSystem::record_compute_commands(VkCommandBuffer cmd, float delta_time) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Execute all GPU systems in optimal order
    physics_system_->simulate_step(delta_time);
    particle_system_->update_particles(delta_time);
    ecs_integration_->execute_transform_system(delta_time);

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.total_frame_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    update_performance_stats();
}

void GpuComputeSystem::end_frame() {
    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void GpuComputeSystem::wait_for_compute_completion() {
    uint32_t frame_index = current_frame_;
    vkWaitForFences(device_, 1, &compute_fences_[frame_index], VK_TRUE, UINT64_MAX);
}

void GpuComputeSystem::enable_autonomous_execution(bool enable) {
    autonomous_execution_enabled_.store(enable);

    if (enable && !autonomous_thread_running_.load()) {
        autonomous_thread_running_.store(true);
        autonomous_thread_ = std::thread(&GpuComputeSystem::autonomous_execution_loop, this);
    } else if (!enable && autonomous_thread_running_.load()) {
        autonomous_thread_running_.store(false);
        autonomous_cv_.notify_all();
        if (autonomous_thread_.joinable()) {
            autonomous_thread_.join();
        }
    }
}

void GpuComputeSystem::autonomous_execution_loop() {
    auto last_frame_time = std::chrono::high_resolution_clock::now();
    const auto target_frame_time = std::chrono::microseconds(16667); // ~60 FPS

    while (autonomous_thread_running_.load()) {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time);

        if (elapsed >= target_frame_time) {
            float delta_time = elapsed.count() / 1000000.0f; // Convert to seconds

            begin_frame();
            execute_compute_frame(delta_time);
            end_frame();

            last_frame_time = current_time;
        } else {
            // Sleep for remaining time
            auto sleep_time = target_frame_time - elapsed;
            std::unique_lock<std::mutex> lock(autonomous_mutex_);
            autonomous_cv_.wait_for(lock, sleep_time);
        }
    }
}

void GpuComputeSystem::update_performance_stats() {
    // Update GPU utilization and other performance metrics
    // This would involve GPU timing queries in a full implementation
    stats_.gpu_utilization = 0.95f; // Placeholder - would be measured
    stats_.total_dispatches++;
}

void GpuComputeSystem::shutdown() {
    if (autonomous_thread_running_.load()) {
        enable_autonomous_execution(false);
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);

        // Shutdown subsystems
        if (ecs_integration_) {
            ecs_integration_->shutdown();
        }
        if (particle_system_) {
            particle_system_->shutdown();
        }
        if (physics_system_) {
            physics_system_->shutdown();
        }

        // Destroy synchronization objects
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (compute_semaphores_[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(device_, compute_semaphores_[i], nullptr);
            }
            if (compute_fences_[i] != VK_NULL_HANDLE) {
                vkDestroyFence(device_, compute_fences_[i], nullptr);
            }
        }

        if (compute_completion_semaphore_ != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, compute_completion_semaphore_, nullptr);
        }

        if (compute_command_pool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, compute_command_pool_, nullptr);
        }
    }
}

// Simplified implementations for the remaining systems
// In a full implementation, these would be complete with all compute shader dispatches

// GpuPhysicsSystem simplified implementation
GpuPhysicsSystem::GpuPhysicsSystem(VkDevice device, VkPhysicalDevice physical_device,
                                   VmaAllocator allocator, VulkanGpuArenaManager& arena_manager)
    : device_(device), physical_device_(physical_device), allocator_(allocator), arena_manager_(arena_manager) {
}

GpuPhysicsSystem::~GpuPhysicsSystem() = default;

void GpuPhysicsSystem::initialize(uint32_t max_rigid_bodies) {
    max_rigid_bodies_ = max_rigid_bodies;
    active_body_count_.store(0);
    gravity_ = glm::vec3(0.0f, -9.81f, 0.0f);

    // Allocate GPU buffers from arena
    rigid_bodies_buffer_ = arena_manager_.allocate_on_gpu(0, max_rigid_bodies_ * sizeof(RigidBody));
    collision_shapes_buffer_ = arena_manager_.allocate_on_gpu(0, max_rigid_bodies_ * sizeof(CollisionShape));

    create_physics_pipelines();
}

void GpuPhysicsSystem::shutdown() {
    // Cleanup would be handled by arena manager
}

void GpuPhysicsSystem::simulate_step(float delta_time) {
    // Dispatch physics integration compute shader
    // This would involve actual GPU compute dispatch in full implementation
    stats_.simulation_time = std::chrono::microseconds(500); // Placeholder
}

void GpuPhysicsSystem::create_physics_pipelines() {
    // Create compute pipelines for physics simulation
    // Would load and compile actual compute shaders
}

uint32_t GpuPhysicsSystem::create_rigid_body(const RigidBody& body, const CollisionShape& shape) {
    uint32_t body_id = active_body_count_.fetch_add(1);
    // Update GPU buffers with new rigid body data
    return body_id;
}

void GpuPhysicsSystem::dispatch_physics_compute(VkPipeline pipeline, uint32_t workgroup_count) {
    // Dispatch compute shader with appropriate parameters
}

// GpuParticleSystem simplified implementation
GpuParticleSystem::GpuParticleSystem(VkDevice device, VkPhysicalDevice physical_device,
                                     VmaAllocator allocator, VulkanGpuArenaManager& arena_manager)
    : device_(device), physical_device_(physical_device), allocator_(allocator), arena_manager_(arena_manager) {
}

GpuParticleSystem::~GpuParticleSystem() = default;

void GpuParticleSystem::initialize(uint32_t max_particles) {
    max_particles_ = max_particles;
    active_particle_count_.store(0);

    // Allocate GPU buffers from arena
    particles_buffer_ = arena_manager_.allocate_on_gpu(1, max_particles_ * sizeof(Particle));
    emitters_buffer_ = arena_manager_.allocate_on_gpu(1, 1024 * sizeof(ParticleEmitter));

    create_particle_pipelines();
}

void GpuParticleSystem::shutdown() {
    // Cleanup handled by arena manager
}

void GpuParticleSystem::update_particles(float delta_time) {
    // Dispatch particle update compute shader
    stats_.update_time = std::chrono::microseconds(300); // Placeholder
}

void GpuParticleSystem::create_particle_pipelines() {
    // Create compute pipelines for particle simulation
}

uint32_t GpuParticleSystem::create_emitter(const ParticleEmitter& emitter) {
    std::lock_guard<std::mutex> lock(emitters_mutex_);
    uint32_t emitter_id = static_cast<uint32_t>(emitters_.size());
    emitters_.push_back(emitter);
    return emitter_id;
}

void GpuParticleSystem::dispatch_particle_compute(VkPipeline pipeline, uint32_t workgroup_count) {
    // Dispatch compute shader
}

// EcsComputeIntegration simplified implementation
EcsComputeIntegration::EcsComputeIntegration(VkDevice device, VkPhysicalDevice physical_device,
                                             VmaAllocator allocator, VulkanGpuArenaManager& arena_manager)
    : device_(device), physical_device_(physical_device), allocator_(allocator), arena_manager_(arena_manager) {
}

EcsComputeIntegration::~EcsComputeIntegration() = default;

void EcsComputeIntegration::initialize(uint32_t max_entities) {
    max_entities_ = max_entities;
    active_entity_count_.store(0);

    // Allocate GPU buffers from arena
    entity_indices_buffer_ = arena_manager_.allocate_on_gpu(2, max_entities_ * sizeof(uint32_t));

    create_ecs_pipelines();
}

void EcsComputeIntegration::shutdown() {
    // Cleanup handled by arena manager
}

void EcsComputeIntegration::execute_transform_system(float delta_time) {
    // Dispatch ECS transform update compute shader
    stats_.total_system_time = std::chrono::microseconds(200); // Placeholder
}

void EcsComputeIntegration::create_ecs_pipelines() {
    // Create compute pipelines for ECS systems
}

void EcsComputeIntegration::dispatch_system_compute(VkPipeline pipeline, uint32_t entity_count) {
    // Dispatch compute shader
}

// Missing method implementations for GPU compute demo
void GpuPhysicsSystem::set_gravity(const glm::vec3& gravity) {
    gravity_ = gravity;
}

void EcsComputeIntegration::batch_update_transforms(const std::vector<std::pair<uint32_t, TransformComponent>>& updates) {
    // Update transform components in batch for efficiency using GPU buffers
    for (const auto& [entity_id, transform] : updates) {
        if (entity_id < max_entities_) {
            // Update GPU-side transform buffer through the arena manager
            // This is a simplified implementation for the demo
            stats_.transform_updates++;

            // Mark entity as active if not already
            if (entity_id >= active_entity_count_.load()) {
                active_entity_count_.store(entity_id + 1);
            }
        }
    }
}

template<>
void EcsComputeIntegration::add_component_to_entity<EcsComputeIntegration::VelocityComponent>(
    uint32_t entity_id, const VelocityComponent& component) {

    if (entity_id < max_entities_) {
        // Add velocity component to GPU-side component buffer
        // This is a simplified implementation for the demo
        // In a full implementation, this would update the GPU buffer via arena manager

        // Mark entity as active if not already
        if (entity_id >= active_entity_count_.load()) {
            active_entity_count_.store(entity_id + 1);
        }
    }
}

} // namespace lore::graphics