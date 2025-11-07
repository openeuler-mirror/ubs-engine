add_library(ubse_node STATIC
        ubse_node_module.cpp
)
target_include_directories(ubse_node PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_node
        PRIVATE ubse_com ubse_election ubse_context ubse_config ubse_timer ubse_serial api_server ubse_utils)