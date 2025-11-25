add_library(ubse_ipc_client STATIC ubse_ipc_socket.cpp client/ubse_uds_client.cpp client/ubse_ipc_client.cpp)
target_include_directories(ubse_ipc_client
        PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/client
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(ubse_ipc_server STATIC ubse_ipc_socket.cpp server/ubse_uds_server.cpp server/ubse_ipc_server.cpp)
target_include_directories(ubse_ipc_server
        PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/server ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(ubse_ipc_server PRIVATE ubse_thread_pool ubse_utils ubse_log ubse_context ubse_security ubse_election)