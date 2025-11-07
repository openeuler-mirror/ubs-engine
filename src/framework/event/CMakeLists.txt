add_library(ubse_event STATIC
        ubse_event.cpp
        ubse_event_module.cpp
        ubse_event_distribute.cpp
        ubse_event_queue.cpp
        ubse_event_thread_pool.cpp
)
target_include_directories(ubse_event PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_event PUBLIC ubse_utils
        PRIVATE ubse_context ubse_config)