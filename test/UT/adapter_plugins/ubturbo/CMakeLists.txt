add_ut(ubturbo)
target_link_libraries(ubs-engine_ubturbo_ut
        PRIVATE
        ubse_ubturbo
        ubse_utils
        ubse_context
        ubse_config
)
