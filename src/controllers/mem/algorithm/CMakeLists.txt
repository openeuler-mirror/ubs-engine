file(GLOB_RECURSE RM_MEM_ALGO_SRCS
        ${CMAKE_CURRENT_LIST_DIR}/strategy/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/preprocess/*.cpp
)

# 编译a
add_library(rm_mem_algo_static OBJECT ${RM_MEM_ALGO_SRCS})
target_include_directories(rm_mem_algo_static PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/preprocess
        ${CMAKE_CURRENT_LIST_DIR}/common
        ${CMAKE_CURRENT_LIST_DIR}/strategy
)
target_link_libraries(rm_mem_algo_static PUBLIC boundscheck ubse_utils ubse_context ubse_config)