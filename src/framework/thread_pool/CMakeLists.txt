add_library(UbseThreadPool STATIC
        ubse_thread_pool.cpp
)
target_include_directories(UbseThreadPool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(UbseThreadPool PUBLIC UbseLog)

add_library(ubse_thread_pool STATIC
        ubse_thread_pool.cpp
        ubse_thread_pool_module.cpp
)

target_include_directories(ubse_thread_pool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_thread_pool PRIVATE ubse_utils ubse_log ubse_context)