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

#include "ubse_mem_share_capabilities.h"

#include <chrono>
#include <string>
#include <thread>

#include "ubse_logger.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_scheduler.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_util.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::mem::scheduler;
using namespace ubse::nodeController;

namespace {
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
} // namespace

void NormalizeShareRegion(UbseMemShareBorrowReq &req)
{
    if (req.shmRegion.nodeNum != 0 || !req.shmRegion.nodelist.empty()) {
        return;
    }

    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    req.shmRegion.nodeNum = nodeInfos.size();
    for (const auto &[_, nodeInfo] : nodeInfos) {
        ubse::adapter_plugins::mmi::UbseNodeInfo ubseNodeInfo{nodeInfo.slotId, nodeInfo.nodeId, nodeInfo.hostName};
        req.shmRegion.nodelist.push_back(ubseNodeInfo);
    }
}

bool ValidateAffinityParams(const UbseMemShareBorrowReq &req)
{
    if (!req.withAffinity.enableCreateWithAffinity) {
        return true;
    }
    if (req.withAffinity.createReqNodeId.empty()) {
        UBSE_LOG_ERROR << "Invalid affinity parameters: nodeId is empty, nodeId=" << req.withAffinity.createReqNodeId
                       << ",socketId=" << req.withAffinity.affinitySocketId;
        return false;
    }
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(req.withAffinity.createReqNodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "Invalid affinity parameters: node not found, nodeId=" << req.withAffinity.createReqNodeId
                    << ", socketId=" << req.withAffinity.affinitySocketId;
        return false;
    }    
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

bool CheckBorrowDuplicate(const std::string &name, IShareStore &store)
{
    return store.ExistBorrow(name);
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
            UBSE_LOG_ERROR << "Invalid argument: " << e.what() << ", nodeId=" << node.nodeId
                        << ", fallback index=" << (nodeInfo.index);
        } catch (const std::out_of_range &e) {
            nodeInfo.index = index++;
            UBSE_LOG_ERROR << "Out of range: " << e.what();
        }
    }
    return UBSE_OK;
}

UbseResult ShareAllocate(const UbseMemShareBorrowReq &req, UbseMemShareBorrowExportObj &exportObj)
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

bool CheckRegions(const UbseMemShareAttachReq &req, const UbseMemShareBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "Share import node id=" << req.importNodeId << ", requestId=" << req.requestId;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Export's exportNumaInfo is empty, requestId=" << req.requestId;
        return false;
    }
    for (const auto &node : exportObj.req.shmRegion.nodelist) {
        if (node.nodeId == req.importNodeId) {
            return true;
        }
    }
    UBSE_LOG_ERROR << "Failed to check share regions, requestId=" << req.requestId;
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

void ConstructShareImportObj(UbseMemShareBorrowImportObj &importObj, const UbseMemShareAttachReq &req)
{
    if (req.size == 0) {
        // attach 请求未指定 size，复用 borrow 时记录的 size，无需更新 importObj.req.size
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
}

} // namespace ubse::mem::controller
