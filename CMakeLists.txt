cmake_minimum_required(VERSION 3.28)
project(gara-image)

# Workaround for CMake 4.x compatibility with older dependencies
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CROW_ENABLE_COMPRESSION ON)

# Code coverage option
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# Include FetchContent module for dependency management
include(FetchContent)

# Find system packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)

# MySQL support (optional)
option(ENABLE_MYSQL "Enable MySQL database support" ON)
if(ENABLE_MYSQL)
    pkg_check_modules(MYSQL mysqlclient)
    if(MYSQL_FOUND)
        message(STATUS "MySQL support enabled via pkg-config")
        message(STATUS "  MYSQL_INCLUDE_DIRS (raw): ${MYSQL_INCLUDE_DIRS}")
        message(STATUS "  MYSQL_LIBRARY_DIRS: ${MYSQL_LIBRARY_DIRS}")
        message(STATUS "  MYSQL_LIBRARIES: ${MYSQL_LIBRARIES}")
        message(STATUS "  MYSQL_CFLAGS: ${MYSQL_CFLAGS}")

        # Fix: pkg-config may return path ending in /mysql, but we need parent dir
        # since we #include <mysql/mysql.h>
        if(MYSQL_INCLUDE_DIRS)
            string(REGEX REPLACE "/mysql$" "" MYSQL_INCLUDE_DIRS "${MYSQL_INCLUDE_DIRS}")
            message(STATUS "  MYSQL_INCLUDE_DIRS (fixed): ${MYSQL_INCLUDE_DIRS}")
        endif()

        # On macOS, MYSQL_INCLUDE_DIRS may be empty, use MYSQL_INCLUDE_DIR if provided
        if(NOT MYSQL_INCLUDE_DIRS)
            if(MYSQL_INCLUDE_DIR)
                # Use the path passed from command line
                set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
                message(STATUS "  Using provided MYSQL_INCLUDE_DIR: ${MYSQL_INCLUDE_DIRS}")
            elseif(APPLE)
                # Try Homebrew locations
                find_path(MYSQL_INCLUDE_DIR_FOUND mysql/mysql.h
                    HINTS
                        /opt/homebrew/opt/mysql-client/include
                        /usr/local/opt/mysql-client/include
                        /opt/homebrew/include
                        /usr/local/include
                )
                if(MYSQL_INCLUDE_DIR_FOUND)
                    set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR_FOUND})
                    message(STATUS "  Found MySQL headers at: ${MYSQL_INCLUDE_DIRS}")
                endif()
            endif()
        endif()

        add_definitions(-DGARA_MYSQL_SUPPORT)
    else()
        message(WARNING "MySQL client library not found, MySQL support disabled")
        set(ENABLE_MYSQL OFF)
    endif()
endif()
# DynamoDB support (optional)
option(ENABLE_DYNAMODB "Enable DynamoDB database support" OFF)
if(ENABLE_DYNAMODB)
    find_package(AWSSDK COMPONENTS dynamodb)
    if(AWSSDK_FOUND)
        message(STATUS "DynamoDB support enabled via AWS SDK")
        message(STATUS "  AWS SDK version: ${AWSSDK_VERSION}")
        message(STATUS "  AWS SDK libraries: ${AWSSDK_LINK_LIBRARIES}")
        add_definitions(-DGARA_DYNAMODB_SUPPORT)
    else()
        message(WARNING "AWS SDK not found, DynamoDB support disabled")
        set(ENABLE_DYNAMODB OFF)
    endif()
endif()

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

# Fetch spdlog for structured logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

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
set(GARA_LIB_SOURCES
    src/models/image_metadata.cpp
    src/models/album.cpp
    src/utils/file_utils.cpp
    src/utils/id_generator.cpp
    src/utils/logger.cpp
    src/utils/metrics.cpp
    src/db/sqlite_client.cpp
    src/services/local_file_service.cpp
    src/services/image_processor.cpp
    src/services/cache_manager.cpp
    src/services/local_config_service.cpp
    src/services/watermark_service.cpp
    src/services/album_service.cpp
    src/middleware/auth_middleware.cpp
    src/controllers/image_controller.cpp
    src/controllers/album_controller.cpp
)

# Add MySQL client source if MySQL support is enabled
if(ENABLE_MYSQL AND MYSQL_FOUND)
    list(APPEND GARA_LIB_SOURCES src/db/mysql_client.cpp)
endif()

# Add DynamoDB client source if DynamoDB support is enabled
if(ENABLE_DYNAMODB AND AWSSDK_FOUND)
    list(APPEND GARA_LIB_SOURCES src/db/dynamodb_client.cpp)
endif()

add_library(gara_lib ${GARA_LIB_SOURCES})

target_include_directories(gara_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLIB_INCLUDE_DIRS}
    ${GOBJECT_INCLUDE_DIRS}
)

# Add MySQL include directories if available
if(ENABLE_MYSQL AND MYSQL_FOUND)
    message(STATUS "Applying MySQL includes: MYSQL_INCLUDE_DIRS=${MYSQL_INCLUDE_DIRS}")
    # Use MYSQL_INCLUDE_DIRS if available
    if(MYSQL_INCLUDE_DIRS)
        # Use both global and target-specific includes to ensure visibility
        include_directories(${MYSQL_INCLUDE_DIRS})
        target_include_directories(gara_lib PUBLIC ${MYSQL_INCLUDE_DIRS})
        message(STATUS "Added MySQL include directory: ${MYSQL_INCLUDE_DIRS}")
    endif()
    if(MYSQL_LIBRARY_DIRS)
        target_link_directories(gara_lib PUBLIC ${MYSQL_LIBRARY_DIRS})
    endif()
    # Add any additional compile flags from pkg-config
    if(MYSQL_CFLAGS)
        target_compile_options(gara_lib PUBLIC ${MYSQL_CFLAGS})
    endif()
endif()

target_link_directories(gara_lib PUBLIC
    ${GLIB_LIBRARY_DIRS}
    ${GOBJECT_LIBRARY_DIRS}
)

set(GARA_LINK_LIBRARIES
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    SQLite::SQLite3
    Crow::Crow
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
    vips-cpp
    ${GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
)

# Add MySQL libraries if available
if(ENABLE_MYSQL AND MYSQL_FOUND)
    list(APPEND GARA_LINK_LIBRARIES ${MYSQL_LIBRARIES})
endif()

# Add AWS SDK libraries if available
if(ENABLE_DYNAMODB AND AWSSDK_FOUND)
    list(APPEND GARA_LINK_LIBRARIES ${AWSSDK_LINK_LIBRARIES})
endif()

target_link_libraries(gara_lib PUBLIC ${GARA_LINK_LIBRARIES})

# Main executable
add_executable(gara-image src/main.cpp)
target_link_libraries(gara-image PRIVATE gara_lib)

# Install the main executable
install(TARGETS gara-image
        RUNTIME DESTINATION bin)


# Enable testing (only if tests directory exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()


