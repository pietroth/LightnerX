cmake_minimum_required(VERSION 3.28)

project(LightnerX LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/GameNetworkingSockets)

add_executable(LightnerX
    main.cpp
)

add_subdirectory(communication)
add_subdirectory(network)

target_include_directories(LightnerX PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(LightnerX PRIVATE GameNetworkingSockets)
