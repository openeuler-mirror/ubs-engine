aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} MMI_SRC)
add_library(mmi STATIC ${MMI_SRC})
target_include_directories(mmi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../controllers/include ${CMAKE_CURRENT_SOURCE_DIR}/../../controllers/node ${CMAKE_CURRENT_SOURCE_DIR}/../../res_plugins/mti/  ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/misc/)

target_link_libraries(mmi PRIVATE ubse_context ubse_config ubse_security ubse_http)
