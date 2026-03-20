file(GLOB_RECURSE UCACHE_SRCS *.cpp *.h)
add_library(ucache_plugin SHARED ${UCACHE_SRCS})
add_library(ucache_plugin_static EXCLUDE_FROM_ALL STATIC ${UCACHE_SRCS})

add_library(ucache_deps INTERFACE)
target_include_directories(ucache_deps INTERFACE
         ${PROJECT_SOURCE_DIR}/src/include
         ${PROJECT_SOURCE_DIR}/deps/rapidjson/include
         ${PROJECT_SOURCE_DIR}/deps/rapidjson/include/rapidjson
         ${PROJECT_SOURCE_DIR}/src/framework/config

         ${CMAKE_CURRENT_SOURCE_DIR}/
         ${CMAKE_CURRENT_SOURCE_DIR}/include
         ${CMAKE_CURRENT_SOURCE_DIR}/common
         ${CMAKE_CURRENT_SOURCE_DIR}/common/util
         ${CMAKE_CURRENT_SOURCE_DIR}/message
         ${CMAKE_CURRENT_SOURCE_DIR}/export
         ${CMAKE_CURRENT_SOURCE_DIR}/ucache_master
         ${CMAKE_CURRENT_SOURCE_DIR}/ucache_master/master_task_controller
         ${CMAKE_CURRENT_SOURCE_DIR}/ucache_master/bottleneck_strategy
         ${CMAKE_CURRENT_SOURCE_DIR}/ucache_master/rack_msg
         ${CMAKE_CURRENT_SOURCE_DIR}/ucache_master/node_fault_handler
         ${CMAKE_CURRENT_SOURCE_DIR}/ucache_master/data_collect
         ${CMAKE_CURRENT_SOURCE_DIR}/ucache_agent
         ${CMAKE_CURRENT_SOURCE_DIR}/ucache_agent/agent_task_processor
)

target_link_libraries(ucache_deps INTERFACE rapidjson ubse_utils)

target_link_libraries(ucache_plugin PUBLIC ucache_deps)
target_link_libraries(ucache_plugin_static PUBLIC ucache_deps)

set_target_properties(ucache_plugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                             ${CMAKE_BINARY_DIR}/lib)
set_target_properties(ucache_plugin_static PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                             ${CMAKE_BINARY_DIR}/lib)

file(COPY conf/plugin_ucache.conf DESTINATION ${CMAKE_BINARY_DIR}/expand_conf FILE_PERMISSIONS OWNER_READ OWNER_WRITE)