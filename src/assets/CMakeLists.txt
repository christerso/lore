# Assets library with Lore package system
add_library(lore_assets STATIC
    assets.cpp
)

# Configure compile features
target_compile_features(lore_assets PUBLIC cxx_std_23)

# Set compile definitions
target_compile_definitions(lore_assets PRIVATE
    LORE_ASSETS_BASIC_BUILD=1
)

# Include directories
target_include_directories(lore_assets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Link required libraries
target_link_libraries(lore_assets PUBLIC
    lore_ecs
    lore_math
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(lore_assets PRIVATE
        kernel32
        user32
        advapi32
    )
elseif(UNIX)
    target_link_libraries(lore_assets PRIVATE
        pthread
        dl
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(lore_assets PRIVATE
        /W4
        /wd4996  # Disable deprecated function warnings
        /wd4251  # Disable DLL interface warnings
        /permissive-
        /Zc:__cplusplus
    )

    # Enable parallel compilation
    target_compile_options(lore_assets PRIVATE /MP)

else()
    target_compile_options(lore_assets PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )

    # Enable position independent code
    set_target_properties(lore_assets PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# Debug/Release specific options
target_compile_definitions(lore_assets PRIVATE
    $<$<CONFIG:Debug>:LORE_ASSETS_DEBUG=1>
    $<$<CONFIG:Release>:LORE_ASSETS_RELEASE=1>
    $<$<CONFIG:Release>:NDEBUG>
)

# Set target properties
set_target_properties(lore_assets PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "lore_assets"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Create alias target
add_library(lore::assets ALIAS lore_assets)

message(STATUS "Configured lore_assets with basic Lore package support")