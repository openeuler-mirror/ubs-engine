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

#include "../logging_lock_guard.h"
#include "../message/ubse_mem_share_borrow_exportobj_simpo.h"
#include "../message/ubse_mem_share_borrow_importobj_simpo.h"
#include "../ubse_mem_account.h"
#include "../ubse_mem_controller_ledger.h"
#include "../ubse_mem_rpc_processor.h"
#include "src/controllers/mem/mem_scheduler/ubse_mem_topology_info_manager.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_logger_audit.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mem_util.h"
#include "ubse_mmi_module.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_node_controller_util.h"
#include "ubse_topo_util.h"

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
const std::string ClusterHandlerKey = "NODE_CLUSTER_HDL";

void FindShareBorrowObjByNameWhenBorrow(const NodeMemDebtInfoMap &debtInfoMap, const std::string &name,
                                        std::vector<UbseMemShareBorrowExportObj> &exportObjs,
                                        std::vector<UbseMemShareBorrowImportObj> &importObjs)
{
    mapLock.LockRead();
    for (const auto &[nodeKey, debtInfo] : debtInfoMap) {
        for (const auto &[objKey, obj] : debtInfo.shareExportObjMap) {
            if (obj.req.name == name && (obj.status.state != UBSE_MEM_EXPORT_DESTROYED)) {
                UBSE_LOG_INFO << "obj state is " << static_cast<uint32_t>(obj.status.state);
                exportObjs.push_back(obj);
            }
        }
        for (const auto &[objKey, obj] : debtInfo.shareImportObjMap) {
            if (obj.req.name == name && (obj.status.state != UBSE_MEM_IMPORT_DESTROYED)) {
                UBSE_LOG_INFO << "obj state is " << static_cast<uint32_t>(obj.status.state);
                importObjs.push_back(obj);
            }
        }
    }
    mapLock.UnLock();
}

void NodeControllerReadLock(const UbseMemShareBorrowReq &req)
{
    if (!req.baseNodeId.empty()) {
        UbseNodeControllerLockMgr::ReadLock(req.baseNodeId);
    }
}

void NodeControllerReadUnLock(const UbseMemShareBorrowReq &req)
{
    if (!req.baseNodeId.empty()) {
        UbseNodeControllerLockMgr::ReadUnLock(req.baseNodeId);
    }
}

uint32_t SetNodeIndex(UbseMemShareBorrowReq &req)
{
    int index = 0;
    for (ubse::adapter_plugins::mmi::UbseNodeInfo &nodeInfo : req.shmRegion.nodelist) {
        ubse::nodeController::UbseNodeInfo node = UbseNodeController::GetInstance().GetNodeById(nodeInfo.nodeId);
        if (node.nodeId.empty()) {
            UBSE_LOG_WARN << "[MMC] Failed to SetNodeIndex, nodeId= " << nodeInfo.nodeId;
            continue;
        }
        // 使用nodeId作为Index
        try {
            nodeInfo.index = std::stoi(node.nodeId) - 1;
        } catch (const std::invalid_argument &e) {
            nodeInfo.index = index++;
            UBSE_LOG_ERROR << "Invalid argument: " << e.what();
        } catch (const std::out_of_range &e) {
            nodeInfo.index = index++;
            UBSE_LOG_ERROR << "Out of range: " << e.what();
        }
    }
    return UBSE_OK;
}

static UbseResult ShareAllocate(const UbseMemShareBorrowReq &req, UbseMemShareBorrowExportObj &exportObj)
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

UbseResult AgentSendShareExportObj(const std::shared_ptr<UbseComModule> &comModule, SendParam &sendParam,
                                   UbseMemShareBorrowExportobjSimpoPtr &ptr, UbseBaseMessagePtr &ubseResponsePtr,
                                   const UbseMemShareBorrowExportObj &exportObj)
{
    const uint32_t maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION;
    auto ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    uint32_t retryCount = 0;
    while (ret != UBSE_OK && retryCount < maxRetryTimes) {
        UBSE_LOG_ERROR << "Send to exportObj, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
        retryCount++;
        sleep(SEND_RETRY_DURATION);
        std::string masterId{};
        ret = UbseGetMasterNodeId(masterId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get master nodeId failed, " << FormatRetCode(ret);
            continue;
        }
        sendParam.SetRemoteId(masterId);
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    }
    return ret;
}

UbseResult SendShareExportObj(const std::string &nodeId, const UbseMemShareBorrowExportObj &exportObj,
                              const bool isMaster)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule."
                       << ";requestId: " << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_EXPORT_OBJ_CALLBACK));
    UbseMemShareBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemShareBorrowExportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr."
                       << ";requestId: " << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemShareBorrowExportobj(exportObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ubseResponsePtr."
                       << ";requestId: " << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                              << exportObj.req.requestNodeId << ";requestId: " << exportObj.req.requestId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                           << exportObj.req.requestNodeId << ", wait to retry"
                           << ";requestId: " << exportObj.req.requestId;
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                       << exportObj.req.requestNodeId << ";requestId: " << exportObj.req.requestId;
        return ret;
    }

    // 履行侧发回
    return AgentSendShareExportObj(comModule, sendParam, ptr, ubseResponsePtr, exportObj);
}

uint32_t HandleSendExportError(UbseMemOperationResp &resp, const UbseMemShareBorrowReq &req,
                               const UbseMemShareBorrowExportObj &exportObj)
{
    mapLock.LockWrite();
    nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].shareExportObjMap.erase(req.name);
    mapLock.UnLock();

    auto copy = exportObj;
    copy.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemShmExportObjStateChangeHandler(copy);

    return BuildOperationRespWhenFail(
        resp, req.name, req.requestNodeId,
        "Failed to Send export, exportNodeId is " + exportObj.algoResult.exportNumaInfos[0].nodeId,
        UBSE_ERR_INTERNAL, MemOperationType::SHARED_BORROW);
}

bool ValidateAffinityParams(const UbseMemShareBorrowReq &req)
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
    for (const auto &[location, info] : nodeInfo.numaInfos) {
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

void RegisterExportObjectDebtInfo(const UbseMemShareBorrowExportObj &exportObj, const std::string &name)
{
    mapLock.LockWrite();
    nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].shareExportObjMap[name] = exportObj;
    mapLock.UnLock();
}

uint32_t UbseMemShareBorrow(const UbseMemShareBorrowReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Share borrow begins, name is" << req.name << ", requestNodeId is " << req.requestNodeId
                  << "; requestId: " << req.requestId;
    auto lock = LoggingLockGuard(req.name);
    auto requestNodeId = req.requestNodeId;
    auto name = req.name;
    resp.name = name;
    resp.requestId = req.requestId;
    std::vector<UbseMemShareBorrowExportObj> exportObjs;
    std::vector<UbseMemShareBorrowImportObj> importObjs;
    FindShareBorrowObjByNameWhenBorrow(nodeMemDebtInfoMap, name, exportObjs, importObjs);
    if (!exportObjs.empty() || !importObjs.empty()) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Resource Exist.",
                                          UBSE_ERR_EXISTED, MemOperationType::SHARED_BORROW);
    }
    if (!ValidateAffinityParams(req)) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Invalid Affinity parameters",
                                          UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL,
                                          MemOperationType::SHARED_BORROW);
    }
    UbseMemShareBorrowExportObj exportObj;
    exportObj.req = req;
    exportObj.status.state = UBSE_MEM_SCHEDULING;
    if (SetNodeIndex(exportObj.req) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MMC] Failed to SetNodeIndex, name is " << exportObj.req.name << " ,requestNodeId is "
                       << exportObj.req.requestNodeId << "; requestId: " << req.requestId;
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "SetNodeIndex Failed.",
                                          UBSE_ERR_INTERNAL, MemOperationType::SHARED_BORROW);
    }
    auto ret = ShareAllocate(req, exportObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MMC] Failed to allocate, name is " << exportObj.req.name << " ,requestNodeId is "
                       << exportObj.req.requestNodeId << FormatRetCode(ret) << ";requestId: " << resp.requestId;
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to allocate",
                                          UBSE_ERR_ALLOCATE, MemOperationType::SHARED_BORROW);
    }
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    RegisterExportObjectDebtInfo(exportObj, name);
    ret = SendShareExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true);
    if (ret != UBSE_OK) {
        return HandleSendExportError(resp, req, exportObj);
    }
    return UBSE_OK;
}

void FindShareBorrowObjByName(const NodeMemDebtInfoMap &debtInfoMap, const std::string &name,
                              std::vector<UbseMemShareBorrowExportObj> &exportObjs,
                              std::vector<UbseMemShareBorrowImportObj> &importObjs)
{
    std::pair<UbseMemShareBorrowExportObj, std::vector<UbseMemShareBorrowImportObj>> queryObjRes =
        GetMaxRefCountExportObj(name);
    UbseMemShareBorrowExportObj exportObjTmp = queryObjRes.first;
    std::vector<UbseMemShareBorrowImportObj> importObjTmps = queryObjRes.second;
    UBSE_LOG_INFO << "obj state is " << static_cast<uint32_t>(exportObjTmp.status.state);
    if (exportObjTmp.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        exportObjs.push_back(exportObjTmp);
    }
    for (const auto &importObjTmp : importObjTmps) {
        UBSE_LOG_INFO << "obj state is " << static_cast<uint32_t>(importObjTmp.status.state);
        if (importObjTmp.status.state != UBSE_MEM_IMPORT_DESTROYED) {
            importObjs.push_back(importObjTmp);
        }
    }
    if (!exportObjs.empty() || !importObjs.empty()) {
        UBSE_LOG_INFO << "export obj size is " << exportObjs.size() << ", import obj size is " << importObjs.size();
    }
}

bool CheckRegions(const UbseMemShareAttachReq &req, const UbseMemShareBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "Share import node id is " << req.importNodeId << ";requestId: " << req.requestId;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Export's exportNumaInfo is empty;requestId: " << req.requestId;
        return false;
    }
    for (const auto &node : exportObj.req.shmRegion.nodelist) {
        UBSE_LOG_INFO << "Share region node : " << node.nodeId << ";requestId: " << req.requestId;
        if (node.nodeId == req.importNodeId) {
            return true;
        }
    }
    UBSE_LOG_ERROR << "Failed to check share regions;requestId: " << req.requestId;
    return false;
}

bool ExistImportObj(const std::string &name, const std::string &nodeId,
                    const std::vector<UbseMemShareBorrowImportObj> &existImportObjs,
                    UbseMemShareBorrowImportObj &importObj)
{
    for (const auto &existImportObj : existImportObjs) {
        if (existImportObj.req.name == name && existImportObj.importNodeId == nodeId &&
            existImportObj.status.state != UBSE_MEM_IMPORT_DESTROYED) {
            importObj = existImportObj;
            return true;
        }
    }
    return false;
}

uint32_t ExistImportObjHandler(const UbseMemShareAttachReq &req, UbseMemShareBorrowImportObj importObj,
                               UbseMemOperationResp &resp, const std::string &name, const std::string &requestNodeId)
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
    for (const auto &numaInfo : importObj.algoResult.exportNumaInfos) {
        SafeAdd(realSize, numaInfo.size, realSize);
    }
    resp.realSize = std::to_string(realSize);
    return BuildOperationRespWhenFail(resp, name, requestNodeId, "The importNodeId has attached.",
                                      UBSE_ERR_EXISTED, MemOperationType::SHARED_ATTACH);
}

UbseResult ShmAttachPreCheck(const UbseMemShareAttachReq &req, UbseMemOperationResp &resp,
                             const std::vector<UbseMemShareBorrowExportObj> &exportObjs,
                             const std::vector<UbseMemShareBorrowImportObj> &importObjs,
                             UbseMemShareBorrowImportObj importObj)
{
    if (exportObjs.size() != 1 || exportObjs[0].algoResult.exportNumaInfos.empty() ||
        exportObjs[0].status.exportObmmInfo.empty()) {
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "exportObj is Invaild.",
                                   UBSE_ERR_NOT_EXIST,
                                   MemOperationType::SHARED_ATTACH);
        return UBSE_ERROR;
    }
    if (!CheckRegions(req, exportObjs[0])) {
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "The node is not true region.",
                                   UBSE_ERR_INTERNAL, MemOperationType::SHARED_ATTACH);
        return UBSE_ERROR_ACCES;
    }
    if (!exportObjs[0].req.udsInfo.CheckPermission(req.udsInfo)) {
        UBSE_LOG_ERROR << "name:" << req.name << " auth failed,req username:" << req.udsInfo.username
                       << ", req uid:" << req.udsInfo.uid
                       << ", export obj username:" << exportObjs[0].req.udsInfo.username
                       << ", export obj uid:" << exportObjs[0].req.udsInfo.uid;
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Error auth",
                                   UBSE_ERR_AUTH_FAILED, MemOperationType::SHARED_ATTACH);
        return UBSE_ERROR_ACCES;
    }
    if (ExistImportObj(req.name, req.importNodeId, importObjs, importObj)) {
        ExistImportObjHandler(req, importObj, resp, req.name, req.importNodeId);
        return UBSE_ERR_EXISTED;
    }
    return UBSE_OK;
}

void ShareImportUpdateState(UbseMemShareBorrowImportObj &importObj, const UbseMemState &state)
{
    importObj.status.state = state;
    mapLock.LockWrite();
    nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap[importObj.req.name] = importObj;
    mapLock.UnLock();
}

void ConstructShareImportObj(UbseMemShareBorrowImportObj &importObj, const UbseMemShareAttachReq &req)
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

UbseResult AgentSendShareImportObj(const std::shared_ptr<UbseComModule> &comModule, SendParam &sendParam,
                                   UbseMemShareBorrowImportobjSimpoPtr &ptr, UbseBaseMessagePtr &ubseResponsePtr,
                                   const UbseMemShareBorrowImportObj &exportObj)
{
    const uint32_t maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION;
    auto ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    uint32_t retryCount = 0;
    while (ret != UBSE_OK && retryCount < maxRetryTimes) {
        UBSE_LOG_ERROR << "Send to importObj, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
        retryCount++;
        sleep(SEND_RETRY_DURATION);
        std::string masterId{};
        ret = UbseGetMasterNodeId(masterId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get master nodeId failed, " << FormatRetCode(ret);
            continue;
        }
        sendParam.SetRemoteId(masterId);
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    }
    return ret;
}

UbseResult SendShareImportObj(const std::string &nodeId, const UbseMemShareBorrowImportObj &importObj,
                              const bool isMaster)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule."
                       << ";requestId: " << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_IMPORT_OBJ_CALLBACK));
    UbseMemShareBorrowImportobjSimpoPtr ptr = new (std::nothrow) UbseMemShareBorrowImportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr."
                       << ";requestId: " << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemShareBorrowImportobj(importObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ubseResponsePtr."
                       << ";requestId: " << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send importObj, name is " << importObj.req.name << "requestNodeId id is "
                              << importObj.req.requestNodeId << ";requestId: " << importObj.req.requestId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                           << importObj.req.requestNodeId << ", wait to retry"
                           << ";requestId: " << importObj.req.requestId;
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                       << importObj.req.requestNodeId << ";requestId: " << importObj.req.requestId;
        return ret;
    }

    // 履行侧发回
    return AgentSendShareImportObj(comModule, sendParam, ptr, ubseResponsePtr, importObj);
}

std::string GetChipInfoBySocketId(const ubse::nodeController::UbseNodeInfo &nodeInfo, uint32_t socketId)
{
    for (const auto &[loc, cpuInfo] : nodeInfo.cpuInfos) {
        if (cpuInfo.socketId == socketId) {
            UBSE_LOG_INFO << "chipId is " << cpuInfo.chipId;
            return cpuInfo.chipId;
        }
    }
    UBSE_LOG_ERROR << "Can't find chipId in nodeInfo, socketId is " << socketId << " node Id is " << nodeInfo.nodeId;
    return "";
}

void GetPortCnaByPortId(const nodeController::UbseNodeInfo &remoteInfo, const std::string &importChipId,
                        const uint32_t portId, uint32_t &portCna)
{
    for (const auto &cpuInfo : remoteInfo.cpuInfos) {
        for (const auto &[key, portInfo] : cpuInfo.second.portInfos) {
            if (portInfo.remoteChipId != importChipId || portInfo.portId != std::to_string(portId)) {
                continue;
            }
            portCna = portInfo.portCna;
            return;
        }
    }
    portCna = UINT32_MAX;
}

void GetMinPortInfoByCpuInfo(const nodeController::UbseNodeInfo &remoteInfo, const std::string &importChipId,
                             uint32_t &minPortId, uint32_t &minPortCna)
{
    for (const auto &cpuInfo : remoteInfo.cpuInfos) {
        for (const auto &[key, portInfo] : cpuInfo.second.portInfos) {
            if (portInfo.remoteChipId != importChipId) {
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

uint32_t GetPortInfo(const std::string &importNodeId, const UbseMemShareBorrowImportObj &importObj,
                     const std::string &remoteNode, uint32_t &portId, uint32_t &portCna)
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
        GetPortCnaByPortId(remoteInfo, importChipId, portId, portCna);
    } else {
        GetMinPortInfoByCpuInfo(remoteInfo, importChipId, portId, portCna);
    }
    UBSE_LOG_INFO << "minPortCna is " << portCna << "min portId is " << portId;
    if (portCna == UINT32_MAX) {
        UBSE_LOG_ERROR << "Failed Get minPortCna from topoInfo";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t GetCnaTopoByPeerNodeInfo(const UbseMemShareAttachReq &req, const UbseMemShareBorrowExportObj exportObj,
                                  UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj)
{
    auto remoteNode = exportObj.algoResult.exportNumaInfos[0].nodeId;
    if (exportObj.algoResult.exportNumaInfos[0].nodeId == req.importNodeId) {
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "req info: importNodeId is " << req.importNodeId << ", exportNodeId is " << remoteNode
                  << " export socketId is " << exportObj.algoResult.exportNumaInfos[0].socketId;
    auto ret = GetCnaInfoWhenImport(exportObj.algoResult.exportNumaInfos[0].nodeId, req.importNodeId, importObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get cna info when import" << FormatRetCode(ret) << ";requestId: " << req.requestId;
        return UBSE_ERROR;
    }
    // 多路径情况下，会给dcna重新赋值，当指定port时以指定port为准，否则选择直连导入socket的最小端口号，作为单路径路由表的配置
    if (IsSameSocketMultiPortTopo()) {
        uint32_t minPortId = UINT32_MAX;
        uint32_t minPortCna{};
        if (GetPortInfo(req.importNodeId, importObj, remoteNode, minPortId, minPortCna) != UBSE_OK) {
            return UBSE_ERROR;
        }
        for (auto &obmmInfo : importObj.exportObmmInfo) {
            obmmInfo.desc.dcna = minPortCna;
            obmmInfo.desc.marId = minPortId / 4; // minPortId / 4 能得到marId
        }
    }
    return UBSE_OK;
}

uint32_t UbseMemShareAttach(const UbseMemShareAttachReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Share attach begins, name is" << req.name << ", requestNodeId is " << req.requestNodeId
                  << ";requestId: " << req.requestId;
    auto lock = LoggingLockGuard(req.name + "_" + req.requestNodeId);
    resp.requestId = req.requestId;
    if (req.importNodeId.empty()) {
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "attach with no node is valid.",
                                          UBSE_ERR_SHM_NODE_EMPTY, MemOperationType::SHARED_ATTACH);
    }
    UbseNodeControllerLockMgr::WriteLock(ClusterHandlerKey);
    std::vector<UbseMemShareBorrowExportObj> exportObjs{};
    std::vector<UbseMemShareBorrowImportObj> importObjs{};
    FindShareBorrowObjByName(nodeMemDebtInfoMap, req.name, exportObjs, importObjs);
    UbseMemShareBorrowImportObj importObj;
    if (const auto ret = ShmAttachPreCheck(req, resp, exportObjs, importObjs, importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "precheck failed" << FormatRetCode(ret) << ";requestId: " << req.requestId;
        UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
        return ret;
    }
    importObj.exportObmmInfo = exportObjs[0].status.exportObmmInfo;
    importObj.algoResult = exportObjs[0].algoResult;
    if (GetCnaTopoByPeerNodeInfo(req, exportObjs[0], resp, importObj) != UBSE_OK) {
        UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to get cna info when import",
                                          UBSE_ERR_INTERNAL, MemOperationType::SHARED_ATTACH);
    }
    importObj.req = exportObjs[0].req;
    importObj.req.trustRingData.ClearReqSignedDataMemory();  // 清除import对象里请求签名信息
    UBSE_LOG_INFO << "import size: " << importObj.req.size << ";requestId: " << req.requestId;
    ConstructShareImportObj(importObj, req);
    UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
    if (auto ret = SendShareImportObj(req.importNodeId, importObj, true); ret != UBSE_OK) {
        mapLock.LockWrite();
        nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap.erase(req.name);
        mapLock.UnLock();
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to Send import", UBSE_ERR_INTERNAL,
                                          MemOperationType::SHARED_ATTACH);
    }
    return UBSE_OK;
}

uint32_t UbseMemShareDetach(const UbseMemShareDetachReq &req, UbseMemOperationResp &resp,
                            const std::string &realRequestNodeId)
{
    UBSE_LOG_INFO << "Share detach begins, name is" << req.name << ", requestNodeId is " << req.requestNodeId
                  << ";requestId: " << req.requestId << ", realRequestNodeId:" << realRequestNodeId;
    auto lock = LoggingLockGuard(req.name + "_" + req.requestNodeId);
    resp.requestId = req.requestId;
    if (req.unImportNodeId.empty()) {
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Detach with no node is valid.",
                                          UBSE_ERR_SHM_NODE_EMPTY, MemOperationType::SHARED_DETACH);
    }
    auto waitResult = WaitNodeStateWork(req.unImportNodeId);
    if (waitResult != UBSE_OK) {
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "importNode is not ok",
                                          waitResult,
                                          MemOperationType::FD_RETURN);
    }
    std::vector<UbseMemShareBorrowExportObj> exportObjs{};
    std::vector<UbseMemShareBorrowImportObj> importObjs{};
    UbseMemShareBorrowImportObj importObj{};
    FindShareBorrowObjByName(nodeMemDebtInfoMap, req.name, exportObjs, importObjs);
    if (!ExistImportObj(req.name, req.unImportNodeId, importObjs, importObj)) {
        return BuildOperationRespWhenFail(
            resp, req.name, req.requestNodeId, "Detach is not allowed, because the node is not attach.",
            UBSE_ERR_SHM_NO_ATTACH, MemOperationType::SHARED_DETACH);
    }
    UbseMemStage memStage = GetMemStageByShareImportObjState(importObj, true);
    if (memStage == UbseMemStage::UBSE_CREATING || memStage == UbseMemStage::UBSE_DELETING) {
        UBSE_LOG_INFO << "resource is being borrowed or returned, name is " << req.name;
        auto ret = (memStage == UbseMemStage::UBSE_CREATING) ? UBSE_ERR_CREATING :UBSE_ERR_DELETING;
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "resource being borrowed or returned",
                                          ret, MemOperationType::SHARED_DETACH);
    }
    if (!CheckShareDetachPermission(importObj.req.udsInfo, req.udsInfo, realRequestNodeId, importObj.importNodeId)) {
        UBSE_LOG_ERROR << "name:" << req.name << " auth failed,req username:" << req.udsInfo.username << ", req uid:" << req.udsInfo.uid
                       << ", importObj obj username:" << importObj.req.udsInfo.username << ", import obj uid:" << importObj.req.udsInfo.uid
                       << "importObj.importNodeId" << importObj.importNodeId << ", realRequestNodeId:" << realRequestNodeId;
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Error auth.", UBSE_ERR_AUTH_FAILED,
                                          MemOperationType::SHARED_DETACH);
    }
    importObj.req.requestId = req.requestId;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    //  下发importObj;
    if (SendShareImportObj(req.unImportNodeId, importObj, true) != UBSE_OK) {
        importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
        ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to Send import",
                                          UBSE_ERR_INTERNAL, MemOperationType::SHARED_DETACH);
    }
    return UBSE_OK;
}

void ShareExportUpdateState(UbseMemShareBorrowExportObj &exportObj, const UbseMemState &state)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    exportObj.status.state = state;
    mapLock.LockWrite();
    UBSE_LOG_INFO << "Update share export state, name is " << exportObj.req.name << ", state: " << state
                  << ", requestId: " << exportObj.req.requestId;
    nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].shareExportObjMap[exportObj.req.name] =
        exportObj;
    mapLock.UnLock();
}

void EraseShareExport(const UbseMemShareBorrowExportObj &exportObj)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto name = exportObj.req.name;
    mapLock.LockWrite();
    UBSE_LOG_INFO << "Erase share export, name is " << exportObj.req.name
                  << ", requestId: " << exportObj.req.requestId;
    if (nodeMemDebtInfoMap[exportNodeId].shareExportObjMap.find(name) !=
        nodeMemDebtInfoMap[exportNodeId].shareExportObjMap.end()) {
        nodeMemDebtInfoMap[exportNodeId].shareExportObjMap.erase(name);
    }
    mapLock.UnLock();
}

void EraseShareImport(const UbseMemShareBorrowImportObj &importObj)
{
    auto name = importObj.req.name;
    auto importNodeId = importObj.importNodeId;
    mapLock.LockWrite();
    UBSE_LOG_INFO << "Erase share import, name is " << importObj.req.name
                  << ", requestId: " << importObj.req.requestId;
    if (nodeMemDebtInfoMap[importNodeId].shareImportObjMap.find(name) !=
        nodeMemDebtInfoMap[importNodeId].shareImportObjMap.end()) {
        nodeMemDebtInfoMap[importNodeId].shareImportObjMap.erase(name);
    }
    mapLock.UnLock();
}

void ShareExportFillResp(UbseMemOperationResp &resp, const UbseMemShareBorrowExportObj &exportObj)
{
    for (const auto obmmInfo : exportObj.status.exportObmmInfo) {
        resp.memIdList.push_back(obmmInfo.memId);
    }
}

uint32_t ShareExportRunningAgentCallback(UbseMemOperationResp &resp, UbseMemShareBorrowExportObj &exportObj,
                                         const std::string &masterNodeId, const std::string &name,
                                         const std::string &requestNodeId, const std::string &exportNodeId)
{
    UBSE_LOG_INFO << "Share export running agent callback. name is " << name
                  << ";requestId: " << exportObj.req.requestId;
    mapLock.LockRead();
    auto curNode = GetCurNodeId();
    if (nodeMemDebtInfoMap[curNode].shareExportObjMap.find(exportObj.req.name) !=
        nodeMemDebtInfoMap[curNode].shareExportObjMap.end()) {
        auto nowObj = nodeMemDebtInfoMap[curNode].shareExportObjMap[exportObj.req.name];
        if (nowObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
            mapLock.UnLock();
            return SendShareExportObj(masterNodeId, nowObj, false);
        }
    }
    mapLock.UnLock();
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_RUNNING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to export, name is " << name << ", requestNodeId is " << requestNodeId
                       << ";requestId: " << exportObj.req.requestId;
        exportObj.errorCode = ret;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        EraseShareExport(exportObj);
        return SendShareExportObj(masterNodeId, exportObj, false);
    }
    UBSE_LOG_INFO << "Success to export share, name is " << name << ";requestId: " << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << exportNodeId << " ShareMemory Export"
                             << std::to_string(exportObj.req.size) << "Bytes Success";
    // 高安配置下签名并验签
    if (IsHighSafety()) {
        UbseExportSignReq trustReq{ exportObj.req.trustRingData.reqSignedData, "share" };
        trustReq.exportObmmInfo = exportObj.status.exportObmmInfo;
        trustReq.trustRingId = exportObj.req.trustRingData.trustRingId;
        if (const auto ret = UbseMemSignVerifier::SignAndVerify(trustReq, exportObj.req.trustRingData.lendSignedDatas);
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to sign for lend information, " << FormatRetCode(ret);
            EraseShareExport(exportObj);
            exportObj.errorCode = ret;
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            return SendShareExportObj(masterNodeId, exportObj, false);
        }
    }
    exportObj.req.trustRingData.ClearReqSignedDataMemory();
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return SendShareExportObj(masterNodeId, exportObj, false);
}

uint32_t ShareExportDestroyingAgentCallback(UbseMemOperationResp &resp, UbseMemShareBorrowExportObj &exportObj,
                                            const std::string &masterNodeId, const std::string &name,
                                            const std::string &requestNodeId, const std::string &exportNodeId)
{
    UBSE_LOG_INFO << "Share export Destroying agent callback. name is " << name
                  << ";requestId: " << exportObj.req.requestId;
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    mapLock.LockRead();
    bool directReply = nodeMemDebtInfoMap[exportNodeId].shareExportObjMap.find(name) ==
                       nodeMemDebtInfoMap[exportNodeId].shareExportObjMap.end();
    mapLock.UnLock();
    if (directReply) {
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        return SendShareExportObj(masterNodeId, exportObj, false);
    }
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmUnExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Success to unexport, name is " << name << ";requestId: " << exportObj.req.requestId;
        exportObj.errorCode = ret;
        ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        return SendShareExportObj(masterNodeId, exportObj, false);
    }
    UBSE_LOG_INFO << "Success to unexport share, name is " << name << ";requestId: " << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << exportNodeId << " ShareMemory UnExport "
                               << std::to_string(exportObj.req.size) << " Bytes Success";
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    EraseShareExport(exportObj);
    return SendShareExportObj(masterNodeId, exportObj, false);
}

uint32_t ShareExportAgentCallback(const std::string &exportNodeId, UbseMemShareBorrowExportObj &exportObj,
                                  const std::string &name, const std::string &masterNodeId)
{
    UBSE_LOG_INFO << "Share export agent callback name=" << name << ", state=" << exportObj.status.state
                  << ";requestId: " << exportObj.req.requestId;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    auto requestNodeId = exportObj.req.requestNodeId;
    if (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        return ShareExportRunningAgentCallback(resp, exportObj, masterNodeId, name, requestNodeId, exportNodeId);
    }
    return ShareExportDestroyingAgentCallback(resp, exportObj, masterNodeId, name, requestNodeId, exportNodeId);
}

uint32_t ShareExportMasterCallback(const std::string &exportNodeId, UbseMemShareBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "Share export master callback name=" << exportObj.req.name << ", state=" << exportObj.status.state
                  << ";requestId: " << exportObj.req.requestId;
    auto lock = LoggingLockGuard(exportObj.req.name);
    auto name = exportObj.req.name;
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_SUCCESS) {
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
        UBSE_LOG_INFO << "this is shm callback before shm exportObjstateChange, name is" << exportObj.req.name
                      << ";requestId: " << exportObj.req.requestId;
        return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_BORROW);
    }
    // 归还逻辑
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_DESTROYED) {
        auto req = exportObj.returnReq;
        std::string requestNodeId = req.requestNodeId;
        resp.requestNodeId = requestNodeId;
        resp.requestId = req.requestId;
        if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            EraseShareExport(exportObj);
            UBSE_LOG_INFO << "this is shm callback before shm exportObjstateChange, name is" << exportObj.req.name
                          << ";requestId: " << exportObj.req.requestId;
            UbseMemShmExportObjStateChangeHandler(exportObj);
            UBSE_LOG_INFO << "this is shm return, name is" << exportObj.req.name
                          << ";requestId: " << exportObj.req.requestId;
            // requestNodeId为空则当前场景为对账删除导出账本
            return requestNodeId.empty() ?
                       UBSE_OK :
                       BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_RETURN);
        }
        // 归还失败
        ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        // requestNodeId为空则当前场景为对账删除导出账本
        return requestNodeId.empty() ? UBSE_OK :
                                       BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to unexport",
                                                                  exportObj.errorCode, MemOperationType::SHARED_RETURN);
    }
    return UBSE_OK;
}

uint32_t UbseMemShareBorrowExportObjCallback(const UbseMemShareBorrowExportObj &exportObj)
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
        return ShareExportAgentCallback(exportNodeId, copy, name, masterInfo.nodeId);
    }
    return ShareExportMasterCallback(exportNodeId, copy);
}
void ShareImportFillResp(UbseMemOperationResp &resp, const UbseMemShareBorrowImportObj &importObj)
{
    for (const auto &importResult : importObj.status.importResults) {
        resp.memIdList.push_back(importResult.memId);
    }
    uint64_t realSize{};
    for (const auto &numaInfo : importObj.algoResult.exportNumaInfos) {
        realSize += numaInfo.size;
    }
    resp.realSize = std::to_string(realSize);
}

uint32_t RealImportDecoder(const std::pair<uint32_t, uint32_t> &chipDiePair, UbseMemShareBorrowImportObj &importObj)
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
    res = SetMarIdByLinkInfo(importObj.importNodeId, importObj.algoResult.exportNumaInfos[0].nodeId, chipDiePair,
                             remoteChipDiePair, importParam);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "SetParamMarId by socketId failed";
        return UBSE_ERR_INTERNAL;
    }
    importParam.isHighSafety = IsHighSafety();
    importParam.trustRingData = importObj.req.trustRingData;
    importParam.type = "share";
    res = ImportToAddDecoderEntry(chipDiePair, importObj.exportObmmInfo, importParam, importObj.status);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "ImportToAddDecoderEntry failed, res is " << res;
        uint8_t decoderId = importObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
        UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
        return UBSE_ERR_INTERNAL;
    }
    importObj.req.trustRingData.ClearLendSignedDataMemory();
    return UBSE_OK;
}

uint32_t ShareImportRunningHandler(UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj,
                                   const std::string &masterNodeId, const std::string &name,
                                   const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "ShareImportRunningAgent callback. name is " << name << ";requestId: " << importObj.req.requestId;
    mapLock.LockRead();
    if (nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap.find(importObj.req.name) !=
        nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap.end()) {
        auto oldImportObj = nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap[importObj.req.name];
        if (oldImportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
            mapLock.UnLock();
            return UBSE_OK;
        }
    }
    mapLock.UnLock();
    bool realExe = importObj.importNodeId == importObj.algoResult.exportNumaInfos[0].nodeId ? false : true;
    std::pair<uint32_t, uint32_t> chipDiePair{};
    if (realExe) {
        auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
            return UBSE_ERR_INTERNAL;
        }
    }
    if (realExe) {
        auto res = RealImportDecoder(chipDiePair, importObj);
        if (res != UBSE_OK) {
            return res;
        }
    }
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to import, name is " << name << ", requestNodeId is " << requestNodeId
                       << ";requestId: " << importObj.req.requestId;
        if (realExe) {
            uint8_t decoderId = importObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
            UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
        }
        EraseShareImport(importObj);
        return ret;
    }
    UBSE_LOG_INFO << "Success to import share, name is " << name << ";requestId: " << importObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << importObj.importNodeId << " ShareMemory Import "
                             << std::to_string(importObj.req.size) << " Bytes Success";
    return UBSE_OK;
}

uint32_t ShareImportRunningAgentCallBack(UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj,
                                         const std::string &masterNodeId, const std::string &name,
                                         const std::string &requestNodeId)
{
    mapLock.LockRead();
    if (nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap.find(importObj.req.name) !=
        nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap.end()) {
        auto nowObj = nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap[importObj.req.name];
        if (nowObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
            mapLock.UnLock();
            return SendShareImportObj(masterNodeId, nowObj, false);
        }
    }
    mapLock.UnLock();
    auto res = ShareImportRunningHandler(resp, importObj, masterNodeId, name, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    } else {
        ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    }
    return SendShareImportObj(masterNodeId, importObj, false);
}

uint32_t ShareImportDestroyingHandler(UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj,
                                      const std::string &name, const std::string &masterNodeId,
                                      const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "Share import destroying agent callback. name is " << name
                  << ";requestId: " << importObj.req.requestId;
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    mapLock.LockRead();
    bool directReply = nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap.find(name) ==
                       nodeMemDebtInfoMap[importObj.importNodeId].shareImportObjMap.end();
    mapLock.UnLock();
    if (directReply) {
        return UBSE_OK;
    }
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmUnImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unimport name is " << name << ";requestId: " << importObj.req.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "Success to unimport share. name is " << name << ";requestId: " << importObj.req.requestId;
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

uint32_t ShareImportDestroyingAgentCallBack(UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj,
                                            const std::string &name, const std::string &masterNodeId,
                                            const std::string &requestNodeId)
{
    auto res = ShareImportDestroyingHandler(resp, importObj, name, masterNodeId, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        UBSE_LOG_ERROR << "ShareUnImport Failed, Failed count:" << ++g_shareUnimportFailedCount << ". advice: Caller should clear memory and retry. "
                       << "If failures persist, migrate the workload and restart the host.";
    } else {
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        EraseShareImport(importObj);
    }
    return SendShareImportObj(masterNodeId, importObj, false);
}

uint32_t ShareImportAgentCallBack(UbseMemShareBorrowImportObj &importObj, const std::string &masterNodeId,
                                  const std::string &name, const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "Share import agent callback. name=" << name << ", state=" << importObj.status.state
                  << ";requestId: " << importObj.req.requestId;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        return ShareImportRunningAgentCallBack(resp, importObj, masterNodeId, name, requestNodeId);
    }
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    return ShareImportDestroyingAgentCallBack(resp, importObj, name, masterNodeId, requestNodeId);
}

uint32_t ShareImportMasterCallBack(UbseMemShareBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "Share import master callback. name=" << importObj.req.name << ", state=" << importObj.status.state
                  << ";requestId: " << importObj.req.requestId;
    auto lock = LoggingLockGuard(importObj.req.name);
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};
    if (importObj.status.expectState == UBSE_MEM_IMPORT_SUCCESS) {
        if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
            ShareImportUpdateState(importObj, importObj.status.state);
            ShareImportFillResp(resp, importObj);
            UBSE_LOG_INFO << "this is shm callback before shm importObjstateChange, name is" << importObj.req.name
                          << ";requestId: " << importObj.req.requestId;
            UbseMemShmImportObjStateChangeHandler(importObj);
            return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_ATTACH);
        }
        EraseShareImport(importObj);
        return BuildOperationRespWhenFail(resp, importObj.req.name, importObj.req.requestNodeId, "Failed to import.",
                                          importObj.errorCode, MemOperationType::SHARED_ATTACH);
    }
    if (importObj.status.expectState == UBSE_MEM_IMPORT_DESTROYED) {
        if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
            EraseShareImport(importObj);
            UBSE_LOG_INFO << "this is shm callback before shm importObjstateChange, name is" << importObj.req.name
                          << ";requestId: " << importObj.req.requestId;
            UbseMemShmImportObjStateChangeHandler(importObj);
            return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_DETACH);
        }
        ShareImportUpdateState(importObj, importObj.status.state);
        return BuildOperationRespWhenFail(resp, importObj.req.name, importObj.req.requestNodeId, "Failed to unimport.",
                                          importObj.errorCode, MemOperationType::SHARED_DETACH);
    }
    return UBSE_OK;
}

uint32_t UbseMemShareBorrowImportObjCallback(const UbseMemShareBorrowImportObj &importObj)
{
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Failed to get master's info, " << FormatRetCode(ret);
        return ret;
    }

    auto importNodeId = importObj.importNodeId;
    auto name = importObj.req.name;
    auto copy = importObj;
    if (importNodeId == currentNodeInfo.nodeId &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        return ShareImportAgentCallBack(copy, masterInfo.nodeId, name, importNodeId);
    }

    return ShareImportMasterCallBack(copy);
}
uint32_t DealSendShareUnExportObjFailed(UbseMemShareBorrowExportObj &exportObj, const UbseMemReturnReq &req,
                                        UbseMemOperationResp &resp, const std::string &name)
{
    resp.name = name;
    resp.requestNodeId = req.requestNodeId;
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to send exportObj.",
                                      UBSE_ERR_UNIMPORT_SUCCESS);
}

NodeMemDebtInfoMap GetNodeDebtInfoMap()
{
    mapLock.LockRead();
    auto map = nodeMemDebtInfoMap;
    mapLock.UnLock();
    return map;
}

uint32_t UbseMemShareReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp,
                            const std::string &realRequestNodeId)
{
    UBSE_LOG_INFO << "Start to share return, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ";requestId: " << req.requestId << ", realRequestNodeId:" << realRequestNodeId;
    auto lock = LoggingLockGuard(req.name);
    InitializeResponse(req, resp);
    NodeMemDebtInfoMap map = GetNodeDebtInfoMap();
    std::vector<UbseMemShareBorrowExportObj> exportObjs;
    std::vector<UbseMemShareBorrowImportObj> importObjs;
    FindShareBorrowObjByName(map, req.name, exportObjs, importObjs);
    UbseMemShareBorrowImportObj importObj{};
    if (!importObjs.empty()) {
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Resource attached.",
                                          UBSE_ERR_SHM_ATTACH_USING, MemOperationType::SHARED_RETURN);
    }
    if (exportObjs.empty()) {
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "resource not found.",
                                          UBSE_ERR_NOT_EXIST, MemOperationType::SHARED_RETURN);
    }
    UbseMemShareBorrowExportObj exportObj = exportObjs[0];
    UbseMemStage memStage = GetMemStageByExportObjState(exportObj, true);
    if (memStage == UbseMemStage::UBSE_CREATING || memStage == UbseMemStage::UBSE_DELETING) {
        UBSE_LOG_INFO << "resource is being borrowed or returned, name is " << req.name;
        auto ret = (memStage == UbseMemStage::UBSE_CREATING) ? UBSE_ERR_CREATING : UBSE_ERR_DELETING;
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "resource being borrowed or returned",
                                          static_cast<uint32_t>(ret), MemOperationType::SHARED_RETURN);
    }
    if (!CheckShareReturnPermission(exportObj.req.udsInfo, req.udsInfo, realRequestNodeId, exportObj.req.shmRegion)) {
        std::string shmRegionIds;
        for (const auto &node : exportObj.req.shmRegion.nodelist) {
            shmRegionIds += node.nodeId;
            shmRegionIds += ", ";
        }
        UBSE_LOG_ERROR << "name:" << req.name << " auth failed,req username:" << req.udsInfo.username
                       << ", req uid:" << req.udsInfo.uid << ", export obj username:" << exportObj.req.udsInfo.username
                       << ", export obj uid:" << exportObj.req.udsInfo.uid
                       << ", realRequestNodeId:" << realRequestNodeId << ", shmRegionIds: " << shmRegionIds;
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Error auth", UBSE_ERR_AUTH_FAILED,
                                          MemOperationType::SHARED_RETURN);
    }
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.returnReq = req;
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    if (SendShareExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true) != UBSE_OK) {
        return DealSendShareUnExportObjFailed(exportObj, req, resp, req.name);
    }
    return UBSE_OK;
}

uint32_t UpdateFaultShareExportObj(const std::string &nodeId, uint64_t memId, const std::string &memName,
                                   UbMemFaultType type)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to update shared memory debt due to fault.";
    mapLock.LockWrite();

    auto &debt = nodeMemDebtInfoMap[nodeId].shareExportObjMap;
    auto itor = debt.find(memName);
    if (itor == debt.end()) {
        mapLock.UnLock();
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find fault memory by name=" << memName << ".";
        return UBSE_ERROR;
    }
    auto &obmmVec = itor->second.status.exportObmmInfo;
    auto obmmItor = std::find_if(obmmVec.begin(), obmmVec.end(),
                                 [&](const UbseMemObmmInfo &info) -> bool { return info.memId == memId; });
    if (obmmItor == obmmVec.end()) {
        mapLock.UnLock();
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find fault memory by memId=" << memId << ".";
        return UBSE_ERROR;
    }
    obmmItor->memIdStatus = type;
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to update the state of export memId= " << memId
                  << " to fault type=" << static_cast<uint16_t>(obmmItor->memIdStatus) << ".";

    mapLock.UnLock();
    return UBSE_OK;
}

uint32_t AddShareImport(const UbseMemShareBorrowImportObj &importObj)
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

uint32_t AddShareExport(const UbseMemShareBorrowExportObj &exportObj)
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

uint32_t DeleteShareExport(const UbseMemShareBorrowExportObj &exportObj)
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
    return SendShareExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, copy, true);
}
} // namespace ubse::mem::controller
