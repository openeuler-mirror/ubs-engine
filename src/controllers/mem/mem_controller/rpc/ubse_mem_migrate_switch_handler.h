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

#ifndef UBSE_MEM_MIGRATE_SWITCH_HANDLER_H
#define UBSE_MEM_MIGRATE_SWITCH_HANDLER_H
#include <ubse_com_module.h>

#include "ubse_com_base.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller::rpc {
using ubse::adapter_plugins::mmi::UbseMemMigrateDataCopyReq;
using ubse::adapter_plugins::mmi::UbseMemMigrateSwitchEndImportReq;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::com::UbseBaseMessagePtr;
using ubse::com::UbseComBaseMessageHandler;
using ubse::com::UbseComBaseMessageHandlerCtxPtr;
using ubse::com::UbseComModule;
using ubse::com::UbseMemBorrowCallbackOpCode;
using ubse::com::UbseModuleCode;

// 逐块迁移块级切换 begin handler（master->导入节点 0x0011, : 载荷=master 组包的 importObj）：
// decoder 下发(恒远端链路, 无条件执行) + 内核换链准备 + 账本打迁移标记
class UbseMemMigrateSwitchBeginImportHandler : public UbseComBaseMessageHandler {
public:
    UbseMemMigrateSwitchBeginImportHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_IMPORT);
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

// 目标节点通过 begin_export 将已有导入设备切为导出设备；响应 desc 写入 master 迁移上下文。
class UbseMemMigrateBeginExportHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_EXPORT);
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

class UbseMemMigrateEndExportHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_EXPORT_END);
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

// 逐块迁移 数据拷贝 handler（master->目标节点 0x0012）：本节点新导出块与源块经 fabric 的数据同步
class UbseMemMigrateDataCopyHandler : public UbseComBaseMessageHandler {
public:
    UbseMemMigrateDataCopyHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_DATA_COPY);
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

// 逐块迁移块级切换 end handler（master->导入节点 0x0013）：success=1 切新链路 + 账本收敛；0 回退
class UbseMemMigrateSwitchEndImportHandler : public UbseComBaseMessageHandler {
public:
    UbseMemMigrateSwitchEndImportHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_END);
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

// handler只负责入队确认，业务结果由线程池处理。
class UbseMemMigrateBeginImportResultHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;
    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_IMPORT_RESULT);
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

class UbseMemMigrateBeginExportResultHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;
    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_EXPORT_RESULT);
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

class UbseMemMigrateCopyResultHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;
    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_COPY_RESULT);
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

class UbseMemMigrateEndResultHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;
    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_END_RESULT);
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

// handler只负责入队确认，业务结果由线程池处理。

// handler只负责入队确认，业务结果由线程池处理。

// 请求及业务结果上报handler统一注册，RPC应答仅表示线程池入队成功。
UbseResult RegisterMigrateSwitchHandlers(const std::shared_ptr<UbseComModule>& comModule);
} // namespace ubse::mem::controller::rpc

#endif // UBSE_MEM_MIGRATE_SWITCH_HANDLER_H
