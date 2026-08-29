add_subdirectory(vm_state_monitor)
add_subdirectory(resource_collection)

#npu_controller_header_dep
add_library(npu_controller_header_dep INTERFACE)
target_include_directories(npu_controller_header_dep INTERFACE ${CMAKE_CURRENT_LIST_DIR})

#npu_controller_def
add_library(npu_controller_def STATIC ubse_npu_source_def.cpp)
target_link_libraries(npu_controller_def
        PUBLIC npu_controller_header_dep)
target_include_directories(npu_controller_def PUBLIC
        ${PROJECT_SOURCE_DIR}/src/include
)

#npu_controller_core
add_library(npu_controller_core STATIC
        ubse_npu_manager_api.cpp
        ubse_npu_controller_process.cpp)
target_link_libraries(npu_controller_core PUBLIC npu_resource_collection npu_controller_header_dep
        PRIVATE npu_controller_def)
target_include_directories(npu_controller_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/include)

#npu_plugin
add_library(npu_plugin SHARED
        ubse_npu_controller_module.cpp
        ubse_npu_controller_dispatcher.cpp
        ubse_npu_msg_execute.cpp)

target_include_directories(npu_plugin PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/include)

target_include_directories(npu_plugin PRIVATE
        $<TARGET_PROPERTY:ubse_ipc_server,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:api_server,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:ubse_config,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:ubse_context,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:ubse_utils,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(npu_plugin PUBLIC npu_controller_header_dep npu_controller_core
        PRIVATE npu_controller_def npu_libvirt_monitor_api)
target_link_options(npu_plugin PRIVATE -Wl,--allow-shlib-undefined)
set_target_properties(npu_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)