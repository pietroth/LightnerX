option(LIGHTNERX_BUILD_NATIVE_REFERENCE_TESTS "Compare compact expected with native C++23 expected" OFF)

function(lightnerx_compact_test name standard native)
    set(target lightnerx_test_compact_${name})
    if(native)
        set(target lightnerx_test_native_${name})
    endif()
    add_executable(${target} ${name}.cpp)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD ${standard} CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
        # CMake 4.5 may otherwise synthesize standard modules for native oracles.
        CXX_MODULE_STD OFF CXX_SCAN_FOR_MODULES OFF)
    if(native)
        target_compile_definitions(${target} PRIVATE LIGHTNERX_NATIVE_REFERENCE)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG -Wall -Wextra -pedantic-errors)
    endif()
    if(LIGHTNERX_SANITIZERS)
        target_compile_options(${target} PRIVATE "-fsanitize=${LIGHTNERX_SANITIZERS}" -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_link_options(${target} PRIVATE "-fsanitize=${LIGHTNERX_SANITIZERS}")
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endfunction()

lightnerx_compact_test(expected 17 FALSE)
add_test(NAME lightnerx.compact_compile_rejections COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/negative.py" --compiler "${CMAKE_CXX_COMPILER}"
    "--flags=${CMAKE_CXX_FLAGS}" --include "${PROJECT_SOURCE_DIR}/include")
if(LIGHTNERX_BUILD_NATIVE_REFERENCE_TESTS)
    lightnerx_compact_test(expected 23 TRUE)
    add_test(NAME lightnerx.native_compile_rejections COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/negative.py" --compiler "${CMAKE_CXX_COMPILER}"
        "--flags=${CMAKE_CXX_FLAGS}" --include "${PROJECT_SOURCE_DIR}/include" --native)
    add_test(NAME lightnerx.compact_expected_parity COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/compare.py"
        "$<TARGET_FILE:lightnerx_test_compact_expected>" "$<TARGET_FILE:lightnerx_test_native_expected>")
endif()
