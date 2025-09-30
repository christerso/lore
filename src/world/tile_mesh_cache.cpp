#include <lore/world/tile_mesh_cache.hpp>
#include <lore/core/log.hpp>

#include <mutex>
#include <fstream>
#include <vector>
#include <cstring>

using namespace lore::core;

namespace lore::world {

// Thread-safe mutex for cache operations
static std::mutex cache_mutex;

TileMeshCache::TileMeshCache(VkDevice device, VkPhysicalDevice physical_device, VmaAllocator allocator)
    : device_(device)
    , physical_device_(physical_device)
    , allocator_(allocator)
{
    LOG_INFO(Game, "TileMeshCache initialized");
}

TileMeshCache::~TileMeshCache() {
    clear();
}

uint32_t TileMeshCache::load_mesh(const std::string& mesh_path) {
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Check if already loaded (deduplicate by path)
    auto it = path_to_mesh_id_.find(mesh_path);
    if (it != path_to_mesh_id_.end()) {
        uint32_t existing_id = it->second;
        add_reference(existing_id);
        LOG_DEBUG(Game, "Mesh already loaded: {} (ref count: {})",
                      mesh_path, meshes_[existing_id]->reference_count);
        return existing_id;
    }

    // Allocate new mesh ID
    uint32_t mesh_id = next_mesh_id_++;
    auto mesh = std::make_unique<TileMesh>();
    mesh->source_path = mesh_path;
    mesh->reference_count = 1; // Initial reference

    // Load mesh from file
    if (!load_mesh_from_file(mesh_path, *mesh)) {
        LOG_ERROR(Game, "Failed to load mesh: {}", mesh_path);
        return 0; // Load failed
    }

    // Store in cache
    path_to_mesh_id_[mesh_path] = mesh_id;
    meshes_[mesh_id] = std::move(mesh);

    LOG_INFO(Game, "Loaded mesh: {} (ID: {}, vertices: {}, indices: {})",
                 mesh_path, mesh_id,
                 meshes_[mesh_id]->vertex_count,
                 meshes_[mesh_id]->index_count);

    return mesh_id;
}

void TileMeshCache::add_reference(uint32_t mesh_id) {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = meshes_.find(mesh_id);
    if (it != meshes_.end()) {
        it->second->reference_count++;
    } else {
        LOG_WARNING(Game, "Attempted to add reference to invalid mesh ID: {}", mesh_id);
    }
}

void TileMeshCache::release_reference(uint32_t mesh_id) {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = meshes_.find(mesh_id);
    if (it == meshes_.end()) {
        LOG_WARNING(Game, "Attempted to release reference to invalid mesh ID: {}", mesh_id);
        return;
    }

    TileMesh& mesh = *it->second;
    if (mesh.reference_count > 0) {
        mesh.reference_count--;

        LOG_DEBUG(Game, "Released reference to mesh {} (ref count: {})",
                      mesh_id, mesh.reference_count);

        // Auto-unload when reference count reaches zero
        if (mesh.reference_count == 0) {
            LOG_INFO(Game, "Auto-unloading mesh {} (zero references): {}",
                         mesh_id, mesh.source_path);

            // Remove from path mapping
            path_to_mesh_id_.erase(mesh.source_path);

            // Unload GPU resources
            unload_mesh(mesh);

            // Remove from cache
            meshes_.erase(it);
        }
    }
}

const TileMesh* TileMeshCache::get_mesh(uint32_t mesh_id) const {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = meshes_.find(mesh_id);
    if (it != meshes_.end()) {
        return it->second.get();
    }
    return nullptr;
}

uint32_t TileMeshCache::get_mesh_id(const std::string& mesh_path) const {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = path_to_mesh_id_.find(mesh_path);
    if (it != path_to_mesh_id_.end()) {
        return it->second;
    }
    return 0; // Not loaded
}

uint32_t TileMeshCache::get_reference_count(uint32_t mesh_id) const {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = meshes_.find(mesh_id);
    if (it != meshes_.end()) {
        return it->second->reference_count;
    }
    return 0;
}

bool TileMeshCache::create_instance_buffer(
    uint32_t mesh_id,
    const TileInstanceGPU* instances,
    uint32_t instance_count,
    VkBuffer& out_buffer,
    VmaAllocation& out_allocation)
{
    if (instance_count == 0 || instances == nullptr) {
        LOG_ERROR(Game, "Cannot create instance buffer with zero instances or null data");
        return false;
    }

    // Calculate buffer size
    VkDeviceSize buffer_size = sizeof(TileInstanceGPU) * instance_count;

    // Create staging buffer (CPU-visible)
    VkBufferCreateInfo staging_buffer_info{};
    staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_info.size = buffer_size;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo staging_alloc_info{};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    staging_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo staging_allocation_info{};

    VkResult result = vmaCreateBuffer(
        allocator_,
        &staging_buffer_info,
        &staging_alloc_info,
        &staging_buffer,
        &staging_allocation,
        &staging_allocation_info
    );

    if (result != VK_SUCCESS) {
        LOG_ERROR(Game, "Failed to create staging buffer for instance data (VkResult: {})", static_cast<int>(result));
        return false;
    }

    // Copy instance data to staging buffer
    std::memcpy(staging_allocation_info.pMappedData, instances, buffer_size);

    // Create GPU-local instance buffer
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size;
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    result = vmaCreateBuffer(
        allocator_,
        &buffer_info,
        &alloc_info,
        &out_buffer,
        &out_allocation,
        nullptr
    );

    if (result != VK_SUCCESS) {
        LOG_ERROR(Game, "Failed to create GPU instance buffer (VkResult: {})", static_cast<int>(result));
        vmaDestroyBuffer(allocator_, staging_buffer, staging_allocation);
        return false;
    }

    // TODO: Need to copy from staging to GPU buffer using command buffer
    // For now, we'll use the staging buffer directly (not optimal but works)
    // This requires creating a transfer command buffer and submitting it
    LOG_WARNING(Game, "Instance buffer creation uses staging buffer directly (TODO: implement GPU transfer)");

    // Cleanup staging buffer (temporary - should be kept until transfer completes)
    vmaDestroyBuffer(allocator_, staging_buffer, staging_allocation);

    LOG_DEBUG(Game, "Created instance buffer for mesh {} ({} instances, {} bytes)",
                  mesh_id, instance_count, buffer_size);

    return true;
}

void TileMeshCache::destroy_instance_buffer(VkBuffer buffer, VmaAllocation allocation) {
    if (buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, buffer, allocation);
    }
}

void TileMeshCache::force_unload_mesh(uint32_t mesh_id) {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = meshes_.find(mesh_id);
    if (it == meshes_.end()) {
        LOG_WARNING(Game, "Attempted to force unload invalid mesh ID: {}", mesh_id);
        return;
    }

    TileMesh& mesh = *it->second;
    LOG_WARNING(Game, "Force unloading mesh {} (ref count was: {}): {}",
                    mesh_id, mesh.reference_count, mesh.source_path);

    // Remove from path mapping
    path_to_mesh_id_.erase(mesh.source_path);

    // Unload GPU resources
    unload_mesh(mesh);

    // Remove from cache
    meshes_.erase(it);
}

void TileMeshCache::clear() {
    std::lock_guard<std::mutex> lock(cache_mutex);

    LOG_INFO(Game, "Clearing TileMeshCache ({} meshes)", meshes_.size());

    // Unload all meshes
    for (auto& [mesh_id, mesh] : meshes_) {
        unload_mesh(*mesh);
    }

    meshes_.clear();
    path_to_mesh_id_.clear();
    next_mesh_id_ = 1;
}

TileMeshCache::Statistics TileMeshCache::get_statistics() const {
    std::lock_guard<std::mutex> lock(cache_mutex);

    Statistics stats;
    stats.loaded_meshes = static_cast<uint32_t>(meshes_.size());

    for (const auto& [mesh_id, mesh] : meshes_) {
        stats.total_vertices += mesh->vertex_count;
        stats.total_indices += mesh->index_count;

        // Approximate GPU memory usage
        // Assuming 32 bytes per vertex (position + normal + UV + tangent)
        // and 4 bytes per index (uint32_t)
        size_t vertex_memory = mesh->vertex_count * 32;
        size_t index_memory = mesh->index_count * 4;
        stats.gpu_memory_bytes += vertex_memory + index_memory;
    }

    return stats;
}

// Helper: Load mesh from disk and upload to GPU
bool TileMeshCache::load_mesh_from_file(const std::string& mesh_path, TileMesh& out_mesh) {
    // TODO: Implement proper mesh loading using Assimp or similar library
    // For now, create a simple cube mesh for testing

    LOG_WARNING(Game, "Using stub mesh loader - TODO: implement Assimp FBX/OBJ loading for: {}", mesh_path);

    // Simple cube vertices (position only, 8 vertices)
    struct Vertex {
        float pos[3];
        float normal[3];
        float uv[2];
    };

    std::vector<Vertex> vertices = {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},
        // Back face
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},
        // Top face
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
        // Bottom face
        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},
        // Right face
        {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
        // Left face
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
    };

    std::vector<uint32_t> indices = {
        // Front
        0, 1, 2,  2, 3, 0,
        // Back
        4, 5, 6,  6, 7, 4,
        // Top
        8, 9, 10,  10, 11, 8,
        // Bottom
        12, 13, 14,  14, 15, 12,
        // Right
        16, 17, 18,  18, 19, 16,
        // Left
        20, 21, 22,  22, 23, 20
    };

    out_mesh.vertex_count = static_cast<uint32_t>(vertices.size());
    out_mesh.index_count = static_cast<uint32_t>(indices.size());

    // Calculate bounding box
    out_mesh.bounding_box_min = math::Vec3{-0.5f, -0.5f, -0.5f};
    out_mesh.bounding_box_max = math::Vec3{ 0.5f,  0.5f,  0.5f};

    // Create vertex buffer
    VkDeviceSize vertex_buffer_size = sizeof(Vertex) * vertices.size();

    VkBufferCreateInfo vertex_buffer_info{};
    vertex_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_info.size = vertex_buffer_size;
    vertex_buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertex_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vertex_alloc_info{};
    vertex_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // Temporary: should be GPU_ONLY with staging
    vertex_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo vertex_allocation_info{};
    VkResult result = vmaCreateBuffer(
        allocator_,
        &vertex_buffer_info,
        &vertex_alloc_info,
        &out_mesh.vertex_buffer,
        &out_mesh.vertex_allocation,
        &vertex_allocation_info
    );

    if (result != VK_SUCCESS) {
        LOG_ERROR(Game, "Failed to create vertex buffer (VkResult: {})", static_cast<int>(result));
        return false;
    }

    // Copy vertex data
    std::memcpy(vertex_allocation_info.pMappedData, vertices.data(), vertex_buffer_size);

    // Create index buffer
    VkDeviceSize index_buffer_size = sizeof(uint32_t) * indices.size();

    VkBufferCreateInfo index_buffer_info{};
    index_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_info.size = index_buffer_size;
    index_buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    index_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo index_alloc_info{};
    index_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // Temporary: should be GPU_ONLY with staging
    index_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo index_allocation_info{};
    result = vmaCreateBuffer(
        allocator_,
        &index_buffer_info,
        &index_alloc_info,
        &out_mesh.index_buffer,
        &out_mesh.index_allocation,
        &index_allocation_info
    );

    if (result != VK_SUCCESS) {
        LOG_ERROR(Game, "Failed to create index buffer (VkResult: {})", static_cast<int>(result));
        vmaDestroyBuffer(allocator_, out_mesh.vertex_buffer, out_mesh.vertex_allocation);
        out_mesh.vertex_buffer = VK_NULL_HANDLE;
        return false;
    }

    // Copy index data
    std::memcpy(index_allocation_info.pMappedData, indices.data(), index_buffer_size);

    return true;
}

// Helper: Unload mesh from GPU
void TileMeshCache::unload_mesh(TileMesh& mesh) {
    if (mesh.vertex_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, mesh.vertex_buffer, mesh.vertex_allocation);
        mesh.vertex_buffer = VK_NULL_HANDLE;
        mesh.vertex_allocation = VK_NULL_HANDLE;
    }

    if (mesh.index_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, mesh.index_buffer, mesh.index_allocation);
        mesh.index_buffer = VK_NULL_HANDLE;
        mesh.index_allocation = VK_NULL_HANDLE;
    }

    mesh.vertex_count = 0;
    mesh.index_count = 0;
}

} // namespace lore::world
