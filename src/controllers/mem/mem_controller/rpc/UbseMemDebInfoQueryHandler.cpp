/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "UbseMemDebInfoQueryHandler.h"
#include "../message/ubse_mem_debtInfo_query_req_simpo.h"
#include "../message/node_mem_debt_info_simpo.h"
#include "../ubse_mem_controller_api.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "../ubse_mem_utils.h"
namespace ubse::mem_controller::rpc {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

using namespace ubse::context;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::obj;
using namespace ubse::mem::obj;
using namespace ubse::resource::mem;
using namespace ubse::mem::utils;
UbseResult UbseMemDebInfoQueryHandler::RegUbseMemDebInfoQueryHandler()
{
    UbseComBaseMessageHandlerPtr ubseComBaseMessageHandler = new (std::nothrow) UbseMemDebInfoQueryHandler();
    if (ubseComBaseMessageHandler == nullptr) {
        UBSE_LOG_ERROR << "new register UbseMemDebInfoQueryHandler failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    UbseContext &ctx = UbseContext::GetInstance();
    auto ubseComModule = ctx.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "get UbseComModule failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    auto retCode =
        ubseComModule->RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>(ubseComBaseMessageHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "ubseComBaseMessageHandler register fail," << FormatRetCode(retCode);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemDebInfoQueryHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                              UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req和resp
    auto reqPtr = UbseBaseMessage::DeConvert<NodeMemDebtInfoQueryReqSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new NodeMemDebtInfoQueryReqSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据;
    ubse::resource::mem::NodeMemDebtInfoMap data;
    if (reqPtr->GetNodeId().empty()) {
        // 全量数据
        data = mem::controller::GetNodeMemDebtInfoMap();
    } else {
        // 指定节点数据
        auto debtInfo = mem::controller::GetNodeMemDebtInfoMap()[reqPtr->GetNodeId()];
        data[reqPtr->GetNodeId()] = debtInfo;
    }
    ubse::resource::mem::NodeMemDebtInfoMap filterData;

    // 检查数据是否有效
    if (data.empty()) {
        UBSE_LOG_ERROR << "node mem debt info is empty!";
        return UBSE_ERROR;
    }
    FilterFdAndNumaObjs(filterData, data);
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<NodeMemDebtInfoSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new NodeMemDebtInfoSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetNodeMemDebtInfoSimpo(filterData);
    return UBSE_OK;
}
} // namespace ubse::mem_controller::rpc