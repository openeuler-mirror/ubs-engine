add_ut(ubse_urma_controller)
target_link_libraries(ubs-engine_ubse_urma_controller_ut
        PRIVATE
        ubse_urma_controller
        ubse_serial
        ubse_node_controller
        syssentry
)

target_include_directories(ubs-engine_ubse_urma_controller_ut PUBLIC ${SRC_DIR}/include/adapter_plugins/urma ${SRC_DIR}/framework/misc ${SRC_DIR}/controllers/urma ${SRC_DIR}/framework/context ${SRC_DIR}/framework/com ${SRC_DIR}/api_server)