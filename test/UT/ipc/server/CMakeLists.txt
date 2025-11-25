add_ut(ubse_ipc_server)
target_link_libraries(${CMAKE_PROJECT_NAME}_ubse_ipc_server_ut PRIVATE
        ubse_thread_pool ubse_utils ubse_log ubse_context ubse_security ubse_election ubse_ipc_client)
