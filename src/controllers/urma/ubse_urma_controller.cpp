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

#include "ubse_urma_controller.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_logger.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller.h"
#include "ubse_smbios.h"
#include "ubse_thread_pool_module.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_qos.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_controller_util.h"
#include "ubse_urma_def.h"
#include "ubse_urma_resource_view.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::urma;
using namespace ubse::task_executor;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::nodeController;
using namespace ubse::election;
using namespace ubse::adapter_plugins::smbios;

UBSE_DEFINE_THIS_MODULE("ubse");

const std::string PATH_PREFIX = "/dev/uburma/";
const uint32_t BYTE_TO_BIT = 8;

std::shared_ptr<UbseFeInfo> GetUrmaVfeFromEidGroup(EidGroup& eidGroup)
{
    if (eidGroup.feInfo) {
        return eidGroup.feInfo;
    }
    return nullptr;
}

UbseResult UbseUrmaController::UbseTopoLinkChangeHandler([[maybe_unused]] std::string& eventId,
                                                         [[maybe_unused]] const std::string& eventMessage)
{
    // 这里要切一个线程,避免耗时操作阻塞事件回调
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto urmaExecutor = taskExecutor->Get("UrmaExecutor");
    if (urmaExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor for urma failed";
        return UBSE_ERROR_NULLPTR;
    }
    urmaExecutor->Execute([]() { return UbseUrmaController::GetInstance().HandleTopoLinkChangeWithRetry(); });
    return UBSE_OK;
}

std::string GetUrmaDevEidByUrmaName(const std::string& urmaName)
{
    UbseUrmaInfo urmaInfo;
    auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfoByName(urmaName, urmaInfo);
    if (ret != UBSE_OK || urmaInfo.urmaDevEid.empty()) {
        UBSE_LOG_WARN << "Failed to find urma info by urmaName=" << urmaName;
        return "";
    }
    return urmaInfo.urmaDevEid;
}

static UbseResult GetRequiredUrmaSubpath(const std::string& eid, std::string& subpath)
{
    auto ret = UbseGetUrmaSubpathByEid(eid, subpath);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query URMA subpath, eid=" << eid << ", ret=" << ret;
        return ret;
    }
    if (subpath.empty()) {
        UBSE_LOG_ERROR << "URMA subpath is empty, eid=" << eid;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

struct HostUrmaBacking {
    std::string bondingEid;
    std::vector<std::string> feEids;
};

static UbseResult QueryHostUrmaBacking(HostUrmaBacking& backing)
{
    const auto curNode = UbseNodeController::GetInstance().GetCurNode();
    if (curNode.nodeId.empty()) {
        UBSE_LOG_ERROR << "Failed to get current node while querying host URMA backing";
        return UBSE_URMACONTRL_ERROR_GET_NODE_INFO_FAILED;
    }
    std::vector<UbseUrmaUvsNodeInfo> planning;
    auto ret = UbseNodeController::GetInstance().GetPlanningHostBondingByNodeId(curNode.nodeId, planning);
    constexpr size_t feCount = 2;
    if (ret != UBSE_OK || planning.size() != 1 || planning[0].nodeId != curNode.nodeId ||
        planning[0].devList.size() != 1 || planning[0].devList[0].feList.size() != feCount) {
        UBSE_LOG_ERROR << "Failed to query complete host URMA planning, backingName=" << UBSE_HOST_URMA_DEV_NAME
                       << ", nodeId=" << curNode.nodeId << ", ret=" << ret;
        return ret == UBSE_OK ? UBSE_ERROR_INVAL : ret;
    }
    const auto& planned = planning[0].devList[0];
    if (planned.urmaDevEid.empty() || std::any_of(planned.feList.begin(), planned.feList.end(),
                                                  [](const auto& fe) { return fe.primaryEid.empty(); })) {
        UBSE_LOG_ERROR << "Host URMA planning metadata is incomplete, backingName=" << UBSE_HOST_URMA_DEV_NAME;
        return UBSE_ERROR_INVAL;
    }
    HostUrmaBacking result;
    result.bondingEid = planned.urmaDevEid;
    for (const auto& fe : planned.feList) {
        result.feEids.push_back(fe.primaryEid);
    }
    backing = std::move(result);
    return UBSE_OK;
}

static UbseResult CheckHostUrmaBackingActive(const std::string& bondingEid)
{
    bool isActive = false;
    auto ret = UbseGetBondingActiveStateByEid(bondingEid, isActive);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query host URMA backing state, backingName=" << UBSE_HOST_URMA_DEV_NAME
                       << ", bondingEid=" << bondingEid << ", ret=" << ret;
        return ret;
    }
    if (isActive) {
        return UBSE_OK;
    }
    // bonding_dev_0 由 Node 创建，SDK 只能使用已激活的真实设备。
    UBSE_LOG_WARN << "Host URMA backing is not active, backingName=" << UBSE_HOST_URMA_DEV_NAME
                  << ", bondingEid=" << bondingEid;
    return UBSE_URMACONTRL_ERROR_DEV_NOT_INACTIVE;
}

static UbseResult BuildHostUrmaPaths(const HostUrmaBacking& backing, UbseUrmaDevPath& devPaths)
{
    UbseUrmaDevPath result;
    std::string subpath;
    auto ret = GetRequiredUrmaSubpath(backing.bondingEid, subpath);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to build host URMA bonding path, bondingEid=" << backing.bondingEid
                       << ", ret=" << ret;
        return ret;
    }
    result.bondingPath = PATH_PREFIX + subpath;
    for (const auto& feEid : backing.feEids) {
        if ((ret = GetRequiredUrmaSubpath(feEid, subpath)) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to build host URMA FE path, bondingEid=" << backing.bondingEid
                           << ", primaryEid=" << feEid << ", ret=" << ret;
            return ret;
        }
        result.vfePaths.push_back(PATH_PREFIX + subpath);
    }
    result.bondingEid = backing.bondingEid;
    devPaths = std::move(result);
    return UBSE_OK;
}

static UbseResult AllocHostUrmaBacking(UbseUrmaDevPath& devPaths)
{
    auto& manager = UbseUrmaControllerManager::GetInstance();
    UbseUrmaDevPath cachedPath;
    if (manager.GetHostUrmaDevPath(cachedPath)) {
        auto ret = CheckHostUrmaBackingActive(cachedPath.bondingEid);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to reuse cached host URMA device path, bondingEid=" << cachedPath.bondingEid
                          << ", ret=" << ret;
            return ret;
        }
        devPaths = std::move(cachedPath);
        return UBSE_OK;
    }

    // bonding_dev_0 不进入 urmaList，首次分配时从 Node planning 获取稳定 EID。
    HostUrmaBacking backing;
    auto ret = QueryHostUrmaBacking(backing);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query host URMA backing for allocation, ret=" << ret;
        return ret;
    }
    ret = CheckHostUrmaBackingActive(backing.bondingEid);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Host URMA backing is unavailable for allocation, bondingEid=" << backing.bondingEid
                      << ", ret=" << ret;
        return ret;
    }
    UbseUrmaDevPath candidate;
    if ((ret = BuildHostUrmaPaths(backing, candidate)) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to build host URMA device paths, bondingEid=" << backing.bondingEid
                       << ", ret=" << ret;
        return ret;
    }
    manager.SetHostUrmaDevPath(candidate);
    devPaths = std::move(candidate);
    return UBSE_OK;
}

bool IsUrmaDevActivated(const std::string& urmaName)
{
    UbseUrmaInfo urmaInfo;
    auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfoByName(urmaName, urmaInfo);
    if (ret != UBSE_OK || urmaInfo.subPath.empty()) {
        UBSE_LOG_WARN << "Failed to find urma info by urmaName=" << urmaName;
        return false;
    }
    return true;
}

void RefreshAllUrmaDevsState(const std::string& nodeId)
{
    /*
     * 1.先查询端口状态是否都down，若都down，则将所有urmaInfo状态设为PORT_DOWN
     * 2.若有端口up，则查询urmaInfo状态是否激活，若激活则设为ACTIVED，否则设为INACTIVED
     */
    UBSE_LOG_INFO << "Refresh URMA info state for node=" << nodeId;
    bool isAllPortDown = false;
    if (auto ret = QueryAllPortsDown(isAllPortDown); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query all ports status from LCNE, ret=" << ret << ", set all urma info to UNKNOWN";
        UbseUrmaControllerManager::GetInstance().SetAllUrmaDevStateForNode(urma::UrmaDevState::UNKNOWN);
        return;
    }
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UBSE_LOG_INFO << "All ports are down, set URMA info to inactive";
        UbseUrmaControllerManager::GetInstance().SetAllUrmaDevStateForNode(UrmaDevState::PORT_DOWN);
        return;
    }
    auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(nodeId);
    for (auto& urmaInfo : nodeInfo.urmaList) {
        auto urmaEid = urmaInfo.second.urmaDevEid;
        bool isUrmaCreated = UbseUrmaController::GetInstance().IsUrmaDevCreated(urmaInfo.second);
        if (isUrmaCreated) {
            UBSE_LOG_INFO << "Urma dev " << urmaInfo.first << " is created";
            UbseUrmaControllerManager::GetInstance().SetUrmaDevStateByDevEid(urmaEid, UrmaDevState::ACTIVED);
            continue;
        }
        UBSE_LOG_INFO << "Urma dev " << urmaInfo.first << " is not created";
        UbseUrmaControllerManager::GetInstance().SetUrmaDevStateByDevEid(urmaEid, UrmaDevState::INACTIVED);
    }
}

void RefreshUrmaDevStateByName(const std::string& nodeId, const std::string& urmaName)
{
    /*
     * 1.先查询端口状态是否都down，若都down，则将所有urmaInfo状态设为PORT_DOWN
     * 2.若有端口up，则查询urmaInfo状态是否激活，若激活则设为ACTIVED，否则设为INACTIVED
     */
    UBSE_LOG_INFO << "Refresh URMA info state for node=" << nodeId << ", urmaName=" << urmaName;
    bool isAllPortDown = false;
    if (auto ret = QueryAllPortsDown(isAllPortDown); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query all ports status from LCNE, ret=" << ret << ", set all urma info to UNKNOWN";
        UbseUrmaControllerManager::GetInstance().SetAllUrmaDevStateForNode(urma::UrmaDevState::UNKNOWN);
        return;
    }
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UBSE_LOG_INFO << "All ports are down, set URMA info to inactive";
        UbseUrmaControllerManager::GetInstance().SetAllUrmaDevStateForNode(UrmaDevState::PORT_DOWN);
        return;
    }
    auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(nodeId);
    if (nodeInfo.urmaList.find(urmaName) == nodeInfo.urmaList.end()) {
        UBSE_LOG_WARN << "Failed to find urma info by urmaName=" << urmaName << " for node=" << nodeId;
        return;
    }
    auto& urmaInfo = nodeInfo.urmaList[urmaName];
    auto urmaEid = urmaInfo.urmaDevEid;
    bool isUrmaCreated = UbseUrmaController::GetInstance().IsUrmaDevCreated(urmaInfo);
    if (isUrmaCreated) {
        UBSE_LOG_INFO << "Urma dev " << urmaName << " is created";
        UbseUrmaControllerManager::GetInstance().SetUrmaDevStateByDevEid(urmaEid, UrmaDevState::ACTIVED);
    } else {
        UBSE_LOG_INFO << "Urma dev " << urmaName << " is not created";
        UbseUrmaControllerManager::GetInstance().SetUrmaDevStateByDevEid(urmaEid, UrmaDevState::INACTIVED);
    }
}

static UbseResult PushUvsTopoBatch(bool isPushShareTopoOnly, const std::string& nodeId)
{
    bool isClos = UbseSmbios::GetInstance().IsClosType();
    const uint32_t batchSize = isClos ? 32 : 0;
    const uint32_t batchNum = isClos ? (UBSE_CLOS_MAX_NODE_NUM + batchSize - 1) / batchSize : 1;
    bool isBuildHostOnly = isPushShareTopoOnly;
    for (uint32_t i = 0; i < batchNum; ++i) {
        std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
        UbseUrmaControllerManager::GetInstance().BuildUvsTopoNodeInfo(isBuildHostOnly, i * batchSize, batchSize,
                                                                      uvsInfos);
        if (uvsInfos.empty()) {
            UBSE_LOG_WARN << "No uvs info, batch=" << i << ", break";
            return UBSE_ERROR;
        }
        std::vector<PhysicalLink> emptyLinkInfo;
        auto links = isClos ? emptyLinkInfo : GetDirConnectInfo();
        auto ret = isPushShareTopoOnly ? UbsePushShareTopoToUvs(nodeId, links, uvsInfos) :
                                         UbsePushTopoAndBondingToUvs(nodeId, links, uvsInfos);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to push uvs topo batch, batch=" << i << ", ret=" << ret;
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult PushNodesTopoToUvs(const std::string& nodeId)
{
    auto ret = PushUvsTopoBatch(false, nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to push uvs topo batch, isPushShareTopoOnly=false, ret=" << ret;
        return ret;
    }
    if (!UbseSmbios::GetInstance().IsClosType()) {
        return UBSE_OK;
    }
    // 共享拓扑仅在 CLOS 场景追加下发，普通拓扑保持原有流程不变。
    ret = PushUvsTopoBatch(true, nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to push uvs topo batch, isPushShareTopoOnly=true, ret=" << ret;
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaController::DoTopoLinkChange()
{
    AsyncHandlerGuard cntGuard;
    if (ubse::context::g_globalStop) {
        return UBSE_OK;
    }
    auto curNode = UbseNodeController::GetInstance().GetCurNode();

    // 向urma重新查询bounding状态，并更新状态
    RefreshAllUrmaDevsState(curNode.nodeId);
    return UBSE_OK;
}

UbseResult UbseUrmaController::HandleTopoLinkChangeWithRetry()
{
    std::string taskExecutor = "UrmaExecutor";
    std::string taskName = "UrmaTopoLinkChangeRetryTimer";
    auto task = []() {
        return UbseUrmaController::GetInstance().DoTopoLinkChange();
    };
    // 定时器每5s执行一次
    return HandleTaskWithRetry(taskExecutor, taskName, NO_5, task);
}

UbseResult QueryAllPortsDown(bool& isAllPortDown)
{
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    std::vector<PhysicalLink> allLinkInfo;
    if (auto ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodeTopo(allLinkInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query all ports status, please check if topo interface (i.e. topology/nodes) is "
                          "available, ret="
                       << ret;
        return UBSE_URMACONTRL_ERROR_QUERY_PORTS_STATUS_FAILED;
    }
    isAllPortDown = std::all_of(allLinkInfo.begin(), allLinkInfo.end(), [&curNode](const auto& linkInfo) {
        return linkInfo.slotId != curNode.slotId && linkInfo.peerSlotId != curNode.slotId;
    });
    if (isAllPortDown) {
        UBSE_LOG_INFO << "All ports are down for nodeId=" << curNode.nodeId;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaController::DoNodeJoin(const std::string& joinNodeId)
{
    UBSE_LOG_INFO << "Node join, joinNodeId=" << joinNodeId;
    UbseResult ret = UBSE_OK;
    AsyncHandlerGuard cntGuard;
    if (ubse::context::g_globalStop) {
        return ret;
    }
    // 向mti查询本节点所有vfe对应静态urma eid
    std::vector<UbseMtiIouInfo> iouList;
    std::vector<std::vector<UbseMtiFeInfo>> allFeInfos; // allFeInfos[i] 表示第i个iou上的fe信息
    if (ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodeIouList(iouList); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get current node IOU list";
        return ret;
    }
    UBSE_LOG_INFO << "Get current node VFE EID";
    for (auto& iou : iouList) {
        std::vector<UbseMtiFeInfo> tmpFeInfos;
        if (ret = UbseMtiInterface::GetInstance().UbseGetFeEid(iou, tmpFeInfos); ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to get VFE EID for IOU, iou=" << iou.iouId;
            return ret;
        }
        allFeInfos.emplace_back(tmpFeInfos);
    }
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    if (ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo(curNode.nodeId, allFeInfos);
        ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to insert new bounding info";
        return ret;
    }
    // 计算所有port是否都中断
    bool isAllPortDown = false;
    if (ret = QueryAllPortsDown(isAllPortDown); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query all ports status, ret=" << ret;
        return ret;
    }
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UBSE_LOG_INFO << "All ports are down for nodeId=" << curNode.nodeId << ", set all URMA info to PORT_DOWN";
        UbseUrmaControllerManager::GetInstance().SetAllUrmaDevStateForNode(UrmaDevState::PORT_DOWN);
    }
    // 向master节点上报本节点nodeInfo
    if (curNode.nodeId != joinNodeId) {
        UBSE_LOG_INFO << "Current node is not the join node, skip reporting, currentNodeId=" << curNode.nodeId
                      << ", joinNodeId=" << joinNodeId;
        return UBSE_OK;
    }
    return ReportUrmaNodeInfoToMaster(curNode.nodeId);
}

UbseResult UbseUrmaController::HandleNodeJoinWithRetry(const std::string& joinNodeId)
{
    std::string taskExecutor = "UrmaExecutor";
    std::string taskName = "UrmaNodeJoinRetryTimer_" + joinNodeId;
    auto task = [joinNodeId]() {
        return UbseUrmaController::GetInstance().DoNodeJoin(joinNodeId);
    };
    // 定时器每5s执行一次
    return HandleTaskWithRetry(taskExecutor, taskName, NO_5, task);
}

UbseResult UbseUrmaController::UbseNodeJoinHandler([[maybe_unused]] std::string& eventId,
                                                   const std::string& eventMesage)
{
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto urmaExecutor = taskExecutor->Get("UrmaExecutor");
    if (urmaExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get task executor for urma failed";
        return UBSE_ERROR_NULLPTR;
    }
    UBSE_LOG_INFO << "Start to do node join, eventMesage=" << eventMesage;
    urmaExecutor->Execute(
        [eventMesage]() { return UbseUrmaController::GetInstance().HandleNodeJoinWithRetry(eventMesage); });
    return UBSE_OK;
}

UbseResult UbseUrmaController::UbseUrmaGetDevs(std::vector<std::string>& nameInfo, std::vector<uint32_t>& status,
                                               std::vector<uint64_t>& hwResIds)
{
    const auto ret = UbseUrmaResourceView::GetInstance().GetDeviceSummaries(nameInfo, status, hwResIds);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query local URMA device summaries, ret=" << ret;
    }
    return ret;
}

bool UbseUrmaController::IsUrmaDevCreated(const UbseUrmaInfo& urmaInfo)
{
    return UbseUrmaResourceView::GetInstance().IsBackingCreated(urmaInfo);
}

static UbseResult CheckPortStatusForUrmaAlloc(const std::string& urmaName, bool& allocBlocked)
{
    bool isAllPortDown = false;
    auto ret = QueryAllPortsDown(isAllPortDown);
    allocBlocked = ret != UBSE_OK || isAllPortDown;
    if (allocBlocked) {
        UBSE_LOG_WARN << "Failed to query all ports status or all ports are down, cannot allocate urma dev, urmaName="
                      << urmaName << ", allPortsDown=" << static_cast<int>(isAllPortDown) << ", ret=" << ret;
        return ret;
    }
    return UBSE_OK;
}

static UbseResult AllocManagerUrmaBacking(const std::string& backingName, UbseUrmaDevPath& devPaths)
{
    // 逻辑名称解析完成后，普通真实设备完整复用原有 manager 分配流程。
    UbseUrmaInfo urmaInfo;
    auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfoByName(backingName, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get urma dev from urma controller manager, ret=" << ret
                      << ", urmaName=" << backingName;
        return ret;
    }
    if (!UbseUrmaController::GetInstance().IsUrmaDevCreated(urmaInfo)) {
        UBSE_LOG_INFO << "URMA backing is not created, backingName=" << backingName << ", try to create it";
        if (UbseUrmaController::GetInstance().ActivateSpecifyUrmaDev(backingName) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to activate URMA backing, backingName=" << backingName;
            return UBSE_URMACONTRL_ERROR_CREATE_DEV_FAILED;
        }
    }
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return UBSE_URMACONTRL_ERROR_GET_NODE_INFO_FAILED;
    }
    RefreshUrmaDevStateByName(currentNodeInfo.nodeId, backingName);
    std::vector<std::string> feNames;
    std::string eid;
    ret = UbseUrmaControllerManager::GetInstance().AllocUrmaDev(backingName, feNames, eid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to allocate URMA backing, backingName=" << backingName << ", ret=" << ret;
        return ret;
    }
    constexpr size_t feCount = 2;
    if (feNames.size() <= feCount) {
        UBSE_LOG_ERROR << "Invalid URMA allocation path count, backingName=" << backingName
                       << ", actualCount=" << feNames.size();
        return UBSE_ERROR;
    }
    devPaths.bondingPath = PATH_PREFIX + feNames[0];
    for (auto it = feNames.begin() + 1; it != feNames.end(); ++it) {
        devPaths.vfePaths.push_back(PATH_PREFIX + *it);
    }
    devPaths.bondingEid = eid;
    return UBSE_OK;
}

static UbseResult AllocRealUrmaBacking(const UrmaAllocTarget& target, UbseUrmaDevPath& devPaths)
{
    // bonding_dev_0 由 Node 创建且不在 urmaList 中，其余真实设备统一走原 manager 流程。
    if (target.IsHostBonding()) {
        return AllocHostUrmaBacking(devPaths);
    }
    return AllocManagerUrmaBacking(target.GetBackingName(), devPaths);
}

UbseResult UbseUrmaController::UbseAllocUrmaDev(const std::string& urmaName, UbseUrmaDevPath& devPaths)
{
    UBSE_LOG_INFO << "Receive urma-alloc request, name=" << urmaName;
    UrmaAllocTarget target;
    auto ret = UbseUrmaResourceView::GetInstance().ResolveAllocTarget(urmaName, target);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to resolve URMA allocation target, name=" << urmaName << ", ret=" << ret;
        return ret;
    }
    bool allocBlocked = false;
    ret = CheckPortStatusForUrmaAlloc(urmaName, allocBlocked);
    if (allocBlocked) {
        return ret;
    }
    ret = AllocRealUrmaBacking(target, devPaths);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to allocate resolved URMA backing, logicalName=" << urmaName
                       << ", backingName=" << target.GetBackingName()
                       << ", isHostBonding=" << static_cast<int>(target.IsHostBonding()) << ", ret=" << ret;
    }
    return ret;
}

UbseResult UbseUrmaController::UbseFreeUrmaDev([[maybe_unused]] const std::string urmaName)
{
    return UBSE_OK;
}

static void IntersectUrmaDevicesByName(const std::vector<std::string>& filter, std::vector<UbseUrmaDevBrief>& devices)
{
    if (filter.empty()) {
        return;
    }
    const std::unordered_set<std::string> allowed(filter.begin(), filter.end());
    devices.erase(
        std::remove_if(devices.begin(), devices.end(),
                       [&allowed](const auto& device) { return allowed.find(device.urmaName) == allowed.end(); }),
        devices.end());
}

UbseResult UbseUrmaController::UbseGetUrmaDevsByRpc(const uint32_t& nodeId, const std::vector<std::string>& filter,
                                                    std::vector<UbseUrmaDevBrief>& urmaInfo)
{
    urmaInfo.clear();
    auto ubseComModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "UbseComModule is null";
        return UBSE_ERROR_NULLPTR;
    }
    UbseUrmaDevQueryReqPtr ubseRequestPtr = new (std::nothrow) UrmaDevQueryReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseUrmaDevQueryReqSimpo failed";
        return UBSE_ERROR_NULLPTR;
    }
    UrmaDevQueryRpcReq req{nodeId, filter};
    ubseRequestPtr->SetUbseUrmaDevReq(req);
    UbseUrmaDevQueryRspPtr ubseResponsePtr = new (std::nothrow) UrmaDevQueryRspSimpo();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseUrmaDevRspSimpo failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "UbseComModule is null";
        return UBSE_ERROR_NULLPTR;
    }
    ubse::election::UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetMasterInfo failed";
        return res;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                        static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_DEV_QUERY)};
    res = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "comModule RpcSend failed, " << FormatRetCode(res);
        return res;
    }
    auto rsp = ubseResponsePtr->GetUbseUrmaDevRsp();
    if (rsp.result != UBSE_OK) {
        UBSE_LOG_ERROR << "response result is not OK, " << FormatRetCode(rsp.result);
        return rsp.result;
    }
    urmaInfo = std::move(rsp.urmaInfos);
    // 发起端再次取交集，兼容旧节点忽略请求尾部过滤字段后返回全量结果。
    IntersectUrmaDevicesByName(filter, urmaInfo);
    return UBSE_OK;
}

static UbseResult ValidateRemoteUrmaQueryNode(uint32_t nodeId)
{
    if (UbseSmbios::GetInstance().IsClosType()) {
        UBSE_LOG_INFO << "Remote URMA device query is unsupported in CLOS mode, nodeId=" << nodeId;
        return UBSE_ERR_NOT_SUPPORTED;
    }
    const auto nodeIdText = std::to_string(nodeId);
    const auto staticNodes = UbseNodeController::GetInstance().GetStaticNodeInfo();
    if (staticNodes.empty()) {
        UBSE_LOG_ERROR << "Failed to load static node information for URMA query, nodeId=" << nodeId;
        return UBSE_ERROR;
    }
    if (!std::any_of(staticNodes.begin(), staticNodes.end(),
                     [&](const auto& info) { return info.nodeId == nodeIdText; })) {
        UBSE_LOG_WARN << "URMA query node is not in the cluster, nodeId=" << nodeId;
        return UBSE_ERR_NOT_EXIST;
    }

    const auto currentNodes = UbseNodeController::GetInstance().GetAllNodes();
    if (currentNodes.empty()) {
        UBSE_LOG_ERROR << "Failed to load current node information for URMA query, nodeId=" << nodeId;
        return UBSE_ERROR_INVAL;
    }
    const auto node = currentNodes.find(nodeIdText);
    if (node == currentNodes.end()) {
        UBSE_LOG_WARN << "URMA query node is not online, nodeId=" << nodeId;
        return UBSE_ERROR_INVAL;
    }
    const auto state = node->second.clusterState;
    if (state == UbseNodeClusterState::UBSE_NODE_UNKNOWN || state == UbseNodeClusterState::UBSE_NODE_FAULT ||
        state == UbseNodeClusterState::UBSE_NODE_PRE_BMC) {
        UBSE_LOG_WARN << "URMA query node is unavailable, nodeId=" << nodeId << ", state=" << static_cast<int>(state);
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaController::UbseGetUrmaDevsByNodeId(const uint32_t& nodeId, std::vector<UbseUrmaDevBrief>& devInfos,
                                                       const std::vector<std::string>& filter)
{
    if (nodeId == UINT32_MAX) {
        const auto ret = GetLocalUrmaDevs(filter, devInfos);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to query local URMA devices, filterCount=" << filter.size() << ", ret=" << ret;
        }
        return ret;
    }
    auto ret = ValidateRemoteUrmaQueryNode(nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to validate URMA query node, nodeId=" << nodeId << ", ret=" << ret;
        return ret;
    }
    AsyncHandlerGuard cntGuard;
    if (ubse::context::g_globalStop) {
        return UBSE_OK;
    }
    ubse::election::UbseRoleInfo currentNodeInfo{};
    ret = ubse::election::UbseGetCurrentNodeInfo(currentNodeInfo);
    if (ret != UBSE_OK) {
        // 延续原行为：获取本节点信息失败时仍尝试通过主节点转发查询。
        UBSE_LOG_WARN << "Failed to get current node while routing URMA query, targetNodeId=" << nodeId
                      << ", ret=" << ret;
    }
    if (std::to_string(nodeId) == currentNodeInfo.nodeId) {
        ret = GetLocalUrmaDevs(filter, devInfos);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to query URMA devices on current node, nodeId=" << nodeId
                           << ", filterCount=" << filter.size() << ", ret=" << ret;
        }
        return ret;
    }
    // 复用原跨节点查询消息，在请求尾部携带可选过滤条件，由目标节点生成逻辑设备视图。
    ret = UbseGetUrmaDevsByRpc(nodeId, filter, devInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query URMA devices on remote node, nodeId=" << nodeId
                       << ", filterCount=" << filter.size() << ", ret=" << ret;
    }
    return ret;
}

std::vector<ubse::nodeController::PhysicalLink> GetDirConnectInfo()
{
    std::vector<ubse::nodeController::PhysicalLink> allLinkInfo;
    auto allLinkMap = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    if (allLinkMap.empty()) {
        UBSE_LOG_WARN << "GetDirConnectInfo failed, try to get current node topology";
        if (auto ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodeTopo(allLinkInfo); ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to get current node topology, ret=" << ret;
            return {};
        }
        return allLinkInfo;
    }
    allLinkInfo.reserve(allLinkMap.size());
    for (const auto& link : allLinkMap) {
        allLinkInfo.push_back(std::move(link.second));
    }
    UBSE_LOG_INFO << "GetDirConnectInfo success, size=" << allLinkInfo.size();
    return allLinkInfo;
}

UbseResult FillUrmaDevByUvsInfo(UbseUrmaUvsAggrDev& dev)
{
    std::string subPath;
    if (auto ret = UbseGetUrmaSubpathByEid(dev.urmaDevEid, subPath); ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    UbseUrmaControllerManager::GetInstance().SetUrmaSubPath(dev.urmaDevEid, subPath);
    for (auto& feInfo : dev.feList) {
        if (ubse::context::g_globalStop) {
            return UBSE_OK;
        }
        std::string urmaEidName;
        if (auto ret = UbseGetUrmaSubpathByEid(feInfo.primaryEid, urmaEidName); ret != UBSE_OK) {
            return UBSE_ERROR;
        }
        UbseUrmaControllerManager::GetInstance().SetFeName(feInfo.primaryEid, urmaEidName);
    }
    UBSE_LOG_INFO << "Recover urma device for eid=" << dev.urmaDevEid << " success";
    return UBSE_OK;
}

void UbseUrmaController::FillUrmaDevsByUvsInfo(const std::string& nodeId, std::vector<UbseUrmaUvsNodeInfo>& uvsInfos)
{
    auto it =
        std::find_if(uvsInfos.begin(), uvsInfos.end(), [&nodeId](const auto& info) { return info.nodeId == nodeId; });
    if (it == uvsInfos.end()) {
        UBSE_LOG_INFO << "Cannot find uvs info for nodeId=" << nodeId;
        return;
    }
    UBSE_LOG_INFO << "Fill urma dev info by uvs info for nodeId=" << nodeId << ", dev num=" << it->devList.size();
    auto urmaModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaModule == nullptr) {
        UBSE_LOG_WARN << "Getting UrmaModule failed.";
        return;
    }
    for (auto& dev : it->devList) {
        if (ubse::context::g_globalStop) {
            return;
        }
        if (FillUrmaDevByUvsInfo(dev) != UBSE_OK) {
            continue;
        }
    }
    return;
}

UbseResult UbseUrmaController::ActivateSpecifyUrmaDev(const std::string& urmaName)
{
    UbseUrmaInfo urmaInfo;
    if (auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfoByName(urmaName, urmaInfo);
        ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get urmaInfo for urmaName=" << urmaName << " in uvsInfos";
        return ret;
    }
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    bool isActivated = UbseActiveBonding(urmaInfo.urmaDevEid, urmaName) == UBSE_OK;
    if (!isActivated) {
        UBSE_LOG_WARN << "Failed to activate bonding device for eid=" << urmaInfo.urmaDevEid;
        return UBSE_ERROR_AGAIN;
    }
    std::string subPath;
    if (auto ret = UbseGetUrmaSubpathByEid(urmaInfo.urmaDevEid, subPath); ret != UBSE_OK) {
        return ret;
    }
    UbseUrmaControllerManager::GetInstance().SetUrmaSubPath(urmaInfo.urmaDevEid, subPath);
    for (auto& eidGroup : urmaInfo.eidGroups) {
        if (ubse::context::g_globalStop) {
            return UBSE_OK;
        }
        std::string feName;
        if (auto ret = UbseGetUrmaSubpathByEid(eidGroup.primaryEid, feName); ret != UBSE_OK) {
            return ret;
        }
        UbseUrmaControllerManager::GetInstance().SetFeName(eidGroup.primaryEid, feName);
    }
    UBSE_LOG_INFO << "Activate bonding device for eid=" << urmaInfo.urmaDevEid << " success";
    return UBSE_OK;
}

UbseResult UbseUrmaController::GetLocalUrmaDevs(const std::vector<std::string>& filter,
                                                std::vector<UbseUrmaDevBrief>& devInfos)
{
    const auto ret = UbseUrmaResourceView::GetInstance().GetDeviceDetails(filter, devInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query local URMA device details, filterCount=" << filter.size() << ", ret=" << ret;
    }
    return ret;
}
} // namespace ubse::urmaController
