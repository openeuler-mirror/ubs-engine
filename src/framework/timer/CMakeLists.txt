add_library(ubse_timer STATIC
        ubse_timer.cpp
        ubse_timer_controller.cpp
)

target_include_directories(ubse_timer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/misc)
target_link_libraries(ubse_timer PRIVATE ubse_log ubse_context ubse_thread_pool)