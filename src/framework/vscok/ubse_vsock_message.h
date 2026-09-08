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

#ifndef UBSE_VSOCK_MESSAGE_H
#define UBSE_VSOCK_MESSAGE_H

#include <cstdint>

#include "src/framework/ipc/ubse_ipc_message.h"
#include "src/include/ubse_api_server.h"

namespace ubse::vsock {
using api::server::UbseIpcMessage;
using api::server::UbseRequestContext;

// vsock 默认监听端口(VM 模式下由 ubse.conf [ubse.ssu] vsockServerPort 指定)
constexpr uint32_t UBSE_VSOCK_DEFAULT_PORT = 6174;

/**
 * @brief vsock 消息类型(opCode 层)
 *
 * 消息由 host 侧 ub-device-manager 发送,VM 侧仅接收与响应;
 * REQ_TYPE_SIGN / REQ_TYPE_VERIFY_AND_SIGN 为现有签名请求,保留兼容。
 */
enum class UbseVsockMsgType : uint32_t {
    REQ_TYPE_SIGN = 0x0010,              // 签名请求(现有,保留)
    REQ_TYPE_VERIFY_AND_SIGN = 0x0012,   // 验签+重签(现有,保留)
    MSG_TYPE_PUSH_ALLOC_INFO = 0x0100,   // 推送分配信息(VM 接收)
    MSG_TYPE_PUSH_CONNECT_INFO = 0x0101, // 推送连接信息(VM 接收)
    MSG_TYPE_QUERY_VM_STATUS = 0x0102,   // 查询 VM 内状态(VM 接收,可选)
    MSG_TYPE_LIFECYCLE_NOTIFY = 0x0103,  // 生命周期通知(VM 接收)
    MSG_TYPE_FULL_SYNC_RESP = 0x0105,    // 全量同步(VM 接收)
    MSG_TYPE_ACK = 0x01FF,               // 通用 ACK(VM 响应)
};

/**
 * @brief vsock 连接状态(VM 侧 server 视角)
 */
enum class UbseVsockConnState : uint8_t {
    WAITING_CONNECT = 0, // 监听中,等待 ub-device-manager 连接
    CONNECTED = 1,       // ub-device-manager 已连接
    DISCONNECTED = 2,    // 连接断开,等待重连
};
} // namespace ubse::vsock

#endif // UBSE_VSOCK_MESSAGE_H
