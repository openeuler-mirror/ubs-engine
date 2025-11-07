add_ut(ubse_log)
target_link_libraries(${PROJECT_NAME}_ubse_log_ut PRIVATE
        ubse_utils ubse_config ubse_context libboundscheck
)
