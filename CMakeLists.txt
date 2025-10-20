cmake_minimum_required(VERSION 3.28)
project(gara)

# Workaround for CMake 4.x compatibility with older dependencies
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CROW_ENABLE_COMPRESSION ON)

# Include FetchContent module for dependency management
include(FetchContent)

# Find system packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)
pkg_check_modules(VIPS REQUIRED vips-cpp)

# Fetch ASIO (required by Crow)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
)
FetchContent_MakeAvailable(asio)

# Set ASIO include directory for Crow
set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)

# Fetch Crow web framework
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG master
)
FetchContent_MakeAvailable(crow)

# Fetch nlohmann/json for JSON handling
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Fetch AWS SDK
set(BUILD_ONLY "dynamodb;s3;secretsmanager" CACHE STRING "AWS SDK components to build")
set(ENABLE_TESTING OFF CACHE BOOL "Disable AWS SDK tests")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build AWS SDK as static libraries")
set(FORCE_SHARED_CRT OFF CACHE BOOL "Use static CRT")
set(USE_SYSTEM_CURL ON CACHE BOOL "Use system CURL library")

FetchContent_Declare(
    awssdk
    GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp.git
    GIT_TAG 1.11.474  # Updated to latest stable version with better CMake 4.x support
)
FetchContent_MakeAvailable(awssdk)

# Fetch Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Create VIPS interface libraries
add_library(vips INTERFACE)
target_include_directories(vips INTERFACE ${VIPS_INCLUDE_DIRS})
target_link_directories(vips INTERFACE ${VIPS_LIBRARY_DIRS})
target_link_libraries(vips INTERFACE ${VIPS_LDFLAGS})

add_library(vips-cpp INTERFACE)
target_link_libraries(vips-cpp INTERFACE vips)

# Library with shared code (for both main app and tests)
add_library(gara_lib
    src/models/image_metadata.cpp
    src/utils/file_utils.cpp
    src/services/s3_service.cpp
    src/services/image_processor.cpp
    src/services/cache_manager.cpp
    src/services/secrets_service.cpp
    src/middleware/auth_middleware.cpp
    src/controllers/image_controller.cpp
)

target_include_directories(gara_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLIB_INCLUDE_DIRS}
    ${GOBJECT_INCLUDE_DIRS}
)

target_link_directories(gara_lib PUBLIC
    ${GLIB_LIBRARY_DIRS}
    ${GOBJECT_LIBRARY_DIRS}
)

target_link_libraries(gara_lib
    PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    Crow::Crow
    nlohmann_json::nlohmann_json
    Threads::Threads
    aws-cpp-sdk-s3
    aws-cpp-sdk-secretsmanager
    aws-cpp-sdk-core
    vips-cpp
    ${GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
)

# Main executable
add_executable(gara src/main.cpp)
target_link_libraries(gara PRIVATE gara_lib)

# Enable testing (only if tests directory exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS gara DESTINATION bin)
