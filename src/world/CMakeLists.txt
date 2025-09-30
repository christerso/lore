# World System Library
add_library(lore_world STATIC
    tilemap_world_system.cpp
    tiled_importer.cpp
    tile_mesh_cache.cpp
    voronoi_fracture.cpp
    debris_physics.cpp
    tile_damage.cpp
)

# Link dependencies
target_link_libraries(lore_world PUBLIC
    lore_core           # For logging
    lore_ecs            # For ECS types
    lore_math           # For Vec3 and math types
    lore_vision         # For vision integration
    lore_graphics       # For Vulkan types (VkDevice, VmaAllocator, etc.)
    glm::glm            # For matrix types
    nlohmann_json::nlohmann_json  # For JSON parsing
    VulkanMemoryAllocator  # For VMA
)

# Include directories
target_include_directories(lore_world PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)