cmake_minimum_required(VERSION 3.28)

project(LightnerX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(LightnerX
    main.cpp
)

add_subdirectory(communication)
add_subdirectory(network)

target_include_directories(LightnerX PRIVATE ${CMAKE_SOURCE_DIR})

if(WIN32)
    target_link_libraries(LightnerX PRIVATE ws2_32 winmm)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(LightnerX PRIVATE Threads::Threads)
endif()
