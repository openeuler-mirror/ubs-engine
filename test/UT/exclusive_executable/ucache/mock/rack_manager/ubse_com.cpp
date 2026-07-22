/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_com.h"

namespace ubse::com {
/**
 * @brief 注册跨节点通信消息处理函数，适用于Master与Agent间的通信
 * @param[in] endpoint: 向Server端注册的ServiceHandler的标识（address不用传）
 * @param[in] handler: 消息处理函数
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseRegRpcService(const UbseComEndpoint& endpoint, const UbseComServiceHandler& handler)
{
    return 0;
}

/**
 * @brief 同步发消息
 * @param[in] endpoint: 目标端通信标识
 * @param[in] reqData: 要发送的数据,reqData申请的内存由调用方释放，框架不释放reqData的内存
 * @param[in，out] ctx: 上下文指针
 * @param[in] handler: 处理返回结果的处理函数,对端返回消息后，同步调用回调函数
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseRpcSend(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                     const UbseComRespHandler& handler)
{
    return 0;
}
} // namespace ubse::com