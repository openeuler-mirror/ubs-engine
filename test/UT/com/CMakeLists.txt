add_ut(ubse_com)

target_link_libraries(ubs-engine_ubse_com_ut
        PRIVATE
        ubse_com
        ubse_log
        ubse_event
        ubse_thread_pool
        ubse_config
        ubse_context
        ubse_security
        ubse_http
        ubse_plugin
        ubse_config
)