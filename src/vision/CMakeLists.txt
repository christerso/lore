# Lore Vision Library
# Standalone 3D Line of Sight and Field of View library for tile-based games

project(lore_vision VERSION 1.0.0 LANGUAGES CXX)

# Create standalone vision library
add_library(lore_vision STATIC
    bresenham_3d.cpp
    shadow_casting_fov.cpp
    vision_debug.cpp
)

# Minimal dependencies - vision library is standalone
target_link_libraries(lore_vision PUBLIC
    lore_math           # Only needs math types (Vec3, etc.)
)

# Include directories
target_include_directories(lore_vision PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Export for external use
set_target_properties(lore_vision PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "../../include/lore/vision/vision_profile.hpp;../../include/lore/vision/bresenham_3d.hpp"
)

# Optional: Install targets for distribution as separate library
install(TARGETS lore_vision
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/lore/vision
)