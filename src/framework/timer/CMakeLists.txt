add_library(ubse_timer STATIC
        ubse_timer.cpp
)

target_include_directories(ubse_timer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_timer PRIVATE ubse_log)