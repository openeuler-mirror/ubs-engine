#################################################
#                UBSE_SDK_EXAMPLE                #
#################################################
add_executable(ubse_sdk_fd_example EXCLUDE_FROM_ALL ubse_mem_fd_example.c)
target_link_libraries(ubse_sdk_fd_example PRIVATE ubse_sdk)

add_executable(ubse_sdk_numa_example EXCLUDE_FROM_ALL ubse_mem_numa_example.c)
target_link_libraries(ubse_sdk_numa_example PRIVATE ubse_sdk)

add_executable(ubse_sdk_node_example EXCLUDE_FROM_ALL ubse_node_example.c)
target_link_libraries(ubse_sdk_node_example PRIVATE ubse_sdk)

add_executable(ubse_mem_share_example EXCLUDE_FROM_ALL ubse_mem_share_example.c)
target_link_libraries(ubse_mem_share_example PRIVATE ubse_sdk)