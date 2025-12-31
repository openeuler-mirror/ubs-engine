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

#include "ubse_urma_controller.h"
#include "lcne/ubse_lcne_vfe_eid.h"
#include "src/res_plugins/mti/lcne/ubse_lcne_qos.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_logger_inner.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"
#include "ubse_topology_interface.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::lcne;
using namespace ubse::com;
using namespace ubse::urma;
using namespace ubse::task_executor;
using namespace ubse::mti;
using namespace ubse::nodeController;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_CONTROLLER_MID)

UbseResult UrmaController::UbseUrmaBandWidthSet(const std::string urmaName, uint32_t minBandWidth,
                                                uint32_t maxBandWidth)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthSet Start," << urmaName << ", minBandWidth=" << minBandWidth
                  << ", maxBandWidth=" << maxBandWidth;
    /* 从urma名获取urma信息，如果找不到返回错误码UBS_ENGINE_ERR_NOT_EXIST */
    def::UbseUrmaInfo urmaInfo;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetLocalUrmaDevInfo failed, urmaName =" << urmaName;
        return UBSE_ERROR_SRCH;
    }
    /* 判断是否独享类型，不是返回不支持 */
    if (urmaInfo.urmaDevType != def::UrmaDevType::UNIQUE) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthSet failed, urmaDevType ="
                       << (uint32_t)urmaInfo.urmaDevType;
        return UBSE_ERROR_SRCH;
    }
    /* 创建profile */
    ubse::mti::UbseQosProfile ubseQosProfile;
    const std::string profileName = "Profile_" + urmaName;
    ubseQosProfile.proflieName = profileName;
    ubseQosProfile.minBandWidth = minBandWidth;
    ubseQosProfile.maxBandWidth = maxBandWidth;
    ret = UbseLcneQos::GetInstance().CreatQosProfile(ubseQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseLcneQos::CreatQosProfile failed," << profileName << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }

    /* 对Fe下发Qos带宽 */
    for (auto i : urmaInfo.eidGroups) {
        std::shared_ptr<ubse::urma::def::UbseFeInfo> ubseFeInfo =
            UbseUrmaControllerManager::GetInstance().GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            return UBSE_ERROR;
        }
        UbseLcneFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseLcneFeType)ubseFeInfo->fetype;
        ret = UbseLcneQos::GetInstance().ApplyVfeQos(lcneFeInfo, profileName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseLcneQos::ApplyVfeQos failed";
            return UBSE_ERROR_SRCH;
        }
    }
    return ret;
}

UbseResult UrmaController::UbseUrmaBandWidthGet(const std::string urmaName, uint32_t &minBandWidth,
                                                uint32_t &maxBandWidth)
{
    ubse::mti::UbseQosProfile ubseQosProfile;
    UBSE_LOG_INFO << "UbseUrmaBandWidthGet Start," << urmaName;
    const std::string profileName = "Profile_" + urmaName;
    uint32_t ret = UbseLcneQos::GetInstance().QureyQosProfile(profileName, ubseQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseLcneQos::QureyQosProfile failed," << profileName << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }
    minBandWidth = ubseQosProfile.minBandWidth;
    maxBandWidth = ubseQosProfile.maxBandWidth;
    return ret;
}

UbseResult UrmaController::UbseUrmaBandWidthReset(const std::string urmaName)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthReset Start," << urmaName;
    /* 从urma名获取urma信息，如果找不到返回错误码UBS_ENGINE_ERR_NOT_EXIST */
    def::UbseUrmaInfo urmaInfo;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetLocalUrmaDevInfo failed, urmaName =" << urmaName;
        return UBSE_ERROR_SRCH;
    }
    /* 判断是否独享类型，不是返回不支持 */
    if (urmaInfo.urmaDevType != def::UrmaDevType::UNIQUE) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthReset failed, urmaDevType ="
                       << (uint32_t)urmaInfo.urmaDevType;
        return UBSE_ERROR_SRCH;
    }
    for (auto i : urmaInfo.eidGroups) {
        std::shared_ptr<ubse::urma::def::UbseFeInfo> ubseFeInfo =
            UbseUrmaControllerManager::GetInstance().GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            return UBSE_ERROR;
        }
        UbseLcneFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseLcneFeType)ubseFeInfo->fetype;
        ret = UbseLcneQos::GetInstance().DeleteVfeQos(lcneFeInfo);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseLcneQos::DeleteVfeQos failed.";
            return UBSE_ERROR_SRCH;
        }
    }

    const std::string profileName = "Profile_" + urmaName;
    ret = UbseLcneQos::GetInstance().DeleteQosProfile(profileName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseLcneQos::DeleteQosProfile failed," << profileName << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }
    return ret;
}

void UrmaController::UbseUrmaBandWidthUpdate(const std::string urmaName)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthUpdate Start," << urmaName;
    def::UbseUrmaInfo urmaInfo;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetLocalUrmaDevInfo failed," << urmaName;
        return;
    }
    /* 先查询是否存在对应的Qos配置，没有则返回成功 */
    ubse::mti::UbseQosProfile ubseQosProfile;
    const std::string profileName = "Profile_" + urmaName;
    ret = UbseLcneQos::GetInstance().QureyQosProfile(profileName, ubseQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "UbseLcneQos::QureyQosProfile failed," << profileName << FormatRetCode(ret);
        return;
    }
    /* 接下来遍历该urma下的Fe是否都生效了该proflie配置 */
    if (UbseUrmaBandWidthCheck(urmaInfo, profileName)) {
        return;
    }
    /* 先删除VFE上面所有的生效Qos，然后再删除profile */
    for (auto i : urmaInfo.eidGroups) {
        std::shared_ptr<ubse::urma::def::UbseFeInfo> ubseFeInfo =
            UbseUrmaControllerManager::GetInstance().GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            return;
        }
        UbseLcneFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseLcneFeType)ubseFeInfo->fetype;
        UbseLcneQos::GetInstance().DeleteVfeQos(lcneFeInfo);
    }
    UbseLcneQos::GetInstance().DeleteQosProfile(profileName);
    return;
}

bool UrmaController::UbseUrmaBandWidthCheck(def::UbseUrmaInfo urmaInfo, const std::string profileName)
{
    for (auto i : urmaInfo.eidGroups) {
        std::string vfeProfileName;
        std::shared_ptr<ubse::urma::def::UbseFeInfo> ubseFeInfo =
            UbseUrmaControllerManager::GetInstance().GetUrmaVfeFromEidGroup(i);
        if (ubseFeInfo == nullptr) {
            return true;
        }
        UbseLcneFeInfo lcneFeInfo;
        lcneFeInfo.slotId = ubseFeInfo->slotId;
        lcneFeInfo.ubpuId = ubseFeInfo->ubpuId;
        lcneFeInfo.iouId = ubseFeInfo->iouId;
        lcneFeInfo.entityId = ubseFeInfo->entityId;
        lcneFeInfo.fetype = (UbseLcneFeType)ubseFeInfo->fetype;
        auto ret = UbseLcneQos::GetInstance().QueryVfeQos(lcneFeInfo, vfeProfileName);
        if ((ret != UBSE_OK) || (vfeProfileName != profileName)) {
            return false;
            break;
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
    urmaExecutor->Execute([]() { return UrmaController::GetInstance().DoTopoLinkChange(); });
    return UBSE_OK;
}

void UrmaController::DoTopoLinkChange()
{
    // 计算所有port是否都中断
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    std::vector<PhysicalLink> allLinkInfo;
    if (auto ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodeTopo(allLinkInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node topology, ret=" << ret;
        return;
    }
    bool isAllPortDown = std::all_of(allLinkInfo.begin(), allLinkInfo.end(), [&curNode](const auto &linkInfo) {
        return linkInfo.slotId != curNode.slotId && linkInfo.peerSlotId != curNode.slotId;
    });
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToInactiveForNode(curNode.nodeId);
    }
    // 下发所有节点拓扑及所有urmaInfo
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    UbseUrmaControllerManager::GetInstance().GetAllUvsInfo(uvsInfos);
    auto uvsModule = ubse::context::UbseContext::GetInstance().GetModule<UbseUrmaUvsModule>();
    if (auto ret = uvsModule->SetUvsInfo(curNode.nodeId, GetDirConnectInfo(), uvsInfos); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to set uvs info, ret=" << ret;
    }
}

void UrmaController::DoNodeJoin()
{
    // 计算所有port是否都中断
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    std::vector<PhysicalLink> allLinkInfo;
    if (auto ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodeTopo(allLinkInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node topology, ret=" << ret;
        return;
    }
    bool isAllPortDown = std::all_of(allLinkInfo.begin(), allLinkInfo.end(), [&curNode](const auto &linkInfo) {
        return linkInfo.slotId != curNode.slotId && linkInfo.peerSlotId != curNode.slotId;
    });
    // 向mti查询本节点所有vfe对应静态urma eid
    std::vector<UbseLcneIouInfo> iouList;
    std::vector<UbseLcneFeInfo> allFeInfos;
    if (UbseNodeComUrmaCollector::GetInstance().GetCurNodeIouList(iouList) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node IOU list";
        return;
    }
    for (auto &iou : iouList) {
        std::vector<UbseLcneFeInfo> tmpFeInfos;
        if (UbseLcneVfeEid::GetInstance().GetVfeEid(iou, tmpFeInfos) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get VFE EID for IOU, iou=" << iou.iouId;
            return;
        }
        allFeInfos.insert(allFeInfos.end(), tmpFeInfos.begin(), tmpFeInfos.end());
    }

    if (UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo(curNode.nodeId, allFeInfos) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to insert new bounding info";
        return;
    }
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToInactiveForNode(curNode.nodeId);
    }
    // 向master节点上报本节点nodeInfo
    if (auto ret = ReportUrmaNodeInfoToMaster(curNode.nodeId,
                                              UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(curNode.nodeId));
        ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to report urma node info to master, " << FormatRetCode(ret);
        return;
    }
}

UbseResult UrmaController::UbseNodeJoinHandler(std::string &eventId, const std::string &eventMesage)
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
    urmaExecutor->Execute([]() { return UrmaController::GetInstance().DoNodeJoin(); });
    return UBSE_OK;
}

std::vector<ubse::nodeController::PhysicalLink> UrmaController::GetDirConnectInfo()
{
    std::vector<ubse::nodeController::PhysicalLink> allLinkInfo;
    auto allLink = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    allLinkInfo.reserve(allLink.size());
    for (const auto &link : allLink) {
        allLinkInfo.emplace_back(std::move(link.second));
    }
    return allLinkInfo;
}

} // namespace ubse::urmaController