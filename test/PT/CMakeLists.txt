set(PT_BINARY ${CMAKE_PROJECT_NAME}_pt)

# 将 PT 目录写入到 ubse_pt_dir.h 中
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/ubse_pt_dir.h.in"
        "${CMAKE_BINARY_DIR}/include/ubse_pt_dir.h"
        @ONLY
)

file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)
add_executable(${PT_BINARY} EXCLUDE_FROM_ALL ${TEST_SOURCES}
        #       外部模块入口
        ${CMAKE_SOURCE_DIR}/src/framework/log/ubse_logger.cpp
        ${CMAKE_SOURCE_DIR}/src/framework/config/ubse_conf.cpp
        ${CMAKE_SOURCE_DIR}/src/framework/com/ubse_com.cpp
)

target_link_libraries(${PT_BINARY} PUBLIC
        GTest::gmock_main
        mockcpp
        ubse_context
        ubse_mock
        ubse_config
        ubse_plugin
)


target_compile_options(${PT_BINARY} PRIVATE -fno-access-control ${DEBUG_FLAGS})

set(RUN_TEST "${CMAKE_BINARY_DIR}/bin/${PT_BINARY} --gtest_output=xml:${CMAKE_BINARY_DIR}/coverage/pt_detail.xml")

# 处理透传参数
set(TRANS_PARAMS $ENV{TRANS_PARAMS})
if (DEFINED TRANS_PARAMS AND NOT "${TRANS_PARAMS}" STREQUAL "")
    set(RUN_TEST "${RUN_TEST} ${TRANS_PARAMS}")
endif ()

if (SKIP_RUN_TESTS)
    set(RUN_TEST "echo 'Skip run test, only build binary ${CMAKE_BINARY_DIR}/bin/${PT_BINARY}'")
endif ()

add_custom_target(
    pt
    COMMAND bash -c "${RUN_TEST}"
    COMMENT "Run PT Tests: ${RUN_TEST}"
)

add_dependencies(pt ${PT_BINARY})

include(GoogleTest)
gtest_add_tests(TARGET ${PT_BINARY})

set_target_properties(${PT_BINARY} PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH_USE_LINK_PATH TRUE
)
# 允许动态库访问可执行程序的全局符号
target_link_options(${PT_BINARY} PRIVATE -rdynamic)

# 构建二进制程序后复制配置文件到构建目录
add_custom_command(
        TARGET ${PT_BINARY}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/conf ${CMAKE_BINARY_DIR}/conf
        VERBATIM
)