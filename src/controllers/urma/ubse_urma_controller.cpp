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
using namespace ubse::com;
using namespace ubse::urma;
using namespace ubse::task_executor;
using namespace ubse::mti;
using namespace ubse::nodeController;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_CONTROLLER_MID)

const int INDEX_NO_2 = 2;
const std::string PATH_PREFIX = "/dev/uburma/";
const uint32_t BYTE_TO_BIT = 8;

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
        std::shared_ptr<UbseFeInfo> ubseFeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaVfeFromEidGroup(i);
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
        std::shared_ptr<UbseFeInfo> ubseFeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaVfeFromEidGroup(i);
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
        std::shared_ptr<UbseFeInfo> ubseFeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaVfeFromEidGroup(i);
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
        std::shared_ptr<UbseFeInfo> ubseFeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaVfeFromEidGroup(i);
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
    urmaExecutor->Execute([]() { return UrmaController::GetInstance().DoTopoLinkChange(); });
    return UBSE_OK;
}

void UrmaController::DoTopoLinkChange()
{
    // 计算所有port是否都中断
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    std::vector<PhysicalLink> allLinkInfo;
    auto getNodeTopoFunc = [&allLinkInfo]() {
        return UbseNodeComUrmaCollector::GetInstance().GetCurNodeTopo(allLinkInfo);
    };
    if (auto ret = CallFuncRetry(getNodeTopoFunc); ret != UBSE_OK) {
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
    if (auto ret = CallFuncRetry([&uvsModule, &curNode, &uvsInfos, this]() {
            return uvsModule->SetUvsInfo(curNode.nodeId, UbseUrmaControllerManager::GetInstance().GetDirConnectInfo(),
                                         uvsInfos);
        });
        ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to set uvs info, ret=" << ret;
    }
}

void UbseUrmaBandwidthInit(const std::string &nodeId)
{
    auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(nodeId);
    for (const auto &urmaInfoPair : nodeInfo.urmaList) {
        UrmaController::GetInstance().UbseUrmaBandWidthUpdate(urmaInfoPair.first);
    }
}

void UrmaController::DoNodeJoin()
{
    // 计算所有port是否都中断
    auto curNode = UbseNodeController::GetInstance().GetCurNode();
    std::vector<PhysicalLink> allLinkInfo;
    auto getNodeTopoFunc = [&allLinkInfo]() {
        return UbseNodeComUrmaCollector::GetInstance().GetCurNodeTopo(allLinkInfo);
    };
    if (auto ret = CallFuncRetry(getNodeTopoFunc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node topology, ret=" << ret;
        return;
    }
    bool isAllPortDown = std::all_of(allLinkInfo.begin(), allLinkInfo.end(), [&curNode](const auto &linkInfo) {
        return linkInfo.slotId != curNode.slotId && linkInfo.peerSlotId != curNode.slotId;
    });
    // 向mti查询本节点所有vfe对应静态urma eid
    std::vector<UbseLcneIouInfo> iouList;
    std::vector<UbseLcneFeInfo> allFeInfos;
    if (CallFuncRetry([&]() { return UbseNodeComUrmaCollector::GetInstance().GetCurNodeIouList(iouList); }) !=
        UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node IOU list";
        return;
    }
    for (auto &iou : iouList) {
        std::vector<UbseLcneFeInfo> tmpFeInfos;
        if (CallFuncRetry([&]() { return UbseGetVfeEid(iou, tmpFeInfos); }) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get VFE EID for IOU, iou=" << iou.iouId;
            return;
        }
        allFeInfos.insert(allFeInfos.end(), tmpFeInfos.begin(), tmpFeInfos.end());
    }

    if (UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo(curNode.nodeId, allFeInfos) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to insert new bounding info";
        return;
    }
    UbseUrmaControllerManager::GetInstance().UrmaCtlActivateUrmaDevice(curNode.nodeId);
    // 初始化带宽模板，可重复调用
    UbseUrmaBandwidthInit(curNode.nodeId);
    if (isAllPortDown) {
        // 将该节点的所有urmaInfo状态改成Inactive
        UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToInactiveForNode(curNode.nodeId);
    }
    // 向master节点上报本节点nodeInfo
    auto urmaNodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(curNode.nodeId);
    auto reportUrmaNodeInfoToMasterFunc = [&curNode, &urmaNodeInfo]() {
        return ReportUrmaNodeInfoToMaster(curNode.nodeId, urmaNodeInfo);
    };
    if (auto ret = CallFuncRetry(reportUrmaNodeInfoToMasterFunc); ret != UBSE_OK) {
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

UbseResult UrmaController::UbseGetLocalUrmaDevInfoByType(const UrmaDevType type, std::vector<std::string> &nameInfo,
                                                         std::vector<uint32_t> &status)
{
    // 判断是否合法类型，非法返回不支持
    if (type >= UrmaDevType::BUTT) {
        UBSE_LOG_ERROR << "get urma name by type failed, type =" << (uint32_t)type;
        return UBSE_ERROR_NOT_SUPPORT;
    }
    return UbseUrmaControllerManager::GetInstance().GetUrmaNameByType(type, nameInfo, status);
}

UbseResult UrmaController::UbseAllocUrmaDev(const std::string urmaName, UbseUrmaDevPath &devPaths)
{
    std::vector<std::string> feNames;
    std::string eid;
    UbseUrmaControllerManager::GetInstance().AllocByUrmaName(urmaName, feNames, eid);
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
    return UBSE_OK;
}

UbseResult UrmaController::UbseGetUrmaDevInfoByNodeIdAndType(const UrmaDevType type, const uint32_t &nodeId,
                                                             std::vector<UbseUrmaInfoForQuery> &devInfos)
{
    ubse::election::UbseRoleInfo currentNodeInfo{};
    ubse::election::UbseGetCurrentNodeInfo(currentNodeInfo);
    if (std::to_string(nodeId) == currentNodeInfo.nodeId) {
        UbseUrmaControllerManager::GetInstance().GetUrmaNameForQueryByType(type, devInfos);
        return UBSE_OK;
    }
    return UbseQueryUrmaInfoByRpc(nodeId, type, devInfos);
}

} // namespace ubse::urmaController