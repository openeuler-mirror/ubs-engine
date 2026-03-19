file(GLOB SOURCES "*.cpp" "*.h" "message/*.cpp" "message/*.h" "water_process/*.cpp")
add_library(ubse_ras STATIC ${SOURCES})
target_include_directories(ubse_ras PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_ras
        PRIVATE ubse_log ubse_com ubse_election ubse_context ubse_serial ubse_node_controller ubse_http ubse_controller ubse_event
        ubse_config api_server
)