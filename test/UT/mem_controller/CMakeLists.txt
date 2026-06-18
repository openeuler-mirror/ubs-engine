add_ut(mem_plugin)

target_link_libraries(${CMAKE_PROJECT_NAME}_mem_plugin_ut
        PRIVATE
        mem_plugin
        mem_controller_adapter ubse_controller_rpc  ubse_controller_api
        ubse_log
        ubse_com
        ubse_election
        ubse_context
        ubse_serial
        ubse_node_controller
        ubse_http
        ubse_controller
        ubse_event
        ubse_config
        api_server
        ubse_mmi
        ubse_mem_decoder_utils
        syssentry
        ubse_vsock_client
        ubse_security
)
target_include_directories(${CMAKE_PROJECT_NAME}_mem_plugin_ut PRIVATE
        ${PROJECT_SOURCE_DIR}/src/controllers/include
        ${PROJECT_SOURCE_DIR}/src/include)