cmake_minimum_required(VERSION 3.10)
project(ServerTests)

set(CMAKE_CXX_STANDARD 17)

include_directories(../server)
include_directories(googletest/include)

file(GLOB TEST_SOURCES "*.cpp")

find_package(Qt5 COMPONENTS Core Gui REQUIRED)
find_package(PostgreSQL REQUIRED)

add_executable(ServerTests ${TEST_SOURCES})

target_link_libraries(ServerTests gtest gtest_main pqxx Qt5::Core Qt5::Gui)