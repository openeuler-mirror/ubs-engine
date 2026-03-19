add_library(ubse_vsock_client STATIC ubse_vsock_client.cpp)
target_link_libraries(ubse_vsock_client PUBLIC ubse_utils ubse_log
                                        PRIVATE ubse_config ubse_context ubse_cert)