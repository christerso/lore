# Input System Tests
# Comprehensive test suite for the event-driven input system

# Test sources
set(INPUT_TEST_SOURCES
    input_system_tests.cpp
)

# Create test executable
add_executable(input_system_tests ${INPUT_TEST_SOURCES})

# Set target properties
set_target_properties(input_system_tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# Include directories
target_include_directories(input_system_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/input/tests
)

# Link libraries
target_link_libraries(input_system_tests PRIVATE
    lore_input
    lore_ecs
    lore_math
    glfw
    glm::glm
    gtest_main
    gmock_main
)

# Add to test suite
add_test(NAME InputSystemTests COMMAND input_system_tests)

# Set test properties
set_tests_properties(InputSystemTests PROPERTIES
    TIMEOUT 300  # 5 minute timeout
    LABELS "input;unit"
)

# Organize in IDE
source_group("Test Sources" FILES ${INPUT_TEST_SOURCES})