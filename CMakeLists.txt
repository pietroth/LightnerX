cmake_minimum_required(VERSION 4.2)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_CXX_MODULE_STD ON)

project(LightnerX LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BUILD_TESTS OFF CACHE BOOL "Build GameNetworkingSockets tests")
set(BUILD_EXAMPLES OFF CACHE BOOL "Build GameNetworkingSockets examples")

add_subdirectory(external/GameNetworkingSockets)

add_library(LightnerX STATIC)
target_compile_features(LightnerX PUBLIC cxx_std_26)
set_target_properties(LightnerX PROPERTIES CXX_MODULE_STD ON)

set(LIGHTNERX_SANITIZERS "" CACHE STRING "Sanitizers for LightnerX and consumers, e.g. address or address,undefined")
if(LIGHTNERX_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "LIGHTNERX_SANITIZERS requires GCC or Clang")
    endif()
    # Instrument this library and its consumers without changing the compiler's
    # std module or the compilation settings of third-party dependencies.
    target_compile_options(LightnerX PUBLIC "-fsanitize=${LIGHTNERX_SANITIZERS}" -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(LightnerX PUBLIC "-fsanitize=${LIGHTNERX_SANITIZERS}")
endif()

add_subdirectory(communication)
add_subdirectory(network)

target_include_directories(LightnerX
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
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
