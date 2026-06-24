set(UT_BINARY ${CMAKE_PROJECT_NAME}_ut)

add_custom_target(ut)

add_subdirectory(cli)
# 将 UT 目录写入到 ubse_ut_dir.h 中
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/ubse_ut_dir.h.in"
        "${CMAKE_BINARY_DIR}/include/ubse_ut_dir.h"
        @ONLY
)

file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false
        main.cpp
        *.h
        *.cpp)

# exclusive_executable目录下的为单独执行的ut模块，不编入大ut中
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/exclusive_executable/.*")

add_executable(${UT_BINARY} EXCLUDE_FROM_ALL ${TEST_SOURCES})
# 打破控制权限
target_compile_options(${UT_BINARY} PRIVATE -fno-access-control ${DEBUG_FLAGS} -w)
target_link_libraries(${UT_BINARY} PUBLIC
        GTest::gmock_main
        mockcpp
        boundscheck
        ubse_cli_ut
        ubse_serial
        ubse_event
        ubse_config
        ubse_http
        ubse_log
        ubse_thread_pool
        ubse_security
        ubse_election
        ubse_com
        ubse_xml
        ubse_mti
        mti_out_of_band
        ubse_urma_uvs
        ubse_mmi
        mem_controller
        cert_api_static
        ubse_cert
        ubse_ras
        ubse_timer
        ubse_context
        ubse_controller
        api_server
        syssentry
        rm_mem_algo_static
        ubse_node_controller
        ubse_plugin
        ubse_storage
        ubse_sdk
        npu_controller
        ubse_urma_controller
        ubse_urma_uvs
)

target_link_libraries(${UT_BINARY} PUBLIC
        ubse_node_controller_register_config
)

target_link_libraries(${UT_BINARY} PUBLIC
        syssentry
)

include_directories(
        ${SRC_DIR}/framework/ipc
        ${SRC_DIR}/framework/ipc/include
        ${SRC_DIR}/framework/ipc/server
        ${SRC_DIR}/framework/misc
        ${SRC_DIR}/framework/misc/lock
        ${SRC_DIR}/framework/config
        ${SRC_DIR}/framework/context
        ${SRC_DIR}/framework/com
        ${SRC_DIR}/framework/serde
        ${SRC_DIR}/framework/security
        ${SRC_DIR}/api_server
        ${SRC_DIR}/controllers/include
        ${SRC_DIR}/ras
        ${SRC_DIR}/adapter_plugins/urma_uvs
)

set_target_properties(${UT_BINARY} PROPERTIES LINK_FLAGS "-Wl,--as-needed")
target_link_options(${UT_BINARY} PRIVATE -rdynamic)

add_subdirectory(exclusive_executable)
add_subdirectory(cert)
add_subdirectory(config)
add_subdirectory(context)
add_subdirectory(election)
add_subdirectory(event)
add_subdirectory(http)
add_subdirectory(ipc)
add_subdirectory(log)
add_subdirectory(mem_controller)
add_subdirectory(security)
# add_subdirectory(timer)
add_subdirectory(serial_tool)
add_subdirectory(thread_pool)
add_subdirectory(utils)
add_subdirectory(ras)
add_subdirectory(algorithm)
add_subdirectory(mem_scheduler)
add_subdirectory(node_controller)
add_subdirectory(api_server)
add_subdirectory(plugin_mgr)
add_subdirectory(adapter_plugins/mmi)
add_subdirectory(adapter_plugins/mti)
add_subdirectory(adapter_plugins/syssentry)
add_subdirectory(storage)
add_subdirectory(sdk)
add_subdirectory(controllers/npu)
add_subdirectory(urma_controller)
add_subdirectory(adapter_plugins/urma_uvs)

add_independent_exec_ut(${UT_BINARY})

include(GoogleTest)
gtest_add_tests(TARGET ${UT_BINARY})