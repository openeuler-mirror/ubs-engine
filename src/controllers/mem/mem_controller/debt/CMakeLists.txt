file(GLOB UBSE_API_SOURCES *.h *.cpp)
add_library(ubse_mem_controller_debt  STATIC ${UBSE_API_SOURCES})
target_link_libraries( ubse_mem_controller_debt
        PRIVATE
        ubse_election
        ubse_log
        ubse_context
        api_server
        ubse_controller_message
        ubse_utils
        ubse_com
        ubse_node_controller
        ubse_http
        mem_controller
        ubse_serial
        ubse_controller_includes
)
target_include_directories(ubse_mem_controller_debt PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)