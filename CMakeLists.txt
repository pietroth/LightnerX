cmake_minimum_required(VERSION 3.22)

project(LightnerX LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BUILD_TESTS OFF CACHE BOOL "Build GameNetworkingSockets tests")
set(BUILD_EXAMPLES OFF CACHE BOOL "Build GameNetworkingSockets examples")

add_subdirectory(external/GameNetworkingSockets)

add_library(LightnerX STATIC)
target_compile_features(LightnerX PUBLIC cxx_std_17)
target_sources(LightnerX PRIVATE include/lightnerx/compact/expected.hpp)

set(LIGHTNERX_SANITIZERS "" CACHE STRING "Sanitizers for LightnerX and consumers, e.g. address or address,undefined")
if(LIGHTNERX_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "LIGHTNERX_SANITIZERS requires GCC or Clang")
    endif()
    # Instrument the library and consumers without changing third-party sources.
    target_compile_options(LightnerX PUBLIC "-fsanitize=${LIGHTNERX_SANITIZERS}" -fno-omit-frame-pointer -fno-sanitize-recover=all)
    # GameNetworkingSockets disables RTTI; its virtual interfaces cannot use vptr checks.
    if(LIGHTNERX_SANITIZERS MATCHES "undefined")
        target_compile_options(LightnerX PUBLIC -fno-sanitize=vptr)
    endif()
    target_link_options(LightnerX PUBLIC "-fsanitize=${LIGHTNERX_SANITIZERS}")
endif()

add_subdirectory(communication)
add_subdirectory(network)

target_include_directories(LightnerX
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(LightnerX SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/span/include
)

target_link_libraries(LightnerX
    PUBLIC
        GameNetworkingSockets
)

option(LIGHTNERX_BUILD_TESTS "Build LightnerX regression tests" OFF)
if(LIGHTNERX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
