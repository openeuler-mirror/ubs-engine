add_subdirectory(node)
add_subdirectory(mem)
add_subdirectory(urma)
add_subdirectory(npu)

add_library(ubse_controller_includes INTERFACE)
target_include_directories(ubse_controller_includes INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mem/mem_decoder_utils)

add_library(ubse_controller STATIC)
target_include_directories(ubse_controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_controller PUBLIC mem_controller ubse_node_controller ubse_urma_controller rm_mem_algo_static ubse_mem_scheduler ubse_controller_includes)
