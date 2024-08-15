cmake_minimum_required(VERSION 3.10)
project(cliser)

set(CMAKE_CXX_STANDARD 17)
include_directories(${CMAKE_SOURCE_DIR}/cpp-httplib)
include_directories(server/images)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(test)
