# Test executable
add_executable(gara_tests
    utils/file_utils_test.cpp
    services/image_processor_test.cpp
    services/cache_manager_test.cpp
    controllers/image_controller_test.cpp
    integration_test.cpp
)

target_include_directories(gara_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(gara_tests
    PRIVATE
    gara_lib
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(gara_tests)

# Add custom target to run tests with verbose output
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS gara_tests
    COMMENT "Running all tests..."
)
