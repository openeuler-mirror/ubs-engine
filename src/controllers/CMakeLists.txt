include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(node)
add_subdirectory(mem)
add_subdirectory(urma)

add_library(ubse_controller STATIC)
target_include_directories(ubse_controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ubse_controller PUBLIC mem_controller ubse_node_controller ubse_urma_controller rm_mem_algo_static ubse_mem_scheduler)