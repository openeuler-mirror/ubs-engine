add_library(api_server STATIC ubse_api_server_module.cpp)
target_include_directories(api_server PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(api_server PRIVATE ubse_context ubse_utils ubse_config ubse_log PUBLIC ubse_ipc_server)