add_library(ubse_node_controller STATIC
        ubse_node_controller.cpp
        ubse_node_controller_module.cpp
        ubse_node_controller_collector.cpp
        ubse_node_controller_util.cpp
        ubse_node_api.cpp
        ubse_node_controller_query_api.cpp
        ubse_node_api_convert.cpp
        ubse_node_controller_master.cpp
        ubse_node_controller_agent.cpp
        ubse_node_info_api.cpp
        ubse_node_com_urma_collector.cpp
)

target_include_directories(ubse_node_controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SRC_DIR}/adapter_plugins/mti
        ${SRC_DIR}/adapter_plugins/syssentry
        ${SRC_DIR}/sdk/c/include
        ${SRC_DIR}/adapter_plugins/urma_uvs
        ${SRC_DIR}/adapter_plugins/smbios)
target_link_libraries(ubse_node_controller PRIVATE
        ubse_com
        ubse_mti
        ubse_utils
        ubse_election
        ubse_context
        ubse_timer
        ubse_serial
        ubse_ras
        api_server
        ubse_http
        ubse_config
        mem_controller
        ubse_urma_uvs
        ubse_security
        ubse_smbios)



add_library(ubse_node_controller_register_config OBJECT ubse_node_controller_register_config.cpp)
target_link_libraries(ubse_node_controller_register_config PRIVATE ubse_config)