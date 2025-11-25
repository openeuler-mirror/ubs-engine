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

#ifndef UBSE_EVENT_MODULE_H
#define UBSE_EVENT_MODULE_H

#include <memory>            // for unique_ptr
#include <string>            // for string

#include "ubse_common_def.h" // for UbseResult
#include "ubse_event.h"      // for UbseEventPriority, UbseEventHandler
#include "ubse_module.h"     // for UbseModule

namespace ubse::event {
using namespace ubse::module;

class UbseEventModule : public UbseModule {
public:
    UbseEventModule();
    ~UbseEventModule() override;

    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    /**
     * @brief
     * 订阅事件,框架处理模块会按照优先级的方式回调处理函数，保证高优先级处理得到优先调用
     * @param [in] eventId: 事件ID
     * @param [in] priority: 事件响应处理的优先级
     * @param [in] registerFunc: 事件响应处理函数
     * @return 成功返回0, 失败返回非0
     */
    UbseResult UbseSubEvent(const std::string &eventId, UbseEventHandler registerFunc,
        UbseEventPriority priority = MEDIUM);

    /**
     * @brief 发布事件,允许发布方携带额外的信息
     * @param [in] eventId: 事件ID
     * @param [in] eventMessage:
     * 事件信息，发布方执行完pub即可释放内存；信息格式由发布方定义，响应方需要对应处理
     * @return 成功返回0, 失败返回非0
     */
    UbseResult UbsePubEvent(const std::string &eventId, std::string &eventMessage);

    /**
     * @brief 取消订阅事件
     * @param [in] eventId: 事件ID
     * @param [in] registerFunc: 事件响应处理函数
     * @return NA
     */
    UbseResult UbseUnSubEvent(const std::string &eventId, UbseEventHandler registerFunc);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};
} // namespace ubse::event
#endif