add_library(ubse_vip STATIC
        ubse_vip_module.cpp
        ubse_vip_manager.cpp
)
target_include_directories(ubse_vip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_vip PUBLIC ubse_utils ubse_http
        PRIVATE ubse_context ubse_config ubse_election ubse_com ubse_node_mgr ubse_cert)
