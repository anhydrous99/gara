cmake_minimum_required(VERSION 3.28)
project(gara)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CROW_ENABLE_COMPRESSION ON)

# Include FetchContent and ExternalProject modules for dependency management
include(FetchContent)
include(ExternalProject)

# Find system packages BEFORE setting static flags
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)

# Static linking configuration (after finding packages)
option(BUILD_STATIC "Build fully static executable" ON)

if(BUILD_STATIC)
    set(BUILD_SHARED_LIBS OFF)

    # For complete static linking on Linux
    if(UNIX AND NOT APPLE)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    endif()

    # For macOS (can only statically link our own libraries, not system libs)
    if(APPLE)
        # macOS doesn't support fully static binaries
        # We'll link our dependencies statically but system libs remain dynamic
        message(STATUS "macOS detected: Building with static dependencies but dynamic system libraries")
    endif()
endif()

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
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build AWS SDK as static libraries")
set(FORCE_SHARED_CRT OFF CACHE BOOL "Use static CRT")
FetchContent_Declare(
    awssdk
    GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp.git
    GIT_TAG 1.11.400
)
FetchContent_MakeAvailable(awssdk)

# Fetch Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Prevent overriding parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Try to find system libvips first
pkg_check_modules(VIPS vips-cpp)

if(VIPS_FOUND)
    # Use system libvips
    message(STATUS "Using system libvips: ${VIPS_VERSION}")
    message(STATUS "VIPS libraries: ${VIPS_LIBRARIES}")
    message(STATUS "VIPS library dirs: ${VIPS_LIBRARY_DIRS}")
    message(STATUS "VIPS include dirs: ${VIPS_INCLUDE_DIRS}")

    add_library(vips INTERFACE)
    target_include_directories(vips INTERFACE ${VIPS_INCLUDE_DIRS})
    target_link_directories(vips INTERFACE ${VIPS_LIBRARY_DIRS})
    target_link_libraries(vips INTERFACE ${VIPS_LDFLAGS})

    add_library(vips-cpp INTERFACE)
    target_link_libraries(vips-cpp INTERFACE vips)
else()
    # Build libvips from source using ExternalProject
    message(STATUS "Building libvips from source")
    set(VIPS_PREFIX ${CMAKE_BINARY_DIR}/libvips-install)
    set(VIPS_INCLUDE_DIR ${VIPS_PREFIX}/include)

    # Platform-specific library configuration
    if(APPLE)
        set(VIPS_LIB ${VIPS_PREFIX}/lib/libvips.dylib)
        set(VIPS_CPP_LIB ${VIPS_PREFIX}/lib/libvips-cpp.dylib)
        set(VIPS_LIB_TYPE SHARED)
    elseif(BUILD_STATIC)
        set(VIPS_LIB ${VIPS_PREFIX}/lib/libvips.a)
        set(VIPS_CPP_LIB ${VIPS_PREFIX}/lib/libvips-cpp.a)
        set(VIPS_LIB_TYPE STATIC)
    else()
        set(VIPS_LIB ${VIPS_PREFIX}/lib/libvips.so)
        set(VIPS_CPP_LIB ${VIPS_PREFIX}/lib/libvips-cpp.so)
        set(VIPS_LIB_TYPE SHARED)
    endif()

    file(MAKE_DIRECTORY ${VIPS_INCLUDE_DIR})

    # Configure meson build options
    if(BUILD_STATIC AND NOT APPLE)
        set(MESON_DEFAULT_LIBRARY "static")
        set(MESON_PREFER_STATIC "-Dprefer_static=true")
    else()
        set(MESON_DEFAULT_LIBRARY "shared")
        set(MESON_PREFER_STATIC "")
    endif()

    ExternalProject_Add(
        libvips_external
        GIT_REPOSITORY https://github.com/libvips/libvips.git
        GIT_TAG v8.16.0
        PREFIX ${CMAKE_BINARY_DIR}/libvips
        CONFIGURE_COMMAND meson setup
                          --prefix=${VIPS_PREFIX}
                          --buildtype=release
                          --default-library=${MESON_DEFAULT_LIBRARY}
                          ${MESON_PREFER_STATIC}
                          -Dintrospection=disabled
                          -Ddeprecated=false
                          <SOURCE_DIR> <BINARY_DIR>
        BUILD_COMMAND meson compile -C <BINARY_DIR>
        INSTALL_COMMAND meson install -C <BINARY_DIR>
        BUILD_BYPRODUCTS ${VIPS_LIB} ${VIPS_CPP_LIB}
        LOG_CONFIGURE TRUE
        LOG_BUILD TRUE
        LOG_INSTALL TRUE
        LOG_OUTPUT_ON_FAILURE TRUE
    )

    add_library(vips ${VIPS_LIB_TYPE} IMPORTED GLOBAL)
    set_target_properties(vips PROPERTIES
        IMPORTED_LOCATION ${VIPS_LIB}
        INTERFACE_INCLUDE_DIRECTORIES ${VIPS_INCLUDE_DIR}
    )
    add_dependencies(vips libvips_external)

    add_library(vips-cpp ${VIPS_LIB_TYPE} IMPORTED GLOBAL)
    set_target_properties(vips-cpp PROPERTIES
        IMPORTED_LOCATION ${VIPS_CPP_LIB}
        INTERFACE_INCLUDE_DIRECTORIES ${VIPS_INCLUDE_DIR}
        INTERFACE_LINK_LIBRARIES vips
    )
    add_dependencies(vips-cpp libvips_external)
endif()

# Library with shared code (for both main app and tests)
add_library(gara_lib
    src/models/image_metadata.cpp
    src/utils/file_utils.cpp
    src/services/s3_service.cpp
    src/services/image_processor.cpp
    src/services/cache_manager.cpp
    src/controllers/image_controller.cpp
)

target_include_directories(gara_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${VIPS_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GOBJECT_INCLUDE_DIRS}
)

target_link_directories(gara_lib PUBLIC
    ${GLIB_LIBRARY_DIRS}
    ${GOBJECT_LIBRARY_DIRS}
)

# Ensure libvips is built before gara_lib (only needed when building from source)
if(NOT VIPS_FOUND)
    add_dependencies(gara_lib vips)
endif()

target_link_libraries(gara_lib
        PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        Crow::Crow
        nlohmann_json::nlohmann_json
        Threads::Threads
        aws-cpp-sdk-s3
        aws-cpp-sdk-core
        vips-cpp
        vips
        ${GOBJECT_LIBRARIES}
        ${GLIB_LIBRARIES}
)

# Main executable
add_executable(gara src/main.cpp)
target_link_libraries(gara PRIVATE gara_lib)

# Additional static linking flags for the executable (Linux only)
if(BUILD_STATIC AND UNIX AND NOT APPLE)
    # Link additional system libraries statically on Linux
    target_link_libraries(gara PRIVATE -lpthread -ldl)
endif()

# Enable testing
enable_testing()
add_subdirectory(tests)

install(TARGETS gara DESTINATION bin)
