#################################################
#                   UBSE_SDK                    #
#################################################
# 主 SDK 源文件(排除 SSU/NPU 相关代码,两者单独拆出构建独立 SDK 库)
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
        "*.cpp" "*.c"   # C/C++ 源文件
)
# 排除 example 目录下的文件
list(FILTER ALL_SOURCES EXCLUDE REGEX "/example/")
# 排除 SSU 相关源文件(SSU 单独拆出构建 ubse_ssu_sdk)
list(FILTER ALL_SOURCES EXCLUDE REGEX "/ssu/")
list(FILTER ALL_SOURCES EXCLUDE REGEX "ubs_engine_ssu\\.cpp$")
# 排除 NPU 相关源文件(NPU 单独拆出构建 ubse_npu_sdk)
list(FILTER ALL_SOURCES EXCLUDE REGEX "ubs_engine_npu\\.cpp$")
list(FILTER ALL_SOURCES EXCLUDE REGEX "libubse_npu_helper\\.cpp$")

add_library(ubse_sdk SHARED ${ALL_SOURCES})
target_include_directories(ubse_sdk PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/ssu
        ${CMAKE_CURRENT_LIST_DIR}/util
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(ubse_sdk PRIVATE ubse_ipc_client boundscheck)


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
target_link_libraries(ubse_sdk_static PRIVATE ubse_ipc_client boundscheck)
# 设置版本和 soname
set_target_properties(ubse_sdk_static PROPERTIES
        OUTPUT_NAME "ubse-client"              # 输出文件名为 libubse-client.a
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 安装主 SDK 头文件(排除 SSU/NPU 头文件,两者分别由 ubse_ssu_sdk_devel/ubse_npu_sdk_devel 组件提供)
install(DIRECTORY include/
        DESTINATION include/ubse
        COMPONENT ubse_sdk
        PATTERN "ubs_engine_ssu.h" EXCLUDE
        PATTERN "ubs_engine_npu.h" EXCLUDE)

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
# 通过 PUBLIC 依赖主 SDK ubse_sdk,运行时需要 libubse-client.so
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
# SSU SDK 依赖主 SDK: PUBLIC 链接 ubse_sdk(因为 SSU 内部调用 ubse_invoke_call 等公共符号)
# 运行时 libubse-ssu-client.so 需要 libubse-client.so.1 提供 IPC 客户端实现
target_link_libraries(ubse_ssu_sdk PUBLIC ubse_sdk PRIVATE ubse_ipc_client boundscheck)

# 设置版本和 soname
set_target_properties(ubse_ssu_sdk PROPERTIES
        OUTPUT_NAME "ubse-ssu-client"   # 输出文件名为 libubse-ssu-client.so.x.x.x
        VERSION "1.0.1"
        SOVERSION "1"
)

# SSU 静态库
add_library(ubse_ssu_sdk_static STATIC ${SSU_SOURCES})
target_include_directories(ubse_ssu_sdk_static PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/ssu
        ${CMAKE_CURRENT_LIST_DIR}/util
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(ubse_ssu_sdk_static PUBLIC ubse_sdk_static PRIVATE ubse_ipc_client boundscheck)
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

install(TARGETS ubse_ssu_sdk_static
        ARCHIVE DESTINATION lib64    # 静态库文件 (.a)
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT ubse_ssu_sdk_devel
)


#################################################
#                UBSE_NPU_SDK                    #
#################################################
# NPU 单独拆出: 仅包含 ubs_engine_npu.cpp 与 libubse_npu_helper.cpp
# 通过 PUBLIC 依赖主 SDK ubse_sdk,运行时需要 libubse-client.so
set(NPU_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ubs_engine_npu.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/libubse_npu_helper.cpp")

# NPU 动态库
add_library(ubse_npu_sdk SHARED ${NPU_SOURCES})
target_include_directories(ubse_npu_sdk PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/util
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
# NPU SDK 依赖主 SDK: PUBLIC 链接 ubse_sdk(NPU 内部调用 ubse_invoke_call 等公共符号)
# 运行时 libubse-npu-client.so 需要 libubse-client.so.1 提供 IPC 客户端实现
target_link_libraries(ubse_npu_sdk PUBLIC ubse_sdk PRIVATE ubse_ipc_client boundscheck)

# 设置版本和 soname
set_target_properties(ubse_npu_sdk PROPERTIES
        OUTPUT_NAME "ubse-npu-client"   # 输出文件名为 libubse-npu-client.so.x.x.x
        VERSION "1.0.1"
        SOVERSION "1"
)

# NPU 静态库
add_library(ubse_npu_sdk_static STATIC ${NPU_SOURCES})
target_include_directories(ubse_npu_sdk_static PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/util
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(ubse_npu_sdk_static PUBLIC ubse_sdk_static PRIVATE ubse_ipc_client boundscheck)
set_target_properties(ubse_npu_sdk_static PROPERTIES
        OUTPUT_NAME "ubse-npu-client"
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 安装 NPU SDK 动态库(归入 ubse_npu_sdk,对应 spec 中的 npu-client-libs)
install(TARGETS ubse_npu_sdk
        LIBRARY DESTINATION lib64
        PERMISSIONS OWNER_READ OWNER_EXECUTE
        COMPONENT ubse_npu_sdk
)

# 安装 NPU SDK 静态库和头文件(归入 ubse_npu_sdk_devel,对应 spec 中的 npu-client-devel)
# 与 ubse_npu_sdk 拆分,让 spec 可以分别通过 --component 控制 libs 和 devel 的安装范围
install(FILES include/ubs_engine_npu.h
        DESTINATION include/ubse
        COMPONENT ubse_npu_sdk_devel)

install(TARGETS ubse_npu_sdk_static
        ARCHIVE DESTINATION lib64
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT ubse_npu_sdk_devel
)
