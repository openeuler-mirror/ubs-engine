#################################################
#                UBSE_SDK_SAMPLE                #
#################################################
add_executable(ubse_sdk_fd_sample EXCLUDE_FROM_ALL ubse_mem_fd_sample.c)
target_link_libraries(ubse_sdk_fd_sample PRIVATE ubse_sdk)

add_executable(ubse_sdk_numa_sample EXCLUDE_FROM_ALL ubse_mem_numa_sample.c)
target_link_libraries(ubse_sdk_numa_sample PRIVATE ubse_sdk)

add_executable(ubse_sdk_node_sample EXCLUDE_FROM_ALL ubse_node_sample.c)
target_link_libraries(ubse_sdk_node_sample PRIVATE ubse_sdk)