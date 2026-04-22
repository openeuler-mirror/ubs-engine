add_ut(npu)
target_link_libraries(${CMAKE_PROJECT_NAME}_npu_ut PRIVATE
        npu_controller_core
        npu_resource_collection
        npu_controller_def
        ubse_mti
        ubse_log
        ubse_utils
        ubse_context
)
target_include_directories(${CMAKE_PROJECT_NAME}_npu_ut PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/controllers/npu
        ${PROJECT_SOURCE_DIR}/src/controllers/npu/resource_collection
        ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti
        ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti/lcne
)