file(GLOB SOURCES "*.cpp" "*.h")
add_library(ubse_controller_message STATIC ${SOURCES})
target_link_libraries(ubse_controller_message PUBLIC ubse_message PRIVATE ubse_utils ubse_timer ubse_serial)
target_include_directories(ubse_controller_message
        PUBLIC
        ${SRC_DIR}/sdk/c/include
        ${SRC_DIR}/controllers/node/
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SRC_DIR}/controllers/mem/mem_controller/
)

add_library(ubse_mem_message_simpo_interface INTERFACE)
target_include_directories(ubse_mem_message_simpo_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})