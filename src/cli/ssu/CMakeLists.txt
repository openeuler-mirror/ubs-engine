add_executable(ubsectl-ssu
        ubse_cli_ssu_main.cpp
        ubse_cli_ssu_cmd_reg.cpp
        ubse_cli_ssu_struct.cpp
)
target_link_libraries(ubsectl-ssu PRIVATE
        ubse_cli_framework
        ubse_cli_consumer
        ubse_ipc_client
        ubse_serial
        boundscheck
        ubse_utils
        trace_context
)
target_compile_options(ubsectl-ssu PRIVATE
        # C 和 C++ 编译选项
        $<$<CONFIG:Debug>: ${DEBUG_FLAGS}>
        $<$<CONFIG:Release>: ${RELEASE_FLAGS}>
        $<$<CONFIG:RelWithDebInfo>: ${RELEASE_FLAGS}>
        $<$<CONFIG:MinSizeRel>: ${RELEASE_FLAGS}>
)
# 允许动态库访问可执行程序的全局符号
target_link_options(ubsectl-ssu PRIVATE -rdynamic -pie)
