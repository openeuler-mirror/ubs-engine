add_library(ubse_ipc_client STATIC ubse_ipc_socket.cpp client/ubse_uds_client.cpp client/ubse_ipc_client.cpp ubse_sync_req.cpp ubse_ipc_utils.cpp ubse_ipc_log.cpp)
target_include_directories(ubse_ipc_client
        PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/client
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(ubse_ipc_client PUBLIC UbseThreadPool ubse_serial ubse_utils)

add_library(api_server_auth STATIC auth/ubse_api_server_auth_manager.cpp auth/ubse_api_server_object_manager.cpp)
target_include_directories(api_server_auth PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/../config)
target_link_libraries(api_server_auth PRIVATE ubse_context ubse_utils ubse_config ubse_log)

add_library(ubse_ipc_server STATIC ubse_ipc_socket.cpp server/ubse_uds_server.cpp server/ubse_ipc_server.cpp
        ubse_sync_req.cpp ubse_ipc_utils.cpp ubse_ipc_log.cpp)
target_include_directories(ubse_ipc_server
        PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/server ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(ubse_ipc_server PRIVATE ubse_thread_pool ubse_utils ubse_log ubse_context ubse_security
        ubse_election api_server_auth)
