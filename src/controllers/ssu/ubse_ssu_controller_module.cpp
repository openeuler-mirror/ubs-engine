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

#include "ubse_ssu_controller_module.h"

#include <array>
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_service_registry.h"
#include "ubse_ssu_http_handler.h"
#include "ubse_ssu_ipc_handler.h"
#include "ubse_ssu_master_online_handler.h"
#include "ubse_ssu_rpc_processor.h"
#include "ubse_ssu_service_imp.h"
#include "ubse_thread_pool_module.h"
#include "config/ubse_conf.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::ssu::controller {
using namespace ubse::log;
using namespace ubse::module;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::task_executor;
using ubse::plugin::service::ssu::UbseSsuService;
using ubse::ssu::service::UbseSsuServiceImp;
using ubse::ssu::service::UbseSsuMasterOnlineHandler;

constexpr const char *UBSE_SSU_VM_FLUSH_EXECUTOR_NAME = "ubseSsuVmFlush";

// host 模式依赖保持不变；VM 模式在插件注册阶段返回空依赖集，避免依赖解析先于
// Initialize 中的 vm_mode 分流而误激活 node/election/com/vip。
static constexpr auto G_UBSE_SSU_HOST_DEPS = std::array<UbseOptionModule, 4>{
    UbseOptionModule::UbseNodeControllerModule,
    UbseOptionModule::UbseElectionModule,
    UbseOptionModule::UbseComModule,
    UbseOptionModule::UbseVipModule,
};

// vm_mode 判定的唯一入口:插件依赖声明(注册期)与模块 Initialize(运行态分流)必须共用
// 同一份读取逻辑,否则两处独立演进会出现「节/键/失败回落语义漂移」,进而导致依赖集与
// 运行态分裂(如依赖按 VM 裁剪、运行却按 host)。
// 插件 dlopen 发生在模块 Start 阶段(UbsePluginModule::Start),CORE 的 UbseConfModule 此时
// 已就绪,故两次调用取到同一配置值;readRet 回传 UbseGetBool 错误码,失败按 host 模式回落。
static bool IsSsuVmModeConfigured(uint32_t &readRet)
{
    bool vmMode = false;
    readRet = ubse::config::UbseGetBool(UBSE_SSU_CONF_SECTION, UBSE_SSU_CONF_VM_MODE_KEY, vmMode);
    return readRet == UBSE_OK && vmMode;
}

std::vector<UbseOptionModule> GetSsuModuleDependencies()
{
    uint32_t readRet = UBSE_OK;
    if (IsSsuVmModeConfigured(readRet)) {
        return {};
    }
    if (readRet != UBSE_OK) {
        // 注册期读不到配置属异常:依赖集只能按 host 回落,与运行态判定共用逻辑保证两侧一致
        UBSE_LOG_WARN << "UbseGetBool(" << UBSE_SSU_CONF_SECTION << "." << UBSE_SSU_CONF_VM_MODE_KEY
                      << ") failed during plugin registration, fallback to host dependencies, ret=" << readRet;
    }
    return {G_UBSE_SSU_HOST_DEPS.begin(), G_UBSE_SSU_HOST_DEPS.end()};
}

PLUGIN_MODULE_IMPL(UbseSsuControllerModule, GetSsuModuleDependencies());

UbseResult UbseSsuControllerModule::Initialize()
{
    // 读取 vm_mode 配置(单次读取,后续只读)
    auto cfgRet = ReadVmModeConfig(vmMode_);
    if (cfgRet != UBSE_OK) {
        UBSE_LOG_WARN << "Read vm_mode config failed, fallback to host mode, ret=" << cfgRet;
        vmMode_ = false;
    }

    if (vmMode_) {
        // VM 模式:不创建 task executor / 不注册 master online handler,
        // 仍创建 ServiceImp 单例,但裁剪其余 host 侧初始化
        auto vmRet = InitializeVmMode();
        if (vmRet != UBSE_OK) {
            UBSE_LOG_ERROR << "InitializeVmMode failed, ret=" << vmRet;
            ResetVmModeState();
            return vmRet;
        }
        UBSE_LOG_INFO << "UbseSsuControllerModule Initialize (VM mode)";
        return UBSE_OK;
    }

    // host 模式:以下逻辑保持现有行为不变
    auto executorModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (executorModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get task executor module";
        return UBSE_ERROR_NULLPTR;
    }
    // 创建SSU控制器任务执行器，数值参考mem 18个线程，队列容量1000
    auto ret = executorModule->Create("ubseSsuController", 18, 1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create ssu controller executor, " << FormatRetCode(ret);
        return ret;
    }

    // 注册主上线事件回调，选举完成确定主节点后启动collector
    // 替代原先在Start()中直接StartCollecting()的做法，避免选举未完成时collecting失败
    UbseSsuMasterOnlineHandler::Initial();

    UBSE_LOG_INFO << "UbseSsuControllerModule Initialize";
    return UBSE_OK;
}

void UbseSsuControllerModule::UnInitialize()
{
    if (vmMode_) {
        // VM 模式:反向释放 VM 侧资源(vsockServer_ / vmCache_ 由 Stop() 已停止)
        ResetVmModeState();
        return;
    }
    // host 模式:现有行为不变
    UbseSsuMasterOnlineHandler::Uninitial();
}

void UbseSsuControllerModule::ResetVmModeState()
{
    // 先销毁持有 controller 回调的 server，再撤销 ServiceImp 中的非拥有型缓存指针。
    vsockServer_.reset();

    if (vmFlushExecutorCreated_) {
        auto executorModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
        if (executorModule != nullptr) {
            executorModule->Remove(UBSE_SSU_VM_FLUSH_EXECUTOR_NAME);
        }
        vmFlushExecutorCreated_ = false;
    }
    vmFlushExecutor_ = nullptr;

    auto &service = UbseSsuServiceImp::GetInstance();
    service.SetVmCache(nullptr);
    service.SetVmMode(false);

    vmCache_.reset();
    vmMode_ = false;
}

UbseResult UbseSsuControllerModule::Start()
{
    UBSE_LOG_INFO << "UbseSsuControllerModule Start";

    if (vmMode_) {
        return StartVmMode();
    }

    // host 模式:以下逻辑保持现有行为不变

    // 将SSU服务单例注册到服务注册表，使北向接口可通过GetSsuService()获取
    // 使用空删除器：单例由GetInstance管理，不应被shared_ptr析构释放
    // GetInstance()返回单例引用，地址永远非空，无需做空指针检查
    ssuService_ = std::shared_ptr<UbseSsuService>(&UbseSsuServiceImp::GetInstance(), [](UbseSsuService *) {});
    ubse::service::UbseServiceRegistry::GetInstance().RegisterService<UbseSsuService>(ssuService_);

    // collector的启动和账本重建移至UbseSsuMasterOnlineHandler中，
    // 通过MASTER_ONLINE_NOTIFICATION事件触发，确保选举完成后再执行

    // 启动空VM BusInstance清理定时器
    auto ret = UbseSsuServiceImp::GetInstance().StartClearTimer();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to start clear timer, ret=" << ret;
        return ret;
    }

    // 注册SSU RPC Handler
    ret = UbseSsuRpcProcessor::RegHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register ssu rpc handlers, ret=" << ret;
        return ret;
    }

    // 注册北向HTTP接口路由
    ret = ubse::ssu::http_handler::RegisterSsuHttpHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register ssu http handlers, ret=" << ret;
        return ret;
    }
    ret = ipc::RegisterSdkDispatcher();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register ssu ipc handlers, ret=" << ret;
        return ret;
    }
    UBSE_LOG_INFO << "UbseSsuControllerModule Start success";
    return UBSE_OK;
}

void UbseSsuControllerModule::Stop()
{
    UBSE_LOG_INFO << "UbseSsuControllerModule Stop";

    if (vmMode_) {
        StopVmMode();
        return;
    }

    // host 模式:以下逻辑保持现有行为不变
    UbseSsuServiceImp::GetInstance().StopClearTimer();
    UbseSsuServiceImp::GetInstance().StopCollecting();

    // 从服务注册表注销
    if (ssuService_ != nullptr) {
        ubse::service::UbseServiceRegistry::GetInstance().UnRegisterService<UbseSsuService>(ssuService_);
        ssuService_.reset();
    }
}

// ============================================================================
// VM 模式分支
// 仅按照 UbseVsockServer / UbseSsuVmCache 公开接口调用,不修改二者实现
// ============================================================================

UbseResult UbseSsuControllerModule::ReadVmModeConfig(bool &vmMode)
{
    // 与 GetSsuModuleDependencies 共用 IsSsuVmModeConfigured:注册期依赖集与运行态分流
    // 取自同一判定入口,不会出现两侧逻辑漂移导致的分裂
    uint32_t readRet = UBSE_OK;
    vmMode = IsSsuVmModeConfigured(readRet);
    if (readRet != UBSE_OK) {
        UBSE_LOG_WARN << "UbseGetBool(" << UBSE_SSU_CONF_SECTION << "." << UBSE_SSU_CONF_VM_MODE_KEY
                      << ") failed, ret=" << readRet << ", fallback to false";
        vmMode = false;
        return readRet;
    }
    UBSE_LOG_INFO << "vm_mode config=" << (vmMode ? "true" : "false");
    return UBSE_OK;
}

UbseResult UbseSsuControllerModule::InitializeVmMode()
{
    // 1) 创建 ServiceImp 单例并标记 VM 模式
    //    SetVmMode 会让查询/attach/detach 走 VM 分支
    UbseSsuServiceImp::GetInstance().SetVmMode(true);

    // 2) 创建本地缓存并加载持久化文件
    //    文件不存在或损坏时返回 false,不阻塞启动,等待全量同步
    // make_unique 失败抛 std::bad_alloc 而非返回空,无需判空
    vmCache_ = std::make_unique<UbseSsuVmCache>();
    // 注入缓存到 ServiceImp,使查询/attach 走 VM 缓存
    UbseSsuServiceImp::GetInstance().SetVmCache(vmCache_.get());

    // 缓存文件路径固定为 /var/lib/ubse/ssu_vm_cache.json,不支持配置
    vmCache_->SetCacheFilePath(UBSE_SSU_DEFAULT_CACHE_FILE);
    if (!vmCache_->LoadFromDisk()) {
        UBSE_LOG_WARN << "LoadFromDisk returned false (file missing/corrupt), waiting for full sync";
    }

    // 2.1) 创建 VM 模式专用 flush 线程池(单条推送异步落盘)
    //      与 host 模式的 ubseSsuController executor 解耦,避免 VM 模式无主流程时占用过多线程
    //      executorModule 缺失时(如单元测试环境)降级:跳过异步落盘,仅依赖 FullSync/Stop 落盘
    auto executorModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (executorModule == nullptr) {
        UBSE_LOG_WARN << "Task executor module not available, vm async flush disabled (degraded)";
    } else {
        auto flushRet = executorModule->Create(UBSE_SSU_VM_FLUSH_EXECUTOR_NAME, 2, 256);
        if (flushRet != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to create ubseSsuVmFlush executor, ret=" << flushRet;
            return flushRet;
        }
        vmFlushExecutorCreated_ = true;
        vmFlushExecutor_ = executorModule->Get(UBSE_SSU_VM_FLUSH_EXECUTOR_NAME);
        if (vmFlushExecutor_ == nullptr) {
            UBSE_LOG_ERROR << "Failed to get ubseSsuVmFlush executor after create";
            return UBSE_ERROR_NULLPTR;
        }
    }

    // 3) 创建 vsockServer_(消费通用框架)
    auto vsockConfig = BuildVsockConfig();
    if (!vsockConfig.IsValid()) {
        UBSE_LOG_ERROR << "Invalid vsock config";
        return UBSE_ERR_INVALID_ARG;
    }
    // make_unique 失败抛 std::bad_alloc 而非返回空,无需判空
    vsockServer_ = std::make_unique<UbseVsockServer>(vsockConfig);

    // 4) 注册 VM 侧推送 handler(分配信息 / 连接信息 / 全量同步)
    auto regRet = RegisterVsockHandlers();
    if (regRet != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterVsockHandlers failed, ret=" << regRet;
        return regRet;
    }

    UBSE_LOG_INFO << "UbseSsuControllerModule InitializeVmMode success";
    return UBSE_OK;
}

ubse::vsock::UbseVsockConfig UbseSsuControllerModule::BuildVsockConfig()
{
    ubse::vsock::UbseVsockConfig cfg{};
    // SSU 推送要求按接收顺序处理；单工作线程保证同一会话消息串行执行。
    cfg.threadPoolSize = 1;
    // 端口:默认 UBSE_VSOCK_DEFAULT_PORT,可通过 vsockServerPort 配置覆盖
    uint32_t port = 0;
    if (ubse::config::UbseGetUInt(UBSE_SSU_CONF_SECTION, UBSE_SSU_CONF_VSOCK_PORT_KEY, port) == UBSE_OK &&
        port != 0) {
        cfg.port = port;
    }

    // enableTls:默认 true,可通过 enable_tls 关闭
    bool enableTls = true;
    if (ubse::config::UbseGetBool(UBSE_SSU_CONF_SECTION, UBSE_SSU_CONF_ENABLE_TLS_KEY, enableTls) == UBSE_OK) {
        cfg.enableTls = enableTls;
    }

    // 证书目录固定为 /var/lib/ubse/lcne_cert/(UbseVsockConfig 默认值),不支持配置
    return cfg;
}

UbseResult UbseSsuControllerModule::RegisterVsockHandlers()
{
    if (vsockServer_ == nullptr) {
        UBSE_LOG_ERROR << "vsockServer_ is null";
        return UBSE_ERROR_NULLPTR;
    }
    // UBSE_VSOCK_SSU_MODULE_CODE 与 IPC ubse_ipc_module_code_t::UBSE_SSU=0x0007 对齐
    // vsock 消息类型枚举详见 ubse_vsock_message.h
    auto ret = vsockServer_->RegisterHandler(
        UBSE_VSOCK_SSU_MODULE_CODE,
        static_cast<uint16_t>(ubse::vsock::UbseVsockMsgType::MSG_TYPE_PUSH_ALLOC_INFO),
        [this](const ubse::vsock::UbseIpcMessage &req, const ubse::vsock::UbseRequestContext &ctx) {
            return HandlePushAllocInfo(req, ctx);
        });
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = vsockServer_->RegisterHandler(
        UBSE_VSOCK_SSU_MODULE_CODE,
        static_cast<uint16_t>(ubse::vsock::UbseVsockMsgType::MSG_TYPE_PUSH_CONNECT_INFO),
        [this](const ubse::vsock::UbseIpcMessage &req, const ubse::vsock::UbseRequestContext &ctx) {
            return HandlePushConnectInfo(req, ctx);
        });
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = vsockServer_->RegisterHandler(
        UBSE_VSOCK_SSU_MODULE_CODE,
        static_cast<uint16_t>(ubse::vsock::UbseVsockMsgType::MSG_TYPE_FULL_SYNC_RESP),
        [this](const ubse::vsock::UbseIpcMessage &req, const ubse::vsock::UbseRequestContext &ctx) {
            return HandleFullSync(req, ctx);
        });
    return ret;
}

UbseResult UbseSsuControllerModule::StartVmMode()
{
    // 1) 将 ServiceImp 单例注册到服务注册表(空删除器,与 host 模式一致)
    ssuService_ = std::shared_ptr<UbseSsuService>(&UbseSsuServiceImp::GetInstance(), [](UbseSsuService *) {});
    ubse::service::UbseServiceRegistry::GetInstance().RegisterService<UbseSsuService>(ssuService_);

    // 2) 注册 IPC dispatcher 子集(仅查询/attach/detach)
    //    host 模式注册逻辑(RegisterSdkDispatcher)保持不变,VM 模式走 RegisterVmModeDispatcher
    auto ret = ipc::RegisterVmModeDispatcher();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register vm mode ipc handlers, ret=" << ret;
        return ret;
    }

    // 3) 启动 vsock server(监听 ub-device-manager 连接)
    if (vsockServer_ == nullptr) {
        UBSE_LOG_ERROR << "vsockServer_ is null during StartVmMode";
        return UBSE_ERROR_NULLPTR;
    }
    ret = vsockServer_->Start();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseVsockServer::Start failed, ret=" << ret;
        return ret;
    }

    UBSE_LOG_INFO << "UbseSsuControllerModule StartVmMode success";
    return UBSE_OK;
}

void UbseSsuControllerModule::StopVmMode()
{
    // 1) 停止 vsock server,确保不再有新推送触发异步落盘
    if (vsockServer_ != nullptr) {
        vsockServer_->Stop();
    }
    // 1.1) 停止 flush 线程池:Stop 会 join 工作线程,仅保证「在途」FlushToDisk 任务执行完毕,
    //      排队中未开始的任务被直接丢弃(不执行 lambda);不丢数据由紧随其后的第 2 步同步全量落盘兜底
    //      必须在 vsockServer_->Stop() 之后(无新任务投递)与最终 FlushToDisk 之前(避免竞争)
    if (vmFlushExecutor_ != nullptr) {
        vmFlushExecutor_->Stop();
    }
    // 2) 最终落盘 vmCache_(停机落盘)
    if (vmCache_ != nullptr) {
        if (!vmCache_->FlushToDisk()) {
            UBSE_LOG_WARN << "FlushToDisk failed during StopVmMode";
        }
    }
    // 3) 从服务注册表注销
    if (ssuService_ != nullptr) {
        ubse::service::UbseServiceRegistry::GetInstance().UnRegisterService<UbseSsuService>(ssuService_);
        ssuService_.reset();
    }
}

// ============================================================================
// vsock 推送处理函数
// 反序列化 → 更新缓存 → 响应 ACK
// 注意:本特性仅交付 handler 业务逻辑,连接/TLS/路由分发由通用框架负责
// ============================================================================

uint32_t UbseSsuControllerModule::HandlePushAllocInfo(const ubse::vsock::UbseIpcMessage &request,
                                                       const ubse::vsock::UbseRequestContext &context)
{
    if (vmCache_ == nullptr) {
        UBSE_LOG_ERROR << "HandlePushAllocInfo: vmCache_ is null";
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERROR_NULLPTR, context.requestId, emptyResp);
    }
    // 反序列化推送消息为 UbseSsuAllocResult
    // 消息体格式由 ub-device-manager 与本模块约定,采用与 SDK 一致的 pack/unpack
    if (request.buffer == nullptr || request.length == 0) {
        UBSE_LOG_ERROR << "HandlePushAllocInfo: empty payload";
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    // 单条分配信息推送:按 name 覆盖更新(幂等)
    // 反序列化失败由调用方响应 NACK
    UbseSsuAllocResult allocResult{};
    // 复用现有 IPC pack/unpack 模式,使用 UbseDeSerialization 反序列化分配信息
    // (协议按现有 IPC 序列化格式对齐)
    ubse::serial::UbseDeSerialization deserializer(request.buffer, request.length);
    using namespace ubse::ssu::message; // 引入 operator>>(UbseDeSerialization&, UbseSsuAllocResult&)
    deserializer >> allocResult;
    // 与缓存落盘不变式对齐:strategy/lbaFormat 非法不得写入,
    // 否则该条目落盘后重启时整个缓存文件会被拒绝并删除(已缓存分配信息全部丢失)
    if (!deserializer.Check() || !UbseSsuVmCache::IsAllocResultPersistable(allocResult)) {
        UBSE_LOG_ERROR << "HandlePushAllocInfo: deserialize failed or invalid alloc entry, nameEmpty="
                       << allocResult.name.empty() << ", length=" << request.length;
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    vmCache_->UpdateAllocInfo(allocResult);
    // 单条推送异步落盘,不阻塞 ACK;StopVmMode 会先停止 executor 并同步全量落盘,
    // 保证停机不丢(排队中被丢弃的异步任务由该同步落盘兜底)
    if (vmFlushExecutor_ != nullptr) {
        (void)vmFlushExecutor_->Execute([this]() {
            if (vmCache_ != nullptr) {
                (void)vmCache_->FlushToDisk();
            }
        });
    } else {
        UBSE_LOG_WARN << "HandlePushAllocInfo: vmFlushExecutor_ is null, skip async flush";
    }
    // ACK 响应:body 为空,仅 statusCode 表达成功(SendResponse 契约)
    ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
    return vsockServer_->SendResponse(UBSE_OK, context.requestId, emptyResp);
}

uint32_t UbseSsuControllerModule::HandlePushConnectInfo(const ubse::vsock::UbseIpcMessage &request,
                                                         const ubse::vsock::UbseRequestContext &context)
{
    if (vmCache_ == nullptr) {
        UBSE_LOG_ERROR << "HandlePushConnectInfo: vmCache_ is null";
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERROR_NULLPTR, context.requestId, emptyResp);
    }
    if (request.buffer == nullptr || request.length == 0) {
        UBSE_LOG_ERROR << "HandlePushConnectInfo: empty payload";
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    // 连接信息推送:按 name 覆盖更新
    // 反序列化格式:name(string) + count(uint32) + connectInfo[count]
    std::string name;
    std::vector<UbseSsuConnectInfo> connectInfoList;
    ubse::serial::UbseDeSerialization deserializer(request.buffer, request.length);
    using namespace ubse::ssu::message; // 引入 operator>>(UbseDeSerialization&, UbseSsuConnectInfo&)
    deserializer >> name;
    uint32_t count = 0;
    deserializer >> count;
    // 限制单次推送条数,防止恶意 size 字段导致内存放大
    constexpr uint32_t MAX_PUSH_CONNECT_INFO_NUM = 4096;
    if (count > MAX_PUSH_CONNECT_INFO_NUM) {
        UBSE_LOG_ERROR << "HandlePushConnectInfo: count exceeds limit, count=" << count;
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    for (uint32_t i = 0; i < count; ++i) {
        UbseSsuConnectInfo ci{};
        deserializer >> ci;
        if (!deserializer.Check()) {
            break;
        }
        connectInfoList.emplace_back(std::move(ci));
    }
    if (!deserializer.Check() || name.empty()) {
        UBSE_LOG_ERROR << "HandlePushConnectInfo: deserialize failed or name empty, length=" << request.length;
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    vmCache_->UpdateConnectInfo(name, connectInfoList);
    // 单条推送异步落盘
    if (vmFlushExecutor_ != nullptr) {
        (void)vmFlushExecutor_->Execute([this]() {
            if (vmCache_ != nullptr) {
                (void)vmCache_->FlushToDisk();
            }
        });
    } else {
        UBSE_LOG_WARN << "HandlePushConnectInfo: vmFlushExecutor_ is null, skip async flush";
    }
    // ACK 响应:body 为空,仅 statusCode 表达成功(SendResponse 契约)
    ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
    return vsockServer_->SendResponse(UBSE_OK, context.requestId, emptyResp);
}

uint32_t UbseSsuControllerModule::HandleFullSync(const ubse::vsock::UbseIpcMessage &request,
                                                  const ubse::vsock::UbseRequestContext &context)
{
    if (vmCache_ == nullptr) {
        UBSE_LOG_ERROR << "HandleFullSync: vmCache_ is null";
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERROR_NULLPTR, context.requestId, emptyResp);
    }
    if (request.buffer == nullptr || request.length == 0) {
        UBSE_LOG_ERROR << "HandleFullSync: empty payload";
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    // 全量同步消息格式:allocCount(uint32) + alloc[allocCount] + connectCount(uint32) + connect[connectCount]
    // 反序列化全部成功后再 Clear+批量更新,避免部分失败时缓存被清空导致查询为空
    ubse::serial::UbseDeSerialization deserializer(request.buffer, request.length);
    using namespace ubse::ssu::message; // 引入 UbseSsuAllocResult / UbseSsuConnectInfo 的 operator>>
    uint32_t allocCount = 0;
    deserializer >> allocCount;
    constexpr uint32_t MAX_FULL_SYNC_ALLOC_NUM = 8192;
    constexpr uint32_t MAX_FULL_SYNC_CONNECT_NUM = 8192;
    if (allocCount > MAX_FULL_SYNC_ALLOC_NUM) {
        UBSE_LOG_ERROR << "HandleFullSync: allocCount exceeds limit, count=" << allocCount;
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    std::vector<UbseSsuAllocResult> allocList;
    allocList.reserve(allocCount);
    // 业务校验失败标志:空 name 属业务非法但不会污染反序列化状态,需显式标记以阻止后续提交
    bool hasInvalidEntry = false;
    for (uint32_t i = 0; i < allocCount; ++i) {
        UbseSsuAllocResult ar{};
        deserializer >> ar;
        // 非法条目(name 空/strategy 或 lbaFormat 非法)会导致 LoadFromDisk 拒绝缓存文件
        // (重启丢数据),与单条推送同标准在 Clear 前整体拒绝,避免缓存被部分清空
        if (!deserializer.Check() || !UbseSsuVmCache::IsAllocResultPersistable(ar)) {
            UBSE_LOG_ERROR << "HandleFullSync: invalid alloc entry, i=" << i << ", nameEmpty=" << ar.name.empty();
            hasInvalidEntry = true;
            break;
        }
        allocList.emplace_back(std::move(ar));
    }
    // alloc 段已非法,整体必被拒绝,直接返回错误,无需继续解析 connect 段
    if (hasInvalidEntry) {
        UBSE_LOG_ERROR << "HandleFullSync: invalid alloc entry, reject without parsing connect section";
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    uint32_t connectCount = 0;
    if (deserializer.Check()) {
        deserializer >> connectCount;
    }
    if (connectCount > MAX_FULL_SYNC_CONNECT_NUM) {
        UBSE_LOG_ERROR << "HandleFullSync: connectCount exceeds limit, count=" << connectCount;
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    std::vector<std::pair<std::string, std::vector<UbseSsuConnectInfo>>> connectList;
    connectList.reserve(connectCount);
    for (uint32_t i = 0; i < connectCount; ++i) {
        std::string name;
        uint32_t ciCount = 0;
        deserializer >> name >> ciCount;
        // name 为空的条目会导致 LoadFromDisk 拒绝缓存文件(重启丢数据),与单条推送同标准整体拒绝
        if (!deserializer.Check() || name.empty() || ciCount > MAX_FULL_SYNC_CONNECT_NUM) {
            UBSE_LOG_ERROR << "HandleFullSync: invalid connect entry, i=" << i << ", nameEmpty=" << name.empty()
                           << ", ciCount=" << ciCount;
            hasInvalidEntry = true;
            break;
        }
        std::vector<UbseSsuConnectInfo> ciList;
        ciList.reserve(ciCount);
        for (uint32_t j = 0; j < ciCount; ++j) {
            UbseSsuConnectInfo ci{};
            deserializer >> ci;
            if (!deserializer.Check()) {
                break;
            }
            ciList.emplace_back(std::move(ci));
        }
        if (!deserializer.Check()) {
            break;
        }
        connectList.emplace_back(std::move(name), std::move(ciList));
    }
    if (!deserializer.Check() || hasInvalidEntry) {
        UBSE_LOG_ERROR << "HandleFullSync: deserialize failed or invalid entry, length=" << request.length;
        ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
        return vsockServer_->SendResponse(UBSE_ERR_INVALID_ARG, context.requestId, emptyResp);
    }
    // 全部反序列化成功后,在缓存内部一个写锁临界区内原子替换快照,
    // 避免并发查询看到 Clear 与逐条 Update 之间的中间状态。
    vmCache_->ReplaceSnapshot(allocList, connectList);
    // 全量同步走同步落盘
    if (!vmCache_->FlushToDisk()) {
        UBSE_LOG_WARN << "HandleFullSync: FlushToDisk failed";
    }
    // ACK 响应:body 为空,仅 statusCode 表达成功(SendResponse 契约)
    ubse::vsock::UbseIpcMessage emptyResp{nullptr, 0};
    return vsockServer_->SendResponse(UBSE_OK, context.requestId, emptyResp);
}
} // namespace ubse::ssu::controller
