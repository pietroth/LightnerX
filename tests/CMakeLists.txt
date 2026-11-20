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
add_executable(lightnerx_test_characterization characterization.cpp)
target_link_libraries(lightnerx_test_characterization PRIVATE LightnerX)
add_test(NAME lightnerx.characterization
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/characterization.py"
        "$<TARGET_FILE:lightnerx_test_characterization>"
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cpp26-behavior.txt")
set_tests_properties(lightnerx.characterization PROPERTIES TIMEOUT 60)

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

add_test(NAME lightnerx.transport_faults
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/transport_faults.py"
        --compiler "${CMAKE_CXX_COMPILER}" "--flags=${CMAKE_CXX_FLAGS}" ${message_fault_options}
)
set_tests_properties(lightnerx.transport_faults PROPERTIES TIMEOUT 180)

set(lifecycle_fault_source "${CMAKE_CURRENT_BINARY_DIR}/lifecycle_faults.cpp")
add_custom_command(OUTPUT "${lifecycle_fault_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle_faults.py"
        --output "${lifecycle_fault_source}"
    DEPENDS lifecycle_faults.py module_source.py
        "${PROJECT_SOURCE_DIR}/network/client.cppm"
        "${PROJECT_SOURCE_DIR}/network/server.cppm"
        "${PROJECT_SOURCE_DIR}/network/network_manager.cppm"
    VERBATIM)
add_executable(lightnerx_test_lifecycle_faults "${lifecycle_fault_source}")
# The generated fault test uses ordinary headers and calls the selected GNS library directly.
set_target_properties(lightnerx_test_lifecycle_faults PROPERTIES CXX_MODULE_STD OFF CXX_SCAN_FOR_MODULES OFF)
target_link_libraries(lightnerx_test_lifecycle_faults PRIVATE ${lightnerx_gns_target})
if(WIN32)
    target_link_libraries(lightnerx_test_lifecycle_faults PRIVATE ws2_32)
endif()
if(message_fault_options)
    target_compile_options(lightnerx_test_lifecycle_faults PRIVATE -fsanitize=address,undefined
        -fno-sanitize=vptr -fno-omit-frame-pointer -fno-sanitize-recover=all)
    target_link_options(lightnerx_test_lifecycle_faults PRIVATE -fsanitize=address,undefined)
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

set(LIGHTNERX_REFERENCE_BINARY "" CACHE FILEPATH "Compatible LightnerX lifecycle executable for cross-version UDP tests")
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
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
            "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
            "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
            "-DBUILD_SHARED_LIB=${BUILD_SHARED_LIB}"
            "-DBUILD_STATIC_LIB=${BUILD_STATIC_LIB}"
            "-DLIGHTNERX_BUILD_SHARED=${LIGHTNERX_BUILD_SHARED}"
            "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
            "-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}"
            "-DLIGHTNERX_SANITIZERS=${LIGHTNERX_SANITIZERS}"
        --test-command lightnerx_consumer${CMAKE_EXECUTABLE_SUFFIX}
)
set_tests_properties(lightnerx.consumer PROPERTIES
    TIMEOUT 600
    ENVIRONMENT "CMAKE_BUILD_PARALLEL_LEVEL=2"
)

set(example_pages
    "${PROJECT_SOURCE_DIR}/README.md"
    "${PROJECT_SOURCE_DIR}/docs/first-steps.md"
    "${PROJECT_SOURCE_DIR}/docs/communication.md"
    "${PROJECT_SOURCE_DIR}/docs/setup.md")
set(example_script "${CMAKE_CURRENT_SOURCE_DIR}/documentation.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${example_pages} "${example_script}")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${example_script}"
    "${CMAKE_CURRENT_BINARY_DIR}/examples" ${example_pages}
    COMMAND_ERROR_IS_FATAL ANY OUTPUT_VARIABLE examples OUTPUT_STRIP_TRAILING_WHITESPACE)
foreach(source IN LISTS examples)
    get_filename_component(name "${source}" NAME_WE)
    add_executable(lightnerx_doc_${name} "${source}")
    target_link_libraries(lightnerx_doc_${name} PRIVATE LightnerX)
endforeach()
add_test(NAME lightnerx.documentation_setup COMMAND lightnerx_doc_setup_1)
