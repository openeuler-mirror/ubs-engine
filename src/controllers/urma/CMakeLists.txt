
file(GLOB UBSE_URMA_CONTROLLER_SOURCES "*.cpp" "*.h")
add_library(ubse_urma_controller STATIC ${UBSE_URMA_CONTROLLER_SOURCES})

target_include_directories(ubse_urma_controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${SRC_DIR}/include/adapter_plugins/mti ${SRC_DIR}/sdk/include ${SRC_DIR}/include/adapter_plugins/smbios ${SRC_DIR}/include/adapter_plugins/urma  ${SRC_DIR}adapter_plugins/mti ${SRC_DIR}/controllers/node)
target_link_libraries(ubse_urma_controller PRIVATE ubse_urma_uvs ubse_com ubse_mti ubse_node_controller ubse_utils ubse_election ubse_context ubse_timer ubse_serial ubse_ras
        api_server ubse_http ubse_config ubse_xml ubse_smbios)
