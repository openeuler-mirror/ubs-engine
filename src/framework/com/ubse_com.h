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

#ifndef UBSE_COM_H
#define UBSE_COM_H
#include <vector>
#include <functional>
#include "ubse_def.h"

namespace ubse::com {
struct UbseComEndpoint {
    uint16_t moduleId;   // 模块Id
    uint32_t serviceId;  // server端的serviceId
    std::string address; // 通信地址
};

struct UbseComUdsInfo {
    uint32_t pid = 0; // 进程Id
    uint32_t uid = 0; // 用户Id
    uint32_t gid = 0; // 用户组Id
};

using UbseComServiceHandler = std::function<void(const UbseByteBuffer &req, UbseByteBuffer &resp)>;

/**
 * @brief 注册跨节点通信消息处理函数，适用于Master与Agent间的通信
 * @param endpoint [in] 向Server端注册的ServiceHandler的标识（address不用传）
 * @param handler [in] 消息处理函数
 * @return #UBSE_OK 0 正确
 * @return #UBSE_ERROR_NULLPTR 12 空指针
 * @return #UBSE_COM_ERROR_GET_ENGINE_FAIL 0x1001-100E 获取通信引擎失败
 * @return #UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE 0x1001-1003 非法操作码
 */
uint32_t UbseRegRpcService(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler);

using UbseComRespHandler = std::function<void(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)>;

/**
 * @brief 同步发消息
 * @param endpoint [in] 目标端通信标识
 * @param reqData [in] 要发送的数据,reqData申请的内存由调用方释放，框架不释放reqData的内存
 * @param ctx [in/out] 上下文指针
 * @param handler [in] 处理返回结果的处理函数,对端返回消息后，同步调用回调函数
 * @return #UBSE_OK 0 成功
 * @return #UBSE_ERROR_NULLPTR 12 空指针
 * @return #UBSE_ERROR_INVAL 14 失败
 * @return #UBSE_ERROR 1 失败
 * @return #UBSE_COM_ERROR_SYNC_CALL_FAIL 0x1001-100A 失败
 */
uint32_t UbseRpcSend(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData, void *ctx,
    const UbseComRespHandler &handler);

/**
 * @brief 异步发消息
 * @param endpoint [in] 目标端通信标识
 * @param reqData [in] 要发送的数据,reqData申请的内存由调用方释放，框架不释放reqData的内存
 * @param ctx [in/out] 上下文指针
 * @param handler [in] 处理返回结果的处理函数,对端返回消息后，异步调用回调函数
 * @return #UBSE_OK 0 成功
 * @return #UBSE_ERROR_NULLPTR 12 空指针
 * @return #UBSE_ERROR_INVAL 14 失败
 * @return #UBSE_ERROR 1 失败
 * @return #其他非零值 失败
 */
uint32_t UbseRpcAsyncSend(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData, void *ctx,
    const UbseComRespHandler &handler);
}
#endif // UBSE_COM_H
