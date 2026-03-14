add_ut(vm)

target_compile_options(${CMAKE_PROJECT_NAME}_vm_ut PRIVATE
        -Wno-unused-parameter
        -Wno-unused-function
        -Wno-missing-field-initializers
        -Wno-maybe-uninitialized
        -Wno-conversion
        -Wno-float-conversion
)
target_link_libraries(${CMAKE_PROJECT_NAME}_vm_ut PRIVATE
        ubse_log ubse_com ubse_event ubse_config ubse_storage ubse_message ubse_plugin
        ubse_ipc_client ubs_virt_agent_sdk
)

set(GTEST_BRIEF "--gtest_brief=0")
set(GTEST_OUTPUT "--gtest_output=xml:${CMAKE_BINARY_DIR}/coverage/ut_detail.xml")

if (FORCE_COLORED_OUTPUT)
    set(GTEST_COLOR "--gtest_color=yes")
else ()
    set(GTEST_COLOR "--gtest_color=auto")
endif ()

set(RUN_VM_TEST "${CMAKE_BINARY_DIR}/bin/${CMAKE_PROJECT_NAME}_vm_ut ${GTEST_COLOR} ${GTEST_BRIEF} ${GTEST_OUTPUT}")

# 处理透传参数
set(TRANS_PARAMS $ENV{TRANS_PARAMS})
if (DEFINED TRANS_PARAMS AND NOT "${TRANS_PARAMS}" STREQUAL "")
    set(RUN_VM_TEST "${RUN_VM_TEST} ${TRANS_PARAMS}")
endif ()

add_custom_target(
        ubse_vm_ut
        COMMENT "Run UT Tests: ${RUN_VM_TEST}"
        COMMAND bash -c "${RUN_VM_TEST}"
)

add_dependencies(ubse_vm_ut "${CMAKE_PROJECT_NAME}_vm_ut")