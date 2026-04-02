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
#include "ubse_logger.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_module.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_uvs_module.h"
#include "ubse_smbios.h"

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

const int INDEX_NO_2 = 2;
const std::string PATH_PREFIX = "/dev/uburma/";
const uint32_t BYTE_TO_BIT = 8;

std::mutex g_invokeUrmaMutex;
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
        return UBSE_ERR_NOT_EXIST;
    }
    // 判断是否独享类型，不是返回不支持
    if (urmaInfo.urmaDevType != UrmaDevType::UNIQUE) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthSet failed, urmaDevType ="
                       << (uint32_t)urmaInfo.urmaDevType;
        return UBSE_ERR_NOT_SUPPORTED;
    }
    // 创建profile
    UbseMtiQosProfile lcneQosProfile;
    const std::string profileName = "Profile_" + urmaName;
    lcneQosProfile.profileName = profileName;
    lcneQosProfile.minBandWidth = minBandWidth;
    lcneQosProfile.maxBandWidth = maxBandWidth;
    ret = UbseMtiInterface::GetInstance().UbseCreateQosProfile(lcneQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMtiQos::CreatQosProfile failed," << profileName << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    // 对Fe下发Qos带宽
    for (auto i : urmaInfo.eidGroups) {
        std::shared_ptr<UbseFeInfo> ubseFeInfo = GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            return UBSE_ERROR;
        }
        UbseMtiFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseMtiFeType)ubseFeInfo->fetype;
        ret = UbseMtiInterface::GetInstance().UbseApplyVfeQos(lcneFeInfo, profileName);
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
        return UBSE_ERR_NOT_EXIST;
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
        return UBSE_ERR_NOT_EXIST;
    }
    // 判断是否独享类型，不是返回不支持
    if (urmaInfo.urmaDevType != UrmaDevType::UNIQUE) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthReset failed, urmaDevType ="
                       << (uint32_t)urmaInfo.urmaDevType;
        return UBSE_ERR_NOT_SUPPORTED;
    }
    for (auto i : urmaInfo.eidGroups) {
        std::shared_ptr<UbseFeInfo> ubseFeInfo = GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            return UBSE_ERROR;
        }
        UbseMtiFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseMtiFeType)ubseFeInfo->fetype;
        ret = UbseMtiInterface::GetInstance().UbseDeleteVfeQos(lcneFeInfo);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseLcneQos::DeleteVfeQos failed.";
            return UBSE_ERROR;
        }
    }

    const std::string profileName = "Profile_" + urmaName;
    ret = UbseMtiInterface::GetInstance().UbseDeleteQosProfile(profileName);
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
    UbseMtiQosProfile ubseLcneQosProfile;
    const std::string profileName = "Profile_" + urmaName;
    ret = UbseMtiInterface::GetInstance().UbseQueryQosProfile(profileName, ubseLcneQosProfile);
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
        UbseMtiFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseMtiFeType)ubseFeInfo->fetype;
        ret = UbseMtiInterface::GetInstance().UbseDeleteVfeQos(lcneFeInfo);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "UbseLcneQos::DeleteVfeQos failed.";
            continue;
        }
    }
    UbseMtiInterface::GetInstance().UbseDeleteQosProfile(profileName);
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
        UbseMtiFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseMtiFeType)ubseFeInfo->fetype;
        auto ret = UbseMtiInterface::GetInstance().UbseQueryVfeQos(lcneFeInfo, vfeProfileName);
        if (ret != UBSE_OK || vfeProfileName != profileName) {
            UBSE_LOG_WARN << "UbseLcneQos::QueryVfeQos failed, " << FormatRetCode(ret);
            return false;
        }
    }
    return true;
}

UbseResult UrmaController::UbseTopoLinkChangeHandler([[maybe_unused]] std::string &eventId,
                                                     [[maybe_unused]] const std::string &eventMessage)
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

void SetUrmaInfoState(const std::string &urmaDevEid, bool isActive, const std::string &nodeId)
{
    if (isActive) {
        UbseUrmaControllerManager::GetInstance().SetActiveState(urmaDevEid, nodeId);
    } else {
        UbseUrmaControllerManager::GetInstance().SetInactiveState(urmaDevEid, nodeId);
    }
}

std::string GetUrmaDevEidByUrmaName(const std::string &urmaName)
{
    UbseUrmaInfo urmaInfo;
    auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK || urmaInfo.urmaDevEid.empty()) {
        UBSE_LOG_WARN << "Failed to find urma info by urmaName=" << urmaName;
        return "";
    }
    return urmaInfo.urmaDevEid;
}

bool IsUdmaDevHealthy(const std::string &feEid)
{
    std::string dummyName;
    return UbseGetUrmaSubpathByEid(feEid, dummyName) == UBSE_OK && !dummyName.empty();
}

bool IsUrmaBondingActivated(const std::string &urmaName)
{
    UbseUrmaInfo urmaInfo;
    auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK || urmaInfo.subPath.empty()) {
        UBSE_LOG_WARN << "Failed to find urma info by urmaName=" << urmaName;
        return false;
    }
    return true;
}

UbseResult QueryUrmaInfoStateFromUrma(const std::string &nodeId, const std::string &urmaName)
{
    bool isAllPortDown = false;
    if (auto ret = QueryAllPortsDown(isAllPortDown); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query all ports status, ret=" << ret;
        return ret;
    }
    bool isQueryOneUrma = !urmaName.empty();
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UBSE_LOG_INFO << "All ports are down, set URMA info to inactive";
        if (isQueryOneUrma) {
            if (auto urmaDevEid = GetUrmaDevEidByUrmaName(urmaName); !urmaDevEid.empty()) {
                SetUrmaInfoState(urmaDevEid, false, nodeId);
                return UBSE_OK;
            }
            return UBSE_ERR_NOT_EXIST;
        } else {
            UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToInactiveForNode(nodeId);
        }
        return UBSE_OK;
    }
    auto urmaModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaModule == nullptr) {
        UBSE_LOG_WARN << "Getting UrmaModule failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(nodeId);
    // 查询指定urma的状态，如果为空则查询所有urma
    if (isQueryOneUrma) {
        if (auto urmaDevEid = GetUrmaDevEidByUrmaName(urmaName); !urmaDevEid.empty()) {
            bool isUrmaActive = false;
            if (UbseGetBondingActiveStateByEid(urmaDevEid, isUrmaActive) != UBSE_OK) {
                UBSE_LOG_WARN << "Failed to get urma state by urmaEid=" << urmaDevEid;
                return UBSE_ERROR;
            }
            UBSE_LOG_INFO << "urma name=" << urmaName << ", isActive=" << static_cast<int>(isUrmaActive);
            SetUrmaInfoState(urmaDevEid, isUrmaActive && IsUrmaBondingActivated(urmaName), nodeId);
            return UBSE_OK;
        }
        return UBSE_ERR_NOT_EXIST;
    }
    for (auto &urmaInfo : nodeInfo.urmaList) {
        auto urmaEid = urmaInfo.second.urmaDevEid;
        bool isUrmaActive = false;
        if (UbseGetBondingActiveStateByEid(urmaEid, isUrmaActive) != UBSE_OK) {
            continue;
        }
        SetUrmaInfoState(urmaEid, isUrmaActive && IsUrmaBondingActivated(urmaInfo.first), nodeId);
    }
    return UBSE_OK;
}

UbseResult UrmaController::DoTopoLinkChange()
{
    AsyncHandlerGuard cntGuard;
    if (ubse::context::g_globalStop) {
        return UBSE_OK;
    }
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    // 下发所有节点拓扑及所有urmaInfo
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    UbseUrmaControllerManager::GetInstance().GetAllUvsInfo(uvsInfos);
    if (auto ret = UbseUrmaControllerSetUvsInfo(curNode.nodeId, GetDirConnectInfo(), uvsInfos); ret != UBSE_OK) {
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

UbseResult QueryAllPortsDown(bool &isAllPortDown)
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
    for (auto &iou : iouList) {
        std::vector<UbseMtiFeInfo> tmpFeInfos;
        if (ret = UbseMtiInterface::GetInstance().UbseGetVfeEid(iou, tmpFeInfos); ret != UBSE_OK) {
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

UbseResult UrmaController::UbseNodeJoinHandler([[maybe_unused]] std::string &eventId, const std::string &eventMesage)
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
    return UbseUrmaControllerManager::GetInstance().GetAllUrmaInfo(nameInfo, status, hwResIds);
}

UbseResult UrmaController::UbseAllocUrmaDev(const std::string &urmaName, UbseUrmaDevPath &devPaths)
{
    std::vector<std::string> feNames;
    std::string eid;
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return UBSE_ERROR;
    }
    (void)QueryUrmaInfoStateFromUrma(currentNodeInfo.nodeId, urmaName);
    UbseUrmaInfo urmaInfo;
    auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK || urmaInfo.state != UrmaDevState::ACTIVED || urmaInfo.subPath.empty()) {
        UBSE_LOG_WARN << "Failed to query urma info state from urma, nodeId=" << currentNodeInfo.nodeId
                      << ", urmaName=" << urmaName << ", try to activate it";
        if (ActivateSpecifyUrmaBonding(urmaName) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to activate urma bonding, urmaName=" << urmaName;
            return UBSE_ERROR;
        }
    }
    if (ret = UbseUrmaControllerManager::GetInstance().AllocByUrmaName(urmaName, feNames, eid); ret != UBSE_OK) {
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
UbseResult UrmaController::UbseFreeUrmaDev([[maybe_unused]] const std::string urmaName)
{
    return UBSE_OK;
}

UbseResult UrmaController::UbseQueryUrmaInfoByRpc(const uint32_t &nodeId, std::vector<UbseUrmaInfoForQuery> &urmaInfo)
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

UbseResult UrmaController::UbseGetUrmaDevInfoByNodeId(const uint32_t &nodeId,
                                                      std::vector<UbseUrmaInfoForQuery> &devInfos)
{
    if (nodeId == UINT32_MAX) {
        UbseUrmaControllerManager::GetInstance().GetUrmaInfoForQuery(devInfos);
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
                     [&](const auto &info) { return info.nodeId == std::to_string(nodeId); })) {
        UBSE_LOG_WARN << "nodeId = " << nodeId << " not in cluster.";
        return UBSE_ERR_NOT_EXIST;
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
        UbseUrmaControllerManager::GetInstance().GetUrmaInfoForQuery(devInfos);
        return UBSE_OK;
    }
    return UbseQueryUrmaInfoByRpc(nodeId, devInfos);
}

UbseResult UrmaController::UbseUrmaCliDevActivate(const std::string &nodeId, const std::string &urmaName)
{
    UBSE_LOG_INFO << "UbseUrmaCliDevActivate nodeId = " << nodeId;
    ubse::election::UbseRoleInfo currentNodeInfo{};
    ubse::election::UbseGetCurrentNodeInfo(currentNodeInfo);
    if (nodeId == currentNodeInfo.nodeId) {
        return ActivateSpecifyUrmaBonding(urmaName);
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
    req->SetUrmaName(urmaName);
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
        UBSE_LOG_WARN << "GetDirConnectInfo failed, try to get current node topology";
        if (auto ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodeTopo(allLinkInfo); ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to get current node topology, ret=" << ret;
            return {};
        }
        return allLinkInfo;
    }
    allLinkInfo.reserve(allLinkMap.size());
    for (const auto &link : allLinkMap) {
        allLinkInfo.push_back(std::move(link.second));
    }
    UBSE_LOG_INFO << "GetDirConnectInfo success, size=" << allLinkInfo.size();
    return allLinkInfo;
}

UbseResult UbseUrmaControllerSetUvsInfo(const std::string &current_slot_id,
                                        const std::vector<PhysicalLink> &allLinkInfo,
                                        const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo)
{
    auto urmaModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaModule == nullptr) {
        UBSE_LOG_ERROR << "Getting UrmaModule failed.";
        return UBSE_ERROR;
    }
    UbseMeshType meshType;
    auto ret = UbseSmbios::GetInstance().GetMeshType(meshType);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get smbios method type, ret=" << ret;
        return ret;
    }
    // 加锁避免多线程下发造成竞态条件
    std::lock_guard<std::mutex> lock(g_invokeUrmaMutex);
    std::string nodeId = current_slot_id;
    std::vector<PhysicalLink> emptyLinkInfo;
    return UbsePushTopoAndBondingToUvs(nodeId, meshType == UbseMeshType::CLOS ? emptyLinkInfo : allLinkInfo,
                                       bondingInfo);
}

UbseResult RecoverOneUrmaDeviceForOneNode(const std::string &nodeId, UbseUrmaUvsAggrDev &dev)
{
    std::string subPath;
    if (auto ret = UbseGetUrmaSubpathByEid(dev.urmaDevEid, subPath); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get urma name for eid=" << dev.urmaDevEid;
        SetUrmaInfoState(dev.urmaDevEid, false, nodeId);
        return UBSE_ERROR;
    }
    UbseUrmaControllerManager::GetInstance().SetUrmaSubPath(dev.urmaDevEid, subPath);
    for (auto &feInfo : dev.feList) {
        if (ubse::context::g_globalStop) {
            return UBSE_OK;
        }
        std::string urmaEidName;
        if (auto ret = UbseGetUrmaSubpathByEid(feInfo.primaryEid, urmaEidName); ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to get fe name for eid=" << feInfo.primaryEid;
            SetUrmaInfoState(dev.urmaDevEid, false, nodeId);
            return UBSE_ERROR;
        }
        UbseUrmaControllerManager::GetInstance().SetFeName(feInfo.primaryEid, urmaEidName);
    }
    UBSE_LOG_INFO << "Recover urma device for eid=" << dev.urmaDevEid << " success";
    SetUrmaInfoState(dev.urmaDevEid, true, nodeId);
    return UBSE_OK;
}

void UrmaController::RecoverUrmaDeviceForOneNode(const std::string &nodeId, std::vector<UbseUrmaUvsNodeInfo> &uvsInfos)
{
    auto it =
        std::find_if(uvsInfos.begin(), uvsInfos.end(), [&nodeId](const auto &info) { return info.nodeId == nodeId; });
    if (it == uvsInfos.end()) {
        UBSE_LOG_WARN << "Failed to find nodeId=" << nodeId << " in uvsInfos";
        return;
    }

    auto urmaModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::urma::UbseUrmaUvsModule>();
    if (urmaModule == nullptr) {
        UBSE_LOG_WARN << "Getting UrmaModule failed.";
        return;
    }
    for (auto &dev : it->devList) {
        if (ubse::context::g_globalStop) {
            return;
        }
        if (RecoverOneUrmaDeviceForOneNode(nodeId, dev) != UBSE_OK) {
            continue;
        }
    }
    return;
}

UbseResult UrmaController::ActivateSpecifyUrmaBonding(const std::string &urmaName)
{
    UbseUrmaInfo urmaInfo;
    if (auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get urmaInfo for urmaName=" << urmaName << " in uvsInfos";
        return ret;
    }
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    std::lock_guard<std::mutex> lock(g_invokeUrmaMutex);
    bool isActivated = UbseActiveBonding(urmaInfo.urmaDevEid) == UBSE_OK;
    SetUrmaInfoState(urmaInfo.urmaDevEid, isActivated, curNode.nodeId);
    if (!isActivated) {
        UBSE_LOG_WARN << "Failed to activate bonding device for eid=" << urmaInfo.urmaDevEid;
        return UBSE_ERROR_AGAIN;
    }
    std::string subPath;
    if (auto ret = UbseGetUrmaSubpathByEid(urmaInfo.urmaDevEid, subPath); ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get urma name for eid=" << urmaInfo.urmaDevEid;
        return ret;
    }
    UbseUrmaControllerManager::GetInstance().SetUrmaSubPath(urmaInfo.urmaDevEid, subPath);
    for (auto &eidGroup : urmaInfo.eidGroups) {
        if (ubse::context::g_globalStop) {
            return UBSE_OK;
        }
        std::string feName;
        if (auto ret = UbseGetUrmaSubpathByEid(eidGroup.primaryEid, feName); ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to get fe name for eid=" << eidGroup.primaryEid;
            return ret;
        }
        UbseUrmaControllerManager::GetInstance().SetFeName(eidGroup.primaryEid, feName);
    }
    UBSE_LOG_INFO << "Activate bonding device for eid=" << urmaInfo.urmaDevEid << " success";
    return UBSE_OK;
}

} // namespace ubse::urmaController