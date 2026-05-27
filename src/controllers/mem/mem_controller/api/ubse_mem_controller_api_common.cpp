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

#include "ubse_mem_controller_api_common.h"

#include <cstdint>
#include <optional>
#include <shared_mutex>

#include "ubse_com_module.h"
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_controller_pre_online.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mmi_interface.h"
#include "ubse_sign_token_bucket.h"
#include "../message/node_mem_debtInfo_query_req_simpo.h"
#include "../message/ubse_mem_operation_resp_simpo.h"
#include "../ubse_mem_account.h"
#include "../ubse_mem_rpc_processor.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::task_executor;
using namespace ubse::com;
using namespace message;
using namespace ubse::mem::strategy;
using namespace adapter_plugins::mti::mami;
using namespace adapter_plugins::mti;
using namespace ubse::utils;
using namespace ubse::config;
static uint32_t MAX_WAIT_TIME(ubse::mem::strategy::API_TIME_OUT); // 单位:second
std::atomic<uint64_t> g_fdUnimportFailedCount{0};
std::atomic<uint64_t> g_numaUnimportFailedCount{0};
std::atomic<uint64_t> g_shareUnimportFailedCount{0};
std::atomic<uint64_t> g_addrUnimportFailedCount{0};
const std::string MEM_FEATURE_NOT_SUPPORTED_MSG = "Memory feature is unsupported.";

std::shared_mutex g_decoderImportMutex;

std::shared_mutex& GetDecoderImportMutex()
{
    return g_decoderImportMutex;
}

bool IsSdkRequest(uint64_t requestId)
{
    return UbseRequestIdUtil::ParseRequestType(requestId) == ubse::utils::UbseRequestType::SDK_REQUEST;
}

bool IsMemBorrowFeatureSupported()
{
    if (UbseIsMemBorrowSupported()) {
        return true;
    }
    UBSE_LOG_WARN << "Memory borrow feature is unsupported.";
    return false;
}

bool IsMemShareFeatureSupported()
{
    if (UbseIsMemShareSupported()) {
        return true;
    }
    UBSE_LOG_WARN << "Memory share feature is unsupported.";
    return false;
}

bool IsMemShareModeFeatureSupported(uint16_t cacheableFlag)
{
    const bool supported = cacheableFlag == 1 ? UbseIsMemShareCcSupported() : UbseIsMemShareNcSupported();
    if (supported) {
        return true;
    }
    UBSE_LOG_WARN << "Memory share mode is unsupported, cacheableFlag=" << cacheableFlag;
    return false;
}

uint32_t BuildMemFeatureNotSupportedResp(UbseMemOperationResp& resp, const std::string& name,
                                         const std::string& requestNodeId, MemOperationType type)
{
    return BuildOperationRespWhenFail(resp, name, requestNodeId, MEM_FEATURE_NOT_SUPPORTED_MSG, UBSE_ERR_NOT_SUPPORTED,
                                      type);
}

void SendParamSwitcher(const MemOperationType& type, SendParam& sendParam)
{
    switch (type) {
        case MemOperationType::SHARED_BORROW:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_BORROW_RESP));
            break;
        case MemOperationType::SHARED_ATTACH:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_ATTACH_RESP));
            break;
        case MemOperationType::SHARED_DETACH:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_DETACH_RESP));
            break;
        case MemOperationType::FD_BORROW:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_BORROW_RESP));
            break;
        case MemOperationType::NUMA_BORROW:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_BORROW_RESP));
            break;
        case MemOperationType::SHARED_RETURN:
        case MemOperationType::NUMA_RETURN:
        case MemOperationType::FD_RETURN:
        case MemOperationType::COMMON_RETURN:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_COMMON_RETURN_RESP));
            break;
        default:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_BORROW_RESULT_NOTIFY));
    }
}

uint32_t BuildOperationRespWhenFail(UbseMemOperationResp& resp, const std::string& name,
                                    const std::string& requestNodeId, std::string errMsg, uint32_t errorCode,
                                    MemOperationType type)
{
    election::UbseRoleInfo currNodeInfo;
    election::UbseGetCurrentNodeInfo(currNodeInfo);
    if (currNodeInfo.nodeRole != election::ELECTION_ROLE_MASTER) {
        UBSE_LOG_WARN << "currNode is not master, stop send response";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "Build response when fail, name=" << name << ", type=" << static_cast<uint32_t>(type)
                  << ", requestId=" << resp.requestId;
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get com module.";
        return UBSE_ERROR_NULLPTR;
    }
    UBSE_LOG_ERROR << errMsg << ", requestId=" << resp.requestId;
    resp.errMsg = errMsg;
    resp.name = name;
    resp.requestNodeId = requestNodeId;
    resp.errorCode = errorCode;
    SendParam sendParam(resp.requestNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_BORROW_RESULT_NOTIFY));
    if (IsSdkRequest(resp.requestId)) {
        SendParamSwitcher(type, sendParam);
    }
    UbseMemOperationRespSimpoPtr ptr = new (std::nothrow) UbseMemOperationRespSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << resp.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemOperationResp(resp);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << resp.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    UbseResult ret = UBSE_OK;
    for (int i = 0; i < RPC_RETRY_TIMES; ++i) {
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        if (ret == UBSE_OK) {
            UBSE_LOG_INFO << "Success to send resp when action is successful, name=" << resp.name
                          << ", requestId=" << resp.requestId;
            return ret;
        }
    }
    UBSE_LOG_ERROR << "Failed to send resp, name=" << resp.name << ", requestId=" << resp.requestId;
    return ret;
}

uint32_t BuildOperationRespWhenSuccess(UbseMemOperationResp& resp, UbseResult errorCode, MemOperationType type)
{
    election::UbseRoleInfo currNodeInfo;
    election::UbseGetCurrentNodeInfo(currNodeInfo);
    if (currNodeInfo.nodeRole != election::ELECTION_ROLE_MASTER) {
        UBSE_LOG_WARN << "currNode is not master, stop send response";
        return UBSE_ERROR;
    }
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get com module, requestId=" << resp.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    resp.errorCode = errorCode;
    SendParam sendParam(resp.requestNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
                        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_BORROW_RESULT_NOTIFY));
    if (IsSdkRequest(resp.requestId)) {
        resp.errorCode = UBSE_OK;
        SendParamSwitcher(type, sendParam);
    }
    UbseMemOperationRespSimpoPtr ptr = new (std::nothrow) UbseMemOperationRespSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << resp.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemOperationResp(resp);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << resp.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    UbseResult ret = UBSE_OK;
    for (int i = 0; i < RPC_RETRY_TIMES; ++i) {
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        if (ret == UBSE_OK) {
            UBSE_LOG_INFO << "Success to send resp when action is successful, name=" << resp.name
                          << ", requestId=" << resp.requestId;
            return ret;
        }
    }
    UBSE_LOG_ERROR << "Failed to send resp, name=" << resp.name << ", requestId=" << resp.requestId;
    return ret;
}

void InitializeResponse(const UbseMemReturnReq& req, UbseMemOperationResp& resp)
{
    resp.name = req.name;
    resp.requestNodeId = req.requestNodeId;
    resp.requestId = req.requestId;
}

void SetMamiImportInfoByDecoderParam(const std::pair<uint32_t, uint32_t>& chipDiePair,
                                     const decoder::utils::ImportDecoderParam& importDecoderParam,
                                     UbseMamiMemImportInfo& mamiImportInfo)
{
    mamiImportInfo.ubpuId = chipDiePair.first;
    mamiImportInfo.iouId = chipDiePair.second;
    mamiImportInfo.importType = importDecoderParam.importType;
    mamiImportInfo.decoderId = importDecoderParam.decoderIdx;
    mamiImportInfo.handle = importDecoderParam.handle;
    mamiImportInfo.flag = importDecoderParam.flag;
    mamiImportInfo.lb = 0;
}

void SetMamiImportInfoByExportInfo(const UbseMemObmmInfo& exportInfo, UbseMamiMemImportInfo& mamiImportInfo)
{
    mamiImportInfo.size = exportInfo.desc.length;
    mamiImportInfo.tokenId = exportInfo.desc.tokenid;
    mamiImportInfo.dstCNA = exportInfo.desc.dcna;
    mamiImportInfo.uba = exportInfo.desc.addr;
    mamiImportInfo.marId = exportInfo.desc.marId;
}

void SetDecoderLocByMamiImportInfo(const UbseMamiMemImportInfo& mamiImportInfo, decoder::utils::DecoderEntryLoc& loc)
{
    loc.ubpuId = mamiImportInfo.ubpuId;
    loc.iouId = mamiImportInfo.iouId;
    loc.decoderId = mamiImportInfo.decoderId;
    loc.marId = mamiImportInfo.marId;
}

UbseResult AddDecoderEntryByPreOnline(const decoder::utils::DecoderEntryLoc& loc, UbseMamiMemImportInfo& mamiImportInfo,
                                      UbseMemImportStatus& status)
{
    UbseMamiMemImportResult importResult{};
    auto res = decoder::utils::UbseMemPrehandleManager::GetInstance().GetPreHandleByDcna(loc, mamiImportInfo.dstCNA,
                                                                                         importResult);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "all hanlde is used, preImport size is not enough";
        return res;
    }
    mamiImportInfo.handle = importResult.handle;
    res = adapter_plugins::mti::UbseMtiInterface::GetInstance().AddDecoderEntry(mamiImportInfo, importResult);
    if (res != UBSE_OK) {
        mamiImportInfo.handle = 0;
        UBSE_LOG_ERROR << "ImportToAddDecoderEntry failed";
        return res;
    }
    importResult.staticHandle = mamiImportInfo.handle;
    status.decoderResult.emplace_back(importResult);
    return UBSE_OK;
}

UbseResult ImportToAddDecoderEntry(const std::pair<uint32_t, uint32_t>& chipDiePair,
                                   const std::vector<UbseMemObmmInfo>& exportObmmInfo,
                                   const decoder::utils::ImportDecoderParam& importDecoderParam,
                                   UbseMemImportStatus& status)
{
    status.decoderResult.clear();
    UbseMamiMemImportInfo mamiImportInfo{};
    SetMamiImportInfoByDecoderParam(chipDiePair, importDecoderParam, mamiImportInfo);
    decoder::utils::DecoderEntryLoc loc{};
    SetDecoderLocByMamiImportInfo(mamiImportInfo, loc);
    UBSE_LOG_DEBUG << "Add decoder entry, marId=" << mamiImportInfo.marId << ", dstCNA=" << mamiImportInfo.dstCNA
                   << ", flag=" << mamiImportInfo.flag;
    if (importDecoderParam.isHighSafety &&
        exportObmmInfo.size() != importDecoderParam.trustRingData.lendSignedDatas.size()) {
        UBSE_LOG_ERROR << "The size of of signed data is illegal.";
        return UBSE_ERROR;
    }
    bool usePreOnline = IsPreOnLineEnable();
    for (int i = 0; i < exportObmmInfo.size(); i++) {
        std::optional<UbseSignTokenBucket::TokenGuard> token;
        if (IsHighSafety()) {
            token.emplace(UbseSignTokenBucket::GetInstance().Acquire());
        }
        SetMamiImportInfoByExportInfo(exportObmmInfo[i], mamiImportInfo);
        UbseMamiMemImportResult importResult{};
        if (usePreOnline) {
            auto res = AddDecoderEntryByPreOnline(loc, mamiImportInfo, status);
            if (res == UBSE_OK) {
                continue;
            }
            // 预上线失败，尝试使用普通上线
            usePreOnline = false;
            UBSE_LOG_ERROR << "PreImportToAddDecoderEntry failed, use normal preImport";
        }

        ubse::adapter_plugins::mti::UbseDecoderTrustRingData trustRingData{importDecoderParam.isHighSafety};
        if (importDecoderParam.isHighSafety) {
            trustRingData.trustRingId = importDecoderParam.trustRingData.trustRingId;
            trustRingData.signedData = importDecoderParam.trustRingData.lendSignedDatas[i];
            trustRingData.type = importDecoderParam.type;
        }
        int retry = 3;
        uint32_t res = UBSE_OK;
        while (retry > 0) {
            res = UbseMtiInterface::GetInstance().AddDecoderEntry(mamiImportInfo, importResult, trustRingData);
            if (res == UBSE_OK) {
                break;
            }
            retry--;
        }
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "ImportToAddDecoderEntry failed";
            return res;
        }
        status.decoderResult.emplace_back(importResult);
    }
    return UBSE_OK;
}

void UnimportToDelDecoderEntry(const std::pair<uint32_t, uint32_t>& chipDiePair, UbseMemImportStatus& status,
                               uint8_t decoderId)
{
    std::vector<UbseMamiMemImportResult> failedDecoderResult{};
    for (const auto& decoderVal : status.decoderResult) {
        UbseMamiMemWithdraw mamiDelInfo{chipDiePair.first, chipDiePair.second, decoderVal.marId, decoderId,
                                        decoderVal.handle};
        auto res = adapter_plugins::mti::UbseMtiInterface::GetInstance().DeleteDecoderEntry(mamiDelInfo);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "UnimportToDelDecoderEntry failed, handle=" << decoderVal.handle
                           << ", hpa=" << decoderVal.hpa << ", marId=" << decoderVal.marId;
            failedDecoderResult.push_back(decoderVal);
        }
    }

    status.decoderResult = failedDecoderResult;
}

uint32_t AgentInvalidateDecoderEntry(uint32_t attachSocketId, UbseMemImportStatus& status, uint8_t decoderId)
{
    std::pair<uint32_t, uint32_t> chipDiePair{attachSocketId, attachSocketId};
    auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(attachSocketId, chipDiePair);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetChipAndDieId failed, decoderId=" << decoderId;
        return res;
    }
    for (auto& decoderVal : status.decoderResult) {
        if (decoderVal.valid) {
            continue;
        }
        UbseMamiMemWithdraw mamiDelInfo{chipDiePair.first, chipDiePair.second, decoderVal.marId, decoderId,
                                        decoderVal.handle};
        auto res = adapter_plugins::mti::UbseMtiInterface::GetInstance().InvalidateDecoderEntry(mamiDelInfo);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "InvalidateDecoderEntry failed, handle=" << decoderVal.handle
                           << ", hpa=" << decoderVal.hpa << ", marId=" << decoderVal.marId;
            return res;
        }
        decoderVal.valid = true;
    }
    return res;
}

UbseMemStage GetMemStageByShareImportObjState(const UbseMemShareBorrowImportObj& importObj, const bool& importObjExist)
{
    if (!importObjExist) {
        return UbseMemStage::UBSE_NOT_EXIST;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        return UbseMemStage::UBSE_CREATING;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYING) {
        return UbseMemStage::UBSE_DELETING;
    }

    return UbseMemStage::UBSE_EXIST;
}

void InitAgentMaxWaitTime(uint32_t timeout)
{
    MAX_WAIT_TIME = timeout;
}

uint32_t GetWaitTimeOut()
{
    return MAX_WAIT_TIME;
}

bool CheckMemBelongToReqUds(const UbseUdsInfo& memUds, const UbseUdsInfo& reqUds)
{
    if (!memUds.username.empty()) {
        return memUds.username == reqUds.username;
    }
    return memUds.uid == reqUds.uid;
}

bool isRequestNodeIdValid(const std::string& realRequestNodeId, const std::vector<std::string>& roleIds)
{
    for (const auto& roleId : roleIds) {
        if (!roleId.empty() && (roleId == realRequestNodeId)) {
            return true;
        }
    }
    return false;
}

bool CheckHasMemOwnerPermission(const UbseUdsInfo& memUds, const UbseUdsInfo& reqUds,
                                const std::string& realRequestNodeId, const std::vector<std::string>& commonRoleIds)
{
    if (!CheckMemBelongToReqUds(memUds, reqUds)) {
        return false;
    }
    return isRequestNodeIdValid(realRequestNodeId, commonRoleIds);
}

bool CheckHasRootPermission(const UbseUdsInfo& reqUds, const std::string& realRequestNodeId,
                            const std::vector<std::string>& rootRoleIds)
{
    if (reqUds.username == "root" || reqUds.username == "ubse" || reqUds.uid == 0) {
        if (isRequestNodeIdValid(realRequestNodeId, rootRoleIds)) {
            return true;
        }
    }
    return false;
}

bool CheckCommonReturnPermission(const UbseUdsInfo& memUds, const UbseUdsInfo& reqUds,
                                 const std::string& realRequestNodeId, const std::string& importNodeId,
                                 const std::string& exportNodeId)
{
    std::string masterId;
    if (ubse::election::UbseGetMasterNodeId(masterId) != UBSE_OK) {
        return false;
    }
    std::vector<std::string> commonRoleIds{importNodeId, exportNodeId};
    if (CheckHasMemOwnerPermission(memUds, reqUds, realRequestNodeId, commonRoleIds)) {
        return true;
    }
    std::vector<std::string> rootRoleIds{masterId, importNodeId};
    if (CheckHasRootPermission(reqUds, realRequestNodeId, rootRoleIds)) {
        return true;
    }
    return false;
}

bool CheckShareReturnPermission(const UbseUdsInfo& memUds, const UbseUdsInfo& reqUds,
                                const std::string& realRequestNodeId, const UbseShmRegionDesc& shareRegion)
{
    std::string masterId;
    if (ubse::election::UbseGetMasterNodeId(masterId) != UBSE_OK) {
        return false;
    }
    std::vector<std::string> commonRoleIds{};
    for (const auto& node : shareRegion.nodelist) {
        commonRoleIds.push_back(node.nodeId);
    }
    if (isRequestNodeIdValid(realRequestNodeId, commonRoleIds)) {
        return true;
    }
    std::vector<std::string> rootRoleIds{masterId};
    if (CheckHasRootPermission(reqUds, realRequestNodeId, rootRoleIds)) {
        return true;
    }
    return false;
}

bool CheckShareDetachPermission(const UbseUdsInfo& memUds, const UbseUdsInfo& reqUds,
                                const std::string& realRequestNodeId, const std::string& importNodeId)
{
    std::vector<std::string> commonRoleIds{importNodeId};
    if (CheckHasMemOwnerPermission(memUds, reqUds, realRequestNodeId, commonRoleIds)) {
        return true;
    }
    std::string masterId;
    if (ubse::election::UbseGetMasterNodeId(masterId) != UBSE_OK) {
        return false;
    }
    std::vector<std::string> rootRoleIds{masterId, importNodeId};
    if (CheckHasRootPermission(reqUds, realRequestNodeId, rootRoleIds)) {
        return true;
    }
    return false;
}

uint32_t WaitNodeStateWork(const std::string& importNode)
{
    auto nodeInfo = nodeController::UbseNodeController::GetInstance().GetNodeById(importNode);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "nodeId:" << importNode << "is not found";
        return UBSE_ERR_NODE_NOT_EXIST;
    }
    int nowTime = 0;
    while (nowTime < RETURN_RETRY_TIME) {
        if (nodeInfo.clusterState == nodeController::UbseNodeClusterState::UBSE_NODE_WORKING) {
            return UBSE_OK;
        }
        if (nodeInfo.clusterState != nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
            UBSE_LOG_ERROR << "nodeId=" << importNode << ", state=" << static_cast<int32_t>(nodeInfo.clusterState);
            return UBSE_ERR_NODE_UNREACHABLE;
        }
        ++nowTime;
        nodeInfo = nodeController::UbseNodeController::GetInstance().GetNodeById(importNode);
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
    }
    UBSE_LOG_WARN << "nodeId=" << importNode << "is still smoothing";
    return UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS;
}
} // namespace ubse::mem::controller
