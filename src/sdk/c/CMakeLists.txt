#################################################
#           UBSE_COMMON (公共对象库)             #
#################################################
# SSU SDK 和主 SDK 共用的公共代码(util/ubs_engine_pack_util.cpp + libubse_helper.cpp)
# OBJECT 库只编译一次生成 .o 文件归档,不生成 .a/.so
# 主 SDK 和 SSU SDK 各自把 .o 文件嵌入自己的 .so,实现"静态依赖"且无代码重复
add_library(ubse_common_obj OBJECT
        ${CMAKE_CURRENT_LIST_DIR}/util/ubs_engine_pack_util.cpp
        ${CMAKE_CURRENT_LIST_DIR}/libubse_helper.cpp)
target_include_directories(ubse_common_obj PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/util
        ${CMAKE_CURRENT_LIST_DIR}/ssu
        ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(ubse_common_obj PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include)
# 编译期需要 IPC 静态库的头文件(libubse_helper.cpp include 了 ubse_ipc_*.h)
target_link_libraries(ubse_common_obj PRIVATE ubse_ipc_client)
# 启用函数级分块,便于链接器 --gc-sections 清理未使用符号
target_compile_options(ubse_common_obj PRIVATE
        -ffunction-sections -fdata-sections)


#################################################
#                   UBSE_SDK (主 SDK)            #
#################################################
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
        "*.cpp" "*.c"   # C/C++ 源文件
)
# 排除 example 目录下的文件
list(FILTER ALL_SOURCES EXCLUDE REGEX "/example/")
# 排除 SSU 相关源文件(SSU 单独拆出构建 ubse_ssu_sdk)
list(FILTER ALL_SOURCES EXCLUDE REGEX "/ssu/")
list(FILTER ALL_SOURCES EXCLUDE REGEX "ubs_engine_ssu\\.cpp$")
# 排除已归入 ubse_common_obj 的源文件,避免重复编译
list(FILTER ALL_SOURCES EXCLUDE REGEX "/util/")
list(FILTER ALL_SOURCES EXCLUDE REGEX "libubse_helper\\.cpp$")

add_library(ubse_sdk SHARED ${ALL_SOURCES})
target_include_directories(ubse_sdk PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/ssu
        ${CMAKE_CURRENT_LIST_DIR}/util
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
# PRIVATE 依赖 ubse_common_obj: 把公共 .o 文件静态嵌入 libubse-client.so
# 不产生运行时依赖(libubse-common.so 之类),符号是 internal 的
target_link_libraries(ubse_sdk PRIVATE ubse_common_obj ubse_ipc_client boundscheck)


# 设置版本和 soname
set_target_properties(ubse_sdk PROPERTIES
        OUTPUT_NAME "ubse-client"       # 输出文件名为 libubse-client.so.x.x.x
        VERSION "1.0.1"                 # 完整版本
        SOVERSION "1"                   # ABI 版本 → 生成 -Wl,-soname,libubse-client.so.1
)

# 静态库
add_library(ubse_sdk_static STATIC ${ALL_SOURCES})
target_include_directories(ubse_sdk_static PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/ssu
        ${CMAKE_CURRENT_LIST_DIR}/util
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(ubse_sdk_static PRIVATE ubse_common_obj ubse_ipc_client boundscheck)
# 设置版本和 soname
set_target_properties(ubse_sdk_static PROPERTIES
        OUTPUT_NAME "ubse-client"              # 输出文件名为 libubse-client.a
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 安装主 SDK 头文件(排除 SSU 头文件,SSU 头文件由 ubse_ssu_sdk 组件提供)
install(DIRECTORY include/
        DESTINATION include/ubse
        COMPONENT ubse_sdk
        PATTERN "ubs_engine_ssu.h" EXCLUDE)

# 安装主 SDK 库文件
install(TARGETS ubse_sdk
        LIBRARY DESTINATION lib64
        PERMISSIONS OWNER_READ OWNER_EXECUTE  # 755
        COMPONENT ubse_sdk
)

install(TARGETS ubse_sdk_static
        ARCHIVE DESTINATION lib64    # 静态库文件 (.a)
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT ubse_sdk
)


#################################################
#                UBSE_SSU_SDK                    #
#################################################
# SSU 单独拆出: 仅包含 ssu/ 目录与 ubs_engine_ssu.cpp
# 公共代码(util/ + libubse_helper.cpp)通过 ubse_common_obj 嵌入,不依赖主 SDK
file(GLOB_RECURSE SSU_SOURCES CONFIGURE_DEPENDS
        "ssu/*.cpp" "ssu/*.c"
)
list(APPEND SSU_SOURCES "${CMAKE_CURRENT_LIST_DIR}/ubs_engine_ssu.cpp")

# SSU 动态库
add_library(ubse_ssu_sdk SHARED ${SSU_SOURCES})
target_include_directories(ubse_ssu_sdk PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/ssu
        ${CMAKE_CURRENT_LIST_DIR}/util
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
# 关键: 不再依赖主 SDK ubse_sdk,改为嵌入公共对象 ubse_common_obj
# ubse_ipc_client 静态嵌入(PRIVATE),提供 ubse_invoke_call/ubse_api_buffer_free 等实现
# 这样 libubse-ssu-client.so 完全自包含,运行时不需要 libubse-client.so
target_link_libraries(ubse_ssu_sdk PRIVATE ubse_common_obj ubse_ipc_client boundscheck)

# 设置版本和 soname
set_target_properties(ubse_ssu_sdk PROPERTIES
        OUTPUT_NAME "ubse-ssu-client"   # 输出文件名为 libubse-ssu-client.so.x.x.x
        VERSION "1.0.1"
        SOVERSION "1"
)
# 启用函数级分块 + 链接器垃圾回收,清理 libubse_helper.cpp 中 SSU 未使用的
# mem/topo/urma/npu 打包函数,避免包体积膨胀
target_compile_options(ubse_ssu_sdk PRIVATE
        -ffunction-sections -fdata-sections)
target_link_options(ubse_ssu_sdk PRIVATE
        -Wl,--gc-sections)

# SSU 静态库
add_library(ubse_ssu_sdk_static STATIC ${SSU_SOURCES})
target_include_directories(ubse_ssu_sdk_static PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/ssu
        ${CMAKE_CURRENT_LIST_DIR}/util
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(ubse_ssu_sdk_static PRIVATE ubse_common_obj ubse_ipc_client boundscheck)
set_target_properties(ubse_ssu_sdk_static PROPERTIES
        OUTPUT_NAME "ubse-ssu-client"
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 安装 SSU SDK 动态库(归入 ubse_ssu_sdk,对应 spec 中的 ssu-client-libs)
install(TARGETS ubse_ssu_sdk
        LIBRARY DESTINATION lib64
        PERMISSIONS OWNER_READ OWNER_EXECUTE
        COMPONENT ubse_ssu_sdk
)

# 安装 SSU SDK 静态库和头文件(归入 ubse_ssu_sdk_devel,对应 spec 中的 ssu-client-devel)
# 与 ubse_ssu_sdk 拆分,让 spec 可以分别通过 --component 控制 libs 和 devel 的安装范围
install(FILES include/ubs_engine_ssu.h
        DESTINATION include/ubse
        COMPONENT ubse_ssu_sdk_devel)
# 注意: ubs_error.h 也由 ubse_ssu_sdk_devel 组件提供,让 ssu-client-devel 自洽
# (ubs_engine_ssu.h include 了 ubs_error.h,必须保证 ssu-client-devel 能独立编译)
install(FILES include/ubs_error.h
        DESTINATION include/ubse
        COMPONENT ubse_ssu_sdk_devel)

install(TARGETS ubse_ssu_sdk_static
        ARCHIVE DESTINATION lib64
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT ubse_ssu_sdk_devel
)
