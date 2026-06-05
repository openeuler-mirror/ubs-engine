add_library(ubse_mem_decoder_utils STATIC 
        ubse_mem_decoder_utils.cpp
        ubse_mem_prehandle_manager.cpp
)
target_include_directories(ubse_mem_decoder_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/controllers/include)
target_link_libraries(ubse_mem_decoder_utils PRIVATE rm_mem_algo_static)
target_include_directories(ubse_mem_decoder_utils PRIVATE
    $<TARGET_PROPERTY:ubse_node_controller,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_context,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_mti,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_config,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_election,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:api_server,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_com,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_xml,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:ubse_http,INTERFACE_INCLUDE_DIRECTORIES>
)