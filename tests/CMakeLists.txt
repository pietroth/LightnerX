foreach(test_name IN ITEMS connection_manager connection_manager_assignment messages lifecycle)
    add_executable(lightnerx_test_${test_name} ${test_name}.cpp)
    target_link_libraries(lightnerx_test_${test_name} PRIVATE LightnerX)
    # Keep assertions enabled even in Release/RelWithDebInfo configurations.
    if(MSVC)
        target_compile_options(lightnerx_test_${test_name} PRIVATE /UNDEBUG)
    else()
        target_compile_options(lightnerx_test_${test_name} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME lightnerx.${test_name} COMMAND lightnerx_test_${test_name})
    set_tests_properties(lightnerx.${test_name} PROPERTIES TIMEOUT 60)
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(domain_test_flags "${CMAKE_CXX_FLAGS}")
if(LIGHTNERX_SANITIZERS)
    # These supplemental tests compile implementation bodies without modules;
    # they can run both sanitizers even on GCC versions with a UBSan/modules ICE.
    string(APPEND domain_test_flags " -fsanitize=address,undefined -fno-omit-frame-pointer")
endif()
add_test(NAME lightnerx.connection_manager_domain
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/connection_manager_domain.py"
        --compiler "${CMAKE_CXX_COMPILER}" "--flags=${domain_test_flags}"
)
set_tests_properties(lightnerx.connection_manager_domain PROPERTIES TIMEOUT 180)

set(message_fault_options)
if(LIGHTNERX_SANITIZERS OR CMAKE_CXX_FLAGS MATCHES "fsanitize=")
    list(APPEND message_fault_options --sanitize)
endif()
add_test(NAME lightnerx.message_faults
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/message_faults.py"
        --compiler "${CMAKE_CXX_COMPILER}" ${message_fault_options}
)
set_tests_properties(lightnerx.message_faults PROPERTIES TIMEOUT 180)

add_test(NAME lightnerx.lifecycle_faults
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle_faults.py"
        --compiler "${CMAKE_CXX_COMPILER}" --library "$<TARGET_FILE:GameNetworkingSockets>"
        ${message_fault_options}
)
set_tests_properties(lightnerx.lifecycle_faults PROPERTIES TIMEOUT 180)

add_test(NAME lightnerx.integration
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/integration.py"
        "$<TARGET_FILE:lightnerx_test_lifecycle>"
)
set_tests_properties(lightnerx.integration PROPERTIES TIMEOUT 60)

# Configure a separate parent project: no inherited target/directory requirements.
add_test(NAME lightnerx.consumer
    COMMAND ${CMAKE_CTEST_COMMAND}
        --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/consumer" "${CMAKE_CURRENT_BINARY_DIR}/consumer"
        --build-generator "${CMAKE_GENERATOR}"
        --build-config "$<CONFIG>"
        --build-target lightnerx_consumer
        --build-options
            "-DLIGHTNERX_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
            "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
            "-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}"
            "-DLIGHTNERX_SANITIZERS=${LIGHTNERX_SANITIZERS}"
        --test-command lightnerx_consumer${CMAKE_EXECUTABLE_SUFFIX}
)
set_tests_properties(lightnerx.consumer PROPERTIES
    TIMEOUT 600
    ENVIRONMENT "CMAKE_BUILD_PARALLEL_LEVEL=2"
)
