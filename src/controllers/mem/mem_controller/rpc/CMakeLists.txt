file(GLOB UBSE_RESOURCE_SOURCES *.h *.cpp)

add_library(ubse_controller_rpc STATIC ${UBSE_RESOURCE_SOURCES})
target_link_libraries(ubse_controller_rpc PUBLIC ubse_mem_controller_debt PRIVATE
        ubse_utils ubse_context ubse_com ubse_controller_message ubse_controller_api ubse_context api_server ubse_http
        ubse_serial
)
target_include_directories(ubse_controller_rpc PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../adapter_plugins/mti
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../node
        ${CMAKE_CURRENT_SOURCE_DIR}/../
)