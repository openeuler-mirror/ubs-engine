add_ut(ubse_storage)
target_link_libraries(${CMAKE_PROJECT_NAME}_ubse_storage_ut PRIVATE
        ubse_com
        ubse_storage
        ubse_config
        ubse_context
)