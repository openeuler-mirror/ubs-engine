add_ut(ubse_ras)

target_link_libraries(ubs-engine_ubse_ras_ut
        PRIVATE
        ubse_ras
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
        syssentry
)