add_subdirectory(message)
add_subdirectory(rpc)
add_library(mem_controller STATIC
        request_helper.cpp
        ubse_mem_controller_ledger.cpp
        ubse_mem_controller_msg.cpp
        ubse_mem_controller_ledger_filter.cpp
        ubse_mem_controller_api_agent.cpp
        ubse_mem_controller_handler.cpp
        ubse_mem_controller_module.cpp
        ubse_mem_debt_info_query.cpp
        ubse_mem_rpc.cpp
        ubse_mem_controller_api.cpp
        AccountObjAdapter.cpp
        ubse_mem_utils.cpp
        logging_lock_guard.cpp
        ubse_mem_api_convert.cpp
        ubse_mem_rpc_to_controller.cpp
        ubse_mem_controller_query_api.cpp
        ubse_mem_api.cpp
        ubse_mem_decoder_utils.cpp
)
target_link_libraries(mem_controller PUBLIC ubse_controller_rpc ubse_controller_message PRIVATE ubse_log ubse_utils ubse_com ubse_config ubse_context ubse_timer mem_scheduler mmi
        ubse_election api_server ubse_http ubse_serial)
target_include_directories(mem_controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../res_plugins/mti
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../res_plugins/mti/lcne
        ${CMAKE_CURRENT_SOURCE_DIR}/../../node
        ${SRC_DIR}/sdk/include
)
