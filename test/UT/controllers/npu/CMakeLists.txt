add_ut(npu_plugin)
target_link_libraries(${CMAKE_PROJECT_NAME}_npu_plugin_ut PRIVATE
        npu_plugin
        api_server
        ubse_ipc_server
        -Wl,--whole-archive syssentry -Wl,--no-whole-archive
)
target_include_directories(${CMAKE_PROJECT_NAME}_npu_plugin_ut PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/controllers/npu
        ${PROJECT_SOURCE_DIR}/src/controllers/npu/resource_collection
        ${PROJECT_SOURCE_DIR}/src/controllers/npu/vm_state_monitor
        ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti
        ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti/lcne
        ${PROJECT_SOURCE_DIR}/src/framework/ipc
        ${PROJECT_SOURCE_DIR}/src/framework/ipc/include
        ${PROJECT_SOURCE_DIR}/src/api_server
)
