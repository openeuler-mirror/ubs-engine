# 版本流水线不运行UT
if (SOURCE_COMPILING AND B_VERSION)
    message("mockcpp is disabled by default for release when using --source-compiling option. To enable testing, avoid using
    the --source-compiling option.")
else ()
    add_library(mockcpp INTERFACE)
    target_include_directories(mockcpp SYSTEM INTERFACE ${DEPS_DIR}/mockcpp/include)
    target_link_libraries(mockcpp INTERFACE ${DEPS_DIR}/mockcpp/lib/libmockcpp.a)
endif ()
