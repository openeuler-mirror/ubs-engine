# 查找 IWYU 路径
find_program(IWYU_PATH NAMES include-what-you-use iwyu)
find_program(IWYU_TOOL NAMES iwyu_tool.py)
find_program(FIX_INCLUDES NAMES fix_includes.py)

# 获取 clang 资源目录
execute_process(
        COMMAND clang -print-resource-dir
        OUTPUT_VARIABLE CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# 检查标准库路径
set(CLANG_STD_INCLUDE "${CLANG_RESOURCE_DIR}/include")
if (NOT EXISTS ${CLANG_STD_INCLUDE})
    message(WARNING "Clang std headers not found at: ${CLANG_STD_INCLUDE}")
else ()
    message(STATUS "Using Clang std headers: ${CLANG_STD_INCLUDE}")
endif ()

option(ENABLE_IWYU "Enable Include What You Use checks" OFF)
if (ENABLE_IWYU)
    if (IWYU_PATH)
        set(IWYU_ARGS
                "${IWYU_PATH}"
                -Xiwyu --cxx17ns
                -Xiwyu --no_fwd_decls
                -Xiwyu --update_comments
                -Xiwyu --mapping_file=${CMAKE_SOURCE_DIR}/iwyu.imp
                -isystem ${CLANG_STD_INCLUDE}
        )

        # 设置全局检查
        message(STATUS "${IWYU_ARGS}")
        set(CMAKE_C_INCLUDE_WHAT_YOU_USE "${IWYU_ARGS}")
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU_ARGS}")
        message(STATUS "IWYU found: ${IWYU_PATH}")

        if (IWYU_TOOL AND FIX_INCLUDES)
            message(STATUS "iwyu_tool.py found: ${IWYU_TOOL}")
            message(STATUS "fix_includes.py found: ${FIX_INCLUDES}")
            add_custom_target(fix_includes
                    COMMAND ${IWYU_TOOL} -v -p ${CMAKE_BINARY_DIR} -- -Xiwyu --cxx17ns -Xiwyu --no_fwd_decls -Xiwyu --mapping_file=${CMAKE_SOURCE_DIR}/iwyu.imp -isystem ${CLANG_STD_INCLUDE} > /tmp/iwyu.raw
                    COMMAND ${FIX_INCLUDES} -b --reorder --update_comments < /tmp/iwyu.raw
            )
        endif ()
    else ()
        message(WARNING "IWYU requested but executable not found")
    endif ()
endif ()

function(fix_includes TARGET)
    if (NOT IWYU_TOOL OR NOT FIX_INCLUDES)
        message(WARNING "IWYU tools not found, skipping target '${TARGET}'")
        return()
    endif ()
    if (NOT EXISTS ${CLANG_STD_INCLUDE})
        message(WARNING "Clang std headers not found at: ${CLANG_STD_INCLUDE}")
        return()
    endif ()
    # 获取目标的所有源文件
    get_target_property(_SOURCES ${TARGET} SOURCES)
    if (NOT _SOURCES)
        message(WARNING "fix_includes: Target '${TARGET}' has no source files")
        return()
    endif ()

    # 过滤C++源文件并转换为绝对路径
    set(_CXX_SOURCES)
    foreach (_FILE ${_SOURCES})
        get_filename_component(_EXT ${_FILE} EXT)
        if (_EXT MATCHES "^(\.cpp|\.cxx|\.cc|\.C)$")
            get_filename_component(_ABS_PATH ${_FILE} ABSOLUTE)
            list(APPEND _CXX_SOURCES ${_ABS_PATH})
        endif ()
    endforeach ()

    if (NOT _CXX_SOURCES)
        message(WARNING "fix_includes: Target '${TARGET}' has no C++ source files")
        return()
    endif ()

    # 定义临时输出文件路径
    set(_IWYU_RAW "${CMAKE_BINARY_DIR}/iwyu_${TARGET}.raw")

    # 添加自定义目标
    add_custom_target(fix_includes_${TARGET}
            COMMAND ${IWYU_TOOL} -v -p ${CMAKE_BINARY_DIR} ${_CXX_SOURCES} --
            -Xiwyu --cxx17ns
            -Xiwyu --no_fwd_decls
            -Xiwyu --update_comments
            -Xiwyu --mapping_file=${CMAKE_SOURCE_DIR}/iwyu.imp
            -isystem ${CLANG_STD_INCLUDE}
            > ${_IWYU_RAW}
            COMMAND ${FIX_INCLUDES} -b --reorder --update_comments < ${_IWYU_RAW}
            COMMENT "Running IWYU analysis and fixing includes for target '${TARGET}'"
            VERBATIM
    )
    add_dependencies(fix_includes_${TARGET} ${TARGET})
    message(STATUS "Added include fixer target for '${TARGET}': fix_includes_${TARGET}")
endfunction()
