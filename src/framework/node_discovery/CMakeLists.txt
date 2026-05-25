add_library(ubse_node_discovery STATIC
        root_mode/message/ubse_node_discovery_serial.cpp
        root_mode/message/ubse_node_discovery_list_serial.cpp
        node_mode/ubse_node_discovery_config_mode.cpp
        ubse_node_discovery.cpp
)

target_include_directories(ubse_node_discovery PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ubse_node_discovery PRIVATE
        ubse_com
        ubse_utils
        ubse_election
        ubse_context
        ubse_serial
        ubse_http
        ubse_config
        ubse_security
        ubse_smbios)