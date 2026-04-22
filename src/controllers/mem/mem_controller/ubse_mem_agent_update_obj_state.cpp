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
#include "ubse_mem_agent_update_obj_state.h"

#include "ubse_com_op_code.h"
#include "ubse_error.h"
#include "ubse_mem_debt_ledger.h"
namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

namespace {
template <typename ImportObjType>
common::def::UbseResult CheckAndUpdateImportObjState(const ImportObjType &importObj)
{
    auto &ledger = debt::UbseMemDebtLedger::GetInstance();
    auto objPtr = ledger.GetDebtMap<ImportObjType>().GetResource(importObj.req.importNodeId, importObj.req.name);
    if (!objPtr) {
        return UBSE_ERROR;
    }

    if (objPtr->status.state == adapter_plugins::mmi::UBSE_MEM_EXPORT_RUNNING) {
        auto copyObj = *objPtr;
        copyObj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
        ledger.GetDebtMap<ImportObjType>().PutResource(importObj.req.importNodeId, importObj.req.name, copyObj);
    }
    return UBSE_OK;
}
} // namespace

common::def::UbseResult UbseMemAgentUpdateObjState::Handle(const ubse::message::UbseBaseMessagePtr &req,
                                                           const ubse::message::UbseBaseMessagePtr &rsp,
                                                           ubse::com::UbseComBaseMessageHandlerCtxPtr ctx)
{
    UBSE_LOG_INFO << "Handle UbseMemAgentUpdateObjState.";
    auto request =
        ubse::message::UbseBaseMessage::DeConvert<ubse::mem::controller::message::UbseMemUpdateObjState>(req);
    if (request == nullptr) {
        UBSE_LOG_ERROR << "Failed to deconvert request.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "update obj state, objType=" << request->objType;
    if (request->objType == "fd") {
        auto importObj = std::get<adapter_plugins::mmi::UbseMemFdBorrowImportObj>(request->obj);
        return CheckAndUpdateImportObjState(importObj);
    } else if (request->objType == "numa") {
        auto importObj = std::get<adapter_plugins::mmi::UbseMemNumaBorrowImportObj>(request->obj);
        return CheckAndUpdateImportObjState(importObj);
    } else if (request->objType == "addr") {
        auto importObj = std::get<adapter_plugins::mmi::UbseMemAddrBorrowImportObj>(request->obj);
        return CheckAndUpdateImportObjState(importObj);
    } else {
        UBSE_LOG_ERROR << "Unsupported objType=" << request->objType;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint16_t UbseMemAgentUpdateObjState::GetOpCode()
{
    return static_cast<uint16_t>(com::UbseMemBorrowCallbackOpCode::UBSE_MEM_UPDATE_OBJ_STATE_CALLBACK);
}

uint16_t UbseMemAgentUpdateObjState::GetModuleCode()
{
    return static_cast<uint16_t>(com::UbseModuleCode::UBSE_MEM_BORROW);
}
} // namespace ubse::mem::controller