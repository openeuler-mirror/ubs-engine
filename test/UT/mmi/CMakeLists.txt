add_ut(mmi)
target_link_libraries(${CMAKE_PROJECT_NAME}_mmi_ut PRIVATE mmi ubse_context ubse_config ubse_security ubse_http)
target_include_directories(${CMAKE_PROJECT_NAME}_mmi_ut PRIVATE ${PROJECT_SOURCE_DIR}/src/res_plugins/mmi
        ${PROJECT_SOURCE_DIR}/src/include)