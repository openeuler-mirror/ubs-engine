# 需要在framework目录下面的目录，移动到framework下，然后从这里移除
add_subdirectory(framework)
add_subdirectory(message)
add_subdirectory(controllers)
add_subdirectory(adapter_plugins)
add_subdirectory(ras)
add_subdirectory(api_server)
add_subdirectory(controllers/mem)
add_subdirectory(sdk)
add_subdirectory(addons)

#################################################
#            Ubse 可执行程序               #
#################################################
add_executable(ubse
        ubse_main.cpp

        #        内部模块入口
        framework/security/ubse_security_module.cpp
        framework/com/ubse_com_module.cpp
        framework/storage/ubse_storage.cpp
        framework/event/ubse_event_module.cpp
        framework/log/ubse_logger_module.cpp
        framework/http/ubse_http_module.cpp
        framework/node_mgr/ubse_node_mgr_module.cpp
        framework/vip/ubse_vip_module.cpp
        controllers/urma/ubse_urma_controller_module.cpp
        controllers/urma/ubse_urma_controller.cpp
        framework/ha/ubse_election_module.cpp
        adapter_plugins/syssentry/sys_sentry_module.cpp
        ras/ubse_ras_module.cpp
        api_server/ubse_api_server_module.cpp
        adapter_plugins/urma_uvs/ubse_urma_uvs_module.cpp
        #       外部模块入口
        framework/security/ubse_security.cpp
        framework/log/ubse_logger.cpp
        framework/com/ubse_com.cpp
        framework/storage/ubse_storage.cpp
        framework/event/ubse_event.cpp
        framework/trace_context/trace_context.cpp
        api_server/ubse_api_server.cpp
)

target_include_directories(ubse PRIVATE ${CMAKE_BINARY_DIR}/rpc)

target_link_libraries(ubse PRIVATE
        ubse_security
        ubse_config
        ubse_com
        ubse_event
        ubse_storage
        ubse_log
        ubse_http
        ubse_vip
        ubse_election
        ubse_mti
        ubse_ras
        syssentry
        cert_api_static
        api_server
        rapidjson
        trace_context
        ubse_urma_uvs
        ubse_urma_controller
        ubse_ssu_controller
        ubse_node_mgr
)

# 手动导出注册模式下会被编译器优化的符号
target_link_libraries(ubse PRIVATE
        -Wl,--whole-archive
        ubse_context
        ubse_plugin
        ubse_mmi
        ubse_node_controller_register_config
        ubse_node_controller
        -Wl,--no-whole-archive
)
if (ASAN_BUILD)
    # 编译选项
    target_compile_options(ubse PRIVATE
            -fsanitize=address
            -fsanitize-recover=address
            -fno-omit-frame-pointer)

    # 给 ubse 单独设置链接选项
    target_link_options(ubse PRIVATE
            -fsanitize=address
            -fsanitize=leak
            -static-libasan
            -fsanitize-recover=address)
endif ()
target_compile_options(ubse PRIVATE
        # C 和 C++ 编译选项
        $<$<CONFIG:Debug>: ${DEBUG_FLAGS}>
        $<$<CONFIG:Release>: ${RELEASE_FLAGS}>
        $<$<CONFIG:RelWithDebInfo>: ${RELEASE_FLAGS}>
        $<$<CONFIG:MinSizeRel>: ${RELEASE_FLAGS}>
)

# 打印 ubse 的所有编译选项
get_target_property(UBSE_MANAGER_COMPILE_OPTIONS ubse COMPILE_OPTIONS)
message(DEBUG "ubse compile options: ${UBSE_MANAGER_COMPILE_OPTIONS}")

# 允许动态库访问可执行程序的全局符号
target_link_options(ubse PRIVATE -rdynamic -pie)
add_subdirectory(cli)
