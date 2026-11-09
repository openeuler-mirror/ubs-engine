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

#ifndef UBSE_MEM_MIGRATE_EXPORT_HANDLER_H
#define UBSE_MEM_MIGRATE_EXPORT_HANDLER_H
#include <ubse_com_module.h>

#include "ubse_com_base.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller::rpc {
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::com::UbseComBaseMessageHandler;
using ubse::com::UbseComBaseMessageHandlerCtxPtr;
using ubse::com::UbseComModule;
using ubse::com::UbseMemBorrowCallbackOpCode;
using ubse::com::UbseModuleCode;

// 普通新导出请求；handler只校验并入队。
class UbseMemMigrateExportHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;
    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_EXPORT);
    }
    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
    }
    bool NeedReply() override
    {
        return true;
    }
    static UbseResult RegUbseMemMigrateExportHandler(const std::shared_ptr<UbseComModule>& comModule);
};

// 普通新导出完成回调；handler只校验并入队。
class UbseMemMigrateNewExportResultHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;
    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_NEW_EXPORT_RESULT);
    }
    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
    }
    bool NeedReply() override
    {
        return true;
    }
};

} // namespace ubse::mem::controller::rpc

#endif // UBSE_MEM_MIGRATE_EXPORT_HANDLER_H
