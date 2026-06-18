add_ut(ubse_node_controller)
target_link_libraries(${CMAKE_PROJECT_NAME}_ubse_node_controller_ut PRIVATE
        ubse_com
        mem_plugin
        ubse_mmi
        ubse_mti
        ubse_context
        ubse_config
        ubse_security
        ubse_http
        ubse_serial
        api_server
        ubse_ras
        ubse_sdk
        syssentry)
target_include_directories(${CMAKE_PROJECT_NAME}_ubse_node_controller_ut PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti/lcne
        ${PROJECT_SOURCE_DIR}/src/framework/ha
        ${SRC_DIR}/adapter_plugins/syssentry
)



#add_executable(register_node_controller_config_ut test_node_controller_register_config.cpp ../main.cpp)
#target_link_libraries(register_node_controller_config_ut PUBLIC
#        -Wl,--whole-archive
#        ubse_node_controller_register_config
#        -Wl,--no-whole-archive
#        googletest
#        mockcpp
#        ubse_config
#        ubse_log
#)