/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MANAGER_UBSE_RAS_OBSERVER_H
#define UBSE_MANAGER_UBSE_RAS_OBSERVER_H
#include <string>
#include "ubse_common_def.h"
#include "ubse_thread_pool.h"
#include "register_xalarm.h"

namespace syssentry {
using namespace ubse::common::def;
using namespace ubse::task_executor;

using XalarmRegisterFunc = int (*)(struct alarm_register **, struct alarm_subscription_info);
using XalarmGetEventFunc = int (*)(struct alarm_msg *, struct alarm_register *);
using XalarmUnRegisterFunc = void (*)(struct alarm_register *);

const std::string UBSE_RAS_CONFIG_SYSSENTRY_TASK_NAME = "UbseRasConfigSysSentryTaskName";
class UbseRasObserver {
public:
    /**
     * 获取UbseRasObserver单例
     * @return 返回UbseRasObserver单例
     */
    static UbseRasObserver &GetInstance();

    /**
     * 删除拷贝构造函数
     */
    UbseRasObserver(const UbseRasObserver &) = delete;

    /**
     * 删除赋值运算符
     * @return
     */
    UbseRasObserver &operator=(const UbseRasObserver &) = delete;

    /**
     * 启动UbseRas监听器
     * @return #UBSE_OK 0 启动成功
     */
    UbseResult Start();

    /**
     * 停止UbseRas监听器
     */
    void Stop();

    UbseResult UbseConfigSysSentry(); // 向sysSentry配置事件开关、用于广播故障的eid
    UbseResult UbseConfigSysSentryWithRetry(); // 向sysSentry配置事件开关、用于广播故障的eid，失败时注册定时器重试
    void UbseConfigSysSentryTimerRun(); // 定时重试配置sysSentry的回调
    bool IsConfigSuccess() const;

private:
    /*
     * 私有默认构造函数
     */
    UbseRasObserver();
    /*
     * 私有析构函数
     */
    ~UbseRasObserver();
    /*
     * sentry消息监听
     */
    void SentryEventListen();
    /*
     * 注册sentry事件
     */
    void RegisterSentryEvent(alarm_register **registerInfo);
    /*
     * 解注册sentry事件
     */
    void UnRegisterXalarm(alarm_register **registerInfo);

    UbseResult Init();

private:
    std::unique_ptr<std::thread> worker;
    std::atomic<bool> stopThread{};

    XalarmGetEventFunc xalarmGetEventFunc = nullptr;
    XalarmRegisterFunc xalarmRegisterFunc = nullptr;
    XalarmUnRegisterFunc xalarmUnRegisterFunc = nullptr;
    std::atomic<bool> configSysSentrySuccess = false;
    std::mutex configSysSentryMtx;
};
} // namespace syssentry
#endif // UBSE_MANAGER_UBSE_RAS_OBSERVER_H
