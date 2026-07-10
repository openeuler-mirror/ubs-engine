set(UBSE_SSU_MESSAGE_SOURCES
    ubse_ssu_obj_message.cpp
    ubse_ssu_handler_message.cpp
)
add_library(ubse_ssu_message STATIC ${UBSE_SSU_MESSAGE_SOURCES})

target_include_directories(ubse_ssu_message PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/include/plugin_services/ssu
    ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/ssu
    ${PROJECT_SOURCE_DIR}/src/framework
    ${PROJECT_SOURCE_DIR}/src/framework/misc
)