cmake_minimum_required(VERSION 3.28)
project(gara)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CROW_ENABLE_COMPRESSION ON)

# Include FetchContent module for dependency management
include(FetchContent)

# Find system packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

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

# Fetch inja template engine
FetchContent_Declare(
    inja
    GIT_REPOSITORY https://github.com/pantor/inja.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(inja)

# Fetch AWS SDK
set(BUILD_ONLY "dynamodb;s3;secretsmanager" CACHE STRING "AWS SDK components to build")
set(ENABLE_TESTING OFF CACHE BOOL "Disable AWS SDK tests")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version")
FetchContent_Declare(
    awssdk
    GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp.git
    GIT_TAG 1.11.400
)
FetchContent_MakeAvailable(awssdk)

add_executable(gara src/main.cpp)

target_link_libraries(gara
        PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        Crow::Crow
        pantor::inja
        Threads::Threads
)

install(TARGETS gara DESTINATION bin)
