#################################################
#                   UBSE_SDK                    #
#################################################
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
        "*.cpp" "*.c"   # C/C++ 源文件
)
# 排除 example 目录下的文件
list(FILTER ALL_SOURCES EXCLUDE REGEX "/example/")

add_library(ubse_sdk SHARED ${ALL_SOURCES})
target_include_directories(ubse_sdk PRIVATE ${CMAKE_CURRENT_LIST_DIR} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(ubse_sdk PRIVATE ubse_ipc_client boundscheck)


# 设置版本和 soname
set_target_properties(ubse_sdk PROPERTIES
        OUTPUT_NAME "ubse-client"       # 输出文件名为 libubse-client.so.x.x.x
        VERSION "1.0.0"                 # 完整版本
        SOVERSION "1"                   # ABI 版本 → 生成 -Wl,-soname,libubse-client.so.1
)

# 静态库
add_library(ubse_sdk_static STATIC ${ALL_SOURCES})
target_include_directories(ubse_sdk_static PRIVATE ${CMAKE_CURRENT_LIST_DIR} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(ubse_sdk_static PRIVATE ubse_ipc_client boundscheck)
# 设置版本和 soname
set_target_properties(ubse_sdk_static PROPERTIES
        OUTPUT_NAME "ubse-client"              # 输出文件名为 libubse-client.a
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 安装头文件
install(DIRECTORY include/
        DESTINATION include/ubse
        COMPONENT ubse_sdk
)

# 安装库文件
install(TARGETS ubse_sdk
        LIBRARY DESTINATION lib64
        PERMISSIONS OWNER_READ OWNER_EXECUTE  # 755
        COMPONENT ubse_sdk
)

install(TARGETS ubse_sdk_static
        ARCHIVE DESTINATION lib64    # 静态库文件 (.a)
        PERMISSIONS OWNER_READ OWNER_WRITE
        COMPONENT ubse_sdk     # 组件名：ubse_sdk_static_lib
)
