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

#include "ubse_mem_controller_rpc_register.h"

#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_mem_agent_update_obj_state.h"
#include "ubse_mem_debt_info_query_handler.h"
#include "ubse_mem_update_obj_state.simpo.h"
#include "message/node_mem_debtInfo_query_req_simpo.h"
#include "message/node_mem_debt_info_simpo.h"
#include "message/ubse_mem_controller_def_simpo.h"
#include "message/ubse_mem_debt_info_partial_fetch_req.h"
#include "message/ubse_mem_debt_info_partial_fetch_res.h"

namespace ubse::mem::controller::rpc {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
using namespace ubse::mem::controller::message;
using namespace ubse::com;
using namespace ubse::log;
UbseResult RegisterMemDebtInfoQueryHandlers(const std::shared_ptr<com::UbseComModule>& comModule);

UbseResult RegAgentUpdateHandler(const std::shared_ptr<com::UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr updateHandler = new (std::nothrow) UbseMemAgentUpdateObjState();
    if (updateHandler == nullptr) {
        UBSE_LOG_ERROR << "new register UbseMemAgentUpdateObjState failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    auto retCode = comModule->RegRpcService<UbseMemUpdateObjState, UbseMemUpdateObjStateReply>(updateHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "updateHandler register fail," << FormatRetCode(retCode);
        return UBSE_ERROR;
    }
    return retCode;
}

UbseResult RegMemControllerHandler()
{
    auto comModule = UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }

    if (auto ret = RegisterMemDebtInfoQueryHandlers(comModule); ret != UBSE_OK) {
        return UBSE_ERROR;
    }

    if (auto ret = RegAgentUpdateHandler(comModule); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register agent update handler, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult RegPartialDebtFetchHandler(const std::shared_ptr<com::UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr partialDebtFetchHandler = new (std::nothrow) UbseMemDebtInfoPartialFetchHandler();
    if (partialDebtFetchHandler == nullptr) {
        UBSE_LOG_ERROR << "new register partialDebtFetchHandler failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    auto retCode = comModule->RegRpcService<UbseMemDebtInfoPartialFetchReq, UbseMemDebtInfoPartialFetchRes>(
        partialDebtFetchHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "partialDebtFetchHandler register fail," << FormatRetCode(retCode);
    }
    return retCode;
}

UbseResult RegisterFdQueryHandler(const std::shared_ptr<com::UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr fdGetHandler = new (std::nothrow) UbseMemDebtInfoFdGetHandler();
    if (fdGetHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>(fdGetHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoFdGetHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    UbseComBaseMessageHandlerPtr fdDescListHandler = new (std::nothrow) UbseMemDebtInfoFdListHandler();
    if (fdDescListHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>(fdDescListHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoFdListHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult RegisterNumaQueryHandler(const std::shared_ptr<com::UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr numaGetHandler = new (std::nothrow) UbseMemDebtInfoNumaGetHandler();
    if (numaGetHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>(numaGetHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoNumaGetHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    UbseComBaseMessageHandlerPtr numaGetWithImportNodeHandler = new (std::nothrow)
        UbseMemDebtInfoNumaGetWithImportNodeHandler();
    if (numaGetWithImportNodeHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>(numaGetWithImportNodeHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoNumaGetWithImportNodeHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    UbseComBaseMessageHandlerPtr numaDescListHandler = new (std::nothrow) UbseMemDebtInfoNumaListHandler();
    if (numaDescListHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>(numaDescListHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoNumaListHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult RegisterShmQueryHandler(const std::shared_ptr<com::UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr shmDescHandler = new (std::nothrow) UbseMemDebtInfoShmGetHandler();
    if (shmDescHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>(shmDescHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoShmGetHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    UbseComBaseMessageHandlerPtr shmDescListHandler = new (std::nothrow) UbseMemDebtInfoShmListHandler();
    if (shmDescListHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescListSimpo>(shmDescListHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoShmListHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    UbseComBaseMessageHandlerPtr shmStatusGetHandler = new (std::nothrow) UbseMemDebtInfoShmStatusGetHandler();
    if (shmStatusGetHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmMemStatusDescSimpo>(shmStatusGetHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoShmStatusGetHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult RegisterAddrQueryHandler(const std::shared_ptr<com::UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr addrGetHandler = new (std::nothrow) UbseMemDebtInfoAddrGetHandler();
    if (addrGetHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemAddrDescSimpo>(addrGetHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoAddrGetHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult RegNodeBorrowQueryHandler(const std::shared_ptr<com::UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr nodeBorrowQueryHandler = new (std::nothrow) UbseMemNodeBorrowQueryHandler();
    if (nodeBorrowQueryHandler == nullptr) {
        UBSE_LOG_ERROR << "new register nodeBorrowQueryHandler failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    auto retCode =
        comModule->RegRpcService<UbseMemNodeBorrowInfoReqMessage, UbseMemNodeBorrowInfoMessage>(nodeBorrowQueryHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeBorrowQueryHandler register fail," << FormatRetCode(retCode);
    }
    return retCode;
}

UbseResult RegMemIdQueryHandler(const std::shared_ptr<com::UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr memIdInfoGetHandler = new (std::nothrow) UbseMemIdInfoGetHandler();
    if (memIdInfoGetHandler == nullptr) {
        UBSE_LOG_ERROR << "new register UbseMemIdInfoGetHandler failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    auto retCode = comModule->RegRpcService<UbseMemIdQueryRequestSimpo, UbseMemExportMemDescSimpo>(memIdInfoGetHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "memIdInfoGetHandler register fail," << FormatRetCode(retCode);
        return UBSE_ERROR;
    }
    return retCode;
}

UbseResult RegisterMemDebtInfoQueryHandlers(const std::shared_ptr<com::UbseComModule>& comModule)
{
    UbseComBaseMessageHandlerPtr debtInfoQueryHandler = new (std::nothrow) UbseMemDebtInfoQueryHandler();
    if (debtInfoQueryHandler == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>(debtInfoQueryHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register UbseMemDebtInfoQueryHandler, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    ret = RegisterFdQueryHandler(comModule);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register fd query handler, " << FormatRetCode(ret);
        return ret;
    }
    ret = RegisterNumaQueryHandler(comModule);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register numa query handler, " << FormatRetCode(ret);
        return ret;
    }
    ret = RegisterShmQueryHandler(comModule);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register shm query handler, " << FormatRetCode(ret);
        return ret;
    }
    ret = RegisterAddrQueryHandler(comModule);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register addr query handler, " << FormatRetCode(ret);
        return ret;
    }
    ret = RegPartialDebtFetchHandler(comModule);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register partial debt fetch handler, " << FormatRetCode(ret);
        return ret;
    }
    ret = RegNodeBorrowQueryHandler(comModule);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register node borrow query handler, " << FormatRetCode(ret);
        return ret;
    }
    ret = RegMemIdQueryHandler(comModule);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Unable to register mem id query handler, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::rpc
