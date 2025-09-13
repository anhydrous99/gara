cmake_minimum_required(VERSION 3.28)
project(gara)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CROW_ENABLE_COMPRESSION ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS dynamodb s3 secretsmanager)
find_package(Crow REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(inja REQUIRED)

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
