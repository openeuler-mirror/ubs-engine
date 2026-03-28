// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "sentry_observer.h"
#include <dlfcn.h>
#include <unistd.h>
#include <cstdint>
#include "securec.h"
#include "src/framework/misc/ubse_os_util.h"
#include "src/framework/security/ubse_security_module.h"
#include "src/ras/ubse_ras_handler.h"
#include "sys_sentry_module.h"
#include "trace_context.h"
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_pointer_process.h"
#include "ubse_ras.h"
#include "ubse_timer.h"

using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::security;

namespace syssentry {
UBSE_DEFINE_THIS_MODULE("ubse");

using LibPtr = void *;

const std::vector<int> ALARM_EVENT_LIST = {ubse::ras::ALARM_REBOOT_EVENT, ubse::ras::ALARM_OOM_EVENT,
                                           ubse::ras::ALARM_PANIC_EVENT, ubse::ras::ALARM_KERNEL_REBOOT_EVENT,
                                           ubse::ras::ALARM_MEM_FAULT};
const int SLEEP_TIME = 5; // 注册alarm失败休眠时间
LibPtr xalarmHandle = nullptr;
const std::string UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME = "UbseRasConfigSysSentryTimer";
const uint32_t UBSE_RAS_CONFIG_SYSSENTRY_TIMER_INTERVAL = NO_5;

const std::string UBSE_RAS_QUERY_MSG_MONITOR_TIMER_NAME = "UbseRasQueryMsgMonitorTimer";
const uint32_t UBSE_RAS_QUERY_MSG_MONITOR_TIMER_INTERVAL = NO_2;

UbseRasObserver &UbseRasObserver::GetInstance()
{
    static UbseRasObserver instance;
    return instance;
}

UbseRasObserver::UbseRasObserver() = default;

UbseRasObserver::~UbseRasObserver() = default;

LibPtr GetFuncByDlsym(void *handle, const std::string &symbo)
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
    xalarmGetEventFunc = reinterpret_cast<XalarmGetEventFunc>(GetFuncByDlsym(xalarmHandle, "xalarm_get_event"));
    if (xalarmGetEventFunc == nullptr) {
        UBSE_LOG_WARN << "xalarm is not registered, ret: xalarm_get_event is null";
        dlclose(xalarmHandle);
        xalarmHandle = nullptr;
        return UBSE_ERROR;
    }
    xalarmRegisterFunc = reinterpret_cast<XalarmRegisterFunc>(GetFuncByDlsym(xalarmHandle, "xalarm_register_event"));
    if (xalarmRegisterFunc == nullptr) {
        dlclose(xalarmHandle);
        xalarmHandle = nullptr;
        return UBSE_ERROR;
    }
    xalarmUnRegisterFunc =
        reinterpret_cast<XalarmUnRegisterFunc>(GetFuncByDlsym(xalarmHandle, "xalarm_unregister_event"));
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
    ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_QUERY_MSG_MONITOR_TIMER_NAME);
    ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME);
    if (worker && worker->joinable()) {
        worker->detach();
        // 确保释放线程资源
        worker.reset();
    }
}

void LogValidFaultMsg(const std::string &invalidStr)
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
    struct alarm_register *registerInfo = nullptr;
    RegisterSentryEvent(&registerInfo);
    if (registerInfo == nullptr) {
        UBSE_LOG_WARN << "xalarm is not registered, ret: register info is null. ";
        return;
    }

    while (!stopThread) {
        auto *msg = new (std::nothrow) alarm_msg();
        if (msg == nullptr) {
            UBSE_LOG_ERROR << "New alarm msg failed. ";
            continue;
        }
        // 阻塞性等待，有故障事件返回
        auto ret = xalarmGetEventFunc(msg, registerInfo);
        if (ret < 0) {
            UBSE_LOG_WARN << "Failed to get msg. ErrorCode=" << ret;
            RegisterSentryEvent(&registerInfo);
            if (ret == -ENOTCONN || ret == -EBADF) {
                configSysSentrySuccess = false;
                UBSE_LOG_INFO << "Re-config sentry";
                UbseConfigSysSentryWithRetry();
                ubse::ras::UbseRasHandler::GetInstance().ClearAllMsgId(); // 内核重插，清除msgId
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
        UBSE_LOG_INFO << "Alarm from sysSentry, id=" << msg->usAlarmId << ", param=" << msg->pucParas;
        auto result = ubse::ras::UbseRasHandler::GetInstance().NodeFaultHandle(msg);
        UBSE_LOG_DEBUG << "Ubse ras handler finished, " << FormatRetCode(result);
        SafeDelete(msg);
        TraceContext::Clear();
    }
    UnRegisterXalarm(&registerInfo);
    SafeDelete(registerInfo);
}

void UbseRasObserver::RegisterSentryEvent(alarm_register **registerInfo)
{
    UBSE_LOG_INFO << "Register sentry event start";
    if (registerInfo == nullptr) {
        UBSE_LOG_ERROR << "register info ptr is nullptr. ";
        return;
    }
    struct alarm_subscription_info idFilter {};
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
        xalarmUnRegisterFunc(*registerInfo);
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

void UbseRasObserver::UnRegisterXalarm(alarm_register **registerInfo)
{
    if (registerInfo != nullptr && *registerInfo != nullptr) {
        xalarmUnRegisterFunc(*registerInfo);
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
    if (configSysSentrySuccess) {
        UBSE_LOG_INFO << "SysSentry has been configured";
        return UBSE_OK;
    }
    if (SetSysSentryFaultEventOn() != UBSE_OK) {
        UBSE_LOG_DEBUG << "Fail to enable fault event";
        return UBSE_RAS_ERROR_SET_FAULT_EVENT_ON;
    }
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
    if (UbseRasObserver::GetInstance().UbseConfigSysSentry() == UBSE_OK) {
        UBSE_LOG_INFO << "Success to config sysSentry";
        ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME);
        return;
    }
    if (stopThread) {
        UBSE_LOG_INFO << "Stop retry config sysSentry";
        ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME);
        return;
    }
    UBSE_LOG_DEBUG << "Unable to config sysSentry, will retry in " << UBSE_RAS_CONFIG_SYSSENTRY_TIMER_INTERVAL << "s";
}

UbseResult UbseRasObserver::UbseConfigSysSentryWithRetry()
{
    UBSE_LOG_INFO << "Start to config sysSentry";
    if (g_globalStop) {
        UBSE_LOG_INFO << "detect global stop flag, will stop config";
        ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME);
        return UBSE_OK;
    }
    if (UbseConfigSysSentry() == UBSE_OK) {
        UBSE_LOG_INFO << "Success to config sysSentry";
        return UBSE_OK;
    }
    // 配置sysSentry可能耗时过长，此处需要再校验一遍
    if (g_globalStop) {
        UBSE_LOG_INFO << "detect global stop flag, will stop config";
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "Failed to config sysSentry, will retry later";
    auto ret = ubse::timer::UbseTimerHandlerRegister(
        UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME,
        []() -> UbseResult {
            if (g_globalStop) {
                UBSE_LOG_INFO << "detect global stop flag, will stop config";
                ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_CONFIG_SYSSENTRY_TIMER_NAME);
                return UBSE_OK;
            }
            auto taskModule = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
            if (taskModule == nullptr) {
                UBSE_LOG_ERROR << "Get task module failed";
                return UBSE_ERROR;
            }
            UbseTaskExecutorPtr executor = taskModule->Get(UBSE_RAS_TASK_NAME);
            if (executor == nullptr) {
                UBSE_LOG_WARN << "executor empty, skip config sysSentry";
                return UBSE_OK;
            }
            executor->Execute([]() -> void { UbseRasObserver::GetInstance().UbseConfigSysSentryTimerRun(); });
            return UBSE_OK;
        },
        UBSE_RAS_CONFIG_SYSSENTRY_TIMER_INTERVAL);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Register timer failed, ret=" << FormatRetCode(ret);
    }
    return ret;
}
} // namespace syssentry