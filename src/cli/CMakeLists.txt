add_subdirectory(ubse_cert)
add_subdirectory(ubse_cli_framework)
add_subdirectory(ubse_cli_consumer)
add_subdirectory(ssu)

add_executable(ubsectl ubse_cli_main.cpp
        ubse_cli_node_cmd_reg.cpp
        ubse_cli_mem_cmd_reg.cpp
        ubse_cli_cert_cmd_reg.cpp
        ubse_cli_urma_cmd_reg.cpp
)

target_link_libraries(ubsectl PRIVATE
        ubse_cli_framework
        cert_api_static
        ubse_ipc_client
        ubse_serial
        boundscheck
        ubse_utils
        trace_context
        ubse_cli_consumer
)
target_compile_options(ubsectl PRIVATE
        # C 和 C++ 编译选项
        $<$<CONFIG:Debug>: ${DEBUG_FLAGS}>
        $<$<CONFIG:Release>: ${RELEASE_FLAGS}>
        $<$<CONFIG:RelWithDebInfo>: ${RELEASE_FLAGS}>
        $<$<CONFIG:MinSizeRel>: ${RELEASE_FLAGS}>
)
# 打印 ubsectl 的所有编译选项
get_target_property(UBSE_CLI_COMPILE_OPTIONS ubsectl COMPILE_OPTIONS)

message(DEBUG "ubsectl compile options: ${UBSE_CLI_COMPILE_OPTIONS}")

# 允许动态库访问可执行程序的全局符号
target_link_options(ubsectl PRIVATE -rdynamic -pie)
