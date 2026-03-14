aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} MMI_SRC)
add_library(ubse_mmi STATIC ${MMI_SRC})
target_include_directories(ubse_mmi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../controllers/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../controllers/node
        ${CMAKE_CURRENT_SOURCE_DIR}/../../adapter_plugins/mti
        ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/misc
)

target_link_libraries(ubse_mmi PRIVATE
        ubse_context
        ubse_config
        ubse_security
        ubse_utils
        ubse_com
        ubse_http
        ubse_election
        ubse_controller)
