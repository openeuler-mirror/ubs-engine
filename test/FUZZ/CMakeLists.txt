#set(FUZZ_BINARY ${CMAKE_PROJECT_NAME}_fuzz)
#
## 将 FUZZ 目录写入到 ubse_fuzz_dir.h 中
#configure_file(
#        "${CMAKE_CURRENT_SOURCE_DIR}/ubse_fuzz_dir.h.in"
#        "${CMAKE_BINARY_DIR}/include/ubse_fuzz_dir.h"
#        @ONLY
#)
#
#file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)
#
#add_executable(${FUZZ_BINARY} EXCLUDE_FROM_ALL ${TEST_SOURCES})
#
#target_link_libraries(${FUZZ_BINARY} PUBLIC
#        asan
#        ubsan
#        googletest
#        mockcpp
#        secodefuzz::static
#
#        ubse_controller_api
#        ubse_mem_decoder_utils
#        ubse_mem_scheduler
#        ubse_ipc_server
#        ubse_utils
#        ubse_serial
#        ubse_event
#        ubse_config
#        ubse_http
#        ubse_log
#        ubse_thread_pool
#        ubse_security
#        ubse_election
#        ubse_com
#        ubse_xml
#        ubse_mti
#        ubse_mmi
#        mem_controller
#        cert_api_static
#        ubse_ras
#        ubse_timer
#        ubse_context
#        ubse_security
#        ubse_controller
#        api_server
#        syssentry
#        rm_mem_algo_static
#        ubse_node_controller
#        ubse_plugin
#
#        virt_agent_sdk_message
#        vm
#        ubse_sdk
#        ubs_virt_agent_sdk
#        ubse_storage
#)
#
#set(FUZZ_COMPILER_FLAGS
#        -g
#        -O0
#        -fno-inline
#        -fsanitize-coverage=trace-pc
#)
#
## Add compile options to FUZZ_BINARY
#target_compile_options(${FUZZ_BINARY} PRIVATE ${FUZZ_COMPILER_FLAGS})
#
#target_compile_options(${FUZZ_BINARY} PRIVATE -fno-access-control ${DEBUG_FLAGS} -w)
#
#execute_process(
#        COMMAND ${CMAKE_C_COMPILER} -print-file-name=libasan.so
#        OUTPUT_VARIABLE LIBASAN_PATH
#        OUTPUT_STRIP_TRAILING_WHITESPACE
#)
#
#set(RUN_TEST "LD_PRELOAD=${LIBASAN_PATH} ${CMAKE_BINARY_DIR}/bin/${FUZZ_BINARY} --gtest_output=xml:${CMAKE_BINARY_DIR}/coverage/fuzz_detail.xml")
#
## 处理透传参数
#set(TRANS_PARAMS $ENV{TRANS_PARAMS})
#if (DEFINED TRANS_PARAMS AND NOT "${TRANS_PARAMS}" STREQUAL "")
#    set(RUN_TEST "${RUN_TEST} ${TRANS_PARAMS}")
#endif ()
#
#if (SKIP_RUN_TESTS)
#    set(RUN_TEST "echo 'Skip run test, only build binary ${CMAKE_BINARY_DIR}/bin/${FUZZ_BINARY}'")
#endif ()
#
#add_custom_target(
#    fuzz
#    COMMAND bash -c "${RUN_TEST}"
#    COMMENT "Run FUZZ Tests: ${RUN_TEST}"
#)
#
#add_dependencies(fuzz ${FUZZ_BINARY})
#
#include(GoogleTest)
#gtest_add_tests(TARGET ${FUZZ_BINARY})
#
#set_target_properties(${FUZZ_BINARY} PROPERTIES
#        INSTALL_RPATH "$ORIGIN/../lib"
#        BUILD_WITH_INSTALL_RPATH TRUE
#        INSTALL_RPATH_USE_LINK_PATH TRUE
#)
## 允许动态库访问可执行程序的全局符号
#target_link_options(${FUZZ_BINARY} PRIVATE -rdynamic)
#
## 构建二进制程序后复制配置文件到构建目录
#add_custom_command(
#        TARGET ${FUZZ_BINARY}
#        POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/conf ${CMAKE_BINARY_DIR}/conf
#        VERBATIM
#)