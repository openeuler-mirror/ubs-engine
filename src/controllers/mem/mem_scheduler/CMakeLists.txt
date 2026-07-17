file(GLOB_RECURSE UBSE_MEM_SCHEDULER_SOURCES *.cpp)
add_library(ubse_mem_scheduler STATIC ${UBSE_MEM_SCHEDULER_SOURCES})

target_include_directories(ubse_mem_scheduler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/controllers/include ${PROJECT_SOURCE_DIR}/src/framework/config)
target_link_libraries(ubse_mem_scheduler PRIVATE ubse_node_controller ubse_config ubse_context ubse_utils ubse_smbios)
add_library(mem_scheduler ALIAS ubse_mem_scheduler)