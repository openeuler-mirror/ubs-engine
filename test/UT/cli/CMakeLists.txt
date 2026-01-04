add_library(rack_cli_ut STATIC
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_buffer_guard.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_reg.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_node_cmd_reg.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_mem_cmd_reg.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_cert_cmd_reg.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_urma_cmd_reg.cpp
        test_mock_invoke.cpp
)
target_link_libraries(rack_cli_ut PUBLIC
        ubse_ipc_client
        ubse_cli_framework
        ubse_serial
        cert_api_static
        boundscheck
)
target_include_directories(rack_cli_ut PUBLIC ${CMAKE_SOURCE_DIR}/src/cli)
add_ut(rack_cli_ut)