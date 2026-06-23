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

add_subdirectory(smoke)
add_subdirectory(election)
add_subdirectory(numa_borrow)

get_property(IT_TEST_TARGETS GLOBAL PROPERTY UBSE_IT_TARGETS)
get_property(IT_BINARIES GLOBAL PROPERTY UBSE_IT_BINARIES)

# Aggregate target `it`: builds all IT binaries then runs all IT tests via ctest.
# Individual <module>_it targets remain available for single-module runs.
add_custom_target(it
    COMMAND ${CMAKE_CTEST_COMMAND} -L "it_.*" --output-on-failure
    COMMENT "Run all IT tests via ctest (labels matching 'it_.*')"
)

if(IT_BINARIES)
    add_dependencies(it ${IT_BINARIES})
else()
    message(WARNING "No IT test binaries registered.")
endif()
