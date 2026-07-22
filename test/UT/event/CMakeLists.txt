add_ut(ubse_event)

target_link_libraries(${CMAKE_PROJECT_NAME}_ubse_event_ut
        PRIVATE
        ubse_config
        ubse_log
        ubse_thread_pool
        UbseLog)