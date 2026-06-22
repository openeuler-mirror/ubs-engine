find_program(GCOV_PATH gcov)
find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)
find_program(GCOVR_PATH gcovr)
find_program(POWERSHELL_PATH NAMES powershell.exe)
find_program(Python3 python3)
find_program(Simpo NAMES simpo PATHS ${DEPS_DIR}/simpo/bin NO_DEFAULT_PATH)

if (CMAKE_CROSSCOMPILING)
    set(GCOV_PATH "$ENV{GCOV_PATH}")
endif ()

macro(FIND_INCLUDE_DIR result curdir)
    file(GLOB_RECURSE children "${curdir}/*.hpp" "${curdir}/*.h")
    set(dirlist "")
    foreach (child ${children})
        string(REGEX REPLACE "(.*)/.*" "\\1" LIB_NAME ${child})
        if (IS_DIRECTORY ${LIB_NAME})
            list(FIND dirlist ${LIB_NAME} list_index)
            if (${list_index} LESS 0)
                LIST(APPEND dirlist ${LIB_NAME})
            endif ()
        endif ()
    endforeach ()
    set(${result} ${dirlist})
endmacro()

function(setup_coverage)
    if (NOT GCOV_PATH)
        message(FATAL_ERROR "gcov not found! Aborting...")
    else ()
        message(STATUS "gcov found at: ${GCOV_PATH}")
    endif ()

    set(COVERAGE_COMPILER_FLAGS "--coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILER_FLAGS}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILER_FLAGS}" PARENT_SCOPE)
    message(STATUS "Appending code coverage compiler flags: ${COVERAGE_COMPILER_FLAGS}")
    link_libraries(gcov)

    set(COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage)
    execute_process(COMMAND mkdir -p ${CMAKE_BINARY_DIR}/coverage
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    if (LCOV_PATH)
        message(STATUS "lcov found in ${LCOV_PATH}.")
        set(HTML_PATH "${COVERAGE_DIR}/index.html")
        add_custom_target(coverage
                COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/build/coverage.sh ${CMAKE_BINARY_DIR}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                VERBATIM
        )
        add_custom_command(TARGET coverage POST_BUILD
                COMMAND ${LCOV_PATH} -z -d ${CMAKE_BINARY_DIR} # 删除 *.gcda
                COMMENT "Removing intermediate coverage files"
        )
        add_custom_command(TARGET coverage POST_BUILD
                COMMAND ;
                COMMENT "Open ${HTML_PATH} in your browser to view the coverage report."
        )
        if (ENABLE_HTTP_SERVER)
            add_custom_command(TARGET coverage POST_BUILD
                    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/build/start_coverage_server.sh ${HTML_PATH}
                    COMMENT "The coverage report has been generated at ${HTML_PATH}."
                    WORKING_DIRECTORY /
                    VERBATIM
            )
        endif ()
    elseif (GCOVR_PATH)
        message(STATUS "gcovr found in ${GCOVR_PATH}.")
        set(HTML_PATH ${CMAKE_BINARY_DIR}/coverage/index.html)
        add_custom_target(coverage
                COMMAND ${GCOVR_PATH} -f src --gcov-exclude '.+log.+' --html-details ${HTML_PATH}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        )
        add_custom_command(TARGET coverage POST_BUILD
                COMMAND ;
                COMMENT "Open ${HTML_PATH} in your browser to view the coverage report."
        )
    else ()
        message(AUTHOR_WARNING "gcovr not found! replaced by gcov.")
        add_custom_target(coverage
                COMMAND rm -rf *.gcov
                COMMAND find ${CMAKE_BINARY_DIR}/src -name '*.gcda' -exec gcov -pb {} +
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/coverage
        )
    endif ()
endfunction()

macro(install_dep dep_name)
    add_custom_target(build_${dep_name}
            COMMAND mkdir -p ${CMAKE_SOURCE_DIR}/deps
            COMMAND rm -rf ${dep_name}/*
            COMMAND tar -xzf ${CMAKE_SOURCE_DIR}/.deps/${dep_name}_aarch64.tar.gz -C ${CMAKE_SOURCE_DIR}/deps
            COMMENT "Install dep ${dep_name} into /deps/${dep_name}."
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endmacro()

function(print_list list)
    foreach (item ${list})
        message(STATUS "${item}")
    endforeach ()
endfunction()

macro(install_lib lib_name)
    file(GLOB ${lib_name}_LIBS "${DEPS_DIR}/${lib_name}/lib/*.so*" "${DEPS_DIR}/${lib_name}/lib64/*.so*" "${DEPS_DIR}/${lib_name}/lib/*.ko")
    file(COPY ${${lib_name}_LIBS} DESTINATION ${CMAKE_BINARY_DIR}/lib/ FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE)
endmacro()

macro(install_lib_cli lib_name)
    file(GLOB ${lib_name}_LIBS "${DEPS_DIR}/${lib_name}/lib/*.so*" "${DEPS_DIR}/${lib_name}/lib/*.ko")
    file(COPY ${${lib_name}_LIBS} DESTINATION ${CMAKE_BINARY_DIR}/cli/lib/ FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE)
endmacro()

macro(install_lib_devel lib_name)
    file(GLOB ${lib_name}_LIBS "${DEPS_DIR}/${lib_name}/lib/*.so*" "${DEPS_DIR}/${lib_name}/lib/*.ko")
    file(COPY ${${lib_name}_LIBS} DESTINATION ${CMAKE_BINARY_DIR}/devel/lib/ FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE)
endmacro()

# 模块序列化头文件生成
macro(build_message module)
    file(GLOB_RECURSE simpo_files *.simpo)
    if (NOT simpo_files)
        message(WARNING "[${module}] No simpo files have been provided!")
    else ()
        message(STATUS "[${module}] Found simpo files:")
        print_list("${simpo_files}")

        set(header_files "")
        foreach (simpo_file IN LISTS simpo_files)
            # 获取文件名（不带路径和扩展名）
            get_filename_component(base_name ${simpo_file} NAME_WE)

            # 生成目标头文件名
            list(APPEND header_files
                    "${CMAKE_BINARY_DIR}/include/${base_name}_builder.h"
                    "${CMAKE_BINARY_DIR}/include/${base_name}_reader.h"
                    "${CMAKE_BINARY_DIR}/include/${base_name}_verifier.h"
            )
        endforeach ()

        # 添加自定义命令来生成头文件
        add_custom_command(
                OUTPUT ${header_files}
                COMMAND mkdir -p ${CMAKE_BINARY_DIR}/include
                COMMAND ${Simpo} -o ${CMAKE_BINARY_DIR}/include --simpo --cpp ${simpo_files}
                COMMENT "[${module}] Generating interface based on simpo files"
                VERBATIM
                DEPENDS ${simpo_files}  # 依赖于 simpo 文件
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        # 创建一个自定义目标来生成头文件
        add_custom_target(build_${module}_message ALL DEPENDS ${header_files})
    endif ()
endmacro()

# 添加模块 UT
macro(add_ut module)
    set(UT_BINARY ${CMAKE_PROJECT_NAME}_${module}_ut)
    file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false
            ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    )
    add_executable(${UT_BINARY} EXCLUDE_FROM_ALL ${TEST_SOURCES} ${CMAKE_SOURCE_DIR}/test/UT/main.cpp)
    target_link_libraries(${UT_BINARY} PUBLIC
            GTest::gmock_main
            mockcpp
            ${module}
    )
    # 打破控制权限
    target_compile_options(${UT_BINARY} PRIVATE -fno-access-control ${DEBUG_FLAGS})
    set_target_properties(${UT_BINARY} PROPERTIES LINK_FLAGS "-Wl,--as-needed")
    set(RUN_TEST "${CMAKE_BINARY_DIR}/bin/${UT_BINARY} \
		--gtest_output=xml:${CMAKE_BINARY_DIR}/coverage/${module}_detail.xml")
    # 处理透传参数
    set(TRANS_PARAMS $ENV{TRANS_PARAMS})
    if (DEFINED TRANS_PARAMS AND NOT "${TRANS_PARAMS}" STREQUAL "")
        set(RUN_TEST "${RUN_TEST} ${TRANS_PARAMS}")
    endif ()
    if (SKIP_RUN_TESTS)
        set(RUN_TEST "echo 'Skip run test, only build binary ${CMAKE_BINARY_DIR}/bin/${UT_BINARY}'")
    endif ()
    add_custom_target(${module}_ut
            COMMAND bash -c "${RUN_TEST}" || true
            COMMENT "Run testing: ${RUN_TEST}"
    )
    add_dependencies(${module}_ut ${UT_BINARY})
    gtest_discover_tests(${UT_BINARY} PROPERTIES LABELS "${module}")
endmacro()

macro(add_it module)
    set(IT_BINARY ${CMAKE_PROJECT_NAME}_${module}_it)
    file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false
            ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    )
    # Module source files compiled directly into IT binary (same as ubse binary)
    # These provide vtables/typeinfo that static libraries don't export
    set(IT_MODULE_SOURCES
            ${SRC_DIR}/framework/security/ubse_security_module.cpp
            ${SRC_DIR}/framework/com/ubse_com_module.cpp
            ${SRC_DIR}/framework/storage/ubse_storage.cpp
            ${SRC_DIR}/framework/event/ubse_event_module.cpp
            ${SRC_DIR}/framework/log/ubse_logger_module.cpp
            ${SRC_DIR}/framework/plugin_mgr/ubse_plugin_module.cpp
            ${SRC_DIR}/framework/http/ubse_http_module.cpp
            ${SRC_DIR}/controllers/node/ubse_node_controller_module.cpp
            ${SRC_DIR}/controllers/mem/mem_controller/ubse_mem_controller_module.cpp
            ${SRC_DIR}/controllers/mem/mem_controller/ubse_mem_controller.cpp
            ${SRC_DIR}/controllers/urma/ubse_urma_controller_module.cpp
            ${SRC_DIR}/controllers/urma/ubse_urma_controller.cpp
            ${SRC_DIR}/controllers/npu/ubse_npu_controller_module.cpp
            ${SRC_DIR}/framework/ha/ubse_election_module.cpp
            ${SRC_DIR}/adapter_plugins/syssentry/sys_sentry_module.cpp
            ${SRC_DIR}/ras/ubse_ras_module.cpp
            ${SRC_DIR}/api_server/ubse_api_server_module.cpp
            ${SRC_DIR}/adapter_plugins/urma_uvs/ubse_urma_uvs_module.cpp
            ${SRC_DIR}/framework/security/ubse_security.cpp
            ${SRC_DIR}/framework/log/ubse_logger.cpp
            ${SRC_DIR}/framework/com/ubse_com.cpp
            ${SRC_DIR}/framework/storage/ubse_storage.cpp
            ${SRC_DIR}/framework/event/ubse_event.cpp
            ${SRC_DIR}/framework/plugin_mgr/ubse_plugin.cpp
            ${SRC_DIR}/framework/trace_context/trace_context.cpp
            ${SRC_DIR}/api_server/ubse_api_server.cpp
    )
    add_executable(${IT_BINARY} EXCLUDE_FROM_ALL ${TEST_SOURCES} ${CMAKE_SOURCE_DIR}/test/IT/main.cpp ${IT_MODULE_SOURCES})
    target_include_directories(${IT_BINARY} PRIVATE ${CMAKE_BINARY_DIR}/rpc)
    target_link_libraries(${IT_BINARY} PUBLIC
            GTest::gmock_main
            ubse_it_infra
    )
    # Force inclusion of register_config symbols (same as ubse binary)
    target_link_libraries(${IT_BINARY} PUBLIC
            -Wl,--whole-archive
            ubse_node_controller_register_config
            -Wl,--no-whole-archive
    )
    target_compile_options(${IT_BINARY} PRIVATE ${DEBUG_FLAGS})
    # Don't use --as-needed for IT binaries; static lib dependency chain requires all symbols
    set_target_properties(${IT_BINARY} PROPERTIES LINK_FLAGS "-Wl,--no-as-needed")
    add_dependencies(${IT_BINARY} ubse)
    add_dependencies(${IT_BINARY} ubsectl)
    add_dependencies(${IT_BINARY} ubse_it_daemon)
    add_dependencies(${IT_BINARY} ubse_interface_preload)
    add_dependencies(${IT_BINARY} obmm_stub)
    add_dependencies(${IT_BINARY} urma_uvs_stub)
    add_dependencies(${IT_BINARY} xalarm_stub)
    set(RUN_TEST "${CMAKE_BINARY_DIR}/bin/${IT_BINARY} \
		--gtest_output=xml:${CMAKE_BINARY_DIR}/coverage/it/${module}_detail.xml")
    set(TRANS_PARAMS $ENV{TRANS_PARAMS})
    if (DEFINED TRANS_PARAMS AND NOT "${TRANS_PARAMS}" STREQUAL "")
        set(RUN_TEST "${RUN_TEST} ${TRANS_PARAMS}")
    endif ()
    if (SKIP_RUN_TESTS)
        set(RUN_TEST "echo 'Skip run IT test, only build binary ${CMAKE_BINARY_DIR}/bin/${IT_BINARY}'")
    endif ()
    add_custom_target(${module}_it
            COMMAND bash -c "${RUN_TEST}"
            COMMENT "Run IT testing: ${RUN_TEST}"
    )
    add_dependencies(${module}_it ${IT_BINARY})
    add_dependencies(${module}_it ubse)
    add_dependencies(${module}_it ubsectl)
    add_dependencies(${module}_it ubse_it_daemon)
    add_dependencies(${module}_it ubse_interface_preload)
    add_dependencies(${module}_it obmm_stub)
    add_dependencies(${module}_it urma_uvs_stub)
    add_dependencies(${module}_it xalarm_stub)
    set_property(GLOBAL APPEND PROPERTY UBSE_IT_TARGETS ${module}_it)
    set_property(GLOBAL APPEND PROPERTY UBSE_IT_BINARIES ${IT_BINARY})
    gtest_discover_tests(${IT_BINARY} PROPERTIES LABELS "it_${module}")
endmacro()

macro(add_independent_exec_ut executable_name)
    set(GTEST_BRIEF "--gtest_brief=0")
    set(GTEST_OUTPUT "--gtest_output=xml:${CMAKE_BINARY_DIR}/coverage/ut/${executable_name}_detail.xml")

    if (FORCE_COLORED_OUTPUT)
        set(GTEST_COLOR "--gtest_color=yes")
    else ()
        set(GTEST_COLOR "--gtest_color=auto")
    endif ()

    set(RUN_TEST "${CMAKE_BINARY_DIR}/bin/${executable_name} ${GTEST_COLOR} ${GTEST_BRIEF} ${GTEST_OUTPUT}")

    # 处理透传参数
    set(TRANS_PARAMS $ENV{TRANS_PARAMS})
    if (DEFINED TRANS_PARAMS AND NOT "${TRANS_PARAMS}" STREQUAL "")
        set(RUN_TEST "${RUN_TEST} ${TRANS_PARAMS}")
    endif ()

    if (SKIP_RUN_TESTS)
        set(RUN_TEST "echo 'Skip run test, only build binary ${CMAKE_BINARY_DIR}/bin/${executable_name}'")
    endif ()

    set(preface_target "${executable_name}_independent_ut")
    message(STATUS "new target ${preface_target}")
    add_custom_target(${preface_target}
            COMMAND bash -c "${RUN_TEST}"
            COMMENT "Run UT Tests: ${RUN_TEST}"
    )
    add_dependencies(${preface_target} ${executable_name})
    add_dependencies(ut ${preface_target})
endmacro()
