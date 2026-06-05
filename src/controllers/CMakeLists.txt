include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(node)
add_subdirectory(urma)
add_subdirectory(ssu)

add_library(ubse_controller INTERFACE)
target_include_directories(ubse_controller INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ubse_controller INTERFACE ubse_node_controller ubse_urma_controller mem_controller_adapter)
