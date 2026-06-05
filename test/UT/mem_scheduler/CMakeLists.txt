add_ut(mem_scheduler)
target_link_libraries(ubs-engine_mem_scheduler_ut
        PUBLIC
        mem_plugin ubse_node_controller ubse_controller ubse_context ubse_timer ubse_config ubse_http ubse_event ubse_com ubse_election
)