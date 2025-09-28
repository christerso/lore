# ECS System Library
add_library(lore_ecs STATIC
    ecs.cpp
)

target_include_directories(lore_ecs
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Enable modern C++ features
target_compile_features(lore_ecs
    PUBLIC
        cxx_std_20
)

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(lore_ecs PRIVATE
        $<$<CONFIG:Release>:/O2>         # Optimize for maximum speed in release only
        /arch:AVX2                       # Enable AVX2 instructions
        /fp:fast                         # Fast floating-point model
        $<$<CONFIG:Release>:/GS->        # Disable buffer security checks for performance in release only
        $<$<CONFIG:Release>:/Oy>         # Omit frame pointers in release only
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(lore_ecs PRIVATE
        -O3              # Maximum optimization
        -march=native    # Optimize for target CPU
        -mavx2           # Enable AVX2 instructions
        -msse4.2         # Enable SSE 4.2 instructions
        -ffast-math      # Fast math optimizations
        -funroll-loops   # Unroll loops for performance
        -fomit-frame-pointer  # Omit frame pointers
    )
endif()

# Link dependencies
target_link_libraries(lore_ecs
    PUBLIC
        # Add any external dependencies here
)

# Set target properties
set_target_properties(lore_ecs PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(lore_ecs PRIVATE /W4)
else()
    target_compile_options(lore_ecs PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add alias for consistent naming
add_library(lore::ecs ALIAS lore_ecs)