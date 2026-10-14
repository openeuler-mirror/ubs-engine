/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// SSU 模块虚机化特性 IT(模式分流集成测试):
// UbseSsuControllerModule 在 vm_mode 配置下的真实 Initialize/Start/Stop 分流行为。
//
// 进程内集成方式:
// - 向 UbseContext 注入真实 UbseApiServerModule(未 Start,handler 挂 pendingHandlers_ 预注册队列)
//   与 UbseTaskExecutorModule,使 IPC dispatcher 注册链路真实可达;
// - 通过 pendingHandlers_ 内省断言 VM 模式 IPC 子集 / host 模式全量注册(排除集合);
// - 配置项(vm_mode/enable_tls/vsockServerPort)以 mockcpp 模拟(缓存文件路径已固定,不支持配置)。
// vsock 通道依赖 vsock_sim(LD_PRELOAD)重定向到 TCP loopback。

#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "config/ubse_conf.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"
#include "ssu_virt_it_vsock_client.h"
#include "ubse_api_server_module.h"
#include "ubse_com.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_serial_util.h"
#include "ubse_service_registry.h"
#include "ubse_ssu_controller_module.h"
#include "ubse_ssu_ipc_handler.h"
#include "ubse_ssu_service_imp.h"
#include "ubse_ssu_vm_cache.h"
#include "ubse_thread_pool_module.h"
#include "src/framework/vscok/ubse_vsock_message.h"

namespace ubse::ssu::virt::it {

using namespace ubse::ssu::message; // 引入 operator<</>>(UbseSerialization, UbseSsuAllocResult)
using ubse::plugin::service::ssu::UbseSsuAllocResult;
using ubse::plugin::service::ssu::UbseSsuNameSpaceInfo;
using ubse::ssu::controller::UbseSsuControllerModule;
using ubse::ssu::controller::UbseSsuVmCache;
using ubse::ssu::service::UbseSsuServiceImp;
using ubse::plugin::service::ssu::UbseSsuService;
using api::server::UbseApiServerModule;
using ubse::task_executor::UbseTaskExecutorModule;

namespace {

// IT 端口探测基址(与 TestSsuVirtPush 同一策略:pid 派生,冲突后移)
constexpr uint32_t IT_MODULE_VSOCK_PORT_BASE = 30000;
constexpr int IT_MAX_PORT_ATTEMPTS = 32;
// vsock 回环 CID 与探测超时
constexpr uint32_t IT_VSOCK_CID_LOCAL = VMADDR_CID_LOCAL;
constexpr int IT_WAIT_CONNECT_TIMEOUT_MS = 2000;

// VM 模式 IPC 注册子集:查询(4)+ attach/detach(6)
const std::set<uint16_t> g_vmModeIpcOps = {
    UBSE_IPC_SSU_LIST_ALLOC_INFO,        UBSE_IPC_SSU_GET_ALLOC_INFO_BY_NAME, UBSE_IPC_SSU_GET_NS_STATS,
    UBSE_IPC_SSU_GET_CONNECT_INFO,       UBSE_IPC_SSU_ATTACH_SPACE,           UBSE_IPC_SSU_DETACH_SPACE,
    UBSE_IPC_SSU_ATTACH_LINEAR_SPACE,    UBSE_IPC_SSU_DETACH_LINEAR_SPACE,    UBSE_IPC_SSU_ATTACH_STRIPED_SPACE,
    UBSE_IPC_SSU_DETACH_STRIPED_SPACE,
};
// host 模式全量:VM 子集 + 权限(2)+ alloc/free(2)+ FE 设备(3)
const std::set<uint16_t> g_hostFullIpcOps = {
    UBSE_IPC_SSU_ADD_ACCESS_PERMISSION, UBSE_IPC_SSU_REMOVE_ACCESS_PERMISSION, UBSE_IPC_SSU_ALLOC_SPACE,
    UBSE_IPC_SSU_FREE_SPACE,            UBSE_IPC_SSU_GET_FE_DEVICE_LIST,        UBSE_IPC_SSU_FE_DEVICE_ALLOC,
    UBSE_IPC_SSU_FE_DEVICE_FREE,
};

// 配置 mock 状态(每个用例独立设置)
bool g_itVmMode = false;

uint32_t MockModuleUbseGetBool(const std::string &section, const std::string &key, bool &val)
{
    (void)section;
    if (key == std::string(ubse::ssu::controller::UBSE_SSU_CONF_VM_MODE_KEY)) {
        val = g_itVmMode;
    } else {
        val = false;
    }
    return UBSE_OK;
}

uint32_t MockModuleUbseGetStr(const std::string &section, const std::string &key, std::string &val)
{
    (void)section;
    (void)key;
    val.clear();
    return UBSE_OK;
}

uint32_t MockModuleUbseGetUInt(const std::string &section, const std::string &key, uint32_t &val)
{
    (void)section;
    if (key == std::string(ubse::ssu::controller::UBSE_SSU_CONF_VSOCK_PORT_KEY)) {
        val = IT_MODULE_VSOCK_PORT_BASE + static_cast<uint32_t>(getpid() % 512) * IT_MAX_PORT_ATTEMPTS;
    } else {
        val = 0;
    }
    return UBSE_OK;
}

// 安装配置 mock(精确函数指针类型匹配,与 UT SetupVmConfigMocks 同一约定)
void SetupModuleConfigMocks()
{
    MOCKER_CPP(static_cast<uint32_t (*)(const std::string &, const std::string &, bool &)>(
        &ubse::config::UbseGetBool))
        .stubs()
        .will(invoke(MockModuleUbseGetBool));
    MOCKER_CPP(static_cast<uint32_t (*)(const std::string &, const std::string &, std::string &)>(
        &ubse::config::UbseGetStr))
        .stubs()
        .will(invoke(MockModuleUbseGetStr));
    MOCKER_CPP(static_cast<uint32_t (*)(const std::string &, const std::string &, uint32_t &)>(
        &ubse::config::UbseGetUInt))
        .stubs()
        .will(invoke(MockModuleUbseGetUInt));
}

// RPC 端点注册依赖跨节点通信框架(需集群建链),进程内 IT 以 mockcpp 模拟 Build 成功,
// host 模式 Start 的其余链路(timer/HTTP/IPC 注册)保持真实执行
std::shared_ptr<ubse::com::UbseRpcEndpoint> MockBuildRpcEndpoint(uint16_t moduleCode, uint16_t opCode,
                                                                 const ubse::com::UbseRpcMessageReceiver &receiver)
{
    (void)moduleCode;
    (void)opCode;
    (void)receiver;
    return std::make_shared<ubse::com::UbseRpcEndpoint>();
}

void SetupRpcEndpointMock()
{
    MOCKER_CPP(&ubse::com::UbseRpcEndpointFactory::Build)
        .stubs()
        .will(invoke(MockBuildRpcEndpoint));
}
} // namespace

class TestSsuVirtModuleMode : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;

protected:
    // 注入真实 ApiServer/TaskExecutor 模块到 UbseContext(IPC 注册链路依赖)
    void InjectContextModules();
    // 移除注入的上下文模块(避免跨用例泄漏 pendingHandlers_)
    void RemoveContextModules();
    // 收集 ApiServer 模块预注册队列中的 SSU opCode 集合
    std::set<uint16_t> RegisteredSsuOpCodes() const;

    std::shared_ptr<UbseApiServerModule> apiServer_;
    std::shared_ptr<UbseTaskExecutorModule> taskExecutorModule_;
    std::unique_ptr<UbseSsuControllerModule> module_;
    std::string cacheFilePath_;
};

void TestSsuVirtModuleMode::SetUp()
{
    // 单例状态复位,避免跨用例污染
    auto &service = UbseSsuServiceImp::GetInstance();
    service.SetVmCache(nullptr);
    service.SetVmMode(false);

    cacheFilePath_ = "/tmp/ssu_virt_module_it_" + std::to_string(static_cast<long>(getpid())) + ".json";
    (void)::remove(cacheFilePath_.c_str());
    SetupModuleConfigMocks();
}

void TestSsuVirtModuleMode::TearDown()
{
    if (module_ != nullptr) {
        module_->Stop();
        module_->UnInitialize();
        module_.reset();
    }
    if (taskExecutorModule_ != nullptr) {
        // 显式移除用例创建的执行器,停止工作线程后再销毁模块实例
        taskExecutorModule_->Remove("ubseSsuVmFlush");
        taskExecutorModule_->Remove("ubseSsuController");
    }
    RemoveContextModules();
    auto &service = UbseSsuServiceImp::GetInstance();
    service.SetVmCache(nullptr);
    service.SetVmMode(false);
    (void)::remove(cacheFilePath_.c_str());
    GlobalMockObject::verify();
}

void TestSsuVirtModuleMode::InjectContextModules()
{
    apiServer_ = std::make_shared<UbseApiServerModule>();
    taskExecutorModule_ = std::make_shared<UbseTaskExecutorModule>();
    auto &ctx = ubse::context::UbseContext::GetInstance();
    // 进程内 IT 注入:IT 目标以 -fno-access-control 编译,可写私有 moduleMap_
    ctx.moduleMap_[UbseApiServerModule::kModuleName] = apiServer_;
    ctx.moduleMap_[UbseTaskExecutorModule::kModuleName] = taskExecutorModule_;
}

void TestSsuVirtModuleMode::RemoveContextModules()
{
    auto &ctx = ubse::context::UbseContext::GetInstance();
    ctx.moduleMap_.erase(UbseApiServerModule::kModuleName);
    ctx.moduleMap_.erase(UbseTaskExecutorModule::kModuleName);
    apiServer_.reset();
    taskExecutorModule_.reset();
}

std::set<uint16_t> TestSsuVirtModuleMode::RegisteredSsuOpCodes() const
{
    std::set<uint16_t> ops;
    for (const auto &reg : apiServer_->pendingHandlers_) {
        EXPECT_EQ(0x0007, reg.moduleCode); // ubse_ipc_module_code_t::UBSE_SSU
        ops.insert(reg.opCode);
    }
    return ops;
}

/*
 * 用例描述:vm_mode=true 时模块 Initialize/Start 完成分流,所有 VM 组件就绪,
 *           Stop 后 vsock server 停止且缓存落盘(含 LoadFromDisk 恢复链路)
 * 测试步骤:
 * 1、预写一份合法缓存文件,注入上下文模块,mock vm_mode=true
 * 2、调用模块 Initialize(缓存路径重定向到用例临时路径并 LoadFromDisk 恢复)+ Start
 * 3、推送一条分配信息(端到端就绪验证)后调用 Stop
 * 预期结果:
 * 1、Initialize/Start 返回 UBSE_OK;ServiceImp 进入 VM 模式且缓存预写数据被 LoadFromDisk 恢复
 * 2、vsock server 监听中(WAITING_CONNECT),推送可收到 ACK,服务已注册到服务注册表
 * 3、IPC 仅注册查询/attach/detach 子集;Stop 后缓存文件存在且服务已注销
 */
TEST_F(TestSsuVirtModuleMode, ModuleInit_Start_VmMode)
{
    // 预写合法缓存文件,验证 Initialize 阶段 LoadFromDisk 真实恢复
    UbseSsuVmCache seedCache;
    seedCache.SetCacheFilePath(cacheFilePath_);
    UbseSsuAllocResult seeded{};
    seeded.name = "it_seed_alloc";
    seeded.strategy = ubse::plugin::service::ssu::UbseSsuAllocStrategy::NORMAL;
    seedCache.UpdateAllocInfo(seeded);
    ASSERT_TRUE(seedCache.FlushToDisk());

    g_itVmMode = true;
    InjectContextModules();
    module_ = std::make_unique<UbseSsuControllerModule>();
    ASSERT_EQ(UBSE_OK, module_->Initialize());
    // 缓存文件路径已固定为 /var/lib/ubse/ssu_vm_cache.json(不支持配置),IT 借助
    // -fno-access-control 将模块缓存重定向到用例临时路径,显式触发 LoadFromDisk 验证恢复链路
    module_->vmCache_->SetCacheFilePath(cacheFilePath_);
    ASSERT_TRUE(module_->vmCache_->LoadFromDisk());
    ASSERT_EQ(UBSE_OK, module_->Start());

    // VM 组件就绪
    EXPECT_TRUE(UbseSsuServiceImp::GetInstance().IsVmMode());
    EXPECT_NE(nullptr, module_->vmCache_.get());
    EXPECT_NE(nullptr, module_->vsockServer_.get());
    EXPECT_EQ(ubse::vsock::UbseVsockConnState::WAITING_CONNECT,
              module_->vsockServer_->GetConnectionStatus());
    // 重启恢复语义:未重连前即可查询上次落盘数据
    UbseSsuAllocResult restored{};
    EXPECT_EQ(UBSE_OK, module_->vmCache_->GetAllocInfo("it_seed_alloc", restored));
    EXPECT_EQ("it_seed_alloc", restored.name);
    // 服务已注册
    EXPECT_TRUE(ubse::service::UbseServiceRegistry::GetInstance().HasService(UbseSsuService::kServiceName));
    // IPC 子集注册(精确集合相等,同时覆盖"包含查询/attach/detach"与"排除 alloc/free/perm/FE")
    EXPECT_EQ(g_vmModeIpcOps, RegisteredSsuOpCodes());

    // 端到端就绪:mock ub-device-manager 推送可达
    ItVsockPushClient client;
    ASSERT_TRUE(client.Connect(IT_VSOCK_CID_LOCAL,
                               IT_MODULE_VSOCK_PORT_BASE + static_cast<uint32_t>(getpid() % 512) * IT_MAX_PORT_ATTEMPTS));
    ubse::serial::UbseSerialization ser;
    UbseSsuAllocResult pushed{};
    pushed.name = "it_module_vm_push";
    pushed.strategy = ubse::plugin::service::ssu::UbseSsuAllocStrategy::NORMAL;
    pushed.nameSpaceList.push_back(ubse::plugin::service::ssu::UbseSsuNameSpaceInfo{});
    pushed.nameSpaceList[0].lbaFormat = ubse::plugin::service::ssu::UbseSsuLBAFormat::LBA_FORMAT_512;
    ser << pushed;
    ASSERT_TRUE(ser.Check());
    auto *buf = ser.GetBuffer();
    std::vector<uint8_t> body(buf, buf + ser.GetLength());
    ASSERT_TRUE(client.SendPush(ubse::vsock::UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO, body, 7001));
    uint32_t statusCode = 0xFFFFFFFF;
    uint64_t respRequestId = 0;
    std::vector<uint8_t> respBody;
    ASSERT_TRUE(client.RecvResponse(statusCode, respRequestId, respBody));
    EXPECT_EQ(0u, statusCode);

    // Stop:server 停止 + 最终落盘
    module_->Stop();
    struct stat st {};
    EXPECT_EQ(0, stat(cacheFilePath_.c_str(), &st));
    EXPECT_FALSE(ubse::service::UbseServiceRegistry::GetInstance().HasService(UbseSsuService::kServiceName));
}

/*
 * 用例描述:vm_mode=false 时模块走 host 分支,现有行为不变
 *           (不创建 VM 组件,创建控制器执行器,IPC 全量注册)
 * 测试步骤:
 * 1、mock vm_mode=false,注入上下文模块
 * 2、调用模块 Initialize + Start
 * 3、调用 Stop
 * 预期结果:
 * 1、Initialize/Start 返回 UBSE_OK;ServiceImp 保持 host 模式,vmCache_/vsockServer_ 均为空
 * 2、"ubseSsuController" 执行器已创建,服务已注册,IPC 全量注册(17 个 opCode)
 * 3、Stop 后服务注销
 */
TEST_F(TestSsuVirtModuleMode, ModuleInit_Start_HostMode)
{
    g_itVmMode = false;
    InjectContextModules();
    // 跨节点 RPC 端点注册依赖集群建链,进程内以 mock 模拟(见 MockBuildRpcEndpoint 注释)
    SetupRpcEndpointMock();
    module_ = std::make_unique<UbseSsuControllerModule>();
    ASSERT_EQ(UBSE_OK, module_->Initialize());
    ASSERT_EQ(UBSE_OK, module_->Start());

    // host 分支:不创建 VM 组件
    EXPECT_FALSE(UbseSsuServiceImp::GetInstance().IsVmMode());
    EXPECT_EQ(nullptr, module_->vmCache_.get());
    EXPECT_EQ(nullptr, module_->vsockServer_.get());
    // host 组件就绪:控制器执行器与服务注册
    EXPECT_NE(nullptr, taskExecutorModule_->Get("ubseSsuController").Get());
    EXPECT_TRUE(ubse::service::UbseServiceRegistry::GetInstance().HasService(UbseSsuService::kServiceName));
    // IPC 全量注册
    std::set<uint16_t> expected = g_vmModeIpcOps;
    expected.insert(g_hostFullIpcOps.begin(), g_hostFullIpcOps.end());
    EXPECT_EQ(expected, RegisteredSsuOpCodes());

    module_->Stop();
    EXPECT_FALSE(ubse::service::UbseServiceRegistry::GetInstance().HasService(UbseSsuService::kServiceName));
}

/*
 * 用例描述:VM 模式 IPC dispatcher 仅注册查询/attach/detach handler 子集
 * 测试步骤:
 * 1、注入 fresh ApiServer 模块
 * 2、调用 ipc::RegisterVmModeDispatcher()
 * 3、断言注册集合
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、注册集合与 VM 子集精确相等(ListAllocInfo/GetAllocInfoByName/GetNsStats/GetConnectInfo
 *    + Attach/Detach 6 个)
 * 3、不含 AllocSpace/FreeSpace/AddAccessPermission/RemoveAccessPermission/FE 设备 opCode
 */
TEST_F(TestSsuVirtModuleMode, RegisterIpc_VmMode_Subset)
{
    InjectContextModules();
    ASSERT_EQ(UBSE_OK, ubse::ssu::ipc::RegisterVmModeDispatcher());
    EXPECT_EQ(g_vmModeIpcOps, RegisteredSsuOpCodes());
}

/*
 * 用例描述:host 模式 IPC dispatcher 全量注册所有 SSU handler
 * 测试步骤:
 * 1、注入 fresh ApiServer 模块
 * 2、调用 ipc::RegisterSdkDispatcher()
 * 3、断言注册集合
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、注册集合为全部 17 个 SSU opCode(含 alloc/free/权限/FE 设备)
 */
TEST_F(TestSsuVirtModuleMode, RegisterIpc_HostMode_FullSet)
{
    InjectContextModules();
    ASSERT_EQ(UBSE_OK, ubse::ssu::ipc::RegisterSdkDispatcher());
    std::set<uint16_t> expected = g_vmModeIpcOps;
    expected.insert(g_hostFullIpcOps.begin(), g_hostFullIpcOps.end());
    EXPECT_EQ(expected, RegisteredSsuOpCodes());
}

} // namespace ubse::ssu::virt::it
