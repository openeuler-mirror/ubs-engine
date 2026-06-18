add_ut(ubse_sdk)

target_link_libraries(ubs-engine_ubse_sdk_ut
        PRIVATE
        ubse_node_controller
        ubse_ipc_client
        mem_plugin
)