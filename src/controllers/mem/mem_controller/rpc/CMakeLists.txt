file(GLOB UBSE_RESOURCE_SOURCES *.h *.cpp)

add_library(ubse_controller_rpc STATIC ${UBSE_RESOURCE_SOURCES})
target_link_libraries(ubse_controller_rpc PRIVATE
        ubse_utils ubse_context ubse_com ubse_controller_message ubse_context api_server ubse_http
)
target_include_directories(ubse_controller_rpc PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../res_plugins/mti
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../node
)