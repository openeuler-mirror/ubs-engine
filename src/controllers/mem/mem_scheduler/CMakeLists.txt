add_library(ubse_mem_scheduler STATIC
        ubse_mem_account_helper.cpp
        ubse_mem_algo_account.cpp
        ubse_mem_scheduler.cpp
        ubse_mem_strategy_helper.cpp
        ubse_mem_topology_info_manager.cpp
        ubse_mem_configuration.cpp
        ubse_mem_validator.cpp
        ubse_borrow_account.cpp
        ubse_shm_account.cpp
        ubse_mem_json_def.cpp
        )
target_include_directories(ubse_mem_scheduler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/controllers/include ${PROJECT_SOURCE_DIR}/src/controllers/mem/algorithm/strategy)
target_link_libraries(ubse_mem_scheduler PRIVATE ubse_node_controller ubse_context rm_mem_algo_static ubse_config ubse_election ubse_com)
add_library(mem_scheduler ALIAS ubse_mem_scheduler)
