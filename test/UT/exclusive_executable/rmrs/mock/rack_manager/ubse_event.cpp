/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_event.h"
#include <thread>
#include <utility>
#include "cassert"
#include "iostream"

namespace ubse::event {
UbseEventHandler g_InitFunc = nullptr;
UbseEventHandler g_MemEvent = nullptr;
bool g_stopCollect = false;
/**
 * @brief 订阅事件,框架处理模块会按照优先级的方式回调处理函数，保证高优先级处理得到优先调用
 * @param[in] eventId: 事件ID
 * @param[in] priority: 事件响应处理的优先级
 * @param[in] registerFunc: 事件响应处理函数
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseSubEvent(std::string& eventId, UbseEventHandler registerFunc, UbseEventPriority priority)
{
    return 0;
}

/**
 * @brief 取消订阅事件
 * @param[in] eventId: 事件ID
 * @param[in] registerFunc: 事件响应处理函数
 * @return NA
 */
uint32_t UbseUnSubEvent(std::string& eventId, UbseEventHandler registerFunc)
{
    return 0;
}

/**
 * @brief 发布事件,允许发布方携带额外的信息
 * @param[in] eventId: 事件ID
 * @param[in] eventMessage: 事件信息，发布方执行完pub即可释放内存；信息格式由发布方定义，响应方需要对应处理
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbsePubEvent(const std::string& eventId, std::string& eventMessage)
{
    return 0;
}
} // namespace ubse::event