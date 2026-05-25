file(GLOB UBSE_API_SOURCES *.h *.cpp)

add_library(ubse_controller_api STATIC ${UBSE_API_SOURCES})
target_link_libraries(ubse_controller_api  PUBLIC ubse_controller_rpc ubse_controller_message PRIVATE ubse_log ubse_utils ubse_com ubse_config ubse_context ubse_timer ubse_mem_scheduler ubse_mmi
        ubse_election api_server ubse_http ubse_serial ubse_node_controller trace_context ubse_ipc_server ubse_mem_controller_debt ubse_controller_includes
)
target_include_directories(ubse_controller_api PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)