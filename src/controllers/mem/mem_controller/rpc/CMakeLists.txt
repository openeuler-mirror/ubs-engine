file(GLOB UBSE_RESOURCE_SOURCES *.h *.cpp)

add_library(ubse_controller_rpc STATIC ${UBSE_RESOURCE_SOURCES})
target_link_libraries(ubse_controller_rpc PUBLIC ubse_mem_controller_debt PRIVATE
        ubse_controller_message ubse_controller_api
)
target_include_directories(ubse_controller_rpc PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../adapter_plugins/mti
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../node
        ${CMAKE_CURRENT_SOURCE_DIR}/../
)
target_include_directories(ubse_controller_rpc PRIVATE
    $<TARGET_PROPERTY:ubse_utils,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_context,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_com,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:api_server,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_http,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_serial,INTERFACE_INCLUDE_DIRECTORIES>
)