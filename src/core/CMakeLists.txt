# Lore Core Library
# Provides fundamental utilities like logging, configuration, and memory management

add_library(lore_core STATIC
    log.cpp
)

target_include_directories(lore_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(lore_core PUBLIC
    # Core library has no external dependencies
)

# Set properties
set_target_properties(lore_core PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Lore/Core"
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(lore_core PRIVATE
        _UNICODE
        UNICODE
    )
endif()

# Install rules
install(TARGETS lore_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)