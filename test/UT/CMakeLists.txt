set(UT_BINARY ${CMAKE_PROJECT_NAME}_ut)

# 将 UT 目录写入到 ubse_ut_dir.h 中
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/ubse_ut_dir.h.in"
        "${CMAKE_BINARY_DIR}/include/ubse_ut_dir.h"
        @ONLY
)

file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)
add_executable(${UT_BINARY} EXCLUDE_FROM_ALL ${TEST_SOURCES})
# 打破控制权限
target_compile_options(${UT_BINARY} PRIVATE -fno-access-control ${DEBUG_FLAGS} -w)
target_link_libraries(${UT_BINARY} PUBLIC
        GTest::gmock_main
        mockcpp
        boundscheck
        rack_cli_ut
        ubse_serial
        ubse_event
        ubse_com
        ubse_config
        ubse_http
        ubse_log
        ubse_thread_pool
        ubse_security
        ubse_election
        ubse_xml
        ubse_mti
        mem_controller
        cert_api_static
        ubse_ras
        ubse_timer
        ubse_context
        ubse_security
        ubse_node
        ubse_controller
        api_server
        syssentry
        rm_mem_algo_static
        mmi
        ubse_node_controller
        mem_scheduler
        ubse_plugin
)
set_target_properties(${UT_BINARY} PROPERTIES LINK_FLAGS "-Wl,--as-needed")
target_link_options(${UT_BINARY} PRIVATE -rdynamic)

add_subdirectory(cli)
add_subdirectory(cert)
add_subdirectory(config)
add_subdirectory(context)
add_subdirectory(election)
add_subdirectory(event)
add_subdirectory(http)
add_subdirectory(lcne)
add_subdirectory(log)
add_subdirectory(mem_controller)
add_subdirectory(node)
add_subdirectory(security)
add_subdirectory(serial_tool)
add_subdirectory(thread_pool)
add_subdirectory(utils)
add_subdirectory(syssentry)
add_subdirectory(ras)
add_subdirectory(algorithm)
add_subdirectory(mem_scheduler)
add_subdirectory(ipc)
add_subdirectory(mmi)
add_subdirectory(node_controller)
add_subdirectory(api_server)
add_subdirectory(plugin_mgr)

# UT禁止链接ubse_c_sdk.so,会造成符号冲突，导致全局变量的二次释放
check_target_dependency(${UT_BINARY} ubse_c_sdk has_dep)
if (has_dep)
    message(FATAL_ERROR "Error: Forbidden library 'ubse_c_sdk' is linked!")
endif ()

set(GTEST_BRIEF "--gtest_brief=0")
set(GTEST_OUTPUT "--gtest_output=xml:${CMAKE_BINARY_DIR}/coverage/ut_detail.xml")

if (FORCE_COLORED_OUTPUT)
    set(GTEST_COLOR "--gtest_color=yes")
else ()
    set(GTEST_COLOR "--gtest_color=auto")
endif ()

set(RUN_TEST "${CMAKE_BINARY_DIR}/bin/${UT_BINARY} ${GTEST_COLOR} ${GTEST_BRIEF} ${GTEST_OUTPUT}")

# 处理透传参数
set(TRANS_PARAMS $ENV{TRANS_PARAMS})
if (DEFINED TRANS_PARAMS AND NOT "${TRANS_PARAMS}" STREQUAL "")
    set(RUN_TEST "${RUN_TEST} ${TRANS_PARAMS}")
endif ()

if (SKIP_RUN_TESTS)
    set(RUN_TEST "echo 'Skip run test, only build binary ${CMAKE_BINARY_DIR}/bin/${UT_BINARY}'")
endif ()

add_custom_target(ut
        COMMAND sudo setcap cap_chown,cap_dac_override,cap_fowner,cap_sys_ptrace,cap_sys_nice,cap_audit_write,cap_net_admin=ep "${CMAKE_BINARY_DIR}/bin/${UT_BINARY}"
        COMMAND bash -c "${RUN_TEST}"
        COMMENT "Run UT Tests: ${RUN_TEST}"
)
add_dependencies(ut ${UT_BINARY})

include(GoogleTest)
gtest_add_tests(TARGET ${UT_BINARY})