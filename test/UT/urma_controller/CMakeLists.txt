add_ut(ubse_urma_controller)
target_link_libraries(ubs-engine_ubse_urma_controller_ut
        PRIVATE
        ubse_urma_controller
        ubse_serial
        ubse_node_controller
        syssentry
)

target_include_directories(ubs-engine_ubse_urma_controller_ut PUBLIC ${SRC_DIR}/include/adapter_plugins/urma ${SRC_DIR}/framework/misc ${SRC_DIR}/controllers/urma ${SRC_DIR}/framework/context ${SRC_DIR}/framework/com ${SRC_DIR}/api_server ${SRC_DIR}/framework/ipc ${SRC_DIR}/framework/ipc/include ${SRC_DIR}/framework/ipc/server ${SRC_DIR}/framework/ipc/client ${SRC_DIR}/framework/config ${SRC_DIR}/framework/utils ${SRC_DIR}/framework/event/include ${SRC_DIR}/framework/event ${SRC_DIR}/framework/log ${SRC_DIR}/framework/serde ${SRC_DIR}/framework/ha ${SRC_DIR}/framework/ha/message ${SRC_DIR}/framework/ha/role ${SRC_DIR}/framework/security ${SRC_DIR}/framework/timer ${SRC_DIR}/framework/thread_pool/include)