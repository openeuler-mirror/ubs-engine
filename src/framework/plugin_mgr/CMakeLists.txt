add_library(ubse_plugin STATIC
        ubse_plugin_config.cpp
        ubse_plugin_admission.cpp
        ubse_plugin_manager.cpp
        ubse_plugin_module.cpp
        ubse_plugin.cpp)

target_include_directories(ubse_plugin PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_plugin PRIVATE
        ubse_com
        ubse_http
        ubse_config
        ubse_context
        ubse_election
        ubse_node_controller)
