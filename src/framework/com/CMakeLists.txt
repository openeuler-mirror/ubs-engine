# UbseCom
add_library(UbseCom STATIC
        engine/ubse_com_engine.cpp
        ubse_com_def.cpp
        ubse_com_base.cpp
        rpc/ubse_rpc_server.cpp
        intercom/ubse_inter_com.cpp
)

target_include_directories(UbseCom PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(UbseCom PUBLIC hcom PRIVATE ubse_context ubse_config ubse_mti ubse_http UbseLog ubse_utils trace_context ubse_security)

# ubse_com 内部模块
add_library(ubse_com STATIC
        ubse_com_module.cpp
        ubse_com.cpp
)

target_include_directories(ubse_com PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ubse_com PUBLIC UbseCom PRIVATE ubse_context ubse_config ubse_mti ubse_http UbseLog ubse_utils ubse_election ubse_security)