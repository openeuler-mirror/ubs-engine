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

#ifndef UBSE_MEM_MIGRATE_HANDLER_H
#define UBSE_MEM_MIGRATE_HANDLER_H
#include <ubse_com_module.h>

#include "ubse_com_base.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller::rpc {
using ubse::adapter_plugins::mmi::UbseMemShmMigrateReq;
using ubse::adapter_plugins::mmi::UbseMemShmMigrateResp;
using ubse::com::UbseComBaseMessageHandler;
using ubse::com::UbseComBaseMessageHandlerCtxPtr;
using ubse::com::UbseComModule;
using ubse::com::UbseMemQueryOpCode;
using ubse::com::UbseModuleCode;

// SHM故障迁移受理请求消息（dispatcher 无条件同步发 master）
class UbseMemShmMigrateReqMessage : public UbseBaseMessage {
public:
    UbseMemShmMigrateReqMessage() = default;

    explicit UbseMemShmMigrateReqMessage(UbseMemShmMigrateReq migrateReq) : migrateReq(std::move(migrateReq)) {}

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    UbseMemShmMigrateReq migrateReq{};
};

// SHM故障迁移受理响应消息：errCode（幂等命中回 MIGRATE_IN_PROGRESS）
class UbseMemShmMigrateRespMessage : public UbseBaseMessage {
public:
    UbseMemShmMigrateRespMessage() = default;

    explicit UbseMemShmMigrateRespMessage(UbseMemShmMigrateResp migrateResp) : migrateResp(migrateResp) {}

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    UbseMemShmMigrateResp migrateResp{};
};

// master 侧受理执行体唯一入口：受理预检 + 会话创建 + 新借出投递
class UbseMemShmMigrateHandler : public UbseComBaseMessageHandler {
public:
    UbseMemShmMigrateHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_SHM_MIGRATE_REQ);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};

UbseResult RegisterMigrateHandlers(const std::shared_ptr<UbseComModule>& comModule);
} // namespace ubse::mem::controller::rpc

#endif // UBSE_MEM_MIGRATE_HANDLER_H
