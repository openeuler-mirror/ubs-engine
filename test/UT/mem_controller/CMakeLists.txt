add_ut(mem_controller)

target_link_libraries(${CMAKE_PROJECT_NAME}_mem_controller_ut PRIVATE ubse_context ubse_utils mem_scheduler api_server ubse_event ubse_com mmi ubse_serial)
target_include_directories(${CMAKE_PROJECT_NAME}_mem_controller_ut PRIVATE
        ${PROJECT_SOURCE_DIR}/src/controllers/include
        ${PROJECT_SOURCE_DIR}/src/include)