cmake_minimum_required(VERSION 4.2)
# CMake 4.2's import-std gate must precede the parent's project() call.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(LightnerXConsumer LANGUAGES C CXX)

if(NOT LIGHTNERX_SOURCE_DIR)
    message(FATAL_ERROR "Pass -DLIGHTNERX_SOURCE_DIR=/path/to/LightnerX")
endif()
set(LIGHTNERX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${LIGHTNERX_SOURCE_DIR}" lightnerx)
add_executable(lightnerx_consumer main.cpp)
# C++26, Steam headers and linking must all propagate through this target.
target_link_libraries(lightnerx_consumer PRIVATE LightnerX)
