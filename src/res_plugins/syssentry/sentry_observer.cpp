// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "sentry_observer.h"
#include <dlfcn.h>
#include <unistd.h>
#include "ubse_thread_pool_module.h"
#include "securec.h"
#include "src/ras/ubse_ras_handler.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_pointer_process.h"

using namespace ubse::log;

namespace syssentry {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_RAS)

using LibPtr = void *;

const std::vector<int> ALARM_EVENT_LIST = {ALARM_REBOOT_EVENT, ALARM_OOM_EVENT, ALARM_PANIC_EVENT,
                                           ALARM_KERNEL_REBOOT_EVENT};
const int SLEEP_TIME = 5; // 注册alarm失败休眠时间
LibPtr xalarmHandle = nullptr;

UbseRasObserver UbseRasObserver::instance;

UbseRasObserver &UbseRasObserver::GetInstance()
{
    return instance;
}

UbseRasObserver::UbseRasObserver() {}

UbseRasObserver::~UbseRasObserver() {}

LibPtr GetFuncByDlsym(void *handle, const std::string &symbo)
{
    dlerror();
    auto func = dlsym(handle, symbo.c_str());
    auto dlsymError = dlerror();
    if (dlsymError) {
        UBSE_LOG_WARN << "[sentry] Fail to find symbol: " << symbo << ", dlsym error: " << dlsymError;
        return nullptr;
    }
    return func;
}

UbseResult UbseRasObserver::Init()
{
    xalarmHandle = dlopen("libxalarm.so", RTLD_LAZY);
    if (xalarmHandle == nullptr) {
        UBSE_LOG_WARN << "[sentry] xalarm is not registered, ret: dlopen libxalarm.so fail";
        return UBSE_ERROR;
    }
    xalarmGetEventFunc = (XalarmGetEventFunc)GetFuncByDlsym(xalarmHandle, "xalarm_get_event");
    if (xalarmGetEventFunc == nullptr) {
        UBSE_LOG_WARN << "[sentry] xalarm is not registered, ret: xalarm_get_event is null";
        dlclose(xalarmHandle);
        return UBSE_ERROR;
    }
    xalarmRegisterFunc = (XalarmRegisterFunc)GetFuncByDlsym(xalarmHandle, "[sentry] xalarm_register_event");
    if (xalarmRegisterFunc == nullptr) {
        dlclose(xalarmHandle);
        return UBSE_ERROR;
    }
    xalarmUnRegisterFunc = (XalarmUnRegisterFunc)GetFuncByDlsym(xalarmHandle, "[sentry] xalarm_unregister_event");
    if (xalarmUnRegisterFunc == nullptr) {
        dlclose(xalarmHandle);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseRasObserver::Start()
{
    auto ret = Init();
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[sentry] Init alarm func failed, " << FormatRetCode(ret);
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
    if (worker && worker->joinable()) {
        worker->detach();
        // 确保释放线程资源
        worker.reset();
    }
    if (xalarmHandle != nullptr) {
        dlclose(xalarmHandle);
    }
}

alarm_msg *AlarmMsgCopy(alarm_msg *msg)
{
    if (msg == nullptr) {
        UBSE_LOG_ERROR << "msg is nullptr. ";
        return nullptr;
    }
    auto msgCpy = new (std::nothrow) alarm_msg();
    if (msgCpy == nullptr) {
        UBSE_LOG_ERROR << "[sentry] new msg failed. ";
        return nullptr;
    }
    auto ret = memcpy_s(msgCpy, sizeof(alarm_msg), msg, sizeof(alarm_msg));
    if (ret != EOK) {
        SafeDelete(msgCpy);
        UBSE_LOG_ERROR << "[sentry] Alarm msg copy failed, " << FormatRetCode(ret);
        return nullptr;
    }
    return msgCpy;
}

void UbseRasObserver::SentryEventListen()
{
    struct alarm_register *registerInfo = nullptr;
    RegisterSentryEvent(&registerInfo);
    if (registerInfo == nullptr) {
        UBSE_LOG_WARN << "[sentry] xalarm is not registered, ret: register info is null. ";
        return;
    }

    while (!stopThread) {
        auto *msg = new (std::nothrow) alarm_msg();
        if (msg == nullptr) {
            UBSE_LOG_ERROR << "[sentry] new alarm msg failed. ";
            continue;
        }
        // 阻塞性等待，有故障事件返回
        auto ret = xalarmGetEventFunc(msg, registerInfo);
        if (ret < 0) {
            RegisterSentryEvent(&registerInfo);
            UBSE_LOG_WARN << "[sentry] Failed to get msg. ErrorCode=" << ret;
            continue;
        }
        UBSE_LOG_INFO << "[sentry] Alarm from sysSentry, id=" << msg->usAlarmId << ", param=" << msg->pucParas;
        auto *msg_handle = AlarmMsgCopy(msg);
        std::thread([&]() {
            auto result = ubse::ras::UbseRasHandler::GetInstance().NodeFaultHandle(msg_handle);
            UBSE_LOG_DEBUG << "[sentry] Ubse ras handler finished, " << FormatRetCode(result);
            SafeDelete(msg_handle);
        }).detach();
        SafeDelete(msg);
    }
    UnRegisterXalarm(&registerInfo);
    SafeDelete(registerInfo);
}

void UbseRasObserver::RegisterSentryEvent(alarm_register **registerInfo)
{
    if (registerInfo == nullptr) {
        UBSE_LOG_ERROR << "register info ptr is nullptr. ";
        return;
    }
    struct alarm_subscription_info idFilter;
    for (size_t i = 0; i < ALARM_EVENT_LIST.size(); i++) {
        idFilter.id_list[i] = ALARM_EVENT_LIST[i];
    }
    idFilter.len = static_cast<unsigned int>(ALARM_EVENT_LIST.size());
    if (*registerInfo != nullptr) {
        xalarmUnRegisterFunc(*registerInfo);
    }
    auto ret = xalarmRegisterFunc(registerInfo, idFilter);
    while (ret < 0) {
        UBSE_LOG_WARN << "[sentry] xalarm register failed, ErrorCode=" << ret;
        sleep(SLEEP_TIME);
        ret = xalarmRegisterFunc(registerInfo, idFilter);
    }
}

void UbseRasObserver::UnRegisterXalarm(alarm_register **registerInfo)
{
    if (registerInfo != nullptr && *registerInfo != nullptr) {
        xalarmUnRegisterFunc(*registerInfo);
        *registerInfo = nullptr;
    }
}
} // namespace syssentry