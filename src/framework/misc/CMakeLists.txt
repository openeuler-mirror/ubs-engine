add_library(ubse_utils STATIC
        ubse_str_util.cpp
        ubse_file_util.cpp
        ubse_os_util.cpp
        ubse_net_util.cpp
        ubse_request_id_util.cpp
        ubse_topo_util.cpp
        ubse_json_util.cpp
        ubse_fs.cpp
)
target_link_libraries(ubse_utils PUBLIC rapidjson boundscheck)

target_include_directories(ubse_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})