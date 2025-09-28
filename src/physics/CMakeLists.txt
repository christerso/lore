# Physics System Library
add_library(lore_physics STATIC
    physics.cpp
)

target_include_directories(lore_physics
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Enable modern C++ features
target_compile_features(lore_physics
    PUBLIC
        cxx_std_20
)

# Physics-specific compiler optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(lore_physics PRIVATE
        $<$<CONFIG:Release>:/O2>        # Optimize for maximum speed in release only
        /arch:AVX2                      # Enable AVX2 instructions for SIMD
        /fp:fast                        # Fast floating-point model for physics calculations
        $<$<CONFIG:Release>:/GS->       # Disable buffer security checks for performance in release only
        $<$<CONFIG:Release>:/Oy>        # Omit frame pointers in release only
        /favor:INTEL64                  # Optimize for 64-bit Intel processors
        $<$<CONFIG:Release>:/Qpar>      # Enable auto-parallelization in release only
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(lore_physics PRIVATE
        -O3                    # Maximum optimization
        -march=native          # Optimize for target CPU
        -mavx2                 # Enable AVX2 instructions
        -msse4.2               # Enable SSE 4.2 instructions
        -mfma                  # Enable FMA instructions for faster math
        -ffast-math            # Fast math optimizations (careful with precision)
        -funroll-loops         # Unroll loops for performance
        -fomit-frame-pointer   # Omit frame pointers
        -ftree-vectorize       # Enable auto-vectorization
        -fopenmp               # Enable OpenMP for parallel execution
    )
endif()

# Physics-specific compiler definitions
target_compile_definitions(lore_physics
    PUBLIC
        LORE_PHYSICS_SIMD_ENABLED    # Enable SIMD optimizations
        LORE_PHYSICS_PARALLEL        # Enable parallel physics processing
    PRIVATE
        _USE_MATH_DEFINES            # Enable M_PI and other math constants
        EIGEN_DONT_PARALLELIZE       # Let us handle parallelization manually
)

# Link dependencies
target_link_libraries(lore_physics
    PUBLIC
        lore::ecs         # ECS system dependency
        lore::math        # Math system dependency
        # Consider adding Bullet Physics or PhysX as optional dependencies
)

# Optional: Link with Bullet Physics for advanced collision detection
option(LORE_USE_BULLET_PHYSICS "Use Bullet Physics library for advanced collision detection" OFF)
if(LORE_USE_BULLET_PHYSICS)
    find_package(Bullet QUIET)
    if(BULLET_FOUND)
        target_link_libraries(lore_physics PUBLIC ${BULLET_LIBRARIES})
        target_include_directories(lore_physics PUBLIC ${BULLET_INCLUDE_DIRS})
        target_compile_definitions(lore_physics PUBLIC LORE_HAS_BULLET_PHYSICS)
    endif()
endif()

# Optional: Link with Intel Threading Building Blocks for parallel algorithms
option(LORE_USE_TBB "Use Intel Threading Building Blocks for parallel physics processing" OFF)
if(LORE_USE_TBB)
    find_package(TBB QUIET)
    if(TBB_FOUND)
        target_link_libraries(lore_physics PUBLIC TBB::tbb)
        target_compile_definitions(lore_physics PUBLIC LORE_HAS_TBB)
    endif()
endif()

# Set target properties
set_target_properties(lore_physics PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Enable comprehensive warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(lore_physics PRIVATE
        /W4                    # High warning level
        /WX                    # Treat warnings as errors
        /wd4201                # Disable warning for nameless struct/union
        /wd4324                # Disable padding warnings for aligned structures
        /wd4127                # Disable conditional expression is constant (for constexpr)
    )
else()
    target_compile_options(lore_physics PRIVATE
        -Wall -Wextra -Wpedantic
        -Werror                # Treat warnings as errors
        -Wno-unused-parameter  # Allow unused parameters in interface functions
        -Wno-padding           # Ignore padding warnings for aligned structures
        -Wno-unknown-pragmas   # Ignore unknown pragma warnings
    )
endif()

# Add alias for consistent naming
add_library(lore::physics ALIAS lore_physics)

# Optional: Enable profiling support
option(LORE_PHYSICS_PROFILING "Enable physics system profiling" OFF)
if(LORE_PHYSICS_PROFILING)
    target_compile_definitions(lore_physics PUBLIC LORE_PHYSICS_PROFILING_ENABLED)

    # Link with a profiling library if available
    find_library(PROFILER_LIB profiler)
    if(PROFILER_LIB)
        target_link_libraries(lore_physics PUBLIC ${PROFILER_LIB})
    endif()
endif()

# Debug configuration specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(lore_physics PRIVATE
        LORE_PHYSICS_DEBUG_ASSERTIONS    # Enable debug assertions
        LORE_PHYSICS_DEBUG_VISUALIZATION # Enable debug visualization
    )
endif()

# Release configuration specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(lore_physics PRIVATE
        LORE_PHYSICS_OPTIMIZED          # Enable release optimizations
        NDEBUG                           # Disable debug assertions
    )
endif()