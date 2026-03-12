add_ut(ubse_election)

target_link_libraries(ubs-engine_ubse_election_ut PRIVATE
        ubse_com
        ubse_context
        ubse_node_controller
        ubse_event
        ubse_log
        ubse_config
        ubse_http
        ubse_plugin
)