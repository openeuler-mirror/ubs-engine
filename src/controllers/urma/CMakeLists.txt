
file(GLOB UBSE_URMA_CONTROLLER_SOURCES "*.cpp" "*.h")
add_library(ubse_urma_controller STATIC ${UBSE_URMA_CONTROLLER_SOURCES})

target_include_directories(ubse_urma_controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${SRC_DIR}/res_plugins/mti ${SRC_DIR}/sdk/include ${SRC_DIR}/res_plugins/urma_uvs)
target_link_libraries(ubse_urma_controller PRIVATE ubse_com ubse_mti ubse_node ubse_utils ubse_election ubse_context ubse_timer ubse_serial ubse_ras
        api_server ubse_http ubse_config)
