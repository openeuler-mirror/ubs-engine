add_subdirectory(message)
set(UBSE_SSU_IPC_SOURCES
        handler/ubse_add_access_permission_handler.cpp
        handler/ubse_alloc_space_handler.cpp
        handler/ubse_attach_linear_space_handler.cpp
        handler/ubse_attach_space_handler.cpp
        handler/ubse_attach_striped_space_handler.cpp
        handler/ubse_detach_linear_space_handler.cpp
        handler/ubse_detach_space_handler.cpp
        handler/ubse_detach_striped_space_handler.cpp
        handler/ubse_fe_device_alloc_handler.cpp
        handler/ubse_fe_device_free_handler.cpp
        handler/ubse_free_space_handler.cpp
        handler/ubse_get_alloc_info_by_name_handler.cpp
        handler/ubse_get_connect_info_handler.cpp
        handler/ubse_get_fe_device_list_handler.cpp
        handler/ubse_get_ns_stats_handler.cpp
        handler/ubse_list_alloc_info_handler.cpp
        handler/ubse_remove_access_permission_handler.cpp
        ubse_ssu_ipc_handler.cpp
)
add_library(ubse_ssu_ipc STATIC ${UBSE_SSU_IPC_SOURCES})

target_include_directories(ubse_ssu_ipc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/handler
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/include/plugin_services/ssu
        ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/ssu
        ${PROJECT_SOURCE_DIR}/src/framework
        ${PROJECT_SOURCE_DIR}/src/framework/misc
)

# ubse_ipc_ssu_message 提供序列化/反序列化支持, handler 实现依赖它
target_link_libraries(ubse_ssu_ipc PUBLIC ubse_ipc_ssu_message)