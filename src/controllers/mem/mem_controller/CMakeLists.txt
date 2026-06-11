add_subdirectory(message)
add_subdirectory(rpc)
add_subdirectory(api)
add_subdirectory(debt)

add_library(mem_plugin SHARED
        ubse_mem_service_impl.cpp
        ubse_mem_account.cpp
        ubse_mem_controller_module.cpp
        ubse_mem_controller_ledger.cpp
        ubse_mem_api.cpp
        ubse_mem_controller_query_api.cpp
        ubse_mem_controller_api.cpp
        ubse_mem_controller_fault_handle.cpp
        ubse_mem_controller_dispatcher.cpp
        ubse_mem_controller_api_agent.cpp
        ubse_mem_util.cpp
        ubse_mem_task_executor.cpp
        ubse_mem_sign_verifier.cpp
        ubse_mem_rpc_processor.cpp
        ubse_mem_controller_pre_online.cpp
        ubse_mem_controller_msg.cpp
        ubse_mem_controller_master_online_handler.cpp
        ubse_mem_controller_ledger_filter.cpp
        ubse_mem_agent_update_obj_state.cpp
        ubse_mem_controller_handler.cpp
        ubse_mem_buffer_convert.cpp
        ubse_mem_async_processor.cpp
        ubse_mem_agent_task_manager.cpp
        ubse_mem_advice.cpp
        logging_lock_guard.cpp
)
set(MEM_PLUGIN_DEP_TARGETS
    ubse_controller_rpc ubse_controller_message ubse_controller_api ubse_mem_controller_debt
    ubse_log ubse_utils ubse_com ubse_config ubse_context ubse_timer ubse_mem_scheduler ubse_mmi
    ubse_election api_server ubse_http ubse_serial ubse_node_controller ubse_mem_decoder_utils
    trace_context ubse_ipc_server ubse_vsock_client ubse_security ubse_mti
    ubse_message rm_mem_algo_static)

# 获取其它模块的头文件路径，
set(MEM_PLUGIN_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../adapter_plugins/ubturbo
    ${PROJECT_SOURCE_DIR}/src/include
    ${SRC_DIR}/sdk/c/include)
foreach(dep IN LISTS MEM_PLUGIN_DEP_TARGETS)
    if(TARGET ${dep})
        get_target_property(inc_dirs ${dep} INTERFACE_INCLUDE_DIRECTORIES)
        if(inc_dirs)
            list(APPEND MEM_PLUGIN_INCLUDES ${inc_dirs})
        endif()
    endif()
endforeach()

target_include_directories(mem_plugin PRIVATE ${MEM_PLUGIN_INCLUDES})
target_link_libraries(mem_plugin PUBLIC  ubse_controller_rpc ubse_controller_message ubse_controller_api
        ubse_mem_controller_debt ubse_mem_scheduler ubse_mem_decoder_utils
         rm_mem_algo_static PRIVATE ubse_message ubse_vsock_client)
target_link_options(mem_plugin PRIVATE -Wl,--allow-shlib-undefined)
set_target_properties(mem_plugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(mem_controller_adapter STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/ubse_mem_controller.cpp
)
target_include_directories(mem_controller_adapter PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../adapter_plugins/mti
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../adapter_plugins/ubturbo
        ${CMAKE_CURRENT_SOURCE_DIR}/../../node
        ${SRC_DIR}/sdk/c/include
)
