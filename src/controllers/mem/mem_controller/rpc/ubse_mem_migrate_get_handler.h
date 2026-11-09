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

#ifndef UBSE_MEM_MIGRATE_GET_HANDLER_H
#define UBSE_MEM_MIGRATE_GET_HANDLER_H
#include <ubse_com_module.h>

#include "ubse_com_base.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller::rpc {
using ubse::adapter_plugins::mmi::UbseMemShmMigrateGetReq;
using ubse::adapter_plugins::mmi::UbseMemShmMigrateGetResp;
using ubse::com::UbseComBaseMessageHandler;
using ubse::com::UbseComBaseMessageHandlerCtxPtr;
using ubse::com::UbseComModule;
using ubse::com::UbseMemQueryOpCode;
using ubse::com::UbseModuleCode;

// SHM故障迁移查询请求消息（dispatcher 无条件同步发 master）
class UbseMemShmMigrateGetReqMessage : public UbseBaseMessage {
public:
    UbseMemShmMigrateGetReqMessage() = default;

    explicit UbseMemShmMigrateGetReqMessage(UbseMemShmMigrateGetReq getReq) : getReq(std::move(getReq)) {}

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    UbseMemShmMigrateGetReq getReq{};
};

// SHM故障迁移查询响应消息：errCode + 会话字段派生进度（无会话时仅 errCode=NOT_EXIST）
class UbseMemShmMigrateGetRespMessage : public UbseBaseMessage {
public:
    UbseMemShmMigrateGetRespMessage() = default;

    explicit UbseMemShmMigrateGetRespMessage(UbseMemShmMigrateGetResp getResp) : getResp(std::move(getResp)) {}

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    UbseMemShmMigrateGetResp getResp{};
};

// master 侧查询执行体：读会话表(活跃优先/终态留存) → 派生进度 → 回包
class UbseMemShmMigrateGetHandler : public UbseComBaseMessageHandler {
public:
    UbseMemShmMigrateGetHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_SHM_MIGRATE_GET_REQ);
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

UbseResult RegisterMigrateGetHandlers(const std::shared_ptr<UbseComModule>& comModule);
} // namespace ubse::mem::controller::rpc

#endif // UBSE_MEM_MIGRATE_GET_HANDLER_H
