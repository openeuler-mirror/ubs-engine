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
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_logger_inner.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"
#include "ubse_topology_interface.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_module.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::urma;
using namespace ubse::task_executor;
using namespace ubse::mti;
using namespace ubse::nodeController;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_CONTROLLER_MID)

const int INDEX_NO_2 = 2;
const std::string PATH_PREFIX = "/dev/uburma/";
const uint32_t BYTE_TO_BIT = 8;

std::shared_ptr<UbseFeInfo> GetUrmaVfeFromEidGroup(EidGroup &eidGroup)
{
    if (eidGroup.feInfo) {
        return eidGroup.feInfo;
    }
    return nullptr;
}

UbseResult UrmaController::UbseUrmaBandWidthSet(const std::string urmaName, uint32_t minBandWidth,
                                                uint32_t maxBandWidth)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthSet Start," << urmaName << ", minBandWidth=" << minBandWidth
                  << ", maxBandWidth=" << maxBandWidth;
    // 从urma名获取urma信息，如果找不到返回错误码UBS_ENGINE_ERR_NOT_EXIST
    UbseUrmaInfo urmaInfo;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetLocalUrmaDevInfo failed, urmaName =" << urmaName;
        return UBSE_ERROR_NOT_EXIST;
    }
    // 判断是否独享类型，不是返回不支持
    if (urmaInfo.urmaDevType != UrmaDevType::UNIQUE) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthSet failed, urmaDevType ="
                       << (uint32_t)urmaInfo.urmaDevType;
        return UBSE_ERROR_NOT_SUPPORT;
    }
    // 创建profile
    UbseLcneQosProfile lcneQosProfile;
    const std::string profileName = "Profile_" + urmaName;
    lcneQosProfile.proflieName = profileName;
    lcneQosProfile.minBandWidth = minBandWidth;
    lcneQosProfile.maxBandWidth = maxBandWidth;
    ret = UbseCreatQosProfile(lcneQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseLcneQos::CreatQosProfile failed," << profileName << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    // 对Fe下发Qos带宽
    for (auto i : urmaInfo.eidGroups) {
        std::shared_ptr<UbseFeInfo> ubseFeInfo = GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            return UBSE_ERROR;
        }
        UbseLcneFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseLcneFeType)ubseFeInfo->fetype;
        ret = UbseApplyVfeQos(lcneFeInfo, profileName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseLcneQos::ApplyVfeQos failed";
            return UBSE_ERROR;
        }
    }
    // Qos信息本地存储，按照8的倍数生效防止重启后LCNE信息不一致
    UrmaQosProfile urmaQosProfile;
    urmaQosProfile.profileName = profileName;
    urmaQosProfile.minBandWidth = minBandWidth / BYTE_TO_BIT * BYTE_TO_BIT;
    urmaQosProfile.maxBandWidth = maxBandWidth / BYTE_TO_BIT * BYTE_TO_BIT;
    return UbseUrmaControllerManager::GetInstance().SetUrmaQos(urmaName, urmaQosProfile);
}

UbseResult UrmaController::UbseUrmaBandWidthGet(const std::string urmaName, uint32_t &minBandWidth,
                                                uint32_t &maxBandWidth)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthGet Start," << urmaName;
    UrmaQosProfile urmaQosProfile;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetUrmaQos(urmaName, urmaQosProfile);
    if (ret != UBSE_OK || urmaQosProfile.profileName == "") {
        UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetUrmaQos failed," << urmaName << FormatRetCode(ret);
        return UBSE_ERROR_NOT_EXIST;
    }
    minBandWidth = urmaQosProfile.minBandWidth;
    maxBandWidth = urmaQosProfile.maxBandWidth;
    return ret;
}

UbseResult UrmaController::UbseUrmaBandWidthReset(const std::string urmaName)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthReset Start," << urmaName;
    // 从urma名获取urma信息，如果找不到返回错误码UBS_ENGINE_ERR_NOT_EXIST
    UbseUrmaInfo urmaInfo;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetLocalUrmaDevInfo failed, urmaName =" << urmaName;
        return UBSE_ERROR_NOT_EXIST;
    }
    // 判断是否独享类型，不是返回不支持
    if (urmaInfo.urmaDevType != UrmaDevType::UNIQUE) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthReset failed, urmaDevType ="
                       << (uint32_t)urmaInfo.urmaDevType;
        return UBSE_ERROR_NOT_SUPPORT;
    }
    for (auto i : urmaInfo.eidGroups) {
        std::shared_ptr<UbseFeInfo> ubseFeInfo = GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            return UBSE_ERROR;
        }
        UbseLcneFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseLcneFeType)ubseFeInfo->fetype;
        ret = UbseDeleteVfeQos(lcneFeInfo);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseLcneQos::DeleteVfeQos failed.";
            return UBSE_ERROR;
        }
    }

    const std::string profileName = "Profile_" + urmaName;
    ret = UbseDeleteQosProfile(profileName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseLcneQos::DeleteQosProfile failed," << profileName << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    UrmaQosProfile urmaQosProfile;
    urmaQosProfile.profileName = "";
    urmaQosProfile.minBandWidth = UINT32_MAX;
    urmaQosProfile.maxBandWidth = UINT32_MAX;
    return UbseUrmaControllerManager::GetInstance().SetUrmaQos(urmaName, urmaQosProfile);
}

void UrmaController::UbseUrmaBandWidthUpdate(const std::string urmaName)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthUpdate Start," << urmaName;
    UbseUrmaInfo urmaInfo;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetLocalUrmaDevInfo failed," << urmaName;
        return;
    }
    // 先查询是否存在对应的Qos配置，没有则返回成功
    UbseLcneQosProfile ubseLcneQosProfile;
    const std::string profileName = "Profile_" + urmaName;
    ret = UbseQureyQosProfile(profileName, ubseLcneQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "UbseLcneQos::QureyQosProfile failed," << profileName << FormatRetCode(ret);
        return;
    }
    // 接下来遍历该urma下的Fe是否都生效了该proflie配置
    if (UbseUrmaBandWidthCheck(urmaInfo, profileName)) {
        // 检查没问题需要回复本地数据
        UrmaQosProfile urmaQosProfile;
        urmaQosProfile.profileName = profileName;
        urmaQosProfile.minBandWidth = ubseLcneQosProfile.minBandWidth;
        urmaQosProfile.maxBandWidth = ubseLcneQosProfile.maxBandWidth;
        UbseUrmaControllerManager::GetInstance().SetUrmaQos(urmaName, urmaQosProfile);
        return;
    }
    // 先删除VFE上面所有的生效Qos，然后再删除profile
    for (auto i : urmaInfo.eidGroups) {
        std::shared_ptr<UbseFeInfo> ubseFeInfo = GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            continue;
        }
        UbseLcneFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseLcneFeType)ubseFeInfo->fetype;
        UbseDeleteVfeQos(lcneFeInfo);
    }
    UbseDeleteQosProfile(profileName);
    return;
}

bool UrmaController::UbseUrmaBandWidthCheck(UbseUrmaInfo urmaInfo, const std::string profileName)
{
    for (auto i : urmaInfo.eidGroups) {
        std::string vfeProfileName;
        std::shared_ptr<UbseFeInfo> ubseFeInfo = GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            return true;
        }
        UbseLcneFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseLcneFeType)ubseFeInfo->fetype;
        auto ret = UbseQueryVfeQos(lcneFeInfo, vfeProfileName);
        if ((ret != UBSE_OK) || (vfeProfileName != profileName)) {
            return false;
        }
    }
    return true;
}

UbseResult UrmaController::UbseTopoLinkChangeHandler(std::string &eventId, const std::string &eventMesage)
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
    urmaExecutor->Execute([]() { return UrmaController::GetInstance().HandleTopoLinkChangeWithRetry(); });
    return UBSE_OK;
}

UbseResult QueryUrmaInfoStateFromUrma(const std::string &nodeId)
{
    bool isAllPortDown = false;
    if (auto ret = QueryAllPortsStatus(isAllPortDown); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query all ports status, ret=" << ret;
        return ret;
    }
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UBSE_LOG_INFO << "All ports are down, set all URMA info to inactive";
        UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToInactiveForNode(nodeId);
        return UBSE_OK;
    }
    auto urmaModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaModule == nullptr) {
        UBSE_LOG_WARN << "Getting UrmaModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(nodeId);
    for (auto &urmaInfo : nodeInfo.urmaList) {
        auto urmaEid = urmaInfo.second.urmaDevEid;
        bool isUrmaActive = false;
        if (urmaModule->GetStateByUrmaEid(urmaEid, isUrmaActive) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to get urma state by urmaEid=" << urmaEid;
            continue;
        }
        if (isUrmaActive) {
            UbseUrmaControllerManager::GetInstance().SetActiveState(urmaEid, nodeId);
        } else {
            UbseUrmaControllerManager::GetInstance().SetInactiveState(urmaEid, nodeId);
        }
    }
    return UBSE_OK;
}

UbseResult UrmaController::DoTopoLinkChange()
{
    AsyncHandlerGuard cntGuard;
    if (g_globalStop) {
        return UBSE_OK;
    }
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    // 下发所有节点拓扑及所有urmaInfo
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    UbseUrmaControllerManager::GetInstance().GetAllUvsInfo(uvsInfos);
    if (auto ret = UrmaControllerSetUvsInfo(curNode.nodeId, GetDirConnectInfo(), uvsInfos); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to set uvs info, ret=" << ret;
        return ret;
    }
    // 向urma重新查询bounding状态，并更新状态
    if (auto ret = QueryUrmaInfoStateFromUrma(curNode.nodeId); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query urma info state from urma, ret=" << ret;
        return ret;
    }
    return UBSE_OK;
}

UbseResult UrmaController::HandleTopoLinkChangeWithRetry()
{
    std::string taskExecutor = "UrmaExecutor";
    std::string taskName = "UrmaTopoLinkChangeRetryTimer";
    auto task = []() {
        return UrmaController::GetInstance().DoTopoLinkChange();
    };
    // 定时器每5s执行一次
    return HandleTaskWithRetry(taskExecutor, taskName, NO_5, task);
}

UbseResult QueryAllPortsStatus(bool &isAllPortDown)
{
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    std::vector<PhysicalLink> allLinkInfo;
    if (auto ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodeTopo(allLinkInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node topology, ret=" << ret;
        return ret;
    }
    isAllPortDown = std::all_of(allLinkInfo.begin(), allLinkInfo.end(), [&curNode](const auto &linkInfo) {
        return linkInfo.slotId != curNode.slotId && linkInfo.peerSlotId != curNode.slotId;
    });
    return UBSE_OK;
}

UbseResult UrmaController::DoNodeJoin(const std::string &joinNodeId)
{
    UBSE_LOG_INFO << "Node join, joinNodeId=" << joinNodeId;
    UbseResult ret = UBSE_OK;
    AsyncHandlerGuard cntGuard;
    if (g_globalStop) {
        return ret;
    }
    // 向mti查询本节点所有vfe对应静态urma eid
    std::vector<UbseLcneIouInfo> iouList;
    std::vector<std::vector<UbseLcneFeInfo>> allFeInfos; // allFeInfos[i] 表示第i个iou上的fe信息
    if (ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodeIouList(iouList); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get current node IOU list";
        return ret;
    }
    UBSE_LOG_INFO << "Get current node VFE EID";
    for (auto &iou : iouList) {
        std::vector<UbseLcneFeInfo> tmpFeInfos;
        if (ret = UbseGetVfeEid(iou, tmpFeInfos); ret != UBSE_OK) {
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
    if (ret = QueryAllPortsStatus(isAllPortDown); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query all ports status, ret=" << ret;
        return ret;
    }
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UBSE_LOG_INFO << "All ports are down for nodeId=" << curNode.nodeId << ", set all URMA info to inactive";
        UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToInactiveForNode(curNode.nodeId);
    }
    // 向master节点上报本节点nodeInfo
    if (curNode.nodeId != joinNodeId) {
        UBSE_LOG_INFO << "Current node is not the join node, skip reporting, currentNodeId=" << curNode.nodeId
                      << ", joinNodeId=" << joinNodeId;
        return UBSE_OK;
    }
    return ReportUrmaNodeInfoToMaster(curNode.nodeId);
}

UbseResult UrmaController::HandleNodeJoinWithRetry(const std::string &joinNodeId)
{
    std::string taskExecutor = "UrmaExecutor";
    std::string taskName = "UrmaNodeJoinRetryTimer_" + joinNodeId;
    auto task = [joinNodeId]() {
        return UrmaController::GetInstance().DoNodeJoin(joinNodeId);
    };
    // 定时器每5s执行一次
    return HandleTaskWithRetry(taskExecutor, taskName, NO_5, task);
}

UbseResult UrmaController::UbseNodeJoinHandler(std::string &eventId, const std::string &eventMesage)
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
        [eventMesage]() { return UrmaController::GetInstance().HandleNodeJoinWithRetry(eventMesage); });
    return UBSE_OK;
}

UbseResult UrmaController::UbseGetLocalUrmaDevInfo(std::vector<std::string> &nameInfo, std::vector<uint32_t> &status,
                                                   std::vector<uint64_t> &hwResIds)
{
    return UbseUrmaControllerManager::GetInstance().GetAllUrmaName(nameInfo, status, hwResIds);
}

UbseResult UrmaController::UbseAllocUrmaDev(const std::string urmaName, UbseUrmaDevPath &devPaths)
{
    std::vector<std::string> feNames;
    std::string eid;
    if (auto ret = UbseUrmaControllerManager::GetInstance().AllocByUrmaName(urmaName, feNames, eid); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to alloc urma dev, ret=" << ret;
        return ret;
    }
    if (feNames.size() <= INDEX_NO_2) {
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
UbseResult UrmaController::UbseFreeUrmaDev(const std::string urmaName)
{
    return UBSE_OK;
}

UbseResult UrmaController::UbseQueryUrmaInfoByRpc(const uint32_t &nodeId, const UrmaDevType type,
                                                  std::vector<UbseUrmaInfoForQuery> &urmaInfo)
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
    UrmaDevQueryRpcReq req = {nodeId, static_cast<uint32_t>(type)};
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
        UBSE_LOG_ERROR << "response result is not OK, " << FormatRetCode(res);
        return rsp.result;
    }
    return UBSE_OK;
}

UbseResult UrmaController::UbseGetUrmaDevInfoByNodeIdAndType(const UrmaDevType type, const uint32_t &nodeId,
                                                             std::vector<UbseUrmaInfoForQuery> &devInfos)
{
    if (nodeId == UINT32_MAX) {
        UbseUrmaControllerManager::GetInstance().GetUrmaNameForQueryByType(type, devInfos);
        return UBSE_OK;
    }
    std::vector<UbseNodeInfo> ubseStaticNodeInfos = UbseNodeController::GetInstance().GetStaticNodeInfo();
    if (ubseStaticNodeInfos.empty()) {
        UBSE_LOG_ERROR << "LoadConfig get allNodes failed.";
        return UBSE_ERROR;
    }
    if (!std::any_of(ubseStaticNodeInfos.begin(), ubseStaticNodeInfos.end(),
                     [&](const auto &info) { return info.nodeId == std::to_string(nodeId); })) {
        UBSE_LOG_WARN << "nodeId = " << nodeId << " not in cluster.";
        return UBSE_ERROR_NOT_EXIST;
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
    if (g_globalStop) {
        return UBSE_OK;
    }
    ubse::election::UbseRoleInfo currentNodeInfo{};
    ubse::election::UbseGetCurrentNodeInfo(currentNodeInfo);
    if (std::to_string(nodeId) == currentNodeInfo.nodeId) {
        UbseUrmaControllerManager::GetInstance().GetUrmaNameForQueryByType(type, devInfos);
        return UBSE_OK;
    }
    return UbseQueryUrmaInfoByRpc(nodeId, type, devInfos);
}

UbseResult UrmaController::UbseUrmaCliDevActivate(const std::string &nodeId)
{
    UBSE_LOG_INFO << "UbseUrmaCliDevActivate nodeId = " << nodeId;
    ubse::election::UbseRoleInfo currentNodeInfo{};
    ubse::election::UbseGetCurrentNodeInfo(currentNodeInfo);
    if (nodeId == currentNodeInfo.nodeId) {
        return UrmaCtlActivateUrmaDevice(nodeId);
    }
    // 转发至主节点处理
    UBSE_LOG_INFO << "Foward activate nodeId=" << nodeId << " request to master";
    auto ubseComModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "UbseComModule is null";
        return UBSE_ERROR_NULLPTR;
    }
    UbseUrmaActivateUrmaInfoReqSimpoPtr req = new (std::nothrow) UbseUrmaActivateUrmaInfoReqSimpo();
    UbseUrmaActivateUrmaInfoRspSimpoPtr rsp = new (std::nothrow) UbseUrmaActivateUrmaInfoRspSimpo();
    if (req == nullptr || rsp == nullptr) {
        UBSE_LOG_WARN << "Failed to allocate memory for request or response";
        return UBSE_ERROR_NULLPTR;
    }
    req->SetNodeId(nodeId);
    ubse::election::UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetMasterInfo failed";
        return ret;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                        static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_DEV_ACTIVATE)};
    ret = ubseComModule->RpcSend(sendParam, req, rsp);
    if (ret != UBSE_OK || rsp->GetErrCode() != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to do rpc send, ret=" << ret << ", " << FormatRetCode(rsp->GetErrCode());
        if (ret != UBSE_OK) {
            return ret;
        }
        return rsp->GetErrCode();
    }
    return UBSE_OK;
}

std::vector<ubse::nodeController::PhysicalLink> GetDirConnectInfo()
{
    std::vector<ubse::nodeController::PhysicalLink> allLinkInfo;
    auto allLinkMap = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    if (allLinkMap.empty()) {
        UBSE_LOG_WARN << "GetDirConnectInfo failed";
        return {};
    }
    allLinkInfo.reserve(allLinkMap.size());
    for (const auto &link : allLinkMap) {
        allLinkInfo.push_back(std::move(link.second));
    }
    UBSE_LOG_INFO << "GetDirConnectInfo success, size=" << allLinkInfo.size();
    return allLinkInfo;
}

UbseResult UrmaControllerSetUvsInfo(const std::string &current_slot_id, const std::vector<PhysicalLink> &allLinkInfo,
                                    const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo)
{
    auto urmaModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaModule == nullptr) {
        UBSE_LOG_ERROR << "Getting UrmaModule failed.";
        return UBSE_ERROR;
    }
    static std::mutex setUvsInfoMutex; // 加锁避免多线程下发造成竞态条件
    std::lock_guard<std::mutex> lock(setUvsInfoMutex);
    std::string nodeId = current_slot_id;
    return urmaModule->SetUvsInfo(nodeId, allLinkInfo, bondingInfo);
}

UbseResult UrmaCtlActivateUrmaDeviceForOneNode(UbseUrmaUvsNodeInfo &devInfo)
{
    auto urmaModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaModule == nullptr) {
        UBSE_LOG_WARN << "Getting UrmaModule failed.";
        return UBSE_ERROR_NULLPTR;
    }

    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    for (auto &dev : devInfo.devList) {
        if (g_globalStop) {
            return UBSE_OK;
        }
        if (urmaModule->ActivateBondingDevice(dev.urmaDevEid) == UBSE_OK) {
            UbseUrmaControllerManager::GetInstance().SetActiveState(dev.urmaDevEid, curNode.nodeId);
        } else {
            UbseUrmaControllerManager::GetInstance().SetInactiveState(dev.urmaDevEid, curNode.nodeId);
            UBSE_LOG_WARN << "Failed to activate bonding device for eid=" << dev.urmaDevEid;
            return UBSE_ERROR_AGAIN;
        }
        std::string subPath;
        if (auto ret = urmaModule->GetNameByUrmaEid(dev.urmaDevEid, subPath); ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to get urma name for eid=" << dev.urmaDevEid;
            return ret;
        }
        UbseUrmaControllerManager::GetInstance().SetUrmaSubPath(dev.urmaDevEid, subPath);
        for (auto &feInfo : dev.feList) {
            if (g_globalStop) {
                return UBSE_OK;
            }
            std::string urmaEidName;
            if (auto ret = urmaModule->GetNameByUrmaEid(feInfo.primaryEid, urmaEidName); ret != UBSE_OK) {
                UBSE_LOG_WARN << "Failed to get fe name for eid=" << feInfo.primaryEid;
                return ret;
            }
            UbseUrmaControllerManager::GetInstance().SetFeName(feInfo.primaryEid, urmaEidName);
        }
    }
    return UBSE_OK;
}

UbseResult UrmaCtlActivateUrmaDevice(const std::string &nodeId)
{
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    UbseUrmaControllerManager::GetInstance().GetAllUvsInfo(uvsInfos);
    if (auto ret = UrmaControllerSetUvsInfo(nodeId, GetDirConnectInfo(), uvsInfos); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to set uvs info, ret=" << ret;
        return ret;
    }

    static std::mutex activeteMutex;
    std::lock_guard<std::mutex> lock(activeteMutex);
    for (auto &devInfo : uvsInfos) {
        if (g_globalStop) {
            break;
        }
        if (devInfo.nodeId != nodeId) {
            continue;
        }
        if (auto ret = UrmaCtlActivateUrmaDeviceForOneNode(devInfo); ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to activate urma device for node=" << nodeId;
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

} // namespace ubse::urmaController