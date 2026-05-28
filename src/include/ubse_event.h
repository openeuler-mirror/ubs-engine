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

#ifndef UBSE_EVENT_H
#define UBSE_EVENT_H
#include <cstdint>
#include <functional>
#include <string>

namespace ubse::event {

enum UbseEventPriority
{
    HIGH,
    MEDIUM,
    LOW
};
/**
 * 事件响应处理函数原型,其中eventMessage为发布方定义的额外信息，处理过程中需要与发布方数据格式一致,EventMessage由框架保证内存释放
 * 事件响应处理函数仅支持普通函数。
 */
using UbseEventHandler = std::function<uint32_t(std::string& eventId, std::string& eventMessage)>;
/**
 * @brief 订阅事件,框架处理模块会按照优先级的方式回调处理函数，保证高优先级处理得到优先调用
 * @param [in] eventId: 事件ID
 * @param [in] priority: 事件响应处理的优先级
 * @param [in] registerFunc: 事件响应处理函数
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseSubEvent(std::string& eventId, UbseEventHandler registerFunc, UbseEventPriority priority = MEDIUM);

/**
 * @brief 取消订阅事件
 * @param [in] eventId: 事件ID
 * @param [in] registerFunc: 事件响应处理函数
 * @return NA
 */
uint32_t UbseUnSubEvent(std::string& eventId, UbseEventHandler registerFunc);

/**
 * @brief 发布事件,允许发布方携带额外的信息
 * @param [in] eventId: 事件ID
 * @param [in] eventMessage: 事件信息，发布方执行完pub即可释放内存；信息格式由发布方定义，响应方需要对应处理
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbsePubEvent(const std::string& eventId, std::string& eventMessage);
} // namespace ubse::event

#endif // UBSE_EVENT_H
