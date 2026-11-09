file(GLOB UBSE_MEM_MIGRATE_SOURCES *.h *.cpp)
add_library(ubse_mem_controller_migrate STATIC ${UBSE_MEM_MIGRATE_SOURCES})
target_link_libraries(ubse_mem_controller_migrate
        PRIVATE
        mem_controller
        ubse_controller_api
        ubse_controller_message
        ubse_mem_controller_debt
        ubse_election
        ubse_log
        ubse_utils
        ubse_com
        ubse_config
        ubse_context
        ubse_timer
        ubse_mem_scheduler
        ubse_mem_decoder_utils
        ubse_mmi
        ubse_node_controller
        api_server
        ubse_http
        ubse_serial
        trace_context
        ubse_ipc_server
        ubse_controller_includes
)
target_include_directories(ubse_mem_controller_migrate PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
