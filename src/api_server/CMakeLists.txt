add_library(api_server STATIC ubse_api_server_module.cpp ubse_api_server.cpp)
target_include_directories(api_server PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(api_server PRIVATE ubse_context ubse_utils ubse_config ubse_log api_server_auth PUBLIC
        ubse_ipc_server ubse_serial)
