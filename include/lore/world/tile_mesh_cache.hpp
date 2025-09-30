#pragma once

#include <lore/math/math.hpp>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>

namespace lore::world {

/**
 * @brief GPU instance data for intact tiles (64 bytes per instance)
 *
 * This structure is uploaded to a GPU buffer for instanced rendering.
 * All intact tiles sharing the same mesh use GPU instancing for
 * massive performance improvement (2 draw calls instead of 121).
 */
struct alignas(16) TileInstanceGPU {
    math::Mat4 transform;           // 64 bytes: model matrix (position, rotation, scale)
    // Future expansion: uint32_t material_override, tint_color, etc.
};

static_assert(sizeof(TileInstanceGPU) == 64, "TileInstanceGPU must be 64 bytes for GPU alignment");

/**
 * @brief Mesh resource data loaded from disk
 *
 * Contains vertex/index buffers and metadata for a 3D mesh.
 * Shared by all intact tiles using this mesh via reference counting.
 */
struct TileMesh {
    // Vulkan mesh resources
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_allocation = VK_NULL_HANDLE;
    uint32_t vertex_count = 0;

    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation index_allocation = VK_NULL_HANDLE;
    uint32_t index_count = 0;

    // Mesh metadata
    std::string source_path;        // Original file path (e.g., "meshes/wall_stone.fbx")
    math::Vec3 bounding_box_min;    // AABB min
    math::Vec3 bounding_box_max;    // AABB max

    // Reference counting
    uint32_t reference_count = 0;   // Number of tiles using this mesh

    bool is_loaded() const {
        return vertex_buffer != VK_NULL_HANDLE && index_buffer != VK_NULL_HANDLE;
    }
};

/**
 * @brief Tile destruction state machine
 *
 * Tracks the lifecycle of a tile from intact to fully destroyed.
 * Intact tiles use GPU instancing, broken tiles have unique geometry.
 */
enum class TileState : uint32_t {
    Pristine = 0,       // Undamaged, uses GPU instancing
    Scratched = 1,      // Minor surface damage, still instanced
    Cracked = 2,        // Visible cracks, still instanced with crack decals
    Damaged = 3,        // Significant damage, still instanced
    Failing = 4,        // Critical damage, cracks propagating rapidly
    Critical = 5,       // About to collapse, final warning
    Collapsed = 6       // Fractured into debris pieces (NO LONGER INSTANCED)
};

/**
 * @brief TileMeshCache - Manages shared mesh resources with reference counting
 *
 * Key Features:
 * - Single mesh shared by thousands of intact tiles
 * - Reference counting: auto-unload when no tiles use mesh
 * - GPU instancing for 2 draw calls per room (not 121)
 * - Memory: 64 bytes per intact tile
 *
 * Usage Example:
 * ```cpp
 * TileMeshCache cache(device, allocator);
 *
 * // Load mesh (reference count: 1)
 * uint32_t mesh_id = cache.load_mesh("meshes/wall_stone.fbx");
 *
 * // 1000 more tiles use it (reference count: 1001)
 * for (int i = 0; i < 1000; ++i) {
 *     cache.add_reference(mesh_id);
 * }
 *
 * // Tiles are destroyed (reference count: 0)
 * cache.release_reference(mesh_id);  // Auto-unloads mesh
 * ```
 */
class TileMeshCache {
public:
    TileMeshCache(VkDevice device, VkPhysicalDevice physical_device, VmaAllocator allocator);
    ~TileMeshCache();

    /**
     * @brief Load mesh from file (if not already loaded)
     *
     * @param mesh_path Path to mesh file (e.g., "meshes/wall_stone.fbx")
     * @return Mesh ID for future references (0 = load failed)
     *
     * Thread-safe: Can be called from multiple threads simultaneously.
     * Reference count is automatically incremented.
     */
    uint32_t load_mesh(const std::string& mesh_path);

    /**
     * @brief Add reference to mesh (increment reference count)
     *
     * Call this when a new tile starts using this mesh.
     *
     * @param mesh_id Mesh ID returned by load_mesh()
     */
    void add_reference(uint32_t mesh_id);

    /**
     * @brief Release reference to mesh (decrement reference count)
     *
     * Call this when a tile stops using this mesh (e.g., destroyed).
     * If reference count reaches 0, mesh is automatically unloaded from GPU.
     *
     * @param mesh_id Mesh ID returned by load_mesh()
     */
    void release_reference(uint32_t mesh_id);

    /**
     * @brief Get mesh by ID (returns nullptr if not loaded)
     *
     * @param mesh_id Mesh ID returned by load_mesh()
     * @return Pointer to mesh data (valid until mesh is unloaded)
     */
    const TileMesh* get_mesh(uint32_t mesh_id) const;

    /**
     * @brief Get mesh ID by file path (0 if not loaded)
     *
     * @param mesh_path Path to mesh file
     * @return Mesh ID or 0 if not loaded
     */
    uint32_t get_mesh_id(const std::string& mesh_path) const;

    /**
     * @brief Get reference count for mesh
     *
     * @param mesh_id Mesh ID returned by load_mesh()
     * @return Number of tiles currently using this mesh (0 if invalid ID)
     */
    uint32_t get_reference_count(uint32_t mesh_id) const;

    /**
     * @brief Create instance buffer for GPU instancing
     *
     * Creates a VkBuffer containing TileInstanceGPU data for all tiles
     * using the specified mesh. This buffer is used for instanced rendering.
     *
     * @param mesh_id Mesh ID
     * @param instances Array of instance transforms
     * @param instance_count Number of instances
     * @param out_buffer Output: instance buffer
     * @param out_allocation Output: buffer allocation
     * @return true if successful
     */
    bool create_instance_buffer(
        uint32_t mesh_id,
        const TileInstanceGPU* instances,
        uint32_t instance_count,
        VkBuffer& out_buffer,
        VmaAllocation& out_allocation
    );

    /**
     * @brief Destroy instance buffer created by create_instance_buffer()
     */
    void destroy_instance_buffer(VkBuffer buffer, VmaAllocation allocation);

    /**
     * @brief Force unload mesh (ignores reference count)
     *
     * WARNING: Only use this if you're certain no tiles are using the mesh.
     * Useful for cleanup or editor hot-reloading.
     *
     * @param mesh_id Mesh ID to unload
     */
    void force_unload_mesh(uint32_t mesh_id);

    /**
     * @brief Unload all meshes (clears entire cache)
     *
     * WARNING: Only call this when shutting down or clearing the world.
     */
    void clear();

    /**
     * @brief Get cache statistics
     */
    struct Statistics {
        uint32_t loaded_meshes = 0;         // Number of meshes in cache
        uint32_t total_vertices = 0;        // Total vertex count across all meshes
        uint32_t total_indices = 0;         // Total index count across all meshes
        size_t gpu_memory_bytes = 0;        // Approximate GPU memory usage
    };
    Statistics get_statistics() const;

private:
    // Vulkan handles
    VkDevice device_;
    VkPhysicalDevice physical_device_;
    VmaAllocator allocator_;

    // Mesh storage
    std::unordered_map<uint32_t, std::unique_ptr<TileMesh>> meshes_;
    uint32_t next_mesh_id_ = 1;

    // Path to mesh ID mapping (for deduplication)
    std::unordered_map<std::string, uint32_t> path_to_mesh_id_;

    // Helper: Load mesh from disk and upload to GPU
    bool load_mesh_from_file(const std::string& mesh_path, TileMesh& out_mesh);

    // Helper: Unload mesh from GPU
    void unload_mesh(TileMesh& mesh);
};

} // namespace lore::world
