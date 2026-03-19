add_ut(ubse_utils)
target_link_libraries(ubs-engine_ubse_utils_ut
        PRIVATE ubse_utils ubse_log ubse_xml boundscheck ubse_node_controller
)