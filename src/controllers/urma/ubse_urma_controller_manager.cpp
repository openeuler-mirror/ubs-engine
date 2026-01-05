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

#include "ubse_urma_controller_manager.h"
#include <cstdint>
#include "securec.h"
#include "ubse_election.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_module.h"
#include "ubse_node_controller.h"
#include "ubse_str_util.h"

namespace ubse::urmaController {
using namespace ubse::election;
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::utils;
using namespace ubse::mti;
using namespace ubse::urma;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_CONTROLLER_MID)

UbseResult UbseUrmaControllerManager::GetLocalUrmaDevInfo(const std::string &urmaName, UbseUrmaInfo &urmaInfo)
{
    /* 获取本节点信息 */
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return UBSE_ERROR;
    }
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    for (auto &info : nodeInfos[currentNodeInfo.nodeId].urmaList) {
        if (info.second.subPath != urmaName) {
            continue;
        }
        urmaInfo = info.second;
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

UbseResult UbseUrmaControllerManager::GetFeInfoByNodeId(const std::string &nodeId, std::vector<UbseFeInfo> &feInfos)
{
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << nodeId;
        return UBSE_ERROR;
    }
    for (auto &urmaInfo : nodeInfos[nodeId].urmaList) {
        for (auto &eidGroup : urmaInfo.second.eidGroups) {
            if (eidGroup.feInfo) {
                feInfos.push_back(*(eidGroup.feInfo));
            }
        }
    }
    return UBSE_OK;
}

bool UbseUrmaControllerManager::IsUrmaInfoExists(const std::string &nodeId)
{
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    auto ret = nodeInfos.find(nodeId) != nodeInfos.end();
    return ret;
}

std::shared_ptr<UbseFeInfo> UbseUrmaControllerManager::GetUrmaVfeFromEidGroup(EidGroup &eidGroup)
{
    if (eidGroup.feInfo) {
        return eidGroup.feInfo;
    }
    return nullptr;
}

std::vector<std::string> UbseUrmaControllerManager::GetEmptyNodeInfo()
{
    std::vector<UbseRoleInfo> allNodeList;
    if (UbseGetAllNodeInfos(allNodeList) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get all node infos";
        return {};
    }
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    std::vector<std::string> res{};
    for (auto info : allNodeList) {
        if (nodeInfos.find(info.nodeId) == nodeInfos.end()) {
            res.push_back(info.nodeId);
        }
    }
    return res;
}

UbseResult UbseUrmaControllerManager::GetUrmaNameByType(const UrmaDevType type, std::vector<std::string> &boundingName,
                                                        std::vector<uint32_t> &status)
{
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return UBSE_ERROR;
    }
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    for (auto &info : nodeInfos[currentNodeInfo.nodeId].urmaList) {
        if (info.second.urmaDevType == type) {
            boundingName.push_back(info.second.subPath);
            status.push_back(static_cast<uint32_t>(info.second.state));
        }
    }

    return UBSE_OK;
}

std::string UbseUrmaControllerManager::GetVfeInfoKey(const UbseFeInfo &info)
{
    return info.slotId + "_" + info.ubpuId + "_" + info.iouId + "_" + info.entityId;
}

std::string UbseUrmaControllerManager::GetVfeInfoKey(const UbseLcneFeInfo &info)
{
    return info.slotId + "_" + info.ubpuId + "_" + info.iouId + "_" + info.entityId;
}

UbseResult UbseUrmaControllerManager::GetVfeByUrmaName(const std::string &urmaName, std::vector<UbseFeInfo> &feInfos)
{
    UbseRoleInfo currentNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(currentNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return ret;
    }
    std::set<std::string> feInfoKeys;
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    for (auto &info : nodeInfos[currentNodeInfo.nodeId].urmaList) {
        if (info.second.subPath != urmaName) {
            continue;
        }
        for (auto &eidGroup : info.second.eidGroups) {
            if (eidGroup.feInfo == nullptr) {
                continue;
            }
            UbseFeInfo info = *(eidGroup.feInfo);
            std::string key = UbseUrmaControllerManager::GetVfeInfoKey(info);
            if (feInfoKeys.find(key) == feInfoKeys.end()) {
                feInfoKeys.insert(key);
                feInfos.push_back(info);
            }
        }
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "There is no matching VFE info for urmaName=" << urmaName;
    return UBSE_OK;
}

UbseResult UbseUrmaControllerManager::AllocByUrmaName(const std::string &urmaName, std::vector<std::string> &feNames,
                                                      std::string &eid)
{
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return UBSE_ERROR;
    }
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    if (nodeInfos.find(currentNodeInfo.nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << currentNodeInfo.nodeId;
        return UBSE_ERROR;
    }
    for (auto &info : nodeInfos[currentNodeInfo.nodeId].urmaList) {
        if (info.second.subPath != urmaName) {
            continue;
        }
        eid = info.second.urmaDevEid;
        for (auto &eidGroup : info.second.eidGroups) {
            feNames.push_back(eidGroup.feInfo->name);
        }
        break;
    }
    return UBSE_OK;
}

void UbseUrmaControllerManager::SetActiveState(const std::string &name, const std::string &nodeId)
{
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << nodeId;
        return;
    }
    for (auto &info : nodeInfos[nodeId].urmaList) {
        if (info.second.subPath == name && info.second.state == UrmaDevState::UNKNOWN) {
            info.second.state = UrmaDevState::ACTIVED;
            break;
        }
    }
    return;
}

void UbseUrmaControllerManager::GetUrmaNameForQueryByType(const UrmaDevType type,
                                                          std::vector<UbseUrmaInfoForQuery> &devInfos)
{
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    const size_t feCntPerUrmaInfo = 2;
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    for (auto info : nodeInfos[currentNodeInfo.nodeId].urmaList) {
        if (info.second.urmaDevType == type) {
            UbseUrmaInfoForQuery urmaInfo;
            urmaInfo.bondingName = info.second.subPath;
            if (info.second.eidGroups.size() != feCntPerUrmaInfo) {
                continue;
            }
            urmaInfo.fe1Name = info.second.eidGroups[0].feInfo->name;
            urmaInfo.fe2Name = info.second.eidGroups[1].feInfo->name;
            urmaInfo.state = info.second.state;
            devInfos.push_back(urmaInfo);
        }
    }
}

UbseResult UbseUrmaControllerManager::GetAllUvsInfo(std::vector<UbseUrmaUvsNodeInfo> &uvsInfos)
{
    for (auto nodeInfo : nodeInfos) {
        UbseUrmaUvsNodeInfo tmpUvsInfo;
        tmpUvsInfo.nodeId = nodeInfo.second.nodeId;
        for (auto urmaInfo : nodeInfo.second.urmaList) {
            UbseUrmaUvsAggrDev dev;
            dev.urmaDevEid = urmaInfo.second.urmaDevEid;

            for (auto group : urmaInfo.second.eidGroups) {
                UbseUrmaUvsFe feInfo;
                feInfo.ubpuId = group.feInfo->ubpuId;
                feInfo.primaryEid = group.primaryEid;
                feInfo.portEid = group.portEids;
                dev.feList.push_back(feInfo);
            }
        }
    }
    return UBSE_OK;
}

void UbseUrmaControllerManager::SetFeName(const std::string feEid, const std::string &urmaEidName)
{
    rwLock.LockWrite();
    auto SetNameIfEidEqual = [&feEid, &urmaEidName](EidGroup &eidGroup) {
        if (eidGroup.primaryEid == feEid && eidGroup.feInfo != nullptr) {
            eidGroup.feInfo->name = urmaEidName;
        }
    };
    auto travelAllEidGroups = [&SetNameIfEidEqual](UbseUrmaNodeInfo &nodeInfo) {
        for (auto &info : nodeInfo.urmaList) {
            for (auto &eidGroup : info.second.eidGroups) {
                SetNameIfEidEqual(eidGroup);
            }
        }
    };
    for (auto &nodeInfo : nodeInfos) {
        travelAllEidGroups(nodeInfo.second);
    }
    rwLock.UnLock();
}

UbseResult UbseUrmaControllerManager::GenerateUrmaDevEid(const UbseLcneFeInfo &fe0, const UbseLcneFeInfo &fe1,
                                                         std::string &devEid)
{
    unsigned char bondingEid[IPV6_BYTE_COUNT];
    uint32_t slotId = 0;
    if (ConvertStrToUint32(fe0.slotId, slotId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to convert slotId=" << fe0.slotId;
        return UBSE_ERROR;
    }
    // 获取两个fe的id，再根据id创建eid
    auto feKey0 = GetVfeInfoKey(fe0);
    auto feKey1 = GetVfeInfoKey(fe1);
    if (feIdMap.find(feKey0) == feIdMap.end()) {
        feIdMap[feKey0] = GenerateUniqueFeId();
    }
    if (feIdMap.find(feKey1) == feIdMap.end()) {
        feIdMap[feKey1] = GenerateUniqueFeId();
    }
    uint32_t fe0Id = feIdMap[feKey0];
    uint32_t fe1Id = feIdMap[feKey1];
    uint32_t copyCnt = 0;
    uint32_t padding = 0;

    // [slotId, fe0Id, fe1Id, 0000]，每个字段占32位
    auto ret =
        memcpy_s(bondingEid + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &slotId, IPV6_SEGMENT_LENGTH);
    ret |= memcpy_s(bondingEid + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &fe0Id, IPV6_SEGMENT_LENGTH);
    ret |= memcpy_s(bondingEid + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &fe1Id, IPV6_SEGMENT_LENGTH);
    ret |= memcpy_s(bondingEid + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &padding, IPV6_SEGMENT_LENGTH);
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Failed to generate bonding eid, ret=" << ret;
        return UBSE_ERROR;
    }
    devEid = std::string(bondingEid, bondingEid + IPV6_BYTE_COUNT);
    return UBSE_OK;
}

struct UbseFeInfoCmp {
    bool operator()(const UbseLcneFeInfo &left, const UbseLcneFeInfo &right) const
    {
        if (left.slotId != right.slotId) {
            return left.slotId < right.slotId;
        }
        if (left.ubpuId != right.ubpuId) {
            return left.ubpuId < right.ubpuId;
        }
        if (left.iouId != right.iouId) {
            return left.iouId < right.iouId;
        }
        return left.entityId < right.entityId;
    }
};

constexpr size_t UBPU_BOUNDARY_CNT = 2; // 一个slot只有两个chip
UbseResult FindTwoUbpuBoundaries(std::vector<UbseLcneFeInfo> &feInfos, std::pair<size_t, size_t> &boundaries)
{
    if (feInfos.size() < UBPU_BOUNDARY_CNT) {
        UBSE_LOG_ERROR << "Not enough feInfos to find two ubpu boundaries";
        return UBSE_ERROR;
    }
    // 假设slot_id相同，先按UbseFeInfoCmp对feInfos排序，便于按ubpuId分组
    std::sort(feInfos.begin(), feInfos.end(), UbseFeInfoCmp());
    auto ubpu0 = feInfos[0].ubpuId;
    auto ubpuEndIt0 = std::partition_point(feInfos.begin(), feInfos.end(),
                                           [ubpu0](const UbseLcneFeInfo &fe) { return fe.ubpuId == ubpu0; });
    size_t ubpuEnd0 = std::distance(feInfos.begin(), ubpuEndIt0); // 第一个非 ubpu0 的索引
    if (ubpuEnd0 == feInfos.size()) {
        UBSE_LOG_ERROR << "Only one ubpuId found, ubpuId=" << ubpu0;
        return UBSE_ERROR;
    }
    auto ubpu1 = feInfos[ubpuEnd0].ubpuId;
    auto ubpuEndIt1 = std::partition_point(ubpuEndIt0, feInfos.end(),
                                           [ubpu1](const UbseLcneFeInfo &fe) { return fe.ubpuId == ubpu1; });
    size_t ubpuEnd1 = std::distance(feInfos.begin(), ubpuEndIt1); // 第一个非 ubpu1 的索引
    if (ubpuEnd1 != feInfos.size()) {
        // 出现第三种ubpuId，为保持鲁棒性，仅日志警告
        UBSE_LOG_WARN << "More than two ubpuId found, ubpuId=" << feInfos[ubpuEnd1].ubpuId;
    }
    UBSE_LOG_INFO << "ubpu0=" << ubpu0 << " end_idx=" << ubpuEnd0 << ", ubpu1=" << ubpu1 << " end_idx=" << ubpuEnd1;
    boundaries = std::make_pair(ubpuEnd0, ubpuEnd1);
    return UBSE_OK;
}

bool UbseUrmaControllerManager::IsUrmaInfoExists(const std::string &nodeId, const std::string &devEid)
{
    return std::any_of(nodeInfos[nodeId].urmaList.begin(), nodeInfos[nodeId].urmaList.end(),
                       [&devEid](const auto &urmaInfo) { return urmaInfo.second.urmaDevEid == devEid; });
}

void UbseUrmaControllerManager::InsertNewNodeInfo(const std::string &nodeId, UbseUrmaNodeInfo &insertNodeInfo)
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    nodeInfos[nodeId] = std::move(insertNodeInfo);
}

UbseResult UbseUrmaControllerManager::GetUrmaQos(const std::string &urmaInfoName, UrmaQosProfile &urmaQosProfile)
{
    return UBSE_OK;
}

UbseResult UbseUrmaControllerManager::SetUrmaQos(const std::string &urmaInfoName, UrmaQosProfile urmaQosProfile)
{
    return UBSE_OK;
}

const std::map<UbseLcneFeType, FeType> g_LcneFeTypeToUrmaFeTypeMap = {
    {UbseLcneFeType::PHYSICAL_TYPE, FeType::PHYSICAL_TYPE},
    {UbseLcneFeType::VIRTUAL_TYPE, FeType::VIRTUAL_TYPE},
    {UbseLcneFeType::BUTT_TYPE, FeType::BUTT_TYPE}};

FeType ConvertLcneFeTypeToUrmaFeType(const UbseLcneFeType &lcneFeType)
{
    auto it = g_LcneFeTypeToUrmaFeTypeMap.find(lcneFeType);
    if (it != g_LcneFeTypeToUrmaFeTypeMap.end()) {
        return it->second;
    }
    return FeType::BUTT_TYPE;
}

void UbseUrmaControllerManager::CreateAndInsertUrmaInfo(const std::string &nodeId, const std::string &devEid,
                                                        UbseLcneFeInfo &lcneFe0, UbseLcneFeInfo &lcneFe1)
{
    auto urmaFe0 = std::make_shared<UbseFeInfo>(UbseFeInfo{.slotId = lcneFe0.slotId,
                                                           .ubpuId = lcneFe0.ubpuId,
                                                           .iouId = lcneFe0.iouId,
                                                           .entityId = lcneFe0.entityId,
                                                           .fetype = ConvertLcneFeTypeToUrmaFeType(lcneFe0.fetype)});
    auto urmaFe1 = std::make_shared<UbseFeInfo>(UbseFeInfo{.slotId = lcneFe1.slotId,
                                                           .ubpuId = lcneFe1.ubpuId,
                                                           .iouId = lcneFe1.iouId,
                                                           .entityId = lcneFe1.entityId,
                                                           .fetype = ConvertLcneFeTypeToUrmaFeType(lcneFe1.fetype)});
    if (!urmaFe0 || !urmaFe1) {
        UBSE_LOG_ERROR << "Failed to allocate memory for UbseFeInfo";
        return;
    }
    UbseUrmaInfo urmaInfo{.urmaDevEid = devEid, .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    if (lcneFe0.eidGroups.size() < 1 || lcneFe1.eidGroups.size() < 1) {
        return;
    }
    // 当前仅支持独享urmaInfo
    EidGroup group0{.primaryEid = lcneFe0.eidGroups[0].primaryEid,
                    .portEids = std::move(lcneFe0.eidGroups[0].portEids),
                    .feInfo = urmaFe0};
    urmaInfo.eidGroups.push_back(group0);
    EidGroup group1{.primaryEid = lcneFe1.eidGroups[0].primaryEid,
                    .portEids = std::move(lcneFe1.eidGroups[0].portEids),
                    .feInfo = urmaFe1};
    urmaInfo.eidGroups.push_back(group1);
    std::string urmaId = "urma_" + std::to_string(GenerateUrmaId());
    nodeInfos[nodeId].urmaList[urmaId] = urmaInfo;
}

uint32_t UbseUrmaControllerManager::GenerateUniqueFeId()
{
    return ++globalFeId;
}

uint64_t UbseUrmaControllerManager::GenerateUrmaId()
{
    return ++globalUrmaId;
}

UbseResult UbseUrmaControllerManager::ConstructNewUrmaInfo(const std::string &nodeId,
                                                           std::vector<UbseLcneFeInfo> &feInfos)
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    UBSE_LOG_INFO << "Begin to construct new bounding info for nodeId=" << nodeId;
    // 根据ubpuId将feInfos划分成两个连续的区域，取得每部分的结束下标
    std::pair<size_t, size_t> boundaries;
    if (FindTwoUbpuBoundaries(feInfos, boundaries) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to find two ubpu boundaries";
        return UBSE_ERROR;
    }
    // 每次从两个区域各取一个fe组成bonding
    size_t ubpuEnd0 = boundaries.first;
    size_t ubpuEnd1 = boundaries.second;
    size_t maxBoundingCnt = std::min(ubpuEnd0, ubpuEnd1 - ubpuEnd0);
    for (size_t i = 0; i < maxBoundingCnt; ++i) {
        UbseLcneFeInfo &lcneFe0 = feInfos[i];
        UbseLcneFeInfo &lcneFe1 = feInfos[ubpuEnd0 + i];
        // 使用第一个fe生成devEid
        std::string devEid;
        if (GenerateUrmaDevEid(lcneFe0, lcneFe1, devEid) != UBSE_OK || IsUrmaInfoExists(nodeId, devEid)) {
            continue;
        }
        CreateAndInsertUrmaInfo(nodeId, devEid, lcneFe0, lcneFe1);
    }
    return UBSE_OK;
}

UbseResult UbseUrmaControllerManager::ConstructNewUrmaInfo(const std::string &nodeId,
                                                           std::vector<UbseLcneFeInfo> &&feInfos)
{
    return ConstructNewUrmaInfo(nodeId, feInfos);
}

void UbseUrmaControllerManager::SetAllUrmaInfoToInactiveForNode(const std::string &nodeId)
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    auto nodeInfo = nodeInfos[nodeId];
    for (auto &urmaInfo : nodeInfo.urmaList) {
        urmaInfo.second.state = UrmaDevState::INACTIVED;
    }
}

UbseUrmaNodeInfo UbseUrmaControllerManager::GetUrmaNodeInfo(const std::string &nodeId)
{
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    return nodeInfos[nodeId];
}

void UbseUrmaControllerManager::UbseUrmaBandwidthInit(const std::string &nodeId,
                                                      const std::function<void(const std::string)> &initFunc)
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    auto nodeInfo = nodeInfos[nodeId];
    for (const auto &urmaInfoPair : nodeInfo.urmaList) { // check
        initFunc(urmaInfoPair.second.subPath);
    }
}
} // namespace ubse::urmaController
