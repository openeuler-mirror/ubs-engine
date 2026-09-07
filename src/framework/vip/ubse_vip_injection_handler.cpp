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

#include "ubse_vip_injection_handler.h"

#include <cstring>
#include <string>
#include <securec.h>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_vip_manager.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::vip {
using namespace ubse::log;

uint32_t UbseVipInjectionHandler::Handle(const api::server::UbseIpcMessage& msg,
                                         const api::server::UbseRequestContext& ctx)
{
    // 调用方以宿主 ubse 用户运行,经 server 权限层(用户名权限点)校验;此处仅记录 uid 供审计。
    UBSE_LOG_DEBUG << "[VIP] injection from uid=" << ctx.clientInfo.uid;

    if (msg.buffer == nullptr) {
        UBSE_LOG_ERROR << "[VIP] invalid payload: null buffer";
        return UBSE_ERR_INVALID_ARG;
    }
    if (msg.length != sizeof(UbseVipCfgPushPayload)) {
        UBSE_LOG_ERROR << "[VIP] invalid payload length: " << msg.length;
        return UBSE_ERR_INVALID_ARG;
    }

    UbseVipCfgPushPayload payload{};
    // 长度已在上方校验为 sizeof(payload),destSize 一致,memcpy_s 不会截断
    if (memcpy_s(&payload, sizeof(payload), msg.buffer, msg.length) != EOK) {
        UBSE_LOG_ERROR << "[VIP] payload memcpy_s failed";
        return UBSE_ERR_INVALID_ARG;
    }

    // 字段校验统一收敛到 UbseVipManager::InjectConfig 单点,此处只做结构反序列化与转发。
    std::string iface(payload.iface, strnlen(payload.iface, sizeof(payload.iface)));
    auto ret = UbseVipManager::GetInstance().InjectConfig(payload.addr, payload.port, payload.prefix, iface);
    if (ret != UBSE_OK) {
        // 失败路径:框架在 HandleRequest 中检测到非 UBSE_OK 返回值时会自动回包。
        return ret;
    }

    // 成功路径框架不会主动回包,须显式发送空响应作为 ack,否则调用方(helper)会在 UDS 上阻塞直至超时。
    api::server::UbseIpcMessage response{nullptr, 0};
    return api::server::SendResponse(UBSE_OK, ctx.requestId, response);
}

} // namespace ubse::vip
