/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "AccountObjAdapter.h"
#include "message/ubse_mem_debtInfo_query_req_simpo.h"
#include "message/node_mem_debt_info_simpo.h"
#include "src/controllers/include/ubse_mem_resource.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_utils.h"
namespace ubse::resource::mem {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::log;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::controller;
using namespace ubse::mem::strategy;
using namespace ubse::mem::utils;
UbseResult SendRpcRequest(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap);
uint32_t UbseGetMemDebtInfo(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap)
{
    return SendRpcRequest(nodeId, memDebtInfoMap);
}

uint32_t UbseGetLocalNodeMemDebtInfo(NodeMemDebtInfo &nodeMemDebtInfo)
{
    // 从本节点获取数据；
    ubse::election::UbseRoleInfo currentNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(currentNodeInfo);
    const std::string currentNodeId = currentNodeInfo.nodeId;
    if (ret != UBSE_OK || currentNodeId.empty()) {
        UBSE_LOG_ERROR << "get currentNodeId failed. " << FormatRetCode(ret);
        return ret;
    }
    NodeMemDebtInfo tempDebtInfo{};
    ret = GetNodeMemDebtInfoById(currentNodeId, tempDebtInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get node mem debt info failed. " << FormatRetCode(ret);
        return ret;
    }
    NodeMemDebtInfoMap data{};
    data[currentNodeId] = tempDebtInfo;
    NodeMemDebtInfoMap filterData;
    FilterFdAndNumaObjs(filterData, data);
    nodeMemDebtInfo = filterData[currentNodeId];
    return UBSE_OK;
}

UbseResult SendRpcRequest(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap)
{
    ubse::election::UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    ubse::com::SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER),
                                   static_cast<uint16_t>(UbseOpCode::UBSE_MEM_DEBINFO_QUERY)};

    NodeMemDebtInfoQueryReqSimpoPtr ubseRequestPtr = new (std::nothrow) NodeMemDebtInfoQueryReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Malloc NodeMemDebtInfoQueryReqSimpo failed";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetNodeId(nodeId);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) NodeMemDebtInfoSimpo();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Malloc memory failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseContext &ubseContext = UbseContext::GetInstance();

    auto ubseComModule = ubseContext.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "Communication module not init, " << FormatRetCode(UBSE_ERROR_MODULE_LOAD_FAILED);
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr, false);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send metric failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto nodeMemDebtInfoSimpoPtr = UbseBaseMessage::DeConvert<NodeMemDebtInfoSimpo>(ubseResponsePtr);
    memDebtInfoMap = nodeMemDebtInfoSimpoPtr->GetNodeMemDebtInfoMap();
    return UBSE_OK;
}
} // namespace ubse::resource::mem
