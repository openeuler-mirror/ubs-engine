add_library(ubse_node_controller_it_mock STATIC
    ubse_node_controller_it_mock.cpp
)

target_include_directories(ubse_node_controller_it_mock PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/controllers/node
)

