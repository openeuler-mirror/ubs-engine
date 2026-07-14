add_subdirectory(message)
set(UBSE_SSU_IPC_SOURCES
        ubse_ssu_ipc_handler.cpp
)
add_library(ubse_ssu_ipc STATIC ${UBSE_SSU_IPC_SOURCES})

target_include_directories(ubse_ssu_ipc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/handler
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/include/plugin_services/ssu
        ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/ssu
        ${PROJECT_SOURCE_DIR}/src/framework
        ${PROJECT_SOURCE_DIR}/src/framework/misc
)