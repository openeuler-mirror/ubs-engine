set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)

# IT test infrastructure library
set(IT_INFRA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/infra)

add_library(ubse_it_infra STATIC
    ${IT_INFRA_DIR}/cluster_manager.cpp
    ${IT_INFRA_DIR}/node_process_manager.cpp
    ${IT_INFRA_DIR}/it_cluster.cpp
    ${IT_INFRA_DIR}/it_sdk_client.cpp
    ${IT_INFRA_DIR}/it_cli_invoker.cpp
    ${IT_INFRA_DIR}/it_config_builder.cpp
    ${IT_INFRA_DIR}/it_wait_helper.cpp
    ${IT_INFRA_DIR}/mock_lcne_server.cpp
)

target_include_directories(ubse_it_infra PUBLIC
    ${IT_INFRA_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/sdk/c/include
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/framework/ipc/include
    ${PROJECT_SOURCE_DIR}/src/framework/ha
    ${PROJECT_SOURCE_DIR}/src/controllers/include
    ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mmi
    ${PROJECT_SOURCE_DIR}/src/adapter_plugins/syssentry
    ${PROJECT_SOURCE_DIR}/src/adapter_plugins/urma_uvs
)

target_link_libraries(ubse_it_infra PUBLIC
    GTest::gmock_main
    ubse_ipc_client
    ubse_sdk_static
    # Same dependency chain as the ubse main binary (src/CMakeLists.txt)
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

# Smoke test subdirectory
add_subdirectory(smoke)

# =====================================================
# IT daemon binary with mock SMBIOS
# Same source files as ubse, but links ubse_smbios_mock via --whole-archive
# to override real ubse_smbios symbols. --allow-multiple-definition
# lets the linker accept duplicate symbols and use the first occurrence (mock).
# =====================================================

add_executable(ubse_it_daemon EXCLUDE_FROM_ALL
    ${SRC_DIR}/ubse_main.cpp
        ${SRC_DIR}/framework/security/ubse_security_module.cpp
        ${SRC_DIR}/framework/com/ubse_com_module.cpp
        ${SRC_DIR}/framework/storage/ubse_storage.cpp
        ${SRC_DIR}/framework/event/ubse_event_module.cpp
        ${SRC_DIR}/framework/log/ubse_logger_module.cpp
        ${SRC_DIR}/framework/plugin_mgr/ubse_plugin_module.cpp
        ${SRC_DIR}/framework/http/ubse_http_module.cpp
        ${SRC_DIR}/controllers/node/ubse_node_controller_module.cpp
        ${SRC_DIR}/controllers/mem/mem_controller/ubse_mem_controller_module.cpp
        ${SRC_DIR}/controllers/mem/mem_controller/ubse_mem_controller.cpp
        ${SRC_DIR}/controllers/urma/ubse_urma_controller_module.cpp
        ${SRC_DIR}/controllers/urma/ubse_urma_controller.cpp
        ${SRC_DIR}/controllers/npu/ubse_npu_controller_module.cpp
        ${SRC_DIR}/framework/ha/ubse_election_module.cpp
        ${SRC_DIR}/adapter_plugins/syssentry/sys_sentry_module.cpp
        ${SRC_DIR}/ras/ubse_ras_module.cpp
        ${SRC_DIR}/api_server/ubse_api_server_module.cpp
        ${SRC_DIR}/adapter_plugins/urma_uvs/ubse_urma_uvs_module.cpp
        ${SRC_DIR}/framework/security/ubse_security.cpp
        ${SRC_DIR}/framework/log/ubse_logger.cpp
        ${SRC_DIR}/framework/com/ubse_com.cpp
        ${SRC_DIR}/framework/storage/ubse_storage.cpp
        ${SRC_DIR}/framework/event/ubse_event.cpp
        ${SRC_DIR}/framework/plugin_mgr/ubse_plugin.cpp
        ${SRC_DIR}/framework/trace_context/trace_context.cpp
        ${SRC_DIR}/api_server/ubse_api_server.cpp
)

# mock SMBIOS must appear first in link order so its symbols override real ones
target_link_libraries(ubse_it_daemon PRIVATE
        -Wl,--whole-archive
        ubse_smbios_mock
        ubse_mti_mock
        ubse_node_controller_it_mock
        ubse_com_engine_it_mock
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

# Election test subdirectory
add_subdirectory(election)

# NUMA borrow test subdirectory
add_subdirectory(numa_borrow)

get_property(IT_TEST_TARGETS GLOBAL PROPERTY UBSE_IT_TARGETS)
add_custom_target(it
    COMMENT "Run IT test targets: ${IT_TEST_TARGETS}"
)

if(IT_TEST_TARGETS)
    add_dependencies(it ${IT_TEST_TARGETS})
else()
    message(WARNING "No IT test targets registered.")
endif()
