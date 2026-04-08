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
#include "ubse_mem_controller_query_api.h"

#include "message/node_mem_debtInfo_query_req_simpo.h"
#include "message/node_mem_debt_info_simpo.h"
#include "message/ubse_mem_controller_def_simpo.h"
#include "src/sdk/c/include/ubs_engine_topo.h"
#include "ubse_com_module.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_logger.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_constants.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_util.h"
#include "ubse_node_controller_query_api.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::mem::strategy;
using namespace ubse::election;
using namespace ubse::utils;
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::log;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::controller;
using namespace ubse::mem::util;
using namespace ubse::mem::strategy;

template <class TSimpo, class TPtr>
uint32_t SendQueryToMasterIfNotMaster(def::UbseMemDebtQueryRequest &request, std::string &masterNodeId,
                                      uint16_t opCode, TPtr &resp)
{
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "Communication module not init, " << FormatRetCode(UBSE_ERROR_MODULE_LOAD_FAILED);
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    SendParam sendParam{masterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY), opCode};
    UbseMemDebtQueryRequestSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemDebtQueryRequestSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemDebtQueryRequest(request);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) TSimpo();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ubseResponsePtr.";
        return UBSE_ERROR_NULLPTR;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr, false);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "master deal failed, " << FormatRetCode(retCode);
        return retCode;
    }
    UBSE_LOG_INFO << "success to deal rpc request. ErrCode=" << ubseResponsePtr->GetErrCode();
    resp = UbseBaseMessage::DeConvert<TSimpo>(ubseResponsePtr);
    return UBSE_OK;
}
UbseResult GetMasterAndLocalNodeId(std::string &masterNodeId, std::string &localNodeId)
{
    // 获取主节点以及当前节点
    UbseRoleInfo masterInfo{};
    if (const auto ret = UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    masterNodeId = std::move(masterInfo.nodeId);
    localNodeId = std::move(currentRoleInfo.nodeId);
    return UBSE_OK;
}
uint32_t UbseMemFdGet(const std::string &name, def::UbseMemFdDesc &fdDesc, const def::UbseUdsInfo *udsInfo)
{
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, ret: " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    def::UbseMemDebtQueryRequest request{.name = name, .importNodeId = localNodeId};
    if (udsInfo != nullptr) {
        request.udsInfo = *udsInfo;
    }
    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        UbseMemFdDescSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<UbseMemFdDescSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_FD_GET),  // 添加类型转换
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        fdDesc = descSimpoPtr.Get()->GetUbseMemFdDesc();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemFdGet(request, fdDesc);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemFdList(const def::UbseUdsInfo &udsInfo, std::vector<def::UbseMemFdDesc> &fdDescs)
{
    fdDescs.clear();
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    def::UbseMemDebtQueryRequest request{.importNodeId = localNodeId, .udsInfo = udsInfo};
    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        UbseMemFdDescListSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<UbseMemFdDescListSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_FD_LIST),
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        fdDescs = descSimpoPtr.Get()->GetUbseMemFdDescList();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemFdList(request, fdDescs);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemShmStatusGet(const std::string &name, def::UbseMemShmMemStatusDesc &shmStatusDesc)
{
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    def::UbseMemDebtQueryRequest request{.name = name};
    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        UbseMemShmMemStatusDescSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<UbseMemShmMemStatusDescSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_STATUS_GET),
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        shmStatusDesc = descSimpoPtr.Get()->GetUbseMemShmMemStatusDesc();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemShmStatusGet(request, shmStatusDesc);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaGet(const std::string &name, def::UbseMemNumaDesc &numaDesc, const UbseUdsInfo *udsInfo)
{
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    def::UbseMemDebtQueryRequest request{.name = name, .importNodeId = localNodeId};
    if (udsInfo != nullptr) {
        request.udsInfo = *udsInfo;
    }
    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        DefUbseMemNumaDescSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<DefUbseMemNumaDescSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_NUMA_GET),
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        numaDesc = descSimpoPtr.Get()->GetUbseMemNumaDesc();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemNumaGet(request, numaDesc);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaList(const def::UbseUdsInfo &udsInfo, std::vector<def::UbseMemNumaDesc> &numaDescs)
{
    numaDescs.clear();
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    def::UbseMemDebtQueryRequest request{.importNodeId = localNodeId, .udsInfo = udsInfo};
    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        DefUbseMemNumaDescListSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<DefUbseMemNumaDescListSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_NUMA_LIST),
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        numaDescs = descSimpoPtr.Get()->GetUbseMemNumaDescList();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemNumaList(request, numaDescs);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemShmGet(const std::string &name, def::UbseMemShmDesc &shmDesc, const def::UbseUdsInfo *udsInfo)
{
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    def::UbseMemDebtQueryRequest request{
        .name = name,
    };
    if (udsInfo != nullptr) {
        request.udsInfo = *udsInfo;
    }
    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        UbseMemShmDescSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<UbseMemShmDescSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_GET),
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        shmDesc = descSimpoPtr.Get()->GetUbseMemShmDesc();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemShmGet(request, shmDesc);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemShmGetByNodeId(const std::string &name, def::UbseMemShmDesc &shmDesc, std::string &srcNodeId)
{
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    def::UbseMemDebtQueryRequest request{.name = name, .importNodeId = srcNodeId};
    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        UbseMemShmDescSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<UbseMemShmDescSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_GET),
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        shmDesc = descSimpoPtr.Get()->GetUbseMemShmDesc();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemShmGet(request, shmDesc);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemShmList(def::UbseMemDebtQueryRequest &request, std::vector<def::UbseMemShmDesc> &shmDescs)
{
    shmDescs.clear();
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        UbseMemShmDescListSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<UbseMemShmDescListSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_LIST),
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        shmDescs = descSimpoPtr.Get()->GetUbseMemShmDescList();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemShmList(request, shmDescs);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}
uint32_t UbseNodeInfoGet(const std::string &nodeId, ubse::adapter_plugins::mmi::UbseNodeInfo &ubseNodeInfo)
{
    if (nodeId.empty()) {
        UBSE_LOG_WARN << "node id is empty;";
        return UBSE_OK;
    }
    if (const auto ret = ConvertStrToUint32(nodeId, ubseNodeInfo.index); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Convert str to uint32 failed, nodeId:" << nodeId << "; ret:" << FormatRetCode(ret);
    }
    ubseNodeInfo.nodeId = nodeId;
    auto nodeMap = nodeController::UbseNodeController::GetInstance().GetAllNodes();
    const auto itNode = nodeMap.find(nodeId);
    if (itNode == nodeMap.end()) {
        UBSE_LOG_WARN << "failed to find nodeId, nodeId: " << nodeId;
        return UBSE_ERROR;
    }
    ubseNodeInfo.hostName = itNode->second.hostName;
    if (itNode->second.localState == nodeController::UbseNodeLocalState::UBSE_NODE_READY) {
        ubseNodeInfo.status = UBSE_NODE_STATUS_NORMAL;
    } else {
        ubseNodeInfo.status = UBSE_NODE_STATUS_ABNORMAL;
    }
    return UBSE_OK;
}

int32_t UbseMemAddrGet(const std::string &name, const std::string &importNodeId, UbseMemAddrDesc &desc)
{
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    def::UbseMemDebtQueryRequest request{.name = name, .importNodeId = importNodeId};
    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        UbseMemAddrDescSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<UbseMemAddrDescSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_ADDR_GET),
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        desc = descSimpoPtr.Get()->GetUbseMemAddrDesc();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemAddrGet(request, desc);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

int32_t UbseMemNumaGetWithImportNode(const std::string &name, const std::string &importNodeId,
                                     UbseMemNumaDesc &numaDesc)
{
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "failed to get master and local node id, " << FormatRetCode(ret);
        return UBSE_ERR_DAEMON_UNREACHABLE;
    }

    def::UbseMemDebtQueryRequest request{.name = name, .importNodeId = importNodeId};
    // 不是master调用RPC发送到主节点处理
    if (localNodeId != masterNodeId) {
        UbseMemNumaDescSimpoPtr descSimpoPtr;
        ret = SendQueryToMasterIfNotMaster<UbseMemNumaDescSimpo>(
            request,
            masterNodeId,
            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_NUMA_GET_WITH_IMPORT_NODE),
            descSimpoPtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
        numaDesc = descSimpoPtr.Get()->GetUbseMemNumaDesc();
    } else {
        // 是master，直接查询
        ret = debt::UbseMemNumaGetWithImportNode(request, numaDesc);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to deal query, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

uint32_t UbseGetMemDebtInfoFromMaster(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap)
{
    ubse::election::UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    const SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBINFO_QUERY)};

    NodeMemDebtInfoQueryReqSimpoPtr ubseRequestPtr = new (std::nothrow) NodeMemDebtInfoQueryReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "new ptr failed";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetNodeId(nodeId);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) NodeMemDebtInfoSimpo();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "new ubseResponsePtr failed";
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
uint32_t GetDebtInfoMapByNodeId(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap)
{
    ubse::election::UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    ubse::com::SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                                   static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBINFO_QUERY)};

    NodeMemDebtInfoQueryReqSimpoPtr ubseRequestPtr = new (std::nothrow) NodeMemDebtInfoQueryReqSimpo();
    if (ubseRequestPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetNodeId(nodeId);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) NodeMemDebtInfoSimpo();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "new ubseResponsePtr failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
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

uint32_t UbseMemNodeBorrowInfoQuery(std::vector<def::UbseNodeBorrowInfo> &nodeBorrowInfo)
{
    // 获取主节点以及当前节点
    std::string masterNodeId{};
    std::string localNodeId{};
    auto ret = GetMasterAndLocalNodeId(masterNodeId, localNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master and local node id" << FormatRetCode(ret);
        return ret;
    }
    // 不是master调用RPC异步发送
    if (localNodeId != masterNodeId) {
        UbseMemNodeBorrowInfoReqMessagePtr requestPtr = new (std::nothrow) UbseMemNodeBorrowInfoReqMessage();
        UbseMemNodeBorrowInfoMessagePtr responsePtr = new (std::nothrow) UbseMemNodeBorrowInfoMessage();
        if (requestPtr == nullptr || responsePtr == nullptr) {
            UBSE_LOG_ERROR << "Failed to new ptr";
            return UBSE_ERROR_NULLPTR;
        }
        SendParam sendParam{masterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY),
                            static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_NODE_BORROW_INFO_QUERY)};

        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "Getting ComModule failed.";
            return UBSE_ERROR_NULLPTR;
        }
        ret = comModule->RpcSend(sendParam, requestPtr, responsePtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to Send to master, ret " << FormatRetCode(ret);
            return ret;
        }
        if (responsePtr == nullptr) {
            UBSE_LOG_ERROR << "Failed to convert responsePtr";
            return UBSE_ERROR_NULLPTR;
        }
        nodeBorrowInfo = responsePtr->GetUbseNodeBorrowInfos();
    } else {
        // 主节点直接查询
        ret = debt::UbseMemNodeBorrowQuery(nodeBorrowInfo);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to query node borrow info, ret " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller