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
        ${PROJECT_SOURCE_DIR}/src/controllers/include
)
target_link_libraries(rm_mem_algo_static PUBLIC boundscheck)
target_include_directories(rm_mem_algo_static PUBLIC
    $<TARGET_PROPERTY:ubse_utils,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_context,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_config,INTERFACE_INCLUDE_DIRECTORIES>
)