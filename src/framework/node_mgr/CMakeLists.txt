add_library(ubse_node_mgr STATIC
        root_mode/message/ubse_node_discovery_serial.cpp
        root_mode/message/ubse_node_discovery_list_serial.cpp
        conf_mode/ubse_node_discovery_config_mode.cpp
        static_mode/ubse_node_discovery_static_mode.cpp
        ubse_node_static_info_mgr.cpp
        ubse_node_mgr_module.cpp
        ubse_node_mgr.cpp
)

target_include_directories(ubse_node_mgr PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SRC_DIR}/adapter_plugins/urma_uvs)

target_link_libraries(ubse_node_mgr PRIVATE
        ubse_com
        ubse_utils
        ubse_context
        ubse_serial
        ubse_http
        ubse_config
        ubse_security
        ubse_smbios
        ubse_mti
        ubse_urma_uvs)
