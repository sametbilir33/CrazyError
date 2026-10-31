cmake_minimum_required(VERSION 3.21)
project(CrazyError VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Multimedia)

add_executable(crazy-error
    src/main.cpp
)

target_link_libraries(crazy-error PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
)

install(TARGETS crazy-error RUNTIME DESTINATION bin)
