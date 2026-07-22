
set(PROCESS_MEM_SOURCES
    process_mem_pid_bridge.cpp
    process_mem_pid_info_manager.cpp
    process_mem_pid_collect.cpp
    process_mem_pid_config_manager.cpp
    process_mem_plugin.cpp
)
add_library(process_mem SHARED ${PROCESS_MEM_SOURCES})
target_include_directories(process_mem PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../rmrs/common/smap
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/serde)
target_link_libraries(process_mem PRIVATE ubse_serial ubse_security rmrs_deps)