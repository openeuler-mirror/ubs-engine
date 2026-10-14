# SSU 模块虚机化特性 IT:vsock 端到端推送(mock ub-device-manager)
# 进程内集成测试:真实 UbseVsockServer(AF_VSOCK 回环)+ 真实推送 handler +
# 真实 UbseSsuVmCache/UbseSsuServiceImp VM 分支;硬件适配器以函数指针 mock 替代。
# 复用 add_it_scene 注册场景二进制,纳入统一 IT 执行入口(bash build.sh it)。

add_it_scene(ssu_virt
    LINK_LIBS
        ssu_plugin
        ubse_ssu
        ubse_vsock_server
        ubse_cert # TLS 冒烟用例客户端直连 OpenSSL(传递 OpenSSL::SSL/Crypto 头文件与链接)
        ubse_config
        ubse_ssu_ipc
        ubse_context
        ubse_com
        ubse_serial
        ubse_election
        ubse_timer
        ubse_utils
        ubse_vip
        ubse_mti
        # 静态库符号被后续依赖(如 node_controller → syssentry)反向引用,
        # 按 UT 约定以 whole-archive 链接,避免链接顺序导致符号丢失
        -Wl,--whole-archive
        ubse_plugin
        ubse_vip
        syssentry
        -Wl,--no-whole-archive
        mockcpp
)

# 访问被测模块私有成员(UbseSsuControllerModule 注入 / UbseSsuAdapterImpl 函数指针),
# 与 test/UT/controllers/ssu 的 UT 约定一致;
# whole-archive 库与 ubse_it_infra 传递闭包重复包含同一归档,与 ubse_it_daemon 同一约定允许多重定义
target_compile_options(${CMAKE_PROJECT_NAME}_ssu_virt_it PRIVATE -fno-access-control -w)
target_link_options(${CMAKE_PROJECT_NAME}_ssu_virt_it PRIVATE -Wl,--allow-multiple-definition)

# 模块分流 IT 依赖的头文件目录(ApiServer 模块 / IPC 公共定义与 UDS 服务)
target_include_directories(${CMAKE_PROJECT_NAME}_ssu_virt_it PRIVATE
    ${CMAKE_SOURCE_DIR}/src/api_server
    ${CMAKE_SOURCE_DIR}/src/framework/ipc/include
    ${CMAKE_SOURCE_DIR}/src/framework/ipc/server
    ${CMAKE_SOURCE_DIR}/src/framework/ipc
)

# =====================================================
# vsock 本机仿真层(LD_PRELOAD):
# 无 vsock_loopback 内核模块的环境(如 WSL2)下,将 AF_VSOCK 重定向到 TCP loopback,
# 使本场景 IT 用例可真实执行。仅影响运行时 socket 域,不修改产品与测试代码。
# =====================================================
add_library(vsock_sim SHARED EXCLUDE_FROM_ALL vsock_sim/vsock_sim.cpp)
set_target_properties(vsock_sim PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
target_link_libraries(vsock_sim PRIVATE ${CMAKE_DL_LIBS})

# 本地运行入口:LD_PRELOAD 仿真层后执行 IT 二进制
# 用法:bash build.sh 后执行 cmake --build <build-dir> --target run_ssu_virt_it_local
add_custom_target(run_ssu_virt_it_local
    COMMAND ${CMAKE_COMMAND} -E env
        LD_PRELOAD=$<TARGET_FILE:vsock_sim>
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}_ssu_virt_it>
    DEPENDS vsock_sim ${CMAKE_PROJECT_NAME}_ssu_virt_it
    COMMENT "Run ssu virt IT with vsock TCP-loopback simulation (LD_PRELOAD)"
    VERBATIM
)
