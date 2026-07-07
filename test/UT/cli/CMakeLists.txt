add_library(ubse_cli_ut STATIC
        ${CMAKE_SOURCE_DIR}/src/cli/ssu/ubse_cli_ssu_cmd_reg.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ssu/ubse_cli_ssu_struct.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_node_cmd_reg.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_mem_cmd_reg.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_cert_cmd_reg.cpp
        ${CMAKE_SOURCE_DIR}/src/cli/ubse_cli_urma_cmd_reg.cpp
        test_mock_invoke.cpp
)
target_link_libraries(ubse_cli_ut PUBLIC
        ubse_ipc_client
        ubse_cli_framework
        ubse_serial
        cert_api_static
        boundscheck
        ubse_cli_consumer
        ubse_mem_message_simpo_interface
)
target_include_directories(ubse_cli_ut PUBLIC
        ${CMAKE_SOURCE_DIR}/src/cli
        ${CMAKE_SOURCE_DIR}/src/cli/ssu
)
add_ut(ubse_cli_ut)
