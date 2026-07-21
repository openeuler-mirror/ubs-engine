add_ut(ubse_vip)

# ubse_vip 仅以 PRIVATE 依赖 ubse_context/ubse_config/ubse_election/ubse_cert，
# 这些库的符号不会传递到 UT 可执行文件，需在此显式链接。
target_link_libraries(${UT_BINARY} PRIVATE
        ubse_context
        ubse_config
        ubse_election
        ubse_cert
        ubse_com
        ubse_node_mgr
)
