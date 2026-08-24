// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "sentry_observer.h"
#include <dlfcn.h>
#include <unistd.h>
#include <cstdint>
#include <sys/socket.h>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_pointer_process.h"
#include "ubse_ras.h"
#include "ubse_timer.h"
#include "securec.h"
#include "src/framework/misc/ubse_os_util.h"
#include "src/framework/security/ubse_security_module.h"
#include "src/ras/ubse_ras_handler.h"
#include "sys_sentry_module.h"
#include "trace_context.h"

using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::security;
using namespace ubse::common::def;
using namespace ubse::task_executor;

namespace syssentry {
UBSE_DEFINE_THIS_MODULE("ubse");

using LibPtr = void*;

const std::vector<int> ALARM_EVENT_LIST = {ubse::ras::ALARM_REBOOT_EVENT, ubse::ras::ALARM_OOM_EVENT,
                                           ubse::ras::ALARM_PANIC_EVENT, ubse::ras::ALARM_KERNEL_REBOOT_EVENT,
                                           ubse::ras::ALARM_MEM_FAULT};
const int SLEEP_TIME = 5; // 注册alarm失败休眠时间
LibPtr xalarmHandle = nullptr;
const std::string UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME = "UbseRasConfigSysSentryTimer";
const uint32_t UBSE_RAS_CONFIG_SYSSENTRY_TIMER_INTERVAL = NO_5;

const std::string UBSE_RAS_QUERY_MSG_MONITOR_TIMER_NAME = "UbseRasQueryMsgMonitorTimer";
const uint32_t UBSE_RAS_QUERY_MSG_MONITOR_TIMER_INTERVAL = NO_2;

UbseRasObserver& UbseRasObserver::GetInstance()
{
    static UbseRasObserver instance;
    return instance;
}

UbseRasObserver::UbseRasObserver() = default;

UbseRasObserver::~UbseRasObserver() = default;

LibPtr GetFuncByDlsym(void* handle, const std::string& symbo)
{
    dlerror();
    auto func = dlsym(handle, symbo.c_str());
    auto dlsymError = dlerror();
    if (dlsymError) {
        UBSE_LOG_WARN << "Fail to find symbol: " << symbo << ", dlsym error: " << dlsymError;
        return nullptr;
    }
    return func;
}

UbseResult UbseRasObserver::Init()
{
    xalarmHandle = dlopen("libxalarm.so", RTLD_LAZY); // 生命周期与进程一致
    if (xalarmHandle == nullptr) {
        UBSE_LOG_WARN << "xalarm is not registered, ret: dlopen libxalarm.so fail";
        return UBSE_ERROR;
    }
    xalarmGetEventFunc = reinterpret_cast<XalarmGetEventFunc>(
        GetFuncByDlsym(xalarmHandle, "xalarm_get_event")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (xalarmGetEventFunc == nullptr) {
        UBSE_LOG_WARN << "xalarm is not registered, ret: xalarm_get_event is null";
        dlclose(xalarmHandle);
        xalarmHandle = nullptr;
        return UBSE_ERROR;
    }
    xalarmRegisterFunc = reinterpret_cast<XalarmRegisterFunc>(
        GetFuncByDlsym(xalarmHandle, "xalarm_register_event")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (xalarmRegisterFunc == nullptr) {
        dlclose(xalarmHandle);
        xalarmHandle = nullptr;
        return UBSE_ERROR;
    }
    xalarmUnRegisterFunc = reinterpret_cast<XalarmUnRegisterFunc>(
        GetFuncByDlsym(xalarmHandle, "xalarm_unregister_event")); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (xalarmUnRegisterFunc == nullptr) {
        dlclose(xalarmHandle);
        xalarmHandle = nullptr;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseRasObserver::UbseQueryMsgMonitorTimerRun()
{
    // 调用sentryctl命令查询sentry_msg_monitor 运行状态
    std::string command = "sentryctl status sentry_msg_monitor 2>/dev/null";
    std::string result;
    auto ret = ubse::utils::UbseOsUtil::Exec(command, result);
    // sysSentry 服务未启动时也会返回错误，因此命令执行失败后视作sysSentry服务异常
    if (ret != UBSE_OK || result.find("RUNNING") == std::string::npos) {
        this->isSentryMsgMonitorRunning = false;
    } else {
        this->isSentryMsgMonitorRunning = true;
    }
}

std::atomic<uint32_t> g_queryCount = 0;
void UbseRasObserver::RegQueryMsgMonitorTimer()
{
    auto ret = ubse::timer::UbseTimerHandlerRegister(
        UBSE_RAS_QUERY_MSG_MONITOR_TIMER_NAME,
        []() -> UbseResult {
            if (g_globalStop) {
                UBSE_LOG_INFO << "detect global stop flag, will stop query msg monitor timer";
                ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_QUERY_MSG_MONITOR_TIMER_NAME);
                return UBSE_OK;
            }
            // 为避免任务队列溢出，允许有两个任务。非严格限制，由于时序，允许超过NO_2个任务
            if (g_queryCount.load() >= NO_2) {
                return UBSE_OK;
            }
            auto taskModule = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
            if (taskModule == nullptr) {
                UBSE_LOG_ERROR << "Get task module failed";
                return UBSE_ERROR;
            }
            UbseTaskExecutorPtr executor = taskModule->Get(UBSE_RAS_TASK_NAME);
            if (executor == nullptr) {
                UBSE_LOG_WARN << "executor empty, skip query msg monitor";
                return UBSE_OK;
            }
            g_queryCount.fetch_add(1);
            bool isAdded = executor->Execute([]() -> void {
                UbseRasObserver::GetInstance().UbseQueryMsgMonitorTimerRun();
                g_queryCount.fetch_sub(1);
            });
            if (!isAdded) {
                // 添加任务失败，减1
                g_queryCount.fetch_sub(1);
            }
            return UBSE_OK;
        },
        UBSE_RAS_QUERY_MSG_MONITOR_TIMER_INTERVAL);
}

UbseResult UbseRasObserver::Start()
{
    auto ret = Init();
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Init alarm func failed, " << FormatRetCode(ret);
        return UBSE_OK;
    }

    if (!worker) {
        worker = SafeMakeUnique<std::thread>(&UbseRasObserver::SentryEventListen, this);
    }
    return UBSE_OK;
}

void UbseRasObserver::Stop()
{
    stopThread = true;
    InvalidateSysSentryConfig();
    UnregisterConfigRetryTimer();
    ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_QUERY_MSG_MONITOR_TIMER_NAME);

    // 仅断开底层 fd 唤醒阻塞在 xalarmGetEventFunc 的工作线程，不释放 alarm_register，
    // 避免工作线程持有的快照成为悬垂指针；结构体由工作线程退出时统一释放。
    {
        std::lock_guard<std::mutex> lock(registerMtx_);
        if (registerInfo_ != nullptr && registerInfo_->register_fd >= 0) {
            // register_fd 为 socket 时 shutdown 可立即唤醒阻塞中的 recv，且 fd 号延迟到
            // 工作线程最终 UnRegisterXalarm 时才释放，避免 close 后 fd 号被其他线程复用；
            // 非 socket（如 FIFO）时 shutdown 失败，退化为 close 并标记 -1，
            // 防止工作线程最终注销时对已释放的 fd 号二次 close。
            if (shutdown(registerInfo_->register_fd, SHUT_RDWR) != 0) {
                close(registerInfo_->register_fd);
                registerInfo_->register_fd = -1; // 标记已断开，防止工作线程重用
            }
        }
    }

    if (worker && worker->joinable()) {
        worker->join();
        worker.reset();
    }
}

void LogValidFaultMsg(const std::string& invalidStr)
{
    std::ostringstream oss;
    for (auto ch : invalidStr) {
        if (std::isalnum(ch) || ubse::ras::IsAllowedSpecialChar(ch)) {
            oss << ch;
        } else {
            // 非法字符用[-]标记位置
            oss << "[-]";
        }
    }
    UBSE_LOG_INFO << "The fault msg is " << oss.str() << ", which the invalid position is marked by '[-]'";
}

void UbseRasObserver::SentryEventListen()
{
    {
        // 首次注册同样纳入 registerMtx_ 保护，避免快速停止场景下与 Stop 并发读写 registerInfo_
        std::lock_guard<std::mutex> lock(registerMtx_);
        RegisterSentryEvent(&registerInfo_);
    }
    if (registerInfo_ == nullptr) {
        UBSE_LOG_WARN << "xalarm is not registered, ret: register info is null. ";
        return;
    }

    while (!stopThread) {
        auto* msg = new (std::nothrow) alarm_msg();
        if (msg == nullptr) {
            UBSE_LOG_ERROR << "New alarm msg failed. ";
            continue;
        }
        // 阻塞性等待，有故障事件返回。取快照后释放锁，避免长时间持锁阻塞 Stop 断开。
        struct alarm_register* snapshot = nullptr;
        {
            std::lock_guard<std::mutex> lock(registerMtx_);
            snapshot = registerInfo_;
        }
        if (snapshot == nullptr || stopThread) {
            // 注册句柄已失效（重注册失败），或通过 while 判断后 Stop 恰好已断开 fd，
            // 直接退出循环，避免带着无效 fd 进入长时间阻塞等待
            SafeDelete(msg);
            break;
        }
        // 阻塞性等待，有故障事件返回
        auto ret = xalarmGetEventFunc(msg, snapshot);
        if (ret < 0) {
            UBSE_LOG_WARN << "Failed to get msg. ErrorCode=" << ret;
            const bool disconnected = ret == -ENOTCONN || ret == -EBADF;
            if (disconnected) {
                InvalidateSysSentryConfig();
            }
            if (!stopThread) {   // 停止中不再重注册，避免与 Stop 竞争 registerInfo_
                std::lock_guard<std::mutex> lock(registerMtx_);
                RegisterSentryEvent(&registerInfo_);
            }
            if (disconnected) {
                UBSE_LOG_INFO << "Re-config sentry";
                UbseConfigSysSentryWithRetry();
                ubse::ras::UbseRasHandler::GetInstance().ClearAllMsgId();
            }
            SafeDelete(msg);
            continue;
        }
        std::string pucParasStr{msg->pucParas};
        // 信息可能是json类型，去除换行符
        std::replace_if(
            pucParasStr.begin(), pucParasStr.end(), [](char c) { return c == '\n' || c == '\r'; }, ' ');
        if (ubse::ras::hasInvalidChars(pucParasStr)) {
            SafeDelete(msg);
            UBSE_LOG_WARN << "Invalid fault info, contains invalid characters";
            LogValidFaultMsg(pucParasStr);
            continue;
        }
        UBSE_LOG_INFO << "Alarm from sysSentry, id=" << msg->usAlarmId << ", param=" << pucParasStr;
        auto result = ubse::ras::UbseRasHandler::GetInstance().NodeFaultHandle(msg);
        UBSE_LOG_INFO << "Ubse ras handler finished, " << FormatRetCode(result);
        SafeDelete(msg);
        TraceContext::Clear();
    }
    {
        std::lock_guard<std::mutex> lock(registerMtx_);
        UnRegisterXalarm(&registerInfo_);
    }
}

void UbseRasObserver::RegisterSentryEvent(alarm_register** registerInfo)
{
    UBSE_LOG_INFO << "Register sentry event start";
    if (registerInfo == nullptr) {
        UBSE_LOG_ERROR << "register info ptr is nullptr. ";
        return;
    }
    struct alarm_subscription_info idFilter {
    };
    for (size_t i = 0; i < ALARM_EVENT_LIST.size(); i++) {
        idFilter.id_list[i] = ALARM_EVENT_LIST[i];
    }
    idFilter.len = static_cast<unsigned int>(ALARM_EVENT_LIST.size());
    std::vector<__u32> caps{CAP_CHOWN, CAP_FOWNER, CAP_DAC_OVERRIDE};
    auto result = ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    if (result != UBSE_OK) {
        UBSE_LOG_ERROR << "Modify Effective Capabilities failed. ";
        return;
    }
    if (*registerInfo != nullptr) {
        xalarmUnRegisterFunc(registerInfo);
        *registerInfo = nullptr;
    }
    auto ret = xalarmRegisterFunc(registerInfo, idFilter);
    while (ret < 0 && !stopThread) {
        sleep(SLEEP_TIME);
        if (xalarmHandle == nullptr || xalarmRegisterFunc == nullptr) {
            break;
        }
        ret = xalarmRegisterFunc(registerInfo, idFilter);
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    if (ret < 0) {
        *registerInfo = nullptr;
        UBSE_LOG_WARN << "Xalarm register failed, ret=" << ret;
    }
    UBSE_LOG_INFO << "Register sentry event end";
}

void UbseRasObserver::UnRegisterXalarm(alarm_register** registerInfo)
{
    if (registerInfo != nullptr && *registerInfo != nullptr) {
        xalarmUnRegisterFunc(registerInfo);
        *registerInfo = nullptr;
    }
}

bool UbseRasObserver::IsConfigSuccess() const
{
    return configSysSentrySuccess;
}

bool UbseRasObserver::IsSentryMsgMonitorRunning() const
{
    return isSentryMsgMonitorRunning;
}

UbseResult UbseRasObserver::UbseConfigSysSentry()
{
    std::unique_lock<std::mutex> mtx(configSysSentryMtx);
    // 模块停止后不再配置 sysSentry，按幂等成功返回。
    if (stopThread || g_globalStop) {
        UBSE_LOG_INFO << "Skip configuring sysSentry because the module is stopping";
        return UBSE_OK;
    }
    // 已完成完整配置时无需重复执行 sentryctl。
    if (configSysSentrySuccess) {
        UBSE_LOG_INFO << "SysSentry has been configured";
        return UBSE_OK;
    }
    // 完整配置任一命令失败时返回错误，由上层统一注册重试定时器。
    if (SetSysSentryFaultReporter() != UBSE_OK) {
        UBSE_LOG_DEBUG << "Fail to set fault reporter";
        return UBSE_RAS_ERROR_SET_SENTRY_REPORTER;
    }
    UBSE_LOG_INFO << "Success to config sysSentry";
    configSysSentrySuccess = true;
    return UBSE_OK;
}

void UbseRasObserver::UbseConfigSysSentryTimerRun()
{
    bool stopRetry = false;
    bool configReady = false;
    {
        std::lock_guard<std::mutex> lock(configSysSentryMtx);
        stopRetry = stopThread || g_globalStop;
        configReady = configSysSentrySuccess;
    }
    if (stopRetry) {
        // 停止阶段立即注销重试定时器，不再执行配置任务。
        UBSE_LOG_INFO << "Stop retry config sysSentry";
        UnregisterConfigRetryTimer();
        return;
    }
    if (configReady) {
        RunBroadcastDomainRefresh();
    } else if (UbseConfigSysSentry() != UBSE_OK) {
        // 完整配置仍未成功时保留定时器，等待下一次重试。
        UBSE_LOG_DEBUG << "Unable to config sysSentry, will retry in " << UBSE_RAS_CONFIG_SYSSENTRY_TIMER_INTERVAL
                       << "s";
        return;
    }
    UnregisterConfigRetryTimerIfIdle();
}

UbseResult UbseRasObserver::UbseConfigSysSentryWithRetry()
{
    UBSE_LOG_INFO << "Start to config sysSentry";
    bool stopRetry = false;
    {
        std::lock_guard<std::mutex> lock(configSysSentryMtx);
        stopRetry = stopThread || g_globalStop;
    }
    if (stopRetry) {
        // 停止阶段注销已有重试定时器，不再启动新的配置。
        UBSE_LOG_INFO << "Stop configuring sysSentry because the module is stopping";
        UnregisterConfigRetryTimer();
        return UBSE_OK;
    }
    // 完整配置成功时无需创建重试定时器。
    if (UbseConfigSysSentry() == UBSE_OK) {
        UBSE_LOG_INFO << "Success to config sysSentry";
        return UBSE_OK;
    }
    // 配置sysSentry可能耗时过长，此处需要再校验一遍
    {
        std::lock_guard<std::mutex> lock(configSysSentryMtx);
        // 配置执行期间收到停止信号时，不再注册后续重试。
        if (stopThread || g_globalStop) {
            UBSE_LOG_INFO << "Stop configuring sysSentry because the module started stopping during configuration";
            return UBSE_OK;
        }
        // 配置期间可能由并发重试完成，此时无需重复注册定时器。
        if (configSysSentrySuccess) {
            UBSE_LOG_DEBUG << "Skip registering sysSentry retry because configuration has completed";
            return UBSE_OK;
        }
    }
    UBSE_LOG_WARN << "Failed to config sysSentry, will retry later";
    return RegisterConfigRetryTimer();
}

uint32_t HandleSysSentryNodeDiscoveryEvent(std::string& eventId, std::string& eventMessage)
{
    (void)eventId;
    (void)eventMessage;
    return UbseRasObserver::GetInstance().RequestBroadcastDomainRefresh();
}

void UbseRasObserver::InvalidateSysSentryConfig()
{
    std::lock_guard<std::mutex> lock(configSysSentryMtx);
    configSysSentrySuccess = false;
    broadcastRefreshPending = false;
    ResetSysSentryFaultBroadcastDomain();
    UBSE_LOG_DEBUG << "Invalidated sysSentry configuration state and broadcast peer snapshot";
}

UbseResult UbseRasObserver::RequestBroadcastDomainRefresh()
{
    {
        std::lock_guard<std::mutex> lock(configSysSentryMtx);
        // 停止阶段不再接收新的广播域刷新请求。
        if (stopThread || g_globalStop) {
            UBSE_LOG_INFO << "Skip requesting sysSentry broadcast refresh because the module is stopping";
            return UBSE_OK;
        }
        // 完整配置会读取最新 NodeMgr 数据，未完成前无需保留动态刷新请求。
        if (!configSysSentrySuccess) {
            UBSE_LOG_DEBUG << "Skip requesting sysSentry broadcast refresh because full configuration is not ready";
            return UBSE_OK;
        }
        broadcastRefreshPending = true;
    }
    return ScheduleBroadcastDomainRefresh();
}

UbseResult UbseRasObserver::ScheduleBroadcastDomainRefresh()
{
    {
        std::lock_guard<std::mutex> lock(configSysSentryMtx);
        // 停止阶段不再向 RAS 执行器投递刷新任务。
        if (stopThread || g_globalStop) {
            UBSE_LOG_INFO << "Skip scheduling sysSentry broadcast refresh because the module is stopping";
            return UBSE_OK;
        }
        // 完整配置失效后应由完整配置流程恢复，不单独刷新广播域。
        if (!configSysSentrySuccess) {
            UBSE_LOG_DEBUG << "Skip scheduling sysSentry broadcast refresh because full configuration is not ready";
            return UBSE_OK;
        }
        // 没有待处理变化时不投递空刷新任务。
        if (!broadcastRefreshPending) {
            UBSE_LOG_DEBUG << "Skip scheduling sysSentry broadcast refresh because no refresh is pending";
            return UBSE_OK;
        }
    }

    auto taskModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    // 缺少任务模块时无法投递刷新任务，保留 pending 并尝试注册统一重试。
    if (taskModule == nullptr) {
        UBSE_LOG_ERROR << "Get task module failed while scheduling broadcast refresh";
        (void)RegisterConfigRetryTimer();
        return UBSE_ERROR;
    }
    auto executor = taskModule->Get(UBSE_RAS_TASK_NAME);
    // RAS 执行器尚不可用时不丢弃刷新请求，交由统一定时器重试。
    if (executor == nullptr) {
        UBSE_LOG_WARN << "RAS executor is unavailable while scheduling broadcast refresh";
        (void)RegisterConfigRetryTimer();
        return UBSE_ERROR;
    }
    // 投递失败时 pending 仍为 true，后续定时器会再次尝试刷新。
    if (!executor->Execute([]() -> void { UbseRasObserver::GetInstance().RunBroadcastDomainRefresh(); })) {
        UBSE_LOG_WARN << "Failed to enqueue sysSentry broadcast refresh";
        (void)RegisterConfigRetryTimer();
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseRasObserver::RunBroadcastDomainRefresh()
{
    UbseResult ret = UBSE_OK;
    {
        std::lock_guard<std::mutex> lock(configSysSentryMtx);
        // 已进入停止阶段的排队任务直接退出，不再访问 sysSentry。
        if (stopThread || g_globalStop) {
            UBSE_LOG_INFO << "Skip running sysSentry broadcast refresh because the module is stopping";
            return;
        }
        // Sentry 断连会使完整配置失效，广播域由后续完整配置恢复。
        if (!configSysSentrySuccess) {
            UBSE_LOG_DEBUG << "Skip running sysSentry broadcast refresh because full configuration is not ready";
            return;
        }
        // 重复排队的任务没有 pending 时无需执行命令。
        if (!broadcastRefreshPending) {
            UBSE_LOG_DEBUG << "Skip running sysSentry broadcast refresh because no refresh is pending";
            return;
        }
        broadcastRefreshPending = false;
        ret = RefreshSysSentryFaultBroadcastDomain();
        // 仅在运行态且完整配置仍有效时保留失败请求；停止或断连由生命周期流程重新配置。
        if (ret != UBSE_OK && !stopThread && !g_globalStop && configSysSentrySuccess) {
            broadcastRefreshPending = true;
        }
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to refresh sysSentry broadcast domain, " << FormatRetCode(ret);
        (void)RegisterConfigRetryTimer();
    }
}

UbseResult UbseRasObserver::RegisterConfigRetryTimer()
{
    auto taskModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    // 缺少任务模块时无法创建可执行的重试回调，直接返回注册失败。
    if (taskModule == nullptr) {
        UBSE_LOG_ERROR << "Get task module failed while registering sysSentry retry";
        return UBSE_ERROR;
    }
    auto executor = taskModule->Get(UBSE_RAS_TASK_NAME);
    // 重试回调依赖 RAS 执行器，执行器不存在时不能注册无效定时器。
    if (executor == nullptr) {
        UBSE_LOG_WARN << "RAS executor is unavailable while registering sysSentry retry";
        return UBSE_ERROR;
    }
    std::lock_guard<std::mutex> lock(configSysSentryMtx);
    // 停止阶段不再注册新的重试定时器。
    if (stopThread || g_globalStop) {
        UBSE_LOG_INFO << "Skip registering sysSentry retry timer because the module is stopping";
        return UBSE_OK;
    }
    // 回调只使用预先获取的执行器，避免定时器注销等待回调时与配置锁互相等待。
    auto ret = ubse::timer::UbseTimerHandlerRegister(
        UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME,
        [executor]() -> UbseResult {
            auto& observer = UbseRasObserver::GetInstance();
            // Stop 已负责注销定时器；并发触发的尾部回调只需幂等退出。
            if (observer.stopThread || g_globalStop) {
                UBSE_LOG_INFO << "Skip enqueueing sysSentry retry because the module is stopping";
                return UBSE_OK;
            }
            // 队列拒绝任务时向 timer 框架返回失败，保留后续触发机会。
            if (!executor->Execute([]() -> void { UbseRasObserver::GetInstance().UbseConfigSysSentryTimerRun(); })) {
                UBSE_LOG_WARN << "Failed to enqueue sysSentry retry task";
                return UBSE_ERROR;
            }
            return UBSE_OK;
        },
        UBSE_RAS_CONFIG_SYSSENTRY_TIMER_INTERVAL);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Register timer failed, ret=" << FormatRetCode(ret);
    }
    return ret;
}

void UbseRasObserver::UnregisterConfigRetryTimer()
{
    std::lock_guard<std::mutex> lock(configSysSentryMtx);
    UBSE_LOG_DEBUG << "Unregister sysSentry retry timer";
    ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME);
}

void UbseRasObserver::UnregisterConfigRetryTimerIfIdle()
{
    std::lock_guard<std::mutex> lock(configSysSentryMtx);
    const bool stopping = stopThread || g_globalStop;
    // 完整配置仍未成功时保留定时器，继续重试完整配置。
    if (!stopping && !configSysSentrySuccess) {
        UBSE_LOG_DEBUG << "Keep sysSentry retry timer because full configuration is not ready";
        return;
    }
    // 仍有动态变化待处理时保留定时器，继续重试广播域刷新。
    if (!stopping && broadcastRefreshPending) {
        UBSE_LOG_DEBUG << "Keep sysSentry retry timer because a broadcast refresh is pending";
        return;
    }
    // 停止或所有配置均已收敛时，定时器不再需要继续触发。
    UBSE_LOG_DEBUG << "Unregister sysSentry retry timer because no retry work remains";
    ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME);
}

} // namespace syssentry
