add_ut(mem_scheduler)
target_link_libraries(ubs-engine_mem_scheduler_ut
        PUBLIC
        mem_scheduler ubse_node_controller ubse_controller ubse_context ubse_config ubse_utils ubse_smbios syssentry
)