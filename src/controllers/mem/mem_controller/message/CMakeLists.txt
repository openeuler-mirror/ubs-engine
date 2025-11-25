file(GLOB SOURCES "*.cpp" "*.h")
add_library(ubse_controller_message STATIC ${SOURCES})
target_link_libraries(ubse_controller_message PUBLIC ubse_message PRIVATE ubse_utils ubse_timer ubse_serial)