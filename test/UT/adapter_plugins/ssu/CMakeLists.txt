add_ut(ubse_ssu)

target_include_directories(${CMAKE_PROJECT_NAME}_ubse_ssu_ut PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/ssu
        ${PROJECT_SOURCE_DIR}/src/adapter_plugins/ssu
)

target_link_libraries(ubs-engine_ubse_ssu_ut
        PRIVATE
        ubse_ssu
        ubse_utils
        ubse_config
        ubse_log
)
