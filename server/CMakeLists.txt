cmake_minimum_required(VERSION 3.10)
project(StudentServer)

set(CMAKE_CXX_STANDARD 17)

find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Qt5 COMPONENTS Core Network Gui Widgets REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpqxx -lpq")
set(PQXX /usr/include/pqxx)
find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")

include_directories(${PostgreSQL_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${nlohmann_json_INCLUDE_DIRS})
include_directories(${PQXX_INCLUDE_DIRS})

add_executable(server server.cpp ${SOURCE_FILES}
        ../httplib.h)

target_link_libraries(server ${PostgreSQL_LIBRARIES})
target_link_libraries(server ${OPENSSL_LIBRARIES})
target_link_libraries(server nlohmann_json::nlohmann_json)
target_link_libraries(server ${PQXX_LIB} ${PQ_LIB})
target_link_libraries(server Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network)
target_link_libraries(server ${Protobuf_LIBRARIES})

set_target_properties(server PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        AUTOUIC ON
)

if(Qt5Core_VERSION VERSION_GREATER_EQUAL "5.15.0")
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()
