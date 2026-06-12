
file(GLOB UBSE_SSU_CONTROLLER_SOURCES "*.cpp" "*.h")
add_library(ubse_ssu_controller STATIC ${UBSE_SSU_CONTROLLER_SOURCES})

target_include_directories(ubse_ssu_controller PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/adapter_plugins/ssu
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/framework/filter_chain
)
target_link_libraries(ubse_ssu_controller PRIVATE ubse_log ubse_context)
