add_ut(ubse_sdk)

target_include_directories(ubs-engine_ubse_sdk_ut PRIVATE
        ${SRC_DIR}/sdk/c
)

target_link_libraries(ubs-engine_ubse_sdk_ut
        PRIVATE
        ubse_node_controller
        ubse_ipc_client
        mem_plugin
        npu_plugin
        ubse_ssu_sdk
        ubse_npu_sdk
)