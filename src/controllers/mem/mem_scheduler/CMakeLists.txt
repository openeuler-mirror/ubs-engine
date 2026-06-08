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
        ../../../ras/water_process/ubse_mem_json_def.cpp
        )
target_include_directories(ubse_mem_scheduler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/controllers/include ${PROJECT_SOURCE_DIR}/src/controllers/mem/algorithm/strategy  ${PROJECT_SOURCE_DIR}/src/include)
target_link_libraries(ubse_mem_scheduler PRIVATE rm_mem_algo_static)
target_include_directories(ubse_mem_scheduler PRIVATE
    $<TARGET_PROPERTY:ubse_node_controller,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_context,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_config,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_election,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_com,INTERFACE_INCLUDE_DIRECTORIES>
)
