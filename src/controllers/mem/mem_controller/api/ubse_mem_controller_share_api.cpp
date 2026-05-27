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
#include "ubse_mem_controller_share_api.h"

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_logger_audit.h"
#include "ubse_mem_advice.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mem_util.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_util.h"
#include "ubse_topo_util.h"
#include "../logging_lock_guard.h"
#include "../message/ubse_mem_share_borrow_exportobj_simpo.h"
#include "../message/ubse_mem_share_borrow_importobj_simpo.h"
#include "../ubse_mem_account.h"
#include "../ubse_mem_controller_ledger.h"
#include "../ubse_mem_rpc_processor.h"
#include "src/controllers/mem/mem_scheduler/ubse_mem_topology_info_manager.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::mem::scheduler;
using namespace ubse::nodeController;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::task_executor;
using namespace ubse::com;
using namespace message;
using namespace ubse::mmi;
using namespace ubse::mem::strategy;
using namespace ubse::mem::util;
using namespace ubse::mem::controller::debt;
const std::string ClusterHandlerKey = "NODE_CLUSTER_HDL";

void FindShareBorrowObjByNameWhenBorrow(const std::string& name, std::vector<UbseMemShareBorrowExportObj>& exportObjs,
                                        std::vector<UbseMemShareBorrowImportObj>& importObjs)
{
    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto& exportMap = ledger.GetDebtMap<UbseMemShareBorrowExportObj>();
    auto& importMap = ledger.GetDebtMap<UbseMemShareBorrowImportObj>();

    auto allExportNodeMaps = exportMap.GetAllNodeMaps();
    for (const auto& [nodeId, nodeMap] : allExportNodeMaps) {
        auto allExportResources = nodeMap->GetAll();
        for (const auto& [resId, objPtr] : allExportResources) {
            if (objPtr->req.name == name && (objPtr->status.state != UBSE_MEM_EXPORT_DESTROYED)) {
                UBSE_LOG_INFO << "obj state is " << static_cast<uint32_t>(objPtr->status.state);
                exportObjs.push_back(*objPtr);
            }
        }
    }

    auto allImportNodeMaps = importMap.GetAllNodeMaps();
    for (const auto& [nodeId, nodeMap] : allImportNodeMaps) {
        auto allImportResources = nodeMap->GetAll();
        for (const auto& [resId, objPtr] : allImportResources) {
            if (objPtr->req.name == name && (objPtr->status.state != UBSE_MEM_IMPORT_DESTROYED)) {
                UBSE_LOG_INFO << "obj state is " << static_cast<uint32_t>(objPtr->status.state);
                importObjs.push_back(*objPtr);
            }
        }
    }
}

void NodeControllerReadLock(const UbseMemShareBorrowReq& req)
{
    if (!req.baseNodeId.empty()) {
        UbseNodeControllerLockMgr::ReadLock(req.baseNodeId);
    }
}

void NodeControllerReadUnLock(const UbseMemShareBorrowReq& req)
{
    if (!req.baseNodeId.empty()) {
        UbseNodeControllerLockMgr::ReadUnLock(req.baseNodeId);
    }
}

uint32_t SetNodeIndex(UbseMemShareBorrowReq& req)
{
    int index = 0;
    for (ubse::adapter_plugins::mmi::UbseNodeInfo& nodeInfo : req.shmRegion.nodelist) {
        ubse::nodeController::UbseNodeInfo node = UbseNodeController::GetInstance().GetNodeById(nodeInfo.nodeId);
        if (node.nodeId.empty()) {
            UBSE_LOG_WARN << "[MMC] Failed to SetNodeIndex, nodeId= " << nodeInfo.nodeId;
            continue;
        }
        // 使用nodeId作为Index
        try {
            nodeInfo.index = std::stoi(node.nodeId) - 1;
        } catch (const std::invalid_argument& e) {
            nodeInfo.index = index++;
            UBSE_LOG_ERROR << "Invalid argument: " << e.what();
        } catch (const std::out_of_range& e) {
            nodeInfo.index = index++;
            UBSE_LOG_ERROR << "Out of range: " << e.what();
        }
    }
    return UBSE_OK;
}

void NormalizeShareRegion(UbseMemShareBorrowReq& req)
{
    if (req.shmRegion.nodeNum != 0 || !req.shmRegion.nodelist.empty()) {
        return;
    }

    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    req.shmRegion.nodeNum = nodeInfos.size();
    for (const auto& [_, nodeInfo] : nodeInfos) {
        ubse::adapter_plugins::mmi::UbseNodeInfo ubseNodeInfo{nodeInfo.slotId, nodeInfo.nodeId, nodeInfo.hostName};
        req.shmRegion.nodelist.push_back(ubseNodeInfo);
    }
}

static UbseResult ShareAllocate(const UbseMemShareBorrowReq& req, UbseMemShareBorrowExportObj& exportObj)
{
    uint8_t retryTimes = ALLOCATE_RETRY_TIME;
    auto ret = UBSE_OK;
    while (retryTimes--) {
        NodeControllerReadLock(req);
        ret = UbseMemShmExportObjStateChangeHandler(exportObj);
        NodeControllerReadUnLock(req);
        if (ret == UBSE_SCHEDULER_ERROR_NODE_RECONCILE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
            UBSE_LOG_WARN << "retry time is " << ALLOCATE_RETRY_TIME - retryTimes << ", no node can be lend";
            continue;
        }
        break;
    }
    return ret;
}

UbseResult AgentSendShareExportObj(const std::shared_ptr<UbseComModule>& comModule, SendParam& sendParam,
                                   UbseMemShareBorrowExportobjSimpoPtr& ptr, UbseBaseMessagePtr& ubseResponsePtr,
                                   const UbseMemShareBorrowExportObj& exportObj)
{
    const uint32_t maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION;
    auto ret = UBSE_ERROR;
    uint32_t retryCount = 0;
    while (ret != UBSE_OK && retryCount < maxRetryTimes) {
        std::string masterId{};
        ret = UbseGetMasterNodeId(masterId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get master nodeId failed, " << FormatRetCode(ret);
            retryCount++;
            sleep(SEND_RETRY_DURATION);
            continue;
        }
        sendParam.SetRemoteId(masterId);
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        if (ret == UBSE_OK) {
            break;
        }
        UBSE_LOG_ERROR << "Send to exportObj, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
        retryCount++;
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

UbseResult SendShareExportObj(const UbseMemShareBorrowExportObj& exportObj, const bool isMaster,
                              const std::string& nodeId = "")
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule."
                       << ", requestId=" << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_EXPORT_OBJ_CALLBACK));
    UbseMemShareBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemShareBorrowExportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemShareBorrowExportobj(exportObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ubseResponsePtr, requestId=" << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send exportObj, name is " << exportObj.req.name
                              << ", requestNodeId=" << exportObj.req.requestNodeId
                              << ", requestId=" << exportObj.req.requestId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to exportObj, name=" << exportObj.req.name
                           << ", requestNodeId=" << exportObj.req.requestNodeId << ", wait to retry"
                           << ", requestId=" << exportObj.req.requestId;
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to exportObj, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId
                       << ", requestId=" << exportObj.req.requestId;
        return ret;
    }

    // 履行侧发回
    return AgentSendShareExportObj(comModule, sendParam, ptr, ubseResponsePtr, exportObj);
}

uint32_t HandleSendExportError(UbseMemOperationResp& resp, const UbseMemShareBorrowReq& req,
                               const UbseMemShareBorrowExportObj& exportObj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(
        exportObj.algoResult.exportNumaInfos[0].nodeId, req.name);

    auto copy = exportObj;
    copy.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemShmExportObjStateChangeHandler(copy);

    return BuildOperationRespWhenFail(
        resp, req.name, req.requestNodeId,
        "Failed to Send export, exportNodeId is " + exportObj.algoResult.exportNumaInfos[0].nodeId, UBSE_ERR_INTERNAL,
        MemOperationType::SHARED_BORROW);
}

bool ValidateAffinityParams(const UbseMemShareBorrowReq& req)
{
    if (!req.withAffinity.enableCreateWithAffinity) {
        return true;
    }
    if (req.withAffinity.createReqNodeId.empty()) {
        UBSE_LOG_ERROR << "Invalid affinity parameters: nodeId is empty" << req.withAffinity.createReqNodeId
                       << ",socketId=" << req.withAffinity.affinitySocketId;
        return false;
    }
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(req.withAffinity.createReqNodeId);
    bool found = false;
    for (const auto& [location, info] : nodeInfo.numaInfos) {
        if (info.socketId == req.withAffinity.affinitySocketId) {
            found = true;
            break;
        }
    }
    if (!found) {
        UBSE_LOG_ERROR << "Invalid affinity parameters: socketId=" << req.withAffinity.affinitySocketId;
    }
    return found;
}

void RegisterExportObjectDebtInfo(const UbseMemShareBorrowExportObj& exportObj, const std::string& name)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(
        exportObj.algoResult.exportNumaInfos[0].nodeId, name, exportObj);
}

static uint32_t ShareBorrowFailed(const UbseMemShareBorrowReq& req, UbseMemOperationResp& resp, const std::string& msg,
                                  uint32_t errCode, MemAdvice advice)
{
    BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "SHARE_BORROW", req.size, "", "", errCode, advice);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, msg, errCode, MemOperationType::SHARED_BORROW);
}

uint32_t UbseMemShareBorrow(const UbseMemShareBorrowReq& req, UbseMemOperationResp& resp)
{
    UbseMemShareBorrowReq normalizedReq = req;
    NormalizeShareRegion(normalizedReq);

    UBSE_LOG_INFO << "Share borrow begins, name=" << normalizedReq.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << normalizedReq.requestId;
    auto lock = LoggingLockGuard(normalizedReq.name);
    auto requestNodeId = normalizedReq.requestNodeId;
    auto name = normalizedReq.name;
    resp.name = name;
    resp.requestId = normalizedReq.requestId;
    if (!IsMemShareModeFeatureSupported(normalizedReq.ubseMemPrivData.cacheableFlag)) {
        return BuildMemFeatureNotSupportedResp(resp, name, requestNodeId, MemOperationType::SHARED_BORROW);
    }
    std::vector<UbseMemShareBorrowExportObj> exportObjs;
    std::vector<UbseMemShareBorrowImportObj> importObjs;
    FindShareBorrowObjByNameWhenBorrow(name, exportObjs, importObjs);
    if (!exportObjs.empty() || !importObjs.empty()) {
        return ShareBorrowFailed(normalizedReq, resp, "Resource Exist.", UBSE_ERR_EXISTED, MemAdvice::RESOURCE_EXIST);
    }
    if (!ValidateAffinityParams(normalizedReq)) {
        return ShareBorrowFailed(normalizedReq, resp, "Invalid Affinity parameters",
                                 UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL, MemAdvice::CHECK_FAILED);
    }
    UbseMemShareBorrowExportObj exportObj;
    exportObj.req = normalizedReq;
    exportObj.status.state = UBSE_MEM_SCHEDULING;
    if (SetNodeIndex(exportObj.req) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MMC] Failed to SetNodeIndex, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << req.requestId;
        return ShareBorrowFailed(normalizedReq, resp, "SetNodeIndex Failed.", UBSE_ERR_INTERNAL,
                                 MemAdvice::SCHEDULE_FAILED);
    }
    auto ret = ShareAllocate(normalizedReq, exportObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MMC] Failed to allocate, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", " << FormatRetCode(ret)
                       << ", requestId=" << resp.requestId;
        return ShareBorrowFailed(normalizedReq, resp, "Failed to allocate", UBSE_ERR_ALLOCATE,
                                 MemAdvice::SCHEDULE_FAILED);
    }
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    RegisterExportObjectDebtInfo(exportObj, name);
    ret = SendShareExportObj(exportObj, true, exportObj.algoResult.exportNumaInfos[0].nodeId);
    if (ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, normalizedReq.name, "SHARE_BORROW", normalizedReq.size,
                           exportObj.algoResult.exportNumaInfos[0].nodeId, "", ret, MemAdvice::COMM_FAILED);
        return HandleSendExportError(resp, normalizedReq, exportObj);
    }
    return UBSE_OK;
}

void FindShareBorrowObjByName(const std::string& name, std::vector<UbseMemShareBorrowExportObj>& exportObjs,
                              std::vector<UbseMemShareBorrowImportObj>& importObjs)
{
    auto [exportObjTmp, importObjTmps] = GetMaxRefCountExportObj(name);
    if (exportObjTmp) {
        UBSE_LOG_INFO << "obj_state=" << static_cast<uint32_t>(exportObjTmp->status.state);
        if (exportObjTmp->status.state == UBSE_MEM_EXPORT_SUCCESS) {
            exportObjs.push_back(*exportObjTmp);
        }
    }
    for (const auto& importObjTmp : importObjTmps) {
        UBSE_LOG_INFO << "obj_state=" << static_cast<uint32_t>(importObjTmp->status.state);
        if (importObjTmp->status.state != UBSE_MEM_IMPORT_DESTROYED) {
            importObjs.push_back(*importObjTmp);
        }
    }
    if (!exportObjs.empty() || !importObjs.empty()) {
        UBSE_LOG_INFO << "export_obj_size=" << exportObjs.size() << ", import_obj_size=" << importObjs.size();
    }
}

bool CheckRegions(const UbseMemShareAttachReq& req, const UbseMemShareBorrowExportObj& exportObj)
{
    UBSE_LOG_INFO << "Share import node id=" << req.importNodeId << ", requestId=" << req.requestId;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Export's exportNumaInfo is empty, requestId=" << req.requestId;
        return false;
    }
    for (const auto& node : exportObj.req.shmRegion.nodelist) {
        UBSE_LOG_INFO << "Share region node=" << node.nodeId << ", requestId=" << req.requestId;
        if (node.nodeId == req.importNodeId) {
            return true;
        }
    }
    UBSE_LOG_ERROR << "Failed to check share regions, requestId=" << req.requestId;
    return false;
}

bool ExistImportObj(const std::string& name, const std::string& nodeId,
                    const std::vector<UbseMemShareBorrowImportObj>& existImportObjs,
                    UbseMemShareBorrowImportObj& importObj)
{
    for (const auto& existImportObj : existImportObjs) {
        if (existImportObj.req.name == name && existImportObj.importNodeId == nodeId &&
            existImportObj.status.state != UBSE_MEM_IMPORT_DESTROYED) {
            importObj = existImportObj;
            return true;
        }
    }
    return false;
}

uint32_t ExistImportObjHandler(const UbseMemShareAttachReq& req, UbseMemShareBorrowImportObj importObj,
                               UbseMemOperationResp& resp, const std::string& name, const std::string& requestNodeId)
{
    for (const auto importResult : importObj.status.importResults) {
        resp.memIdList.push_back(importResult.memId);
    }
    if (!importObj.status.importResults.empty()) {
        resp.remoteNumaId = importObj.status.importResults[0].numaId;
    }
    resp.name = name;
    resp.requestNodeId = requestNodeId;
    uint64_t realSize{};
    for (const auto& numaInfo : importObj.algoResult.exportNumaInfos) {
        SafeAdd(realSize, numaInfo.size, realSize);
    }
    resp.realSize = std::to_string(realSize);
    return BuildOperationRespWhenFail(resp, name, requestNodeId, "The importNodeId has attached.", UBSE_ERR_EXISTED,
                                      MemOperationType::SHARED_ATTACH);
}

UbseResult ShmAttachPreCheck(const UbseMemShareAttachReq& req, UbseMemOperationResp& resp,
                             const std::vector<UbseMemShareBorrowExportObj>& exportObjs,
                             const std::vector<UbseMemShareBorrowImportObj>& importObjs,
                             UbseMemShareBorrowImportObj importObj)
{
    if (exportObjs.size() != 1 || exportObjs[0].algoResult.exportNumaInfos.empty() ||
        exportObjs[0].status.exportObmmInfo.empty()) {
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, req.name, "SHARE_BORROW", req.size, "", req.importNodeId,
                           UBSE_ERR_NOT_EXIST, MemAdvice::CHECK_FAILED);
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "exportObj is Invaild.", UBSE_ERR_NOT_EXIST,
                                   MemOperationType::SHARED_ATTACH);
        return UBSE_ERROR;
    }
    if (!CheckRegions(req, exportObjs[0])) {
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, req.name, "SHARE_BORROW", req.size, "", req.importNodeId,
                           UBSE_ERR_INTERNAL, MemAdvice::CHECK_FAILED);
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "The node is not true region.", UBSE_ERR_INTERNAL,
                                   MemOperationType::SHARED_ATTACH);
        return UBSE_ERROR_ACCES;
    }
    if (!exportObjs[0].req.udsInfo.CheckPermission(req.udsInfo)) {
        UBSE_LOG_ERROR << "name=" << req.name << " auth failed,req username=" << req.udsInfo.username
                       << ", req uid=" << req.udsInfo.uid
                       << ", export obj username=" << exportObjs[0].req.udsInfo.username
                       << ", export obj uid=" << exportObjs[0].req.udsInfo.uid;
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, req.name, "SHARE_BORROW", req.size, "", req.importNodeId,
                           UBSE_ERR_INTERNAL, MemAdvice::CHECK_FAILED);
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Error auth", UBSE_ERR_AUTH_FAILED,
                                   MemOperationType::SHARED_ATTACH);
        return UBSE_ERROR_ACCES;
    }
    if (ExistImportObj(req.name, req.importNodeId, importObjs, importObj)) {
        ExistImportObjHandler(req, importObj, resp, req.name, req.importNodeId);
        return UBSE_ERR_EXISTED;
    }
    return UBSE_OK;
}

void ShareImportUpdateState(UbseMemShareBorrowImportObj& importObj, const UbseMemState& state)
{
    importObj.status.state = state;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(
        importObj.importNodeId, importObj.req.name, importObj);
}

void ConstructShareImportObj(UbseMemShareBorrowImportObj& importObj, const UbseMemShareAttachReq& req)
{
    if (req.size == 0) {
        importObj.shareAttr.size = importObj.req.size;
    } else {
        importObj.shareAttr.size = req.size;
        importObj.req.size = req.size;
    }
    importObj.shareAttr.gid = req.udsInfo.gid;
    importObj.shareAttr.uid = req.udsInfo.uid;
    importObj.shareAttr.owner.uid = req.owner.uid;
    importObj.shareAttr.owner.gid = req.owner.gid;
    importObj.shareAttr.owner.pid = req.owner.pid;
    importObj.shareAttr.owner.mode = req.owner.mode;
    importObj.importNodeId = req.importNodeId;
    importObj.req.name = req.name;
    importObj.req.udsInfo = req.udsInfo;
    importObj.req.requestId = req.requestId;
    importObj.req.requestNodeId = req.requestNodeId;
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
}

UbseResult AgentSendShareImportObj(const std::shared_ptr<UbseComModule>& comModule, SendParam& sendParam,
                                   UbseMemShareBorrowImportobjSimpoPtr& ptr, UbseBaseMessagePtr& ubseResponsePtr,
                                   const UbseMemShareBorrowImportObj& importObj)
{
    const uint32_t maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION;
    auto ret = UBSE_ERROR;
    uint32_t retryCount = 0;
    while (ret != UBSE_OK && retryCount < maxRetryTimes) {
        std::string masterId{};
        ret = UbseGetMasterNodeId(masterId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get master nodeId failed, " << FormatRetCode(ret);
            retryCount++;
            sleep(SEND_RETRY_DURATION);
            continue;
        }
        sendParam.SetRemoteId(masterId);
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        if (ret == UBSE_OK) {
            break;
        }
        UBSE_LOG_ERROR << "Send to importObj, name=" << importObj.req.name
                       << ", requestNodeId=" << importObj.req.requestNodeId << ", requestId=" << importObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
        retryCount++;
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

UbseResult SendShareImportObj(const UbseMemShareBorrowImportObj& importObj, const bool isMaster,
                              const std::string& nodeId = "")
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule."
                       << ", requestId=" << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_IMPORT_OBJ_CALLBACK));
    UbseMemShareBorrowImportobjSimpoPtr ptr = new (std::nothrow) UbseMemShareBorrowImportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr."
                       << ", requestId=" << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemShareBorrowImportobj(importObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ubseResponsePtr."
                       << ", requestId=" << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send importObj, name=" << importObj.req.name
                              << ", requestNodeId=" << importObj.req.requestNodeId
                              << ", requestId=" << importObj.req.requestId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to importObj, name=" << importObj.req.name
                           << ", requestNodeId=" << importObj.req.requestNodeId << ", wait to retry"
                           << ", requestId=" << importObj.req.requestId;
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to importObj, name=" << importObj.req.name
                       << ", requestNodeId=" << importObj.req.requestNodeId
                       << ", requestId=" << importObj.req.requestId;
        return ret;
    }

    // 履行侧发回
    return AgentSendShareImportObj(comModule, sendParam, ptr, ubseResponsePtr, importObj);
}

std::string GetChipInfoBySocketId(const ubse::nodeController::UbseNodeInfo& nodeInfo, uint32_t socketId)
{
    for (const auto& [loc, cpuInfo] : nodeInfo.cpuInfos) {
        if (cpuInfo.socketId == socketId) {
            UBSE_LOG_INFO << "chipId=" << cpuInfo.chipId;
            return cpuInfo.chipId;
        }
    }
    UBSE_LOG_ERROR << "Can't find chipId in nodeInfo, socketId=" << socketId << " node Id=" << nodeInfo.nodeId;
    return "";
}

void GetPortCnaByPortId(const nodeController::UbseNodeInfo& remoteInfo, const std::string& importSlotId,
                        const std::string& importChipId, const uint32_t portId, uint32_t& portCna)
{
    for (const auto& cpuInfo : remoteInfo.cpuInfos) {
        for (const auto& [key, portInfo] : cpuInfo.second.portInfos) {
            if (portInfo.remoteSlotId != importSlotId || portInfo.remoteChipId != importChipId ||
                portInfo.portId != std::to_string(portId)) {
                continue;
            }
            portCna = portInfo.portCna;
            return;
        }
    }
    portCna = UINT32_MAX;
}

void GetMinPortInfoByCpuInfo(const nodeController::UbseNodeInfo& remoteInfo, const std::string& importSlotId,
                             const std::string& importChipId, uint32_t& minPortId, uint32_t& minPortCna)
{
    for (const auto& cpuInfo : remoteInfo.cpuInfos) {
        for (const auto& [key, portInfo] : cpuInfo.second.portInfos) {
            if (portInfo.remoteSlotId != importSlotId || portInfo.remoteChipId != importChipId) {
                continue;
            }
            uint32_t selfPortId{};
            auto ret = ubse::utils::ConvertStrToUint32(portInfo.portId, selfPortId);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "portGroupIdStr(" << portInfo.portId << ") is invalid.";
                minPortId = UINT32_MAX;
                return;
            }
            minPortId > selfPortId ? minPortId = selfPortId, minPortCna = portInfo.portCna : 0;
        }
    }
}

uint32_t GetPortInfo(const std::string& importNodeId, const UbseMemShareBorrowImportObj& importObj,
                     const std::string& remoteNode, uint32_t& portId, uint32_t& portCna)
{
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(importNodeId);
    auto remoteInfo = UbseNodeController::GetInstance().GetNodeById(remoteNode);
    std::string importChipId{};
    ubse::nodeController::UbseNodeMemCnaInfoInput cnaInput{};
    cnaInput.borrowNodeId = importNodeId;
    cnaInput.exportNodeId = remoteNode;
    cnaInput.exportSocketId = std::to_string(importObj.algoResult.exportNumaInfos[0].socketId);
    ubse::nodeController::UbseNodeMemCnaInfoOutput cnaOutput;
    if (auto ret = ubse::nodeController::UbseNodeMemGetTopologyCnaInfo(cnaInput, cnaOutput); ret != 0) {
        UBSE_LOG_ERROR << "Failed to get cna, borrowNodeId=" << cnaInput.borrowNodeId
                       << ", exportNodeId=" << cnaInput.exportNodeId << ", socketId=" << cnaInput.exportSocketId;
        return UBSE_ERROR;
    }

    uint32_t borrowSocketId{};
    auto ret = ubse::utils::ConvertStrToUint32(cnaOutput.borrowSocketId, borrowSocketId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "portGroupIdStr(" << cnaOutput.borrowSocketId << ") is invalid.";
        return ret;
    }

    importChipId = GetChipInfoBySocketId(nodeInfo, borrowSocketId);
    if (importChipId.empty()) {
        return UBSE_ERROR;
    }
    if (importObj.req.lenderInfo.portId != UINT32_MAX) {
        GetPortCnaByPortId(remoteInfo, cnaInput.borrowNodeId, importChipId, portId, portCna);
    } else {
        GetMinPortInfoByCpuInfo(remoteInfo, cnaInput.borrowNodeId, importChipId, portId, portCna);
    }
    UBSE_LOG_INFO << "portCna=" << portCna << ", portId=" << portId;
    if (portCna == UINT32_MAX) {
        UBSE_LOG_ERROR << "Failed Get minPortCna from topoInfo";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t GetCnaTopoByPeerNodeInfo(const UbseMemShareAttachReq& req, const UbseMemShareBorrowExportObj& exportObj,
                                  UbseMemOperationResp& resp, UbseMemShareBorrowImportObj& importObj)
{
    auto remoteNode = exportObj.algoResult.exportNumaInfos[0].nodeId;
    if (exportObj.algoResult.exportNumaInfos[0].nodeId == req.importNodeId) {
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "req info: importNodeId=" << req.importNodeId << ", exportNodeId=" << remoteNode
                  << " export socketId=" << exportObj.algoResult.exportNumaInfos[0].socketId;
    auto ret = GetCnaInfoWhenImport(exportObj.algoResult.exportNumaInfos[0].nodeId, req.importNodeId, importObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get cna info when import, " << FormatRetCode(ret)
                       << ", requestId=" << req.requestId;
        return UBSE_ERROR;
    }
    // 多路径情况下，会给dcna重新赋值，当指定port时以指定port为准，否则选择直连导入socket的最小端口号，作为单路径路由表的配置
    if (IsSameSocketMultiPortTopo()) {
        uint32_t portId = importObj.req.lenderInfo.portId;
        uint32_t portCna{};
        if (GetPortInfo(req.importNodeId, importObj, remoteNode, portId, portCna) != UBSE_OK) {
            return UBSE_ERROR;
        }
        for (auto& obmmInfo : importObj.exportObmmInfo) {
            obmmInfo.desc.dcna = portCna;
            obmmInfo.desc.marId = portId / 4; // portId / 4 能得到marId
        }
    }
    return UBSE_OK;
}

uint32_t PrepareShareAttachImportObj(const UbseMemShareAttachReq& req, UbseMemOperationResp& resp,
                                     UbseMemShareBorrowImportObj& importObj)
{
    std::vector<UbseMemShareBorrowExportObj> exportObjs{};
    std::vector<UbseMemShareBorrowImportObj> importObjs{};
    FindShareBorrowObjByName(req.name, exportObjs, importObjs);
    if (const auto ret = ShmAttachPreCheck(req, resp, exportObjs, importObjs, importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "precheck failed, " << FormatRetCode(ret) << ", requestId=" << req.requestId;
        return ret;
    }
    if (!IsMemShareModeFeatureSupported(exportObjs[0].req.ubseMemPrivData.cacheableFlag)) {
        return BuildMemFeatureNotSupportedResp(resp, req.name, req.requestNodeId, MemOperationType::SHARED_ATTACH);
    }

    importObj.exportObmmInfo = exportObjs[0].status.exportObmmInfo;
    importObj.algoResult = exportObjs[0].algoResult;
    importObj.req = exportObjs[0].req;
    if (GetCnaTopoByPeerNodeInfo(req, exportObjs[0], resp, importObj) == UBSE_OK) {
        return UBSE_OK;
    }

    BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "SHARE_BORROW", req.size,
                       importObj.algoResult.exportNumaInfos[0].nodeId, req.importNodeId, UBSE_ERR_INTERNAL,
                       MemAdvice::INTERNAL_FAILED);
    return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to get cna info when import",
                                      UBSE_ERR_INTERNAL, MemOperationType::SHARED_ATTACH);
}

uint32_t UbseMemShareAttach(const UbseMemShareAttachReq& req, UbseMemOperationResp& resp)
{
    UBSE_LOG_INFO << "Share attach begins, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(req.name + "_" + req.requestNodeId);
    // deleteAndBorrowLock 避免attach时获取到export对象后, delete紧跟着对export进行删除,导致单边导入账本
    auto deleteAndBorrowLock = LoggingLockGuard(req.name, LoggingLockGuard::LockType::READ);
    resp.requestId = req.requestId;
    if (!IsMemShareFeatureSupported()) {
        return BuildMemFeatureNotSupportedResp(resp, req.name, req.requestNodeId, MemOperationType::SHARED_ATTACH);
    }
    if (req.importNodeId.empty()) {
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "attach with no node is valid.",
                                          UBSE_ERR_SHM_NODE_EMPTY, MemOperationType::SHARED_ATTACH);
    }
    UbseNodeControllerLockMgr::WriteLock(ClusterHandlerKey);
    UbseMemShareBorrowImportObj importObj{};
    auto ret = PrepareShareAttachImportObj(req, resp, importObj);
    if (ret != UBSE_OK) {
        UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
        return ret;
    }
    importObj.req.trustRingData.ClearReqSignedDataMemory(); // 清除import对象里请求签名信息
    UBSE_LOG_INFO << "import size=" << importObj.req.size << ", requestId=" << req.requestId;
    ConstructShareImportObj(importObj, req);
    UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
    if (auto ret = SendShareImportObj(importObj, true, req.importNodeId); ret != UBSE_OK) {
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().RemoveResource(
            importObj.importNodeId, req.name);
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "SHARE_BORROW", req.size,
                           importObj.algoResult.exportNumaInfos[0].nodeId, req.importNodeId, ret,
                           MemAdvice::COMM_FAILED);
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to Send import", UBSE_ERR_INTERNAL,
                                          MemOperationType::SHARED_ATTACH);
    }
    return UBSE_OK;
}

static uint32_t ShareDetachFailed(const UbseMemShareDetachReq& req, UbseMemOperationResp& resp, const std::string& msg,
                                  uint32_t errCode, MemAdvice advice)
{
    BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, "", req.unImportNodeId, errCode,
                       advice);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, msg, errCode, MemOperationType::SHARED_DETACH);
}
static uint32_t ShareDetachRollback(UbseMemShareBorrowImportObj& importObj, const UbseMemShareDetachReq& req,
                                    UbseMemOperationResp& resp, uint32_t ret)
{
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, "", req.unImportNodeId, ret,
                       MemAdvice::COMM_FAILED);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to Send import", UBSE_ERR_INTERNAL,
                                      MemOperationType::SHARED_DETACH);
}

uint32_t UbseMemShareDetach(const UbseMemShareDetachReq& req, UbseMemOperationResp& resp,
                            const std::string& realRequestNodeId)
{
    UBSE_LOG_INFO << "Share detach begins, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId << ", realRequestNodeId=" << realRequestNodeId;
    auto lock = LoggingLockGuard(req.name + "_" + req.requestNodeId);
    resp.requestId = req.requestId;
    if (!IsMemShareFeatureSupported()) {
        return BuildMemFeatureNotSupportedResp(resp, req.name, req.requestNodeId, MemOperationType::SHARED_DETACH);
    }
    if (req.unImportNodeId.empty()) {
        return ShareDetachFailed(req, resp, "Detach with no node is valid.", UBSE_ERR_SHM_NODE_EMPTY,
                                 MemAdvice::NODE_IN_MAINTENANCE);
    }
    auto waitResult = WaitNodeStateWork(req.unImportNodeId);
    if (waitResult != UBSE_OK) {
        return ShareDetachFailed(req, resp, "importNode is not ok", waitResult, MemAdvice::NODE_IN_MAINTENANCE);
    }
    std::vector<UbseMemShareBorrowExportObj> exportObjs{};
    std::vector<UbseMemShareBorrowImportObj> importObjs{};
    UbseMemShareBorrowImportObj importObj{};
    FindShareBorrowObjByName(req.name, exportObjs, importObjs);
    if (!ExistImportObj(req.name, req.unImportNodeId, importObjs, importObj)) {
        return ShareDetachFailed(req, resp, "Detach is not allowed, because the node is not attach.",
                                 UBSE_ERR_SHM_NO_ATTACH, MemAdvice::RESOURCE_NOT_EXIST);
    }
    UbseMemStage memStage = GetMemStageByShareImportObjState(importObj, true);
    if (memStage == UbseMemStage::UBSE_CREATING || memStage == UbseMemStage::UBSE_DELETING) {
        UBSE_LOG_INFO << "resource is being borrowed or returned, name=" << req.name;
        auto ret = (memStage == UbseMemStage::UBSE_CREATING) ? UBSE_ERR_CREATING : UBSE_ERR_DELETING;
        return ShareDetachFailed(req, resp, "resource being borrowed or returned", ret,
                                 MemAdvice::RESOURCE_OPERATION_CONFLICT);
    }
    if (!CheckShareDetachPermission(importObj.req.udsInfo, req.udsInfo, realRequestNodeId, importObj.importNodeId)) {
        UBSE_LOG_ERROR << "name=" << req.name << " auth failed,req username=" << req.udsInfo.username
                       << ", req uid=" << req.udsInfo.uid
                       << ", importObj obj username=" << importObj.req.udsInfo.username
                       << ", import obj uid=" << importObj.req.udsInfo.uid << "importObj.importNodeId"
                       << importObj.importNodeId << ", realRequestNodeId=" << realRequestNodeId;
        return ShareDetachFailed(req, resp, "Error auth.", UBSE_ERR_AUTH_FAILED,
                                 MemAdvice::UBSE_NO_OPERATION_PERMISSION);
    }
    importObj.req.requestId = req.requestId;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    importObj.isDestroyedReportReceived = false;
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    //  下发importObj;
    if (auto ret = SendShareImportObj(importObj, true, req.unImportNodeId); ret != UBSE_OK) {
        return ShareDetachRollback(importObj, req, resp, ret);
    }
    return UBSE_OK;
}

void ShareExportUpdateState(UbseMemShareBorrowExportObj& exportObj, const UbseMemState& state)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    exportObj.status.state = state;
    UBSE_LOG_INFO << "Update share export state, name=" << exportObj.req.name << ", state=" << state
                  << ", requestId=" << exportObj.req.requestId;
    auto& ledger = UbseMemDebtLedger::GetInstance();
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(exportObj.algoResult.exportNumaInfos[0].nodeId,
                                                                 exportObj.req.name, exportObj);
}

void EraseShareExport(const UbseMemShareBorrowExportObj& exportObj)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto name = exportObj.req.name;
    UBSE_LOG_INFO << "Erase share export, name=" << exportObj.req.name << ", requestId=" << exportObj.req.requestId;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(exportNodeId, name);
}

void EraseShareImport(const UbseMemShareBorrowImportObj& importObj)
{
    auto name = importObj.req.name;
    auto importNodeId = importObj.importNodeId;
    UBSE_LOG_INFO << "Erase share import, name=" << importObj.req.name << ", requestId=" << importObj.req.requestId;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().RemoveResource(importNodeId, name);
}

void ShareExportFillResp(UbseMemOperationResp& resp, const UbseMemShareBorrowExportObj& exportObj)
{
    for (const auto obmmInfo : exportObj.status.exportObmmInfo) {
        resp.memIdList.push_back(obmmInfo.memId);
    }
}

uint32_t SendShareExport(const UbseMemShareBorrowExportObj& exportObj, const std::string& name,
                         const std::string& exportNodeId, bool isMaster)
{
    auto res = SendShareExportObj(exportObj, isMaster);
    if (res != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::EXPORT_FAILED, name, "SHARE_BORROW", exportObj.req.size, exportNodeId, "", res,
                           MemAdvice::COMM_FAILED);
    }
    return res;
}

uint32_t ShareExportRunningAgentCallback(UbseMemOperationResp& resp, UbseMemShareBorrowExportObj& exportObj,
                                         const std::string& name, const std::string& requestNodeId,
                                         const std::string& exportNodeId)
{
    UBSE_LOG_INFO << "Share export running agent callback. name=" << name << ", requestId=" << exportObj.req.requestId;
    auto curNode = GetCurNodeId();
    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto existingObj = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(curNode, exportObj.req.name);
    if (existingObj && existingObj->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS) {
        return UBSE_OK;
    }
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_RUNNING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmExportExecutor(exportObj); ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::EXPORT_FAILED, exportObj.req.name, "SHARE_BORROW", exportObj.req.size,
                           exportNodeId, "", ret, MemAdvice::OBMM_FAILED);
        UBSE_LOG_ERROR << "Failed to export, name=" << name << ", requestNodeId=" << requestNodeId
                       << ", requestId=" << exportObj.req.requestId;
        exportObj.errorCode = ret;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        EraseShareExport(exportObj);
        return SendShareExport(exportObj, name, exportNodeId, false);
    }
    UBSE_LOG_INFO << "Success to export share, name=" << name << ", requestId=" << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << exportNodeId << " ShareMemory Export"
                             << std::to_string(exportObj.req.size) << "Bytes Success";
    // 高安配置下签名并验签
    if (IsHighSafety()) {
        UbseExportSignReq trustReq{exportObj.req.trustRingData.reqSignedData, "share"};
        trustReq.exportObmmInfo = exportObj.status.exportObmmInfo;
        trustReq.trustRingId = exportObj.req.trustRingData.trustRingId;
        if (const auto ret = UbseMemSignVerifier::SignAndVerify(trustReq, exportObj.req.trustRingData.lendSignedDatas);
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to sign for lend information, " << FormatRetCode(ret);
            EraseShareExport(exportObj);
            exportObj.errorCode = ret;
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            return SendShareExport(exportObj, name, exportNodeId, false);
        }
    }
    exportObj.req.trustRingData.ClearReqSignedDataMemory();
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return SendShareExport(exportObj, name, exportNodeId, false);
}

uint32_t ShareExportDestroyingAgentCallback(UbseMemOperationResp& resp, UbseMemShareBorrowExportObj& exportObj,
                                            const std::string& name, const std::string& requestNodeId,
                                            const std::string& exportNodeId)
{
    UBSE_LOG_INFO << "Share export Destroying agent callback. name=" << name
                  << ", requestId=" << exportObj.req.requestId;
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    auto objPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(exportNodeId, name);
    bool directReply = (objPtr == nullptr);
    if (directReply) {
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        if (auto ret = SendShareExportObj(exportObj, false); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                               MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmUnExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Success to unexport, name=" << name << ", requestId=" << exportObj.req.requestId;
        exportObj.errorCode = ret;
        ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                           MemAdvice::OBMM_FAILED);
        if (ret = SendShareExportObj(exportObj, false); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "SHARE_BORROW", 0, exportNodeId, requestNodeId, ret,
                               MemAdvice::COMM_FAILED);
        }
        return ret;
    }
    UBSE_LOG_INFO << "Success to unexport share, name=" << name << ", requestId=" << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << exportNodeId << " ShareMemory UnExport "
                               << std::to_string(exportObj.req.size) << " Bytes Success";
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    EraseShareExport(exportObj);
    if (auto ret = SendShareExportObj(exportObj, false); ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                           MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ShareExportAgentCallback(const std::string& exportNodeId, UbseMemShareBorrowExportObj& exportObj,
                                  const std::string& name)
{
    UBSE_LOG_INFO << "Share export agent callback name=" << name << ", state=" << exportObj.status.state
                  << ", requestId=" << exportObj.req.requestId;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    auto requestNodeId = exportObj.req.requestNodeId;
    if (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        return ShareExportRunningAgentCallback(resp, exportObj, name, requestNodeId, exportNodeId);
    }
    return ShareExportDestroyingAgentCallback(resp, exportObj, name, requestNodeId, exportNodeId);
}

static uint32_t ShareExportReturnCallback(const std::string& exportNodeId, UbseMemShareBorrowExportObj& exportObj,
                                          UbseMemOperationResp& resp)
{
    if (!HasAgentAlreadyReported<UbseMemShareBorrowExportObj>(
            exportObj.req.name, exportNodeId, &UbseMemShareBorrowExportObj::isDestroyedReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export destroyed, name=" << exportObj.req.name
                      << ", requestId=" << exportObj.req.requestId;
        return UBSE_OK;
    }
    auto req = exportObj.returnReq;
    std::string requestNodeId = req.requestNodeId;
    resp.requestNodeId = requestNodeId;
    resp.requestId = req.requestId;
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseShareExport(exportObj);
        UBSE_LOG_INFO << "shm callback exportObjStateChange, name=" << exportObj.req.name
                      << ", requestId=" << exportObj.req.requestId;
        UbseMemShmExportObjStateChangeHandler(exportObj);
        UBSE_LOG_INFO << "shm return, name=" << exportObj.req.name << ", requestId=" << exportObj.req.requestId;
        // requestNodeId为空则当前场景为对账删除导出账本
        if (requestNodeId.empty()) {
            return UBSE_OK;
        }
        if (auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_RETURN); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::RETURN_FAILED, exportObj.req.name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                               MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
    // 归还失败
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    // requestNodeId为空则当前场景为对账删除导出账本
    if (requestNodeId.empty()) {
        return UBSE_OK;
    }
    if (auto ret = BuildOperationRespWhenFail(resp, exportObj.req.name, requestNodeId, "Failed to unexport",
                                              exportObj.errorCode, MemOperationType::SHARED_RETURN);
        ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, exportObj.req.name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                           MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ShareExportMasterCallback(const std::string& exportNodeId, UbseMemShareBorrowExportObj& exportObj)
{
    UBSE_LOG_INFO << "Share export master callback name=" << exportObj.req.name << ", state=" << exportObj.status.state
                  << ", requestId=" << exportObj.req.requestId;
    auto lock = LoggingLockGuard(exportObj.req.name);
    auto name = exportObj.req.name;
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_SUCCESS) {
        if (!HasAgentAlreadyReported<UbseMemShareBorrowExportObj>(
                exportObj.req.name, exportNodeId, &UbseMemShareBorrowExportObj::isCreateReportReceived)) {
            UBSE_LOG_INFO << "No need to callback for export created, name=" << exportObj.req.name
                          << ", requestId=" << exportObj.req.requestId;
            return UBSE_OK;
        }
        auto copy = exportObj;
        if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            EraseShareExport(exportObj);
            copy.status.state = UBSE_MEM_STATE_FAILED;
            UbseMemShmExportObjStateChangeHandler(copy);
            return BuildOperationRespWhenFail(resp, exportObj.req.name, exportObj.req.requestNodeId, "Failed to export",
                                              exportObj.errorCode, MemOperationType::SHARED_BORROW);
        }
        ShareExportFillResp(resp, exportObj);
        ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        UbseMemShmExportObjStateChangeHandler(copy);
        UBSE_LOG_INFO << "this is shm callback before shm exportObjstateChange, name=" << exportObj.req.name
                      << ", requestId=" << exportObj.req.requestId;
        return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_BORROW);
    }
    // 归还逻辑
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_DESTROYED) {
        return ShareExportReturnCallback(exportNodeId, exportObj, resp);
    }
    return UBSE_OK;
}

uint32_t UbseMemShareBorrowExportObjCallback(const UbseMemShareBorrowExportObj& exportObj)
{
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Failed to get master's info, " << FormatRetCode(ret);
        return ret;
    }
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The exportObj with no export numa info will be ignored.";
        return UBSE_ERROR;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto name = exportObj.req.name;
    auto copy = exportObj;

    if (exportNodeId == currentNodeInfo.nodeId &&
        (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING)) {
        return ShareExportAgentCallback(exportNodeId, copy, name);
    }
    return ShareExportMasterCallback(exportNodeId, copy);
}
void ShareImportFillResp(UbseMemOperationResp& resp, const UbseMemShareBorrowImportObj& importObj)
{
    for (const auto& importResult : importObj.status.importResults) {
        resp.memIdList.push_back(importResult.memId);
    }
    uint64_t realSize{};
    for (const auto& numaInfo : importObj.algoResult.exportNumaInfos) {
        realSize += numaInfo.size;
    }
    resp.realSize = std::to_string(realSize);
}

uint32_t RealImportDecoder(const std::pair<uint32_t, uint32_t>& chipDiePair, UbseMemShareBorrowImportObj& importObj)
{
    std::pair<uint32_t, uint32_t> remoteChipDiePair{};
    auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.exportNumaInfos[0].socketId,
                                                                remoteChipDiePair);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        return UBSE_ERR_INTERNAL;
    }
    decoder::utils::ImportDecoderParam importParam{};
    decoder::utils::MemDecoderUtils::SetImportDecoderParam(importParam, importObj.req.ubseMemPrivData);
    importParam.isHighSafety = IsHighSafety();
    importParam.trustRingData = importObj.req.trustRingData;
    importParam.type = "share";
    res = ImportToAddDecoderEntry(chipDiePair, importObj.exportObmmInfo, importParam, importObj.status);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "ImportToAddDecoderEntry failed, res=" << res;
        uint8_t decoderId = importObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
        UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
        return UBSE_ERR_INTERNAL;
    }
    importObj.req.trustRingData.ClearLendSignedDataMemory();
    return UBSE_OK;
}

uint32_t ShareImportRunningHandler(UbseMemOperationResp& resp, UbseMemShareBorrowImportObj& importObj,
                                   const std::string& name, const std::string& requestNodeId)
{
    UBSE_LOG_INFO << "ShareImportRunningAgent callback. name=" << name << ", requestId=" << importObj.req.requestId;
    auto oldImportObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
        importObj.importNodeId, importObj.req.name);
    if (oldImportObjPtr && oldImportObjPtr->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
        return UBSE_OK;
    }
    bool realExe = importObj.importNodeId == importObj.algoResult.exportNumaInfos[0].nodeId ? false : true;
    std::pair<uint32_t, uint32_t> chipDiePair{};
    if (realExe) {
        auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
            return UBSE_ERR_INTERNAL;
        }
    }
    {
        std::shared_lock lock(GetDecoderImportMutex());
        if (realExe) {
            auto res = RealImportDecoder(chipDiePair, importObj);
            if (res != UBSE_OK) {
                return res;
            }
        }
        ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
    }
    if (auto ret = UbseMmiInterface::GetInstance().ShmImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to import, name=" << name << ", requestNodeId=" << requestNodeId
                       << ", requestId=" << importObj.req.requestId;
        if (realExe) {
            uint8_t decoderId = importObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
            UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
        }
        EraseShareImport(importObj);
        return ret;
    }
    UBSE_LOG_INFO << "Success to import share, name=" << name << ", requestId=" << importObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << importObj.importNodeId << " ShareMemory Import "
                             << std::to_string(importObj.req.size) << " Bytes Success";
    return UBSE_OK;
}

uint32_t ShareImportRunningAgentCallBack(UbseMemOperationResp& resp, UbseMemShareBorrowImportObj& importObj,
                                         const std::string& name, const std::string& requestNodeId)
{
    auto nowObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
        importObj.importNodeId, importObj.req.name);
    if (nowObjPtr && nowObjPtr->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
        return UBSE_OK;
    }
    auto res = ShareImportRunningHandler(resp, importObj, name, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, name, "SHARE_BORROW", importObj.req.size,
                           importObj.algoResult.exportNumaInfos[0].nodeId, importObj.importNodeId, res,
                           MemAdvice::OBMM_FAILED);
    } else {
        ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    }
    if (res = SendShareImportObj(importObj, false); res != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, name, "SHARE_BORROW", importObj.req.size,
                           importObj.algoResult.exportNumaInfos[0].nodeId, importObj.importNodeId, res,
                           MemAdvice::COMM_FAILED);
    }
    return res;
}

uint32_t ShareImportDestroyingHandler(UbseMemOperationResp& resp, UbseMemShareBorrowImportObj& importObj,
                                      const std::string& name, const std::string& requestNodeId)
{
    UBSE_LOG_INFO << "Share import destroying agent callback. name=" << name
                  << ", requestId=" << importObj.req.requestId;
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    auto objPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
        importObj.importNodeId, name);
    bool directReply = (objPtr == nullptr);
    if (directReply) {
        return UBSE_OK;
    }
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmUnImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unimport name=" << name << ", requestId=" << importObj.req.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "Success to unimport share. name=" << name << ", requestId=" << importObj.req.requestId;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << importObj.importNodeId << " ShareMemory UnImport "
                               << std::to_string(importObj.req.size) << " Bytes Success";

    bool realExe = !(importObj.importNodeId == importObj.algoResult.exportNumaInfos[0].nodeId);
    if (realExe) {
        std::pair<uint32_t, uint32_t> chipDiePair{};
        auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        }
        uint8_t decoderId = importObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
        UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
    }
    if (!importObj.status.decoderResult.empty()) {
        UBSE_LOG_ERROR << "UnimportToDelDecoderEntry failed";
    }
    return UBSE_OK;
}

uint32_t ShareImportDestroyingAgentCallBack(UbseMemOperationResp& resp, UbseMemShareBorrowImportObj& importObj,
                                            const std::string& name, const std::string& requestNodeId)
{
    auto res = ShareImportDestroyingHandler(resp, importObj, name, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        UBSE_LOG_ERROR << "ShareUnImport Failed, Failed count:" << ++g_shareUnimportFailedCount
                       << ". advice: Caller should clear memory and retry. "
                       << "If failures persist, migrate the workload and restart the host.";
        BorrowFailedAdvice(
            ProcessType::UNIMPORT_FAILED, name, "SHARE_BORROW", 0,
            importObj.algoResult.exportNumaInfos.empty() ? "" : importObj.algoResult.exportNumaInfos.begin()->nodeId,
            importObj.importNodeId, res, MemAdvice::OBMM_FAILED);
    } else {
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        EraseShareImport(importObj);
    }
    if (auto ret = SendShareImportObj(importObj, false); ret != UBSE_OK) {
        BorrowFailedAdvice(
            ProcessType::UNIMPORT_FAILED, name, "SHARE_BORROW", 0,
            importObj.algoResult.exportNumaInfos.empty() ? "" : importObj.algoResult.exportNumaInfos.begin()->nodeId,
            importObj.importNodeId, ret, MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ShareImportAgentCallBack(UbseMemShareBorrowImportObj& importObj, const std::string& name,
                                  const std::string& requestNodeId)
{
    UBSE_LOG_INFO << "Share import agent callback. name=" << name << ", state=" << importObj.status.state
                  << ", requestId=" << importObj.req.requestId;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        return ShareImportRunningAgentCallBack(resp, importObj, name, requestNodeId);
    }
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    return ShareImportDestroyingAgentCallBack(resp, importObj, name, requestNodeId);
}

static uint32_t ShareImportMasterCreateCallback(UbseMemShareBorrowImportObj& importObj, UbseMemOperationResp& resp)
{
    if (!HasAgentAlreadyReported<UbseMemShareBorrowImportObj>(importObj.req.name, importObj.importNodeId,
                                                              &UbseMemShareBorrowImportObj::isCreateReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export created, name=" << importObj.req.name
                      << ", requestId=" << importObj.req.requestId;
        return UBSE_OK;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
        ShareImportUpdateState(importObj, importObj.status.state);
        ShareImportFillResp(resp, importObj);
        UBSE_LOG_INFO << "this is shm callback before shm importObjstateChange, name=" << importObj.req.name
                      << ", requestId=" << importObj.req.requestId;
        UbseMemShmImportObjStateChangeHandler(importObj);
        if (auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_ATTACH); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::RETURN_FAILED, importObj.req.name, "SHARE_BORROW", 0,
                               importObj.algoResult.exportNumaInfos.empty() ?
                                   "" :
                                   importObj.algoResult.exportNumaInfos.begin()->nodeId,
                               importObj.importNodeId, ret, MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
    EraseShareImport(importObj);
    return BuildOperationRespWhenFail(resp, importObj.req.name, importObj.req.requestNodeId, "Failed to import.",
                                      importObj.errorCode, MemOperationType::SHARED_ATTACH);
}

static uint32_t ShareImportMasterDestroyCallback(UbseMemShareBorrowImportObj& importObj, UbseMemOperationResp& resp)
{
    if (!HasAgentAlreadyReported<UbseMemShareBorrowImportObj>(
            importObj.req.name, importObj.importNodeId, &UbseMemShareBorrowImportObj::isDestroyedReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export destroyed, name=" << importObj.req.name
                      << ", requestId=" << importObj.req.requestId;
        return UBSE_OK;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseShareImport(importObj);
        UBSE_LOG_INFO << "this is shm callback before shm importObjstateChange, name=" << importObj.req.name
                      << ", requestId=" << importObj.req.requestId;
        UbseMemShmImportObjStateChangeHandler(importObj);
        return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_DETACH);
    }
    ShareImportUpdateState(importObj, importObj.status.state);
    if (auto ret = BuildOperationRespWhenFail(resp, importObj.req.name, importObj.req.requestNodeId,
                                              "Failed to unimport.", importObj.errorCode,
                                              MemOperationType::SHARED_DETACH);
        ret != UBSE_OK) {
        BorrowFailedAdvice(
            ProcessType::RETURN_FAILED, importObj.req.name, "SHARE_BORROW", 0,
            importObj.algoResult.exportNumaInfos.empty() ? "" : importObj.algoResult.exportNumaInfos.begin()->nodeId,
            importObj.importNodeId, ret, MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ShareImportMasterCallBack(UbseMemShareBorrowImportObj& importObj)
{
    UBSE_LOG_INFO << "Share import master callback. name=" << importObj.req.name << ", state=" << importObj.status.state
                  << ", requestId=" << importObj.req.requestId;
    auto lock = LoggingLockGuard(importObj.req.name);
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};

    if (importObj.status.expectState == UBSE_MEM_IMPORT_SUCCESS) {
        return ShareImportMasterCreateCallback(importObj, resp);
    }
    if (importObj.status.expectState == UBSE_MEM_IMPORT_DESTROYED) {
        return ShareImportMasterDestroyCallback(importObj, resp);
    }
    return UBSE_OK;
}

uint32_t UbseMemShareBorrowImportObjCallback(const UbseMemShareBorrowImportObj& importObj)
{
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);

    auto importNodeId = importObj.importNodeId;
    auto name = importObj.req.name;
    auto copy = importObj;
    if (importNodeId == currentNodeInfo.nodeId &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        return ShareImportAgentCallBack(copy, name, importNodeId);
    }

    return ShareImportMasterCallBack(copy);
}
uint32_t DealSendShareUnExportObjFailed(UbseMemShareBorrowExportObj& exportObj, const UbseMemReturnReq& req,
                                        UbseMemOperationResp& resp, const std::string& name)
{
    resp.name = name;
    resp.requestNodeId = req.requestNodeId;
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to send exportObj.",
                                      UBSE_ERR_UNIMPORT_SUCCESS);
}

static uint32_t ShareReturnFail(const UbseMemReturnReq& req, UbseMemOperationResp& resp, const std::string& msg,
                                uint32_t errCode, MemAdvice advice)
{
    BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, "", "", errCode, advice);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, msg, errCode, MemOperationType::SHARED_RETURN);
}

static uint32_t ShareReturnValidate(const UbseMemReturnReq& req, UbseMemOperationResp& resp,
                                    const std::string& realRequestNodeId, UbseMemShareBorrowExportObj& exportObj,
                                    uint32_t& comErrorCode)
{
    std::vector<UbseMemShareBorrowExportObj> exportObjs;
    std::vector<UbseMemShareBorrowImportObj> importObjs;
    FindShareBorrowObjByName(req.name, exportObjs, importObjs);
    std::string enode = "";
    std::string inode = "";
    if (!exportObjs.empty()) {
        exportObj = exportObjs[0];
        enode = exportObjs[0].algoResult.exportNumaInfos.empty() ? "" :
                                                                   exportObj.algoResult.exportNumaInfos.begin()->nodeId;
        inode = exportObjs[0].algoResult.importNumaInfos.empty() ? "" :
                                                                   exportObj.algoResult.importNumaInfos.begin()->nodeId;
    }
    if (!importObjs.empty()) {
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, enode, inode,
                           UBSE_ERR_SHM_ATTACH_USING, MemAdvice::RESOURCE_EXIST);
        comErrorCode = BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Resource attached.",
                                                  UBSE_ERR_SHM_ATTACH_USING, MemOperationType::SHARED_RETURN);
        return UBSE_ERR_SHM_ATTACH_USING;
    }
    if (exportObjs.empty()) {
        comErrorCode =
            ShareReturnFail(req, resp, "Memory does not exist.", UBSE_ERR_NOT_EXIST, MemAdvice::RESOURCE_NOT_EXIST);
        return UBSE_ERR_NOT_EXIST;
    }
    auto memStage = GetMemStageByExportObjState(exportObj, true);
    if (memStage == UbseMemStage::UBSE_CREATING || memStage == UbseMemStage::UBSE_DELETING) {
        UBSE_LOG_INFO << "resource is being borrowed or returned, name=" << req.name;
        auto ret = (memStage == UbseMemStage::UBSE_CREATING) ? UBSE_ERR_CREATING : UBSE_ERR_DELETING;
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, enode, inode, ret,
                           MemAdvice::RESOURCE_OPERATION_CONFLICT);
        comErrorCode = BuildOperationRespWhenFail(resp, req.name, req.requestNodeId,
                                                  "resource being borrowed or returned", ret,
                                                  MemOperationType::SHARED_RETURN);
        return ret;
    }
    if (!CheckShareReturnPermission(exportObj.req.udsInfo, req.udsInfo, realRequestNodeId, exportObj.req.shmRegion)) {
        std::string shmRegionIds;
        for (const auto& node : exportObj.req.shmRegion.nodelist)
            shmRegionIds += node.nodeId + ", ";
        UBSE_LOG_ERROR << "name=" << req.name << " auth failed, reqUid=" << req.udsInfo.uid
                       << ", objUid=" << exportObj.req.udsInfo.uid << ", realRequestNodeId=" << realRequestNodeId
                       << ", shmRegionIds=" << shmRegionIds;
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, enode, inode, UBSE_ERR_AUTH_FAILED,
                           MemAdvice::UBSE_NO_OPERATION_PERMISSION);
        comErrorCode = BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Error auth", UBSE_ERR_AUTH_FAILED,
                                                  MemOperationType::SHARED_RETURN);
        return UBSE_ERR_AUTH_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseMemShareReturn(const UbseMemReturnReq& req, UbseMemOperationResp& resp,
                            const std::string& realRequestNodeId)
{
    UBSE_LOG_INFO << "Start to share return, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId << ", realRequestNodeId=" << realRequestNodeId;
    auto lock = LoggingLockGuard(req.name);
    InitializeResponse(req, resp);
    if (!IsMemShareFeatureSupported()) {
        return BuildMemFeatureNotSupportedResp(resp, req.name, req.requestNodeId, MemOperationType::SHARED_RETURN);
    }
    UbseMemShareBorrowExportObj exportObj;
    uint32_t comErrorCode = UBSE_OK;
    if (auto ret = ShareReturnValidate(req, resp, realRequestNodeId, exportObj, comErrorCode); ret != UBSE_OK) {
        return comErrorCode;
    }
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.returnReq = req;
    exportObj.isDestroyedReportReceived = false;
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    if (auto ret = SendShareExportObj(exportObj, true, exportObj.algoResult.exportNumaInfos[0].nodeId);
        ret != UBSE_OK) {
        BorrowFailedAdvice(
            ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0,
            exportObj.algoResult.exportNumaInfos.empty() ? "" : exportObj.algoResult.exportNumaInfos.begin()->nodeId,
            exportObj.algoResult.importNumaInfos.empty() ? "" : exportObj.algoResult.importNumaInfos.begin()->nodeId,
            ret, MemAdvice::COMM_FAILED);
        return DealSendShareUnExportObjFailed(exportObj, req, resp, req.name);
    }
    return UBSE_OK;
}

uint32_t UpdateFaultShareExportObj(const std::string& nodeId, uint64_t memId, const std::string& memName,
                                   UbMemFaultType type)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to update shared memory debt due to fault.";
    auto& exportMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    auto objPtr = exportMap.GetResource(nodeId, memName);
    if (!objPtr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find fault memory by name=" << memName << ".";
        return UBSE_ERROR;
    }

    auto obj = *objPtr;
    auto obmmItor = std::find_if(obj.status.exportObmmInfo.begin(), obj.status.exportObmmInfo.end(),
                                 [&](const UbseMemObmmInfo& info) -> bool { return info.memId == memId; });
    if (obmmItor == obj.status.exportObmmInfo.end()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find fault memory by memId=" << memId << ".";
        return UBSE_ERROR;
    }
    obmmItor->memIdStatus = type;
    exportMap.PutResource(nodeId, memName, obj);
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to update the state of export memId= " << memId
                  << " to fault type=" << static_cast<uint16_t>(obmmItor->memIdStatus) << ".";
    return UBSE_OK;
}

uint32_t AddShareImport(const UbseMemShareBorrowImportObj& importObj)
{
    auto copy = importObj;
    if (copy.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseShareImport(copy);
        return UbseMemShmImportObjStateChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add share import, name=" << copy.req.name << ", import node=" << importObj.importNodeId;
    ShareImportUpdateState(copy, copy.status.state);
    return UbseMemShmImportObjStateChangeHandler(copy);
}

uint32_t AddShareExport(const UbseMemShareBorrowExportObj& exportObj)
{
    auto copy = exportObj;
    if (copy.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseShareExport(copy);
        return UbseMemShmExportObjStateChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add share export, name=" << copy.req.name;
    ShareExportUpdateState(copy, copy.status.state);
    return UbseMemShmExportObjStateChangeHandler(copy);
}

uint32_t DeleteShareExport(const UbseMemShareBorrowExportObj& exportObj)
{
    auto copy = exportObj;
    copy.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    copy.status.state = UBSE_MEM_EXPORT_DESTROYING;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The exportObj with no export numa info will be ignored.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "Force delete. name=" << copy.req.name;
    ShareExportUpdateState(copy, UBSE_MEM_EXPORT_DESTROYING);
    return SendShareExportObj(copy, true, exportObj.algoResult.exportNumaInfos[0].nodeId);
}
} // namespace ubse::mem::controller
