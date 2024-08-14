cmake_minimum_required(VERSION 3.14)
project(ClientApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Protobuf REQUIRED)

add_executable(ClientApp client.cpp )

target_link_libraries(ClientApp ${Protobuf_LIBRARIES})

include_directories(${Protobuf_INCLUDE_DIRS})

set(SOURCES
        client.cpp
)

target_link_libraries(ClientApp Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network)

set_target_properties(ClientApp PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        AUTOUIC ON
)

if(Qt5Core_VERSION VERSION_GREATER_EQUAL "5.15.0")
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()
