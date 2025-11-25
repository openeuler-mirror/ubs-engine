add_library(ubse_http STATIC
        ubse_http_common.cpp
        ubse_http_module.cpp
        ubse_http_start.cpp
        ubse_http_tcp_server.cpp
)
target_include_directories(ubse_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_http
        PUBLIC httplib::httplib
        PRIVATE ubse_log ubse_utils ubse_context ubse_config
)