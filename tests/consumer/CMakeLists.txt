cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_EXTENSIONS OFF)
project(LightnerXConsumer LANGUAGES C CXX)

if(NOT LIGHTNERX_SOURCE_DIR)
    message(FATAL_ERROR "Pass -DLIGHTNERX_SOURCE_DIR=/path/to/LightnerX")
endif()
set(LIGHTNERX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${LIGHTNERX_SOURCE_DIR}" lightnerx)
add_executable(lightnerx_consumer main.cpp producer.cpp)
# C++17, Steam headers and linking must all propagate through this target.
target_link_libraries(lightnerx_consumer PRIVATE LightnerX)
# Exercise different NDEBUG settings between consumer translation units.
target_compile_definitions(lightnerx_consumer PRIVATE NDEBUG)
if(MSVC)
    set_source_files_properties(producer.cpp PROPERTIES COMPILE_OPTIONS /UNDEBUG)
else()
    set_source_files_properties(producer.cpp PROPERTIES COMPILE_OPTIONS -UNDEBUG)
endif()
