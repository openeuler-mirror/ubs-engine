add_library(ubse_mem_decoder_utils STATIC 
        ubse_mem_decoder_utils.cpp
        ubse_mem_prehandle_manager.cpp
)
target_include_directories(ubse_mem_decoder_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src/controllers/include)
target_link_libraries(ubse_mem_decoder_utils PRIVATE ubse_node_controller
            ubse_context
            ubse_mti
            ubse_config
            ubse_election
            api_server
            ubse_com
            ubse_xml
            ubse_http)