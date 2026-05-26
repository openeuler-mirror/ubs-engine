add_ut(node_discovery)

target_link_libraries(${CMAKE_PROJECT_NAME}_node_discovery_ut PRIVATE
        ubse_context ubse_config ubse_security ubse_com ubse_serial ubse_node_discovery)
target_include_directories(${CMAKE_PROJECT_NAME}_node_discovery_ut PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
)