add_library(npu_libvirt_monitor STATIC ubse_npu_libvirt_monitor.cpp)
target_include_directories(npu_libvirt_monitor PUBLIC ${CMAKE_CURRENT_LIST_DIR})

add_library(npu_libvirt_monitor_api STATIC ubse_npu_monitor_service_api.cpp)
target_include_directories(npu_libvirt_monitor_api PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(npu_libvirt_monitor_api
        PRIVATE npu_libvirt_monitor ubse_log npu_resource_collection ubse_utils ubse_xml)