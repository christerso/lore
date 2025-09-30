# Math System Library
add_library(lore_math STATIC
    math.cpp
)

target_include_directories(lore_math
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# GLM is added as subdirectory in main CMakeLists.txt, no need to find_package

# Enable modern C++ features
target_compile_features(lore_math
    PUBLIC
        cxx_std_20
)

# Compiler-specific SIMD optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(lore_math PRIVATE
        $<$<CONFIG:Release>:/O2>        # Optimize for maximum speed in release only
        /arch:AVX2                      # Enable AVX2 instructions
        /arch:SSE2                      # Enable SSE2 instructions (baseline)
        /fp:fast                        # Fast floating-point model
        $<$<CONFIG:Release>:/GS->       # Disable buffer security checks for performance in release only
        $<$<CONFIG:Release>:/Oy>        # Omit frame pointers in release only
        /favor:INTEL64                  # Optimize for 64-bit Intel processors
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(lore_math PRIVATE
        -O3                    # Maximum optimization
        -march=native          # Optimize for target CPU
        -mavx2                 # Enable AVX2 instructions
        -msse4.2               # Enable SSE 4.2 instructions
        -mfma                  # Enable FMA instructions
        -ffast-math            # Fast math optimizations
        -funroll-loops         # Unroll loops for performance
        -fomit-frame-pointer   # Omit frame pointers
        -ftree-vectorize       # Enable auto-vectorization
    )
endif()

# Math-specific compiler definitions
target_compile_definitions(lore_math
    PUBLIC
        GLM_FORCE_INTRINSICS        # Use SIMD intrinsics in GLM
        GLM_FORCE_AVX2              # Force AVX2 in GLM
        GLM_FORCE_ALIGNED_GENTYPES  # Force aligned types for SIMD
        GLM_FORCE_XYZW_ONLY         # Only use .xyzw swizzles for performance
        GLM_FORCE_EXPLICIT_CTOR     # Require explicit constructors
        GLM_ENABLE_EXPERIMENTAL     # Enable experimental GLM features
        GLM_FORCE_CTOR_INIT         # Initialize values in constructors
    PRIVATE
        _USE_MATH_DEFINES           # Enable M_PI and other math constants
)

# Link dependencies
target_link_libraries(lore_math
    PUBLIC
        glm::glm
        # Add any additional math libraries here (e.g., Intel MKL)
)

# Set target properties
set_target_properties(lore_math PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Enable comprehensive warnings for better code quality
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(lore_math PRIVATE
        /W4                    # High warning level
        /WX                    # Treat warnings as errors
        /wd4201                # Disable warning for nameless struct/union
        /wd4324                # Disable padding warnings for aligned structures
    )
else()
    target_compile_options(lore_math PRIVATE
        -Wall -Wextra -Wpedantic
        -Werror                # Treat warnings as errors
        -Wno-unused-parameter  # Allow unused parameters in interface functions
        -Wno-padding           # Ignore padding warnings for aligned structures
    )
endif()

# Add alias for consistent naming
add_library(lore::math ALIAS lore_math)

# Optional: Link Intel MKL for advanced mathematical operations
option(LORE_USE_INTEL_MKL "Use Intel Math Kernel Library for advanced math operations" OFF)
if(LORE_USE_INTEL_MKL)
    find_package(MKL QUIET)
    if(MKL_FOUND)
        target_link_libraries(lore_math PUBLIC ${MKL_LIBRARIES})
        target_include_directories(lore_math PUBLIC ${MKL_INCLUDE_DIRS})
        target_compile_definitions(lore_math PUBLIC LORE_HAS_INTEL_MKL)
    endif()
endif()

# Optional: Link AMD LibM for AMD-optimized math functions
option(LORE_USE_AMD_LIBM "Use AMD LibM for AMD-optimized math operations" OFF)
if(LORE_USE_AMD_LIBM)
    find_library(AMD_LIBM amdlibm)
    if(AMD_LIBM)
        target_link_libraries(lore_math PUBLIC ${AMD_LIBM})
        target_compile_definitions(lore_math PUBLIC LORE_HAS_AMD_LIBM)
    endif()
endif()