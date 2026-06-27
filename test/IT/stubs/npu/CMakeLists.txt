add_library(ubse_npu_controller_it_mock STATIC
    ubse_npu_controller_it_mock.cpp
)

target_include_directories(ubse_npu_controller_it_mock PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/mti
    ${PROJECT_SOURCE_DIR}/src/controllers/npu
    ${PROJECT_SOURCE_DIR}/src/controllers/npu/vm_state_monitor
    ${PROJECT_SOURCE_DIR}/src/controllers/npu/resource_collection
    ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti
    ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti/out_of_band
    ${PROJECT_SOURCE_DIR}/src/framework/misc
)
