add_subdirectory(message)
add_subdirectory(rpc)
add_subdirectory(api)
add_subdirectory(debt)
file(GLOB UBSE_MEM_CONTROLLER_SOURCES *.h *.cpp)
add_library(mem_controller STATIC ${UBSE_MEM_CONTROLLER_SOURCES})
target_link_libraries(mem_controller PUBLIC ubse_controller_rpc ubse_controller_message ubse_controller_api ubse_mem_controller_debt PRIVATE ubse_log ubse_utils ubse_com ubse_config ubse_context ubse_timer ubse_mem_scheduler ubse_mmi
        ubse_election api_server ubse_http ubse_serial ubse_node_controller trace_context ubse_ipc_server ubse_mem_decoder_utils ubse_vsock_client)
target_include_directories(mem_controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../adapter_plugins/mti
        ${CMAKE_CURRENT_SOURCE_DIR}/../../node
        ${SRC_DIR}/sdk/c/include
)
