file(GLOB UBSE_API_SOURCES *.h *.cpp)

add_library(ubse_controller_api STATIC ${UBSE_API_SOURCES})
target_link_libraries(ubse_controller_api PUBLIC ubse_controller_rpc ubse_controller_message PRIVATE ubse_mem_scheduler ubse_mem_controller_debt)
target_include_directories(ubse_controller_api PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/controllers/include
)


target_include_directories(ubse_controller_api PRIVATE
    $<TARGET_PROPERTY:ubse_log,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_utils,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_com,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_config,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_context,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_timer,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_mmi,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_election,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:api_server,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_http,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_serial,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_node_controller,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:trace_context,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_ipc_server,INTERFACE_INCLUDE_DIRECTORIES>
)