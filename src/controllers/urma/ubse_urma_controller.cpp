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
#include <cstddef>
#include <cstdint>
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

bool IsUdmaDevHealthy(const std::string& feEid)
{
    std::string dummyName;
    return UbseGetUrmaSubpathByEid(feEid, dummyName) == UBSE_OK && !dummyName.empty();
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

static UbseResult PushUvsTopoBatch(bool isPushShareTopoOnly, uint32_t batchNum, uint32_t batchSize,
                                   const std::string& nodeId)
{
    bool isClos = UbseSmbios::GetInstance().IsClosType();
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
    bool isClos = UbseSmbios::GetInstance().IsClosType();
    const uint32_t batchSize = isClos ? 32 : 0;
    const uint32_t batchNum = isClos ? (UBSE_CLOS_MAX_NODE_NUM + batchSize - 1) / batchSize : 1;

    auto ret = PushUvsTopoBatch(false, batchNum, batchSize, nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to push uvs topo batch, isPushShareTopoOnly=false, ret=" << ret;
        return ret;
    }
    ret = PushUvsTopoBatch(true, batchNum, batchSize, nodeId);
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
    if (auto ret = PushNodesTopoToUvs(curNode.nodeId); ret != UBSE_OK) {
        return ret;
    }
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
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    if (curNode.nodeId.empty()) {
        UBSE_LOG_WARN << "Failed to get current node info";
        return UBSE_ERROR;
    }
    auto urmaNodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(curNode.nodeId);
    // 查询设备激活状态，若接口查询失败，返回上次查询结果
    bool isAllPortDown = false;
    static bool lastQueryResult = false;
    if (auto ret = QueryAllPortsDown(isAllPortDown); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query all ports status, use last query result="
                      << static_cast<int>(lastQueryResult);
        isAllPortDown = lastQueryResult;
    } else {
        lastQueryResult = isAllPortDown;
    }
    for (auto& dev : urmaNodeInfo.urmaList) {
        if (dev.first == UBSE_HOST_URMA_DEV_NAME) {
            continue;
        }
        nameInfo.push_back(dev.first);
        bool health = true;
        for (auto& eidGroup : dev.second.eidGroups) {
            if (isAllPortDown) {
                health = false;
                break;
            }
            if (!IsUdmaDevHealthy(eidGroup.primaryEid)) {
                health = false;
                break;
            }
        }
        status.push_back(static_cast<uint32_t>(health ? UrmaDevState::ACTIVED : UrmaDevState::INACTIVED));
        hwResIds.push_back(dev.second.hwResId);
    }

    return UBSE_OK;
}

bool UbseUrmaController::IsUrmaDevCreated(const UbseUrmaInfo& urmaInfo)
{
    // 判断依据为bonding的subPath、下属fe的name是否为空
    if (urmaInfo.subPath.empty()) {
        UBSE_LOG_INFO << "Urma dev not created, subPath is empty";
        return false;
    }
    bool isActivate = false;
    if (auto ret = UbseGetBondingActiveStateByEid(urmaInfo.urmaDevEid, isActivate); ret != UBSE_OK || !isActivate) {
        UBSE_LOG_INFO << "Urma dev not created, urmaDevEid=" << urmaInfo.urmaDevEid << ", isActivate=" << isActivate;
        return false;
    }
    if (urmaInfo.eidGroups.empty()) {
        UBSE_LOG_INFO << "Urma dev not created, eidGroups is empty";
        return false;
    }
    for (auto& eidGroup : urmaInfo.eidGroups) {
        if (eidGroup.feInfo == nullptr || eidGroup.feInfo->name.empty() ||
            UbseGetBondingActiveStateByEid(eidGroup.primaryEid, isActivate) != UBSE_OK || !isActivate) {
            UBSE_LOG_INFO << "Urma dev not created, feInfo is empty or fe name is empty";
            return false;
        }
    }
    return true;
}

UbseResult UbseUrmaController::UbseAllocUrmaDev(const std::string& urmaName, UbseUrmaDevPath& devPaths)
{
    UBSE_LOG_INFO << "Receive urma-alloc request, name=" << urmaName;
    bool isAllPortDown = false;
    if (auto ret = QueryAllPortsDown(isAllPortDown); ret != UBSE_OK || isAllPortDown) {
        UBSE_LOG_WARN << "Failed to query all ports status or all ports are down, cannot allocate urma dev, urmaName="
                      << urmaName;
        return ret;
    }
    std::vector<std::string> feNames;
    std::string eid;
    UbseUrmaInfo urmaInfo;
    if (auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfoByName(urmaName, urmaInfo);
        ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get urma dev from urma controller manager, ret=" << ret
                      << ", urmaName=" << urmaName;
        return ret;
    }
    // 如果设备还未创建，调用urma接口创建
    if (!IsUrmaDevCreated(urmaInfo)) {
        UBSE_LOG_INFO << "Urma dev not created, urmaName=" << urmaName << ", try to create it";
        if (ActivateSpecifyUrmaDev(urmaName) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to activate urma bonding, urmaName=" << urmaName;
            return UBSE_URMACONTRL_ERROR_CREATE_DEV_FAILED;
        }
    }
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return UBSE_URMACONTRL_ERROR_GET_NODE_INFO_FAILED;
    }
    RefreshUrmaDevStateByName(currentNodeInfo.nodeId, urmaName);
    if (auto ret = UbseUrmaControllerManager::GetInstance().AllocUrmaDev(urmaName, feNames, eid); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to alloc urma dev, ret=" << ret;
        return ret;
    }
    const size_t minFeNamesSize = 2;
    if (feNames.size() <= minFeNamesSize) {
        UBSE_LOG_ERROR << "Failed to alloc for fe name size is less than 2";
        return UBSE_ERROR;
    }
    devPaths.bondingPath = PATH_PREFIX + feNames[0];
    for (auto it = feNames.begin() + 1; it != feNames.end(); ++it) {
        devPaths.vfePaths.push_back(PATH_PREFIX + *it);
    }
    devPaths.bondingEid = eid;
    return UBSE_OK;
}

UbseResult UbseUrmaController::UbseFreeUrmaDev([[maybe_unused]] const std::string urmaName)
{
    return UBSE_OK;
}

UbseResult UbseUrmaController::UbseGetUrmaDevsByRpc(const uint32_t& nodeId, std::vector<UbseUrmaDevBrief>& urmaInfo)
{
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
    UrmaDevQueryRpcReq req;
    req.nodeId = nodeId;
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
    urmaInfo = rsp.urmaInfos;
    if (rsp.result != UBSE_OK) {
        UBSE_LOG_ERROR << "response result is not OK, " << FormatRetCode(rsp.result);
        return rsp.result;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaController::UbseGetUrmaDevsByNodeId(const uint32_t& nodeId, std::vector<UbseUrmaDevBrief>& devInfos)
{
    if (nodeId == UINT32_MAX) {
        this->GetLocalUrmaDevs(devInfos);
        return UBSE_OK;
    }
    if (UbseSmbios::GetInstance().IsClosType()) {
        return UBSE_ERR_NOT_SUPPORTED;
    }
    std::vector<UbseNodeInfo> ubseStaticNodeInfos = UbseNodeController::GetInstance().GetStaticNodeInfo();
    if (ubseStaticNodeInfos.empty()) {
        UBSE_LOG_ERROR << "LoadConfig get allNodes failed.";
        return UBSE_ERROR;
    }
    if (!std::any_of(ubseStaticNodeInfos.begin(), ubseStaticNodeInfos.end(),
                     [&](const auto& info) { return info.nodeId == std::to_string(nodeId); })) {
        UBSE_LOG_WARN << "nodeId = " << nodeId << " not in cluster.";
        return UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST;
    }
    std::unordered_map<std::string, UbseNodeInfo> ubseNodeInfoMap = UbseNodeController::GetInstance().GetAllNodes();
    if (ubseNodeInfoMap.empty()) {
        UBSE_LOG_ERROR << "get allNodes from nodectl failed.";
        return UBSE_ERROR_INVAL;
    }
    if (ubseNodeInfoMap.find(std::to_string(nodeId)) == ubseNodeInfoMap.end()) {
        UBSE_LOG_WARN << "nodeId = " << nodeId << " not node up.";
        return UBSE_ERROR_INVAL;
    }
    auto nodeState = ubseNodeInfoMap[std::to_string(nodeId)].clusterState;
    if (nodeState == UbseNodeClusterState::UBSE_NODE_UNKNOWN || nodeState == UbseNodeClusterState::UBSE_NODE_FAULT ||
        nodeState == UbseNodeClusterState::UBSE_NODE_PRE_BMC) {
        UBSE_LOG_WARN << "nodeId = " << nodeId << " is fault state = " << (int)nodeState;
        return UBSE_ERROR_INVAL;
    }
    AsyncHandlerGuard cntGuard;
    if (ubse::context::g_globalStop) {
        return UBSE_OK;
    }
    ubse::election::UbseRoleInfo currentNodeInfo{};
    ubse::election::UbseGetCurrentNodeInfo(currentNodeInfo);
    if (std::to_string(nodeId) == currentNodeInfo.nodeId) {
        this->GetLocalUrmaDevs(devInfos);
        return UBSE_OK;
    }
    return UbseGetUrmaDevsByRpc(nodeId, devInfos);
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

void UbseUrmaController::GetLocalUrmaDevs(std::vector<UbseUrmaDevBrief>& devInfos)
{
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get current node info";
        return;
    }
    const size_t feCntPerUrmaInfo = 2;
    RefreshAllUrmaDevsState(currentNodeInfo.nodeId);
    auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(currentNodeInfo.nodeId);
    for (auto& info : nodeInfo.urmaList) {
        if (info.first == UBSE_HOST_URMA_DEV_NAME) {
            continue;
        }
        UbseUrmaDevBrief urmaInfo;
        urmaInfo.urmaName = info.first;
        if (info.second.eidGroups.size() != feCntPerUrmaInfo) {
            UBSE_LOG_WARN << "Failed to get fe info for urmaName=" << info.first << " in urmaList";
            continue;
        }
        for (auto& eidGroup : info.second.eidGroups) {
            urmaInfo.feEids.push_back(eidGroup.primaryEid);
            urmaInfo.feNames.push_back(eidGroup.feInfo == nullptr ? "" : eidGroup.feInfo->name);
        }
        urmaInfo.state = info.second.state;
        urmaInfo.devEid = info.second.urmaDevEid;
        urmaInfo.bondingType = info.second.urmaDevType;
        devInfos.push_back(urmaInfo);
    }
}
} // namespace ubse::urmaController
