file(GLOB_RECURSE SRC_FILES *.cpp)
add_library(ubse_cli_consumer STATIC ${SRC_FILES})
target_include_directories(ubse_cli_consumer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(ubse_cli_consumer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../addons/process_mem)

target_link_libraries(ubse_cli_consumer PUBLIC ubse_mem_message_simpo_interface PRIVATE
        ubse_ipc_client
        ubse_serial
        boundscheck
        ubse_utils
        trace_context
        ubse_cli_framework
)