file(GLOB UBSE_API_SOURCES *.h *.cpp)
add_library(ubse_mem_controller_debt  STATIC ${UBSE_API_SOURCES})
target_link_libraries( ubse_mem_controller_debt
        PRIVATE
        ubse_controller_message
        ubse_controller_api
)


# 生成式表达式： 获取所依赖库的头文件
target_include_directories(ubse_mem_controller_debt PRIVATE
    $<TARGET_PROPERTY:ubse_election,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_log,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_context,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:api_server,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_utils,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_com,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_node_controller,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_http,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_serial,INTERFACE_INCLUDE_DIRECTORIES>
)
target_include_directories(ubse_mem_controller_debt PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)