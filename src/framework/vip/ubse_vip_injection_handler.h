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

#ifndef UBSE_VIP_INJECTION_HANDLER_H
#define UBSE_VIP_INJECTION_HANDLER_H

#include <cstdint>
#include "ubse_api_server.h"

namespace ubse::vip {

// 23 字节定长注入 payload,与 Go 侧 inject 包逐字节对齐(LittleEndian)
#pragma pack(push, 1)
struct UbseVipCfgPushPayload {
    uint32_t addr;      // IPv4,网络序值按 LittleEndian 序列化
    uint16_t port;      // [1024, 65535]
    uint8_t  prefix;    // 1~32
    char     iface[16]; // '\0' 结尾,最多 15 有效字符
};
#pragma pack(pop)
static_assert(sizeof(UbseVipCfgPushPayload) == 23, "payload 必须为 23 字节");

// UDS 注入 handler:反序列化后交给 UbseVipManager::InjectConfig 做统一的字段校验与注入。
// 权限校验在 server 权限层完成,本 handler 不含鉴权状态。
class UbseVipInjectionHandler {
public:
    UbseVipInjectionHandler() = default;

    uint32_t Handle(const api::server::UbseIpcMessage& msg, const api::server::UbseRequestContext& ctx);
};

} // namespace ubse::vip

#endif // UBSE_VIP_INJECTION_HANDLER_H
