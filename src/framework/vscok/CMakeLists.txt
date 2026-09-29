add_library(ubse_vsock_client STATIC ubse_vsock_client.cpp)
target_link_libraries(ubse_vsock_client PUBLIC ubse_utils ubse_log
                                        PRIVATE ubse_config ubse_context ubse_cert)

add_library(ubse_vsock_server STATIC
        ubse_vsock_server.cpp
        ubse_vsock_transport.cpp
)
target_link_libraries(ubse_vsock_server PUBLIC ubse_utils ubse_log ubse_thread_pool
                                        PRIVATE ubse_cert)
