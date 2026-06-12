add_ut(ubse_ssu_controller)

target_include_directories(${CMAKE_PROJECT_NAME}_ubse_ssu_controller_ut PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
)

target_link_libraries(${CMAKE_PROJECT_NAME}_ubse_ssu_controller_ut PRIVATE
        ubse_ssu_controller
        ubse_log
)
