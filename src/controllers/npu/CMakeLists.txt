add_subdirectory(vm_state_monitor)
add_subdirectory(resource_collection)


#npu_controller_header_dep
add_library(npu_controller_header_dep INTERFACE)
target_include_directories(npu_controller_header_dep INTERFACE ${CMAKE_CURRENT_LIST_DIR})

#npu_controller_def
add_library(npu_controller_def STATIC ubse_npu_source_def.cpp)

target_link_libraries(npu_controller_def
        PUBLIC npu_controller_header_dep
        PRIVATE ubse_log ubse_utils)

#npu_controller_core
add_library(npu_controller_core STATIC
        ubse_npu_manager_api.cpp
        ubse_npu_controller_process.cpp)

target_link_libraries(npu_controller_core PUBLIC npu_resource_collection npu_controller_header_dep ubse_mti
        PRIVATE ubse_utils npu_controller_def)


#npu_controller
add_library(npu_controller STATIC
        ubse_npu_controller_module.cpp
        ubse_npu_controller_dispatcher.cpp
        ubse_npu_msg_execute.cpp)

target_link_libraries(npu_controller PUBLIC npu_controller_header_dep npu_controller_core ubse_ipc_server ubse_mti
        PRIVATE npu_controller_def npu_libvirt_monitor_api api_server ubse_config
        PRIVATE ubse_context ubse_utils ubse_log)