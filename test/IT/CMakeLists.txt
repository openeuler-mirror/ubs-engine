if(NOT TARGET ubse)
    message(FATAL_ERROR "IT framework requires the production ubse target")
endif()

get_target_property(UBSE_DAEMON_SOURCE_DIR ubse SOURCE_DIR)
get_target_property(UBSE_DAEMON_TARGET_SOURCES ubse SOURCES)
if(NOT UBSE_DAEMON_TARGET_SOURCES)
    message(FATAL_ERROR "Unable to derive IT daemon sources from the ubse target")
endif()
set(IT_DAEMON_SOURCES)
foreach(source IN LISTS UBSE_DAEMON_TARGET_SOURCES)
    if(IS_ABSOLUTE "${source}")
        list(APPEND IT_DAEMON_SOURCES "${source}")
    else()
        list(APPEND IT_DAEMON_SOURCES "${UBSE_DAEMON_SOURCE_DIR}/${source}")
    endif()
endforeach()

# IT test infrastructure library
set(IT_INFRA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/infra)

add_library(ubse_it_infra STATIC
    ${IT_INFRA_DIR}/node_process_manager.cpp
    ${IT_INFRA_DIR}/it_cluster.cpp
    ${IT_INFRA_DIR}/it_cluster_spec.cpp
    ${IT_INFRA_DIR}/it_cluster_builder.cpp
    ${IT_INFRA_DIR}/it_sdk_client.cpp
    ${IT_INFRA_DIR}/it_cli_invoker.cpp
    ${IT_INFRA_DIR}/it_config_builder.cpp
    ${IT_INFRA_DIR}/it_wait_helper.cpp
    ${IT_INFRA_DIR}/mock_lcne_server.cpp
)

add_executable(ubse_it_node_launcher EXCLUDE_FROM_ALL
    ${IT_INFRA_DIR}/node_launcher.cpp
)
target_compile_options(ubse_it_node_launcher PRIVATE ${DEBUG_FLAGS})

target_include_directories(ubse_it_infra PUBLIC
    ${IT_INFRA_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/framework/ha
)

target_link_libraries(ubse_it_infra PUBLIC
    ubse_sdk_static
    cpp-httplib
)

target_compile_options(ubse_it_infra PRIVATE ${DEBUG_FLAGS})

# Generate IT_DIRECTORY macro
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/ubse_it_dir.h.in"
    "${CMAKE_BINARY_DIR}/include/ubse_it_dir.h"
    @ONLY
)

# Build stub libraries
add_subdirectory(stubs)

# =====================================================
# IT daemon binary with mock SMBIOS
# Same source files as ubse, but links ubse_smbios_mock via --whole-archive
# to override real ubse_smbios symbols. --allow-multiple-definition
# lets the linker accept duplicate symbols and use the first occurrence (mock).
# =====================================================

add_executable(ubse_it_daemon EXCLUDE_FROM_ALL
    ${IT_DAEMON_SOURCES}
)

# mock SMBIOS must appear first in link order so its symbols override real ones
target_link_libraries(ubse_it_daemon PRIVATE
        -Wl,--whole-archive
        ubse_smbios_mock
        ubse_mti_mock
        ubse_node_controller_it_mock
        ubse_com_engine_it_mock
        ubse_capability_it_stub
        -Wl,--no-whole-archive
        ubse_security
        ubse_context
        ubse_config
        ubse_com
        ubse_event
        ubse_storage
        ubse_log
        ubse_plugin
        ubse_http
        ubse_election
        ubse_mti
        ubse_node_controller
        ubse_ras
        syssentry
        cert_api_static
        api_server
        rapidjson
        trace_context
        ubse_urma_uvs
        ubse_urma_controller
        npu_controller
        ubse_controller
        -Wl,--whole-archive
        ubse_node_controller_register_config
        -Wl,--no-whole-archive
)

# Duplicate symbols are expected: mock vs real ubse_smbios. Linker uses first occurrence.
target_link_options(ubse_it_daemon PRIVATE
        -Wl,--allow-multiple-definition
        -rdynamic
        -pie
)

target_compile_options(ubse_it_daemon PRIVATE ${DEBUG_FLAGS})
target_include_directories(ubse_it_daemon PRIVATE ${CMAKE_BINARY_DIR}/rpc)
add_dependencies(ubse_it_daemon ubse_it_node_launcher)

add_custom_target(ubse_it_runtime)
add_dependencies(ubse_it_runtime
    ubsectl
    ubse_it_daemon
    ubse_it_node_launcher
    ubse_interface_preload
    obmm_stub
    urma_uvs_stub
    xalarm_stub
)

set(UBSE_IT_MODULES
    smoke
    election
    numa_borrow
)

set(UBSE_IT_NON_MODULE_DIRS
    config_builder
    infra
    stubs
)

file(GLOB UBSE_IT_CHILDREN LIST_DIRECTORIES true RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(child IN LISTS UBSE_IT_CHILDREN)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt")
        list(FIND UBSE_IT_MODULES "${child}" registered_module_index)
        list(FIND UBSE_IT_NON_MODULE_DIRS "${child}" non_module_index)
        if(registered_module_index EQUAL -1 AND non_module_index EQUAL -1)
            message(FATAL_ERROR "IT module directory '${child}' has CMakeLists.txt but is not listed in UBSE_IT_MODULES")
        endif()
    endif()
endforeach()

foreach(module IN LISTS UBSE_IT_MODULES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module}/CMakeLists.txt")
        message(FATAL_ERROR "Registered IT module '${module}' does not have CMakeLists.txt")
    endif()
    add_subdirectory(${module})
endforeach()

get_property(IT_TEST_TARGETS GLOBAL PROPERTY UBSE_IT_TARGETS)
get_property(IT_BINARIES GLOBAL PROPERTY UBSE_IT_BINARIES)

# Aggregate target `it`: builds all IT binaries then runs all IT tests via ctest.
# Individual <module>_it targets remain available for single-module runs.
if(SKIP_RUN_TESTS)
    set(IT_RUN_ALL_COMMAND ${CMAKE_COMMAND} -E echo "Skip run IT tests, only build registered IT binaries")
else()
    set(IT_RUN_ALL_COMMAND
        ${CMAKE_CTEST_COMMAND}
        --test-dir ${CMAKE_BINARY_DIR}/test
        -L "^it_.*"
        --output-on-failure
        --timeout ${UBSE_IT_TEST_TIMEOUT_SECONDS}
        --no-tests=error
    )
endif()
add_custom_target(it
    COMMAND ${IT_RUN_ALL_COMMAND}
    COMMENT "Run all IT tests via ctest (labels matching 'it_.*')"
)

if(IT_BINARIES)
    add_dependencies(it ${IT_BINARIES})
else()
    message(WARNING "No IT test binaries registered.")
endif()
