file(GLOB SOURCES "*.cpp" "*.h")
add_library(mem_scheduler STATIC
        ubse_mem_account_helper.cpp
        ubse_mem_algo_account.cpp
        ubse_mem_scheduler.cpp
        ubse_mem_strategy_helper.cpp
        ubse_mem_topology_info_manager.cpp
        ubse_mem_mgr_configuration.cpp
)
target_include_directories(mem_scheduler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/controllers/include)
target_link_libraries(mem_scheduler PRIVATE ubse_node_controller ubse_context ubse_timer rm_mem_algo_static ubse_config ubse_http)