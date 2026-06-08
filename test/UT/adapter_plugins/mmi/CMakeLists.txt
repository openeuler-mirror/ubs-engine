add_ut(ubse_mmi)
target_link_libraries(${CMAKE_PROJECT_NAME}_ubse_mmi_ut
        PRIVATE
        ubse_mmi
        ubse_context
        ubse_config
        ubse_security
        ubse_http
        mem_plugin
        ubse_rpc_client
        ubse_rpc
)
target_include_directories(${CMAKE_PROJECT_NAME}_ubse_mmi_ut PRIVATE ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mmi
        ${PROJECT_SOURCE_DIR}/src/include)