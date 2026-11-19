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
find_program(LIGHTNERX_BASH bash REQUIRED)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$" OR MSVC)
    message(FATAL_ERROR "LightnerX regression tests require a GCC- or Clang-compatible compiler driver")
endif()
add_executable(lightnerx_test_characterization characterization.cpp)
target_link_libraries(lightnerx_test_characterization PRIVATE LightnerX)
add_test(NAME lightnerx.characterization
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/characterization.py"
        "$<TARGET_FILE:lightnerx_test_characterization>"
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cpp26-behavior.txt")
set_tests_properties(lightnerx.characterization PROPERTIES TIMEOUT 60)

set(domain_test_flags "${CMAKE_CXX_FLAGS}")
if(LIGHTNERX_SANITIZERS)
    # Fault injection uses the same implementation with deterministic transport stubs.
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
        --compiler "${CMAKE_CXX_COMPILER}" "--flags=${CMAKE_CXX_FLAGS}" ${message_fault_options}
)
set_tests_properties(lightnerx.message_faults PROPERTIES TIMEOUT 180)

add_test(NAME lightnerx.transport_faults
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/transport_faults.py"
        --compiler "${CMAKE_CXX_COMPILER}" "--flags=${CMAKE_CXX_FLAGS}" ${message_fault_options})
set_tests_properties(lightnerx.transport_faults PROPERTIES TIMEOUT 180)

file(GLOB lifecycle_fault_headers CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/lightnerx/network/*.hpp")
set(lifecycle_fault_source "${CMAKE_CURRENT_BINARY_DIR}/lifecycle_faults.cpp")
add_custom_command(OUTPUT "${lifecycle_fault_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle_faults.py"
        --output "${lifecycle_fault_source}"
    DEPENDS lifecycle_faults.py source_support.py ${lifecycle_fault_headers}
        "${PROJECT_SOURCE_DIR}/network/client.cpp"
        "${PROJECT_SOURCE_DIR}/network/server.cpp"
        "${PROJECT_SOURCE_DIR}/network/network_manager.cpp"
    VERBATIM)
add_executable(lightnerx_test_lifecycle_faults "${lifecycle_fault_source}")
# The source defines the wrappers itself. On shared builds it must not import them.
target_link_libraries(lightnerx_test_lifecycle_faults PRIVATE ${lightnerx_gns_target})
target_include_directories(lightnerx_test_lifecycle_faults PRIVATE
    "${PROJECT_SOURCE_DIR}/external/span/include"
    "${PROJECT_SOURCE_DIR}/external/expected/include")
target_compile_definitions(lightnerx_test_lifecycle_faults PRIVATE TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION)
if(LIGHTNERX_SANITIZERS)
    target_compile_options(lightnerx_test_lifecycle_faults PRIVATE
        "-fsanitize=${LIGHTNERX_SANITIZERS}" -fno-omit-frame-pointer -fno-sanitize-recover=all)
    if(LIGHTNERX_SANITIZERS MATCHES "undefined")
        target_compile_options(lightnerx_test_lifecycle_faults PRIVATE -fno-sanitize=vptr)
    endif()
    target_link_options(lightnerx_test_lifecycle_faults PRIVATE "-fsanitize=${LIGHTNERX_SANITIZERS}")
endif()
add_test(NAME lightnerx.lifecycle_faults COMMAND lightnerx_test_lifecycle_faults)
set_tests_properties(lightnerx.lifecycle_faults PROPERTIES TIMEOUT 60)

if(LIGHTNERX_BUILD_SHARED AND UNIX)
    foreach(plugin IN ITEMS first second)
        add_library(lightnerx_test_plugin_${plugin} MODULE runtime_plugin.cpp)
        target_link_libraries(lightnerx_test_plugin_${plugin} PRIVATE LightnerX)
    endforeach()
    add_executable(lightnerx_test_shared_runtime shared_runtime.cpp)
    # Link so the process has the sanitizer runtime before loading instrumented modules.
    target_link_libraries(lightnerx_test_shared_runtime PRIVATE LightnerX ${CMAKE_DL_LIBS})
    add_test(NAME lightnerx.shared_runtime COMMAND lightnerx_test_shared_runtime
        "$<TARGET_FILE:lightnerx_test_plugin_first>" "$<TARGET_FILE:lightnerx_test_plugin_second>")
    set_tests_properties(lightnerx.shared_runtime PROPERTIES TIMEOUT 60)
endif()

add_test(NAME lightnerx.integration
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/integration.py"
        "$<TARGET_FILE:lightnerx_test_lifecycle>"
)
set_tests_properties(lightnerx.integration PROPERTIES TIMEOUT 60)

set(LIGHTNERX_REFERENCE_BINARY "" CACHE FILEPATH "Original C++26 lifecycle executable for cross-version UDP tests")
if(LIGHTNERX_REFERENCE_BINARY)
    if(NOT EXISTS "${LIGHTNERX_REFERENCE_BINARY}")
        message(FATAL_ERROR "LIGHTNERX_REFERENCE_BINARY does not exist")
    endif()
    foreach(role IN ITEMS server client)
        add_test(NAME lightnerx.reference_${role}
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/integration.py"
                "$<TARGET_FILE:lightnerx_test_lifecycle>"
                --${role}-binary "${LIGHTNERX_REFERENCE_BINARY}")
        set_tests_properties(lightnerx.reference_${role} PROPERTIES TIMEOUT 60)
    endforeach()
endif()

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
            "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
            "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
            "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
            "-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}"
            "-DBUILD_SHARED_LIB=${BUILD_SHARED_LIB}"
            "-DBUILD_STATIC_LIB=${BUILD_STATIC_LIB}"
            "-DLIGHTNERX_BUILD_SHARED=${LIGHTNERX_BUILD_SHARED}"
            "-DLIGHTNERX_SANITIZERS=${LIGHTNERX_SANITIZERS}"
        --test-command lightnerx_consumer${CMAKE_EXECUTABLE_SUFFIX}
)
set_tests_properties(lightnerx.consumer PROPERTIES
    TIMEOUT 600
    ENVIRONMENT "CMAKE_BUILD_PARALLEL_LEVEL=2"
)

# Compile complete examples with their own includes and check API excerpts.
file(GLOB documentation_pages CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/docs/*.md")
list(APPEND documentation_pages "${PROJECT_SOURCE_DIR}/README.md")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${documentation_pages} "${CMAKE_CURRENT_SOURCE_DIR}/documentation.py")
execute_process(COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/documentation.py"
    --output "${CMAKE_CURRENT_BINARY_DIR}/documentation" --bash "${LIGHTNERX_BASH}"
    RESULT_VARIABLE documentation_status OUTPUT_VARIABLE documentation_sources
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT documentation_status EQUAL 0)
    message(FATAL_ERROR "Documentation validation failed")
endif()
add_executable(lightnerx_test_documentation ${documentation_sources})
target_link_libraries(lightnerx_test_documentation PRIVATE LightnerX)
if(MSVC)
    target_compile_options(lightnerx_test_documentation PRIVATE /UNDEBUG)
else()
    target_compile_options(lightnerx_test_documentation PRIVATE -UNDEBUG -pedantic-errors)
endif()
add_test(NAME lightnerx.documentation COMMAND lightnerx_test_documentation)

# Each public header must compile as the first and only project include.
file(GLOB_RECURSE lightnerx_public_headers CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/lightnerx/*.hpp")
set(header_checks)
foreach(header IN LISTS lightnerx_public_headers)
    file(RELATIVE_PATH include_name "${PROJECT_SOURCE_DIR}/include" "${header}")
    string(MAKE_C_IDENTIFIER "${include_name}" check_name)
    set(check_source "${CMAKE_CURRENT_BINARY_DIR}/headers/${check_name}.cpp")
    file(CONFIGURE OUTPUT "${check_source}"
        CONTENT "#include <${include_name}>\n#include <${include_name}>\n" @ONLY)
    list(APPEND header_checks "${check_source}")
endforeach()
add_library(lightnerx_header_checks OBJECT ${header_checks})
target_link_libraries(lightnerx_header_checks PRIVATE LightnerX)
