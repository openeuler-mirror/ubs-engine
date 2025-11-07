set(IT_BINARY ${CMAKE_PROJECT_NAME}_it)

# 将 IT 目录写入到 ubse_it_dir.h 中
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/ubse_it_dir.h.in"
        "${CMAKE_BINARY_DIR}/include/ubse_it_dir.h"
        @ONLY
)
add_library(mock_deps SHARED ${CMAKE_CURRENT_SOURCE_DIR}/mockdeps/exceptionTest.cpp ${CMAKE_CURRENT_SOURCE_DIR}/mockdeps/exceptionTest.h)
set_target_properties(mock_deps PROPERTIES OUTPUT_NAME "exceptionTest")

file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)
add_executable(${IT_BINARY} EXCLUDE_FROM_ALL ${TEST_SOURCES}
        #       外部模块入口
        ${CMAKE_SOURCE_DIR}/src/framework/log/ubse_logger.cpp
        ${CMAKE_SOURCE_DIR}/src/framework/config/ubse_conf.cpp
        ${CMAKE_SOURCE_DIR}/src/framework/com/ubse_com.cpp
        ${CMAKE_SOURCE_DIR}/src/framework/event/ubse_event.cpp
        ${CMAKE_SOURCE_DIR}/src/framework/config/ubse_conf.h
)

target_link_libraries(${IT_BINARY} PUBLIC
        mockcpp
        GTest::gmock_main
        ubse_context
        ubse_mock
        ubse_config
        ubse_security
        ubse_plugin
        ubse_event
        ubse_election
        ubse_dev
        ubse_role
        ubse_deadloop
        ubse_node
        obj_mgr
        ubse_job
        ubse_mti
        mem_controller
)

add_subdirectory(module)

target_compile_options(${IT_BINARY} PRIVATE -fno-access-control ${DEBUG_FLAGS})

# 设置环境变量
set(RUN_TEST_ENV
        "UBSE_UDS_ADDRESS=${CMAKE_CURRENT_SOURCE_DIR}/uds/ubseAgentUds.socket UBSE_REMOTE_ADDRESS=${CMAKE_CURRENT_SOURCE_DIR}/uds/rpc.socket UBSE_IPC_UDS_PATH_PREFIX=${CMAKE_CURRENT_SOURCE_DIR}/uds"
)

set(RUN_TEST "env ${RUN_TEST_ENV} ${CMAKE_BINARY_DIR}/bin/${IT_BINARY} --gtest_output=xml:${CMAKE_BINARY_DIR}/coverage/it_detail.xml")

# 处理透传参数
set(TRANS_PARAMS $ENV{TRANS_PARAMS})
if (DEFINED TRANS_PARAMS AND NOT "${TRANS_PARAMS}" STREQUAL "")
    set(RUN_TEST "${RUN_TEST} ${TRANS_PARAMS}")
endif ()

if (SKIP_RUN_TESTS)
    set(RUN_TEST "echo 'Skip run test, only build binary ${CMAKE_BINARY_DIR}/bin/${IT_BINARY}'")
endif ()

add_custom_target(
        it
        COMMAND sudo setcap cap_chown,cap_dac_override,cap_fowner,cap_sys_ptrace,cap_sys_nice=ep "${CMAKE_BINARY_DIR}/bin/${IT_BINARY}"
        COMMAND sudo bash -c "${RUN_TEST}"
        COMMENT "Run IT Tests: ${RUN_TEST}"
)

add_dependencies(it ${IT_BINARY})

include(GoogleTest)
gtest_add_tests(TARGET ${IT_BINARY})

# 允许动态库访问可执行程序的全局符号
target_link_options(${IT_BINARY} PRIVATE -rdynamic)
