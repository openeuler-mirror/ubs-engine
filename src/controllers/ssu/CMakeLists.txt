
file(GLOB UBSE_SSU_CONTROLLER_SOURCES "*.cpp" "*.h")
file(GLOB UBSE_SSU_CONTROLLER_MESSAGE_SOURCES "message/*.cpp" "message/*.h")
file(GLOB UBSE_SSU_CONTROLLER_DEBT_SOURCES "debt/*.cpp" "debt/*.h")
set(UBSE_SSU_CONTROLLER_ALL_SOURCES ${UBSE_SSU_CONTROLLER_SOURCES} ${UBSE_SSU_CONTROLLER_MESSAGE_SOURCES} ${UBSE_SSU_CONTROLLER_DEBT_SOURCES})
add_library(ubse_ssu_controller STATIC ${UBSE_SSU_CONTROLLER_ALL_SOURCES})

target_include_directories(ubse_ssu_controller PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/adapter_plugins/ssu
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/framework/filter_chain
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/plugin_services/ssu
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/misc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/serde
    ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/com
)
target_link_libraries(ubse_ssu_controller PRIVATE
    ubse_log
    ubse_context
    ubse_com
    ubse_serial
    ubse_election
    ubse_timer
    ubse_utils
)
