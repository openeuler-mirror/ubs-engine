add_ut(ubse_config)

target_link_libraries(${CMAKE_PROJECT_NAME}_ubse_config_ut
        PRIVATE
        ubse_http
        ubse_utils
        ubse_config_message
        ubse_com
        ubse_event
        ubse_data_sync
        ubse_data_sync_message
        ubse_parse
        ubse_election
        ubse_security
        ubse_dev
        ubse_context)