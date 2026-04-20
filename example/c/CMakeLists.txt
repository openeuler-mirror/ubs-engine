cmake_minimum_required(VERSION 3.11)
project(example VERSION 1.0 LANGUAGES C)

# 设置可执行文件名称
set(EXECUTABLE_NAME_fd fd_demo)
set(EXECUTABLE_NAME_numa numa_demo)

# 指定源文件
set(SOURCE_FILES_fd fd_demo.c)
set(SOURCE_FILES_numa numa_demo.c)

# 创建可执行文件
add_executable(${EXECUTABLE_NAME_fd} ${SOURCE_FILES_fd})
add_executable(${EXECUTABLE_NAME_numa} ${SOURCE_FILES_numa})

# 链接库
target_link_libraries(${EXECUTABLE_NAME_fd} PRIVATE ubse-client boundscheck)
target_link_libraries(${EXECUTABLE_NAME_numa} PRIVATE ubse-client numa boundscheck)

add_executable(ubse_sdk_fd_example ubse_mem_fd_example.c)
target_link_libraries(ubse_sdk_fd_example PRIVATE ubse-client)

add_executable(ubse_sdk_numa_example ubse_mem_numa_example.c)
target_link_libraries(ubse_sdk_numa_example PRIVATE ubse-client)

add_executable(ubse_sdk_node_example ubse_node_example.c)
target_link_libraries(ubse_sdk_node_example PRIVATE ubse-client)

add_executable(ubse_mem_share_example ubse_mem_share_example.c)
target_link_libraries(ubse_mem_share_example PRIVATE ubse-client)

# 添加编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")
