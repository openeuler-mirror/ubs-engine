add_compile_options(
        -fPIC                                           # 实现动态库随机加载
        -fstack-protector-strong                        # 启用栈保护，防止栈溢出攻击
        $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>   # 非 Debug 模式下启用源代码强化（fortification）级别 2
        $<$<CONFIG:Debug>:-ftrapv>                      # Debug 模式下检测整数溢出（性能影响较大）
        $<$<CONFIG:Debug>:-fstack-check>                # Debug 模式下检查栈空间（性能影响较大）
)
add_link_options(
        -Wl,-z,noexecstack          # 堆栈不可执行保护
        -Wl,-z,relro,-z,now         # GOT表全部重定位只读
)

if(NOT DEFINED CMAKE_BUILD_TYPE OR x${CMAKE_BUILD_TYPE} STREQUAL "x")
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

set(SECURE_BUILD_FLAGS
        -fPIE                       # 生成位置无关可执行文件
)

set(BASE_BUILD_FLAGS
        -Wall                       # 启用大多数警告
        -Wextra                     # 启用额外警告
#        -Wpedantic                 # 启用严格警告
        -Wconversion                # 类型转换警告
        -Wshadow=local              # 变量隐藏警告
        -Wfloat-equal               # 浮点数相等比较警告
        -fsigned-char               # char 默认为 signed char
        -fno-common                 # 全局变量同名警告
)
# 检测内存错误
option(ASAN_BUILD "OFF")
if(ASAN_BUILD STREQUAL "ON")
    message(STATUS "AddressSanitizer (ASAN) is enabled.")
endif()

set(DEBUG_FLAGS ${BASE_BUILD_FLAGS} ${SECURE_BUILD_FLAGS}
        -O0                         # 不进行优化
        -ggdb3                      # 生成最详细的调试信息
        -fno-omit-frame-pointer     # 保留帧指针，便于调试时的堆栈跟踪
)
set(RELEASE_FLAGS ${BASE_BUILD_FLAGS} ${SECURE_BUILD_FLAGS})


