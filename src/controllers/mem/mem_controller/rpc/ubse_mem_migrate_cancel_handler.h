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

#ifndef UBSE_MEM_MIGRATE_CANCEL_HANDLER_H
#define UBSE_MEM_MIGRATE_CANCEL_HANDLER_H
#include <ubse_com_module.h>

#include "ubse_com_base.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller::rpc {
using ubse::adapter_plugins::mmi::UbseMemMigrateCancelReq;
using ubse::adapter_plugins::mmi::UbseMemMigrateCancelResp;
using ubse::com::UbseComBaseMessageHandler;
using ubse::com::UbseComBaseMessageHandlerCtxPtr;
using ubse::com::UbseComModule;
using ubse::com::UbseMemQueryOpCode;
using ubse::com::UbseModuleCode;

// SHM故障迁移取消请求消息（dispatcher 无条件同步发 master）
class UbseMemMigrateCancelReqMessage : public UbseBaseMessage {
public:
    UbseMemMigrateCancelReqMessage() = default;

    explicit UbseMemMigrateCancelReqMessage(UbseMemMigrateCancelReq cancelReq) : cancelReq(std::move(cancelReq)) {}

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    UbseMemMigrateCancelReq cancelReq{};
};

// SHM故障迁移取消响应消息：errCode（受理结果；清理异步执行, 回包≠清理完成）
class UbseMemMigrateCancelRespMessage : public UbseBaseMessage {
public:
    UbseMemMigrateCancelRespMessage() = default;

    explicit UbseMemMigrateCancelRespMessage(UbseMemMigrateCancelResp cancelResp) : cancelResp(std::move(cancelResp)) {}

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    UbseMemMigrateCancelResp cancelResp{};
};

// master 侧取消受理执行体：会话锁内判定状态并置 cancelRequested → 返回受理结果
class UbseMemShmMigrateCancelHandler : public UbseComBaseMessageHandler {
public:
    UbseMemShmMigrateCancelHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_SHM_MIGRATE_CANCEL_REQ);
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

UbseResult RegisterMigrateCancelHandlers(const std::shared_ptr<UbseComModule>& comModule);
} // namespace ubse::mem::controller::rpc

#endif // UBSE_MEM_MIGRATE_CANCEL_HANDLER_H
