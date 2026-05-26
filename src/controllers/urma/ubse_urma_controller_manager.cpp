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

#include "ubse_urma_controller_manager.h"
#include <cstdint>
#include "securec.h"
#include "src/controllers/node/ubse_node_com_urma_collector.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_logger.h"
#include "ubse_str_util.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_def.h"
#include "ubse_mti_eid_interface.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::urmaController {
using namespace ubse::election;
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::utils;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::urma;
using namespace ubse::context;
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseUrmaControllerManager::GetLocalUrmaDevInfo(const std::string &urmaName, UbseUrmaInfo &urmaInfo)
{
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    return GetLocalUrmaDevInfoInner(urmaName, urmaInfo);
}

UbseResult UbseUrmaControllerManager::GetLocalUrmaDevInfoInner(const std::string &urmaName, UbseUrmaInfo &urmaInfo)
{
    /* 获取本节点信息 */
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return UBSE_ERROR;
    }
    if (nodeInfos.find(currentNodeInfo.nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << currentNodeInfo.nodeId;
        return UBSE_ERR_NOT_EXIST;
    }
    if (nodeInfos[currentNodeInfo.nodeId].urmaList.find(urmaName) == nodeInfos[currentNodeInfo.nodeId].urmaList.end()) {
        UBSE_LOG_WARN << "There is no urma info for name=" << urmaName;
        return UBSE_ERR_NOT_EXIST;
    }
    urmaInfo = nodeInfos[currentNodeInfo.nodeId].urmaList[urmaName];
    return UBSE_OK;
}

UbseResult UbseUrmaControllerManager::GetAllUrmaInfo(std::vector<std::string> &urmaInfoName,
                                                     std::vector<uint32_t> &status, std::vector<uint64_t> &hwResIds)
{
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return UBSE_ERROR;
    }
    // 查询设备激活状态
    bool isAllPortDown = false;
    if (QueryAllPortsDown(isAllPortDown) != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query all ports status, all ports are down should be down";
        isAllPortDown = true;
    }
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    if (nodeInfos.find(currentNodeInfo.nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << currentNodeInfo.nodeId;
        return UBSE_ERROR;
    }
    for (auto &info : nodeInfos[currentNodeInfo.nodeId].urmaList) {
        urmaInfoName.push_back(info.first);
        bool health = true;
        for (auto &eidGroup : info.second.eidGroups) {
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
        hwResIds.push_back(info.second.hwResId);
    }

    return UBSE_OK;
}

std::string GetVfeInfoKey(const UbseFeInfo &info)
{
    return info.slotId + "_" + info.ubpuId + "_" + info.iouId + "_" + info.entityId;
}

std::string GetVfeInfoKey(const UbseMtiFeInfo &info)
{
    return info.slotId + "_" + info.ubpuId + "_" + info.iouId + "_" + info.entityId;
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
        return UBSE_ERR_NOT_EXIST;
    }
    if (nodeInfos[currentNodeInfo.nodeId].urmaList.find(urmaName) == nodeInfos[currentNodeInfo.nodeId].urmaList.end()) {
        UBSE_LOG_WARN << "There is no urma info for name=" << urmaName;
        return UBSE_ERR_NOT_EXIST;
    }
    auto info = nodeInfos[currentNodeInfo.nodeId].urmaList[urmaName];
    if (info.state != UrmaDevState::ACTIVED) {
        UBSE_LOG_WARN << "The urma is not actived, name=" << urmaName << ", cannot be allocated";
        return UBSE_ERR_NOT_EXIST;
    }
    eid = info.urmaDevEid;
    feNames.push_back(info.subPath);
    for (auto &eidGroup : info.eidGroups) {
        if (eidGroup.feInfo == nullptr) {
            UBSE_LOG_WARN << "There is no FE info for EID group=" << eidGroup.primaryEid;
            continue;
        }
        feNames.push_back(eidGroup.feInfo->name);
    }
    return UBSE_OK;
}

void UbseUrmaControllerManager::SetActiveState(const std::string &urmaDevEid, const std::string &nodeId)
{
    std::string urmaName{""};
    {
        ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
        if (nodeInfos.find(nodeId) == nodeInfos.end()) {
            UBSE_LOG_WARN << "There is no urma info for node=" << nodeId;
            return;
        }
        for (auto &info : nodeInfos[nodeId].urmaList) {
            if (info.second.urmaDevEid == urmaDevEid) {
                UBSE_LOG_DEBUG << "Success to find urma device by eid, name=" << info.first;
                urmaName = info.first;
                break;
            }
        }
    }
    if (urmaName.empty()) {
        UBSE_LOG_WARN << "Failed to set urma active, urmaDevEid=" << urmaDevEid;
        return;
    }
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << nodeId;
        return;
    }
    UBSE_LOG_DEBUG << "Success to set urma active, name=" << urmaName;
    nodeInfos[nodeId].urmaList[urmaName].state = UrmaDevState::ACTIVED;
}

void UbseUrmaControllerManager::SetInactiveState(const std::string &urmaDevEid, const std::string &nodeId)
{
    std::string urmaName{""};
    {
        ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
        if (nodeInfos.find(nodeId) == nodeInfos.end()) {
            UBSE_LOG_WARN << "There is no urma info for node=" << nodeId;
            return;
        }
        for (auto &info : nodeInfos[nodeId].urmaList) {
            if (info.second.urmaDevEid == urmaDevEid) {
                UBSE_LOG_DEBUG << "Success to find urma device by eid, name=" << info.first;
                urmaName = info.first;
                break;
            }
        }
    }
    if (urmaName.empty()) {
        UBSE_LOG_WARN << "Failed to set urma inactive, urmaDevEid=" << urmaDevEid;
        return;
    }
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << nodeId;
        return;
    }
    UBSE_LOG_DEBUG << "Success to set urma inactive, name=" << urmaName;
    nodeInfos[nodeId].urmaList[urmaName].state = UrmaDevState::INACTIVED;
}

void UbseUrmaControllerManager::GetUrmaInfoForQuery(std::vector<UbseUrmaInfoForQuery> &devInfos)
{
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get current node info";
        return;
    }
    const size_t feCntPerUrmaInfo = NO_2;
    (void)QueryUrmaInfoStateFromUrma(currentNodeInfo.nodeId);
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    if (nodeInfos.find(currentNodeInfo.nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << currentNodeInfo.nodeId;
        return;
    }
    PrintNodeInfo(nodeInfos[currentNodeInfo.nodeId]);
    for (auto &info : nodeInfos[currentNodeInfo.nodeId].urmaList) {
        UbseUrmaInfoForQuery urmaInfo;
        urmaInfo.urmaName = info.first;
        if (info.second.eidGroups.size() != feCntPerUrmaInfo) {
            continue;
        }
        for (auto &eidGroup : info.second.eidGroups) {
            urmaInfo.feEids.push_back(eidGroup.primaryEid);
            urmaInfo.feNames.push_back(eidGroup.feInfo == nullptr ? "" : eidGroup.feInfo->name);
        }
        urmaInfo.state = info.second.state;
        urmaInfo.devEid = info.second.urmaDevEid;
        urmaInfo.bondingType = info.second.urmaDevType;
        urmaInfo.qosProfile = info.second.urmaQosProfile;
        UBSE_LOG_DEBUG << "Found URMA info for query: " << urmaInfo.urmaName << ", state=" << (uint32_t)urmaInfo.state;
        devInfos.push_back(urmaInfo);
    }
}

void GetHostUrmaDev(std::vector<UbseUrmaUvsNodeInfo> &hostUrmaInfos, UbseUrmaUvsNodeInfo &uvsInfo)
{
    for (auto &hostUrmaInfo : hostUrmaInfos) {
        if (hostUrmaInfo.nodeId != uvsInfo.nodeId) {
            continue;
        }
        if (hostUrmaInfo.devList.empty()) {
            UBSE_LOG_WARN << "host urma dev list is empty";
            break;
        }
        // host urma只有一个聚合设备，且下发时必须放在第一个
        uvsInfo.devList.insert(uvsInfo.devList.begin(), hostUrmaInfo.devList[0]);
        break;
    }
}

UbseResult FillUrmaUvsNodeInfo(std::vector<UbseUrmaUvsNodeInfo> &hostUrmaInfos, UbseUrmaNodeInfo &nodeInfo,
                               UbseUrmaUvsNodeInfo &tmpUvsInfo)
{
    tmpUvsInfo.nodeId = nodeInfo.nodeId;
    GetHostUrmaDev(hostUrmaInfos, tmpUvsInfo);
    for (auto &urmaInfo : nodeInfo.urmaList) {
        UbseUrmaUvsAggrDev dev{};
        dev.urmaDevEid = urmaInfo.second.urmaDevEid;
        for (auto &group : urmaInfo.second.eidGroups) {
            UbseUrmaUvsFe feInfo{};
            if (group.feInfo == nullptr) {
                UBSE_LOG_ERROR << "fe info is null, urmaDevEid=" << urmaInfo.second.urmaDevEid;
                return UBSE_ERROR;
            }
            feInfo.ubpuId = group.feInfo->ubpuId;
            feInfo.primaryEid = group.primaryEid;
            feInfo.portEid = group.portEids;
            feInfo.entityId = group.feInfo->entityId;
            dev.feList.push_back(feInfo);
        }
        tmpUvsInfo.devList.push_back(dev);
    }
    return UBSE_OK;
}

UbseResult UbseUrmaControllerManager::GetAllUvsInfo(std::vector<UbseUrmaUvsNodeInfo> &uvsInfos)
{
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos;
    auto ret = UbseNodeComUrmaCollector::GetInstance().GetAllComUrma(hostUrmaInfos);
    if (ret != UBSE_OK || hostUrmaInfos.empty()) {
        UBSE_LOG_ERROR << "Get all com urma info failed.";
        return UBSE_ERROR;
    }

    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    for (auto &nodeInfo : nodeInfos) {
        UbseUrmaUvsNodeInfo tmpUvsInfo{};
        if (FillUrmaUvsNodeInfo(hostUrmaInfos, nodeInfo.second, tmpUvsInfo) != UBSE_OK) {
            UBSE_LOG_ERROR << "Fill urma uvs info failed.";
            return UBSE_ERROR;
        }
        uvsInfos.push_back(tmpUvsInfo);
    }
    return UBSE_OK;
}

void UbseUrmaControllerManager::SetUrmaSubPath(const std::string &urmaEid, const std::string &urmaSubPath)
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    for (auto &nodeInfo : nodeInfos) {
        for (auto &info : nodeInfo.second.urmaList) {
            if (info.second.urmaDevEid == urmaEid) {
                info.second.subPath = urmaSubPath;
                return;
            }
        }
    }
}

void UbseUrmaControllerManager::SetFeName(const std::string &feEid, const std::string &urmaEidName)
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    auto SetNameIfEidEqual = [&feEid, &urmaEidName](EidGroup &eidGroup) -> bool {
        if (eidGroup.primaryEid == feEid && eidGroup.feInfo != nullptr) {
            eidGroup.feInfo->name = urmaEidName;
            return true;
        }
        return false;
    };
    auto travelAllEidGroups = [&SetNameIfEidEqual](UbseUrmaInfo &urmaInfo) -> bool {
        for (auto &eidGroup : urmaInfo.eidGroups) {
            if (SetNameIfEidEqual(eidGroup)) {
                return true;
            }
        }
        return false;
    };
    for (auto &nodeInfo : nodeInfos) {
        for (auto &urmaInfo : nodeInfo.second.urmaList) {
            if (travelAllEidGroups(urmaInfo.second)) {
                return;
            }
        }
    }
}

UbseResult UbseUrmaControllerManager::GenerateUrmaDevEid(const uint32_t superNodeId, const uint32_t slotId,
                                                         const uint32_t fe0Id, const uint32_t fe1Id,
                                                         std::string &devEid)
{
    unsigned char bondingEid[ubse::urma::IPV6_BYTE_COUNT];
    // 获取两个fe的id，再根据id创建eid
    uint32_t copyCnt = 0;
    // [superNodeId, slotId, fe0Id, fe1Id]，每个字段占32位
    const size_t segmentLength = ubse::urma::IPV6_SEGMENT_LENGTH;
    auto ret = memcpy_s(bondingEid + segmentLength * copyCnt++, segmentLength, &superNodeId, segmentLength);
    ret |= memcpy_s(bondingEid + segmentLength * copyCnt++, segmentLength, &slotId, segmentLength);
    ret |= memcpy_s(bondingEid + segmentLength * copyCnt++, segmentLength, &fe0Id, segmentLength);
    ret |= memcpy_s(bondingEid + segmentLength * copyCnt++, segmentLength, &fe1Id, segmentLength);
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Failed to generate bonding eid, ret=" << ret;
        return UBSE_ERROR;
    }
    const uint32_t fullFormatLength = ubse::urma::IPV6_FULL_FORMAT_LENGTH;
    std::array<char, fullFormatLength + 1> buffer{};
    int res = snprintf_s(buffer.data(), buffer.size(), fullFormatLength,
                         "%02x%02x:"
                         "%02x%02x:"
                         "%02x%02x:"
                         "%02x%02x:"
                         "%02x%02x:"
                         "%02x%02x:"
                         "%02x%02x:"
                         "%02x%02x",
                         bondingEid[NO_0], bondingEid[NO_1], bondingEid[NO_2], bondingEid[NO_3], bondingEid[NO_4],
                         bondingEid[NO_5], bondingEid[NO_6], bondingEid[NO_7], bondingEid[NO_8], bondingEid[NO_9],
                         bondingEid[NO_10], bondingEid[NO_11], bondingEid[NO_12], bondingEid[NO_13], bondingEid[NO_14],
                         bondingEid[NO_15]);
    if (res < 0) {
        UBSE_LOG_WARN << "Failed to convert bytes to bonding eid string";
        return UBSE_ERROR;
    }
    devEid = buffer.data();
    return UBSE_OK;
}

bool CompareStringsNumerically(const std::string &left, const std::string &right)
{
    try {
        uint32_t leftNum = std::stoul(left);
        uint32_t rightNum = std::stoul(right);
        return leftNum < rightNum;
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Failed to compare strings numerically: " << e.what() << ", first op=" << left
                       << ", second op=" << right;
    }
    if (left.size() != right.size()) {
        return left.size() < right.size();
    }
    return left < right;
}

void UbseUrmaControllerManager::PrintNodeInfo(const UbseUrmaNodeInfo &nodeInfo)
{
    UBSE_LOG_DEBUG << "Node ID=" << nodeInfo.nodeId;
    UBSE_LOG_DEBUG << "URMA List:";
    for (const auto &urma : nodeInfo.urmaList) {
        UBSE_LOG_DEBUG << "  URMA Name=" << urma.first << ", URMA Sub Path=" << urma.second.subPath
                       << ", URMA Dev EID=" << urma.second.urmaDevEid
                       << ", URMA Dev Type=" << static_cast<int>(urma.second.urmaDevType)
                       << ", URMA State=" << static_cast<int>(urma.second.state);
    }
}

void UbseUrmaControllerManager::InsertNewNodeInfo(const std::string &nodeId, UbseUrmaNodeInfo &insertNodeInfo)
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    PrintNodeInfo(insertNodeInfo);
    if (nodeInfos.find(nodeId) != nodeInfos.end() &&
        nodeInfos[nodeId].updateTimeStamp >= insertNodeInfo.updateTimeStamp) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " has the newest info, skip update";
        return;
    }
    nodeInfos[nodeId] = std::move(insertNodeInfo);
}

uint64_t UbseUrmaControllerManager::GetUrmaUpdateTimeStamp(const std::string &nodeId)
{
    ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
    if (nodeInfos.find(nodeId) != nodeInfos.end()) {
        return nodeInfos[nodeId].updateTimeStamp;
    }
    return 0;
}

UbseResult UbseUrmaControllerManager::GetUrmaQos(const std::string &urmaInfoName, UrmaQosProfile &urmaQosProfile)
{
    UbseUrmaInfo urmaInfo{};
    if (auto ret = GetLocalUrmaDevInfoInner(urmaInfoName, urmaInfo)) {
        UBSE_LOG_ERROR << "Failed to get local URMA device info, urmaInfoName=" << urmaInfoName;
        return ret;
    }
    urmaQosProfile = urmaInfo.urmaQosProfile;
    return UBSE_OK;
}

UbseResult UbseUrmaControllerManager::SetUrmaQos(const std::string &urmaInfoName, const UrmaQosProfile &urmaQosProfile)
{
    UbseRoleInfo currentNodeInfo{};
    if (UbseGetCurrentNodeInfo(currentNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info";
        return UBSE_ERROR;
    }
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    if (nodeInfos.find(currentNodeInfo.nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << currentNodeInfo.nodeId;
        return UBSE_ERR_NOT_EXIST;
    }
    if (nodeInfos[currentNodeInfo.nodeId].urmaList.find(urmaInfoName) ==
        nodeInfos[currentNodeInfo.nodeId].urmaList.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << currentNodeInfo.nodeId
                      << ", urmaInfoName=" << urmaInfoName;
        return UBSE_ERR_NOT_EXIST;
    }
    auto &info = nodeInfos[currentNodeInfo.nodeId].urmaList[urmaInfoName];
    info.urmaQosProfile = urmaQosProfile;
    return UBSE_OK;
}

const std::map<UbseMtiFeType, FeType> g_LcneFeTypeToUrmaFeTypeMap = {
    {UbseMtiFeType::PHYSICAL_TYPE, FeType::PHYSICAL_TYPE},
    {UbseMtiFeType::VIRTUAL_TYPE, FeType::VIRTUAL_TYPE},
    {UbseMtiFeType::BUTT_TYPE, FeType::BUTT_TYPE}};

FeType ConvertLcneFeTypeToUrmaFeType(const UbseMtiFeType &lcneFeType)
{
    auto it = g_LcneFeTypeToUrmaFeTypeMap.find(lcneFeType);
    if (it != g_LcneFeTypeToUrmaFeTypeMap.end()) {
        return it->second;
    }
    return FeType::BUTT_TYPE;
}

UbseResult CreateAndInsertUrmaInfoPreset(const UbseMtiFeInfo &lcneFe0, const UbseMtiFeInfo &lcneFe1,
                                         std::shared_ptr<UbseFeInfo> &urmaFe0, std::shared_ptr<UbseFeInfo> &urmaFe1,
                                         uint32_t &slotId)
{
    if (ConvertStrToUint32(lcneFe0.slotId, slotId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to convert slotId=" << lcneFe0.slotId;
        return UBSE_ERROR;
    }
    try {
        urmaFe0 = std::make_shared<UbseFeInfo>(UbseFeInfo{.slotId = lcneFe0.slotId,
                                                          .ubpuId = lcneFe0.ubpuId,
                                                          .iouId = lcneFe0.iouId,
                                                          .entityId = lcneFe0.entityId,
                                                          .fetype = ConvertLcneFeTypeToUrmaFeType(lcneFe0.fetype)});
        urmaFe1 = std::make_shared<UbseFeInfo>(UbseFeInfo{.slotId = lcneFe1.slotId,
                                                          .ubpuId = lcneFe1.ubpuId,
                                                          .iouId = lcneFe1.iouId,
                                                          .entityId = lcneFe1.entityId,
                                                          .fetype = ConvertLcneFeTypeToUrmaFeType(lcneFe1.fetype)});
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Failed to create UbseFeInfo, reason=" << e.what();
        return UBSE_ERROR;
    }
    if (!urmaFe0 || !urmaFe1) {
        UBSE_LOG_ERROR << "Failed to allocate memory for UbseFeInfo";
        return UBSE_ERROR;
    }

    if (lcneFe0.eidGroups.size() < 1 || lcneFe1.eidGroups.size() < 1) {
        UBSE_LOG_WARN << "Eid groups is empty, lcneFe0's eidGroups size=" << lcneFe0.eidGroups.size()
                      << ", lcneFe1's eidGroups size=" << lcneFe1.eidGroups.size();
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint64_t GenerateHwResId(const UbseMtiFeInfo &lcneFe)
{
    uint64_t iouId = static_cast<uint64_t>(std::stoul(lcneFe.iouId));
    uint64_t entityId = static_cast<uint64_t>(std::stoul(lcneFe.entityId));
    return (iouId << NO_32) | entityId;
}

UbseResult UbseUrmaControllerManager::CreateAndInsertUrmaInfo(const std::string &nodeId, UbseMtiFeInfo &lcneFe0,
                                                              UbseMtiFeInfo &lcneFe1)
{
    uint32_t slotId = 0;
    std::shared_ptr<UbseFeInfo> urmaFe0;
    std::shared_ptr<UbseFeInfo> urmaFe1;
    if (CreateAndInsertUrmaInfoPreset(lcneFe0, lcneFe1, urmaFe0, urmaFe1, slotId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to do preset step when create urma bounding info";
        return UBSE_ERROR;
    }
    auto feKey0 = GetVfeInfoKey(lcneFe0);
    auto feKey1 = GetVfeInfoKey(lcneFe1);
    feIdMap[feKey0] = GenerateUniqueFeId();
    feIdMap[feKey1] = GenerateUniqueFeId();
    auto fe0Id = feIdMap[feKey0];
    auto fe1Id = feIdMap[feKey1];
    UrmaDevType devType = lcneFe0.eidGroups.size() > 1 && lcneFe1.eidGroups.size() > 1 ? UrmaDevType::SHARED :
                                                                                         UrmaDevType::UNIQUE;
    size_t urmaBoundingNum = std::min(lcneFe0.eidGroups.size(), lcneFe1.eidGroups.size());
    UBSE_LOG_INFO << "Fe0 " << feKey0 << " has " << lcneFe0.eidGroups.size() << " groups, Fe1 " << feKey1 << " has "
                  << lcneFe1.eidGroups.size() << " groups";
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        nodeInfos[nodeId] = UbseUrmaNodeInfo{.nodeId = nodeId};
    }
    for (size_t idx = 0; idx < urmaBoundingNum; ++idx) {
        std::string devEid;
        if (GenerateUrmaDevEid(0, slotId, fe0Id, fe1Id, devEid) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to generate urma eid, skip this bounding, fe0's primary eid="
                          << lcneFe0.eidGroups[idx].primaryEid
                          << ", fe1's primary eid=" << lcneFe1.eidGroups[idx].primaryEid;
            continue;
        }
        UbseUrmaInfo urmaInfo{.urmaDevEid = devEid, .urmaDevType = devType, .state = UrmaDevState::UNKNOWN};
        EidGroup group0{.primaryEid = lcneFe0.eidGroups[idx].primaryEid,
                        .portEids = std::move(lcneFe0.eidGroups[idx].portEids),
                        .feInfo = urmaFe0};
        urmaInfo.eidGroups.push_back(group0);
        EidGroup group1{.primaryEid = lcneFe1.eidGroups[idx].primaryEid,
                        .portEids = std::move(lcneFe1.eidGroups[idx].portEids),
                        .feInfo = urmaFe1};
        urmaInfo.eidGroups.push_back(group1);
        std::string urmaName = "bonding_dev_" + std::to_string(GenerateUrmaId());
        UBSE_LOG_INFO << "Add urmaInfo for nodeId=" << nodeId << ", urmaName=" << urmaName << ", devEid=" << devEid
                      << ", fe0's primaryEid=" << group0.primaryEid << ", fe1's primaryEid=" << group1.primaryEid;
        urmaInfo.hwResId = GenerateHwResId(lcneFe0);
        nodeInfos[nodeId].urmaList[urmaName] = urmaInfo;
        fe0Id = GenerateUniqueFeId();
        fe1Id = GenerateUniqueFeId();
    }

    return UBSE_OK;
}

uint32_t UbseUrmaControllerManager::GenerateUniqueFeId()
{
    return ++globalFeId;
}

uint64_t UbseUrmaControllerManager::GenerateUrmaId()
{
    return ++globalUrmaId;
}

constexpr size_t UBPU_BOUNDARY_CNT = 2; // 一个slot只有两个chip
bool ValidateLcneFeInfo(const std::vector<std::vector<UbseMtiFeInfo>> &feInfos)
{
    if (feInfos.size() < UBPU_BOUNDARY_CNT) {
        // 少于两种ubpuId，返回错误
        UBSE_LOG_ERROR << "Not enough boundary found";
        return false;
    }
    if (feInfos.size() > UBPU_BOUNDARY_CNT) {
        // 出现超过两种ubpuId，为保持鲁棒性，仅日志警告
        UBSE_LOG_WARN << "More than two ubpuId found";
    }
    auto canConvertToUint32 = [](const std::string &str) -> bool {
        try {
            return std::stoul(str) <= UINT32_MAX;
        } catch (const std::exception &) {
            return false;
        }
    };
    auto checkOneFe = [&canConvertToUint32](const UbseMtiFeInfo &fe) -> bool {
        return canConvertToUint32(fe.slotId) && canConvertToUint32(fe.ubpuId) && canConvertToUint32(fe.iouId) &&
               canConvertToUint32(fe.entityId);
    };
    auto checkAllFeOnOneIou = [&checkOneFe](const std::vector<UbseMtiFeInfo> &feInfosOneIou) -> bool {
        if (feInfosOneIou.empty()) {
            return false; // 不允许为空
        }
        return std::all_of(feInfosOneIou.begin(), feInfosOneIou.end(), checkOneFe);
    };
    return std::all_of(feInfos.begin(), feInfos.end(), checkAllFeOnOneIou);
}

struct UbseFeVecCmp {
    bool operator()(const std::vector<UbseMtiFeInfo> &left, const std::vector<UbseMtiFeInfo> &right) const
    {
        if (left[0].slotId != right[0].slotId) {
            return CompareStringsNumerically(left[0].slotId, right[0].slotId);
        }
        if (left[0].ubpuId != right[0].ubpuId) {
            return CompareStringsNumerically(left[0].ubpuId, right[0].ubpuId);
        }
        return CompareStringsNumerically(left[0].iouId, right[0].iouId);
    }
};

struct UbseFeInfoCmp {
    bool operator()(const UbseMtiFeInfo &left, const UbseMtiFeInfo &right) const
    {
        if (left.eidGroups.size() != right.eidGroups.size()) {
            return left.eidGroups.size() < right.eidGroups.size();
        }
        return CompareStringsNumerically(left.entityId, right.entityId);
    }
};

bool UbseUrmaControllerManager::IsLcneFeUsed(const UbseMtiFeInfo &fe0, const UbseMtiFeInfo &fe1)
{
    auto feKey0 = GetVfeInfoKey(fe0);
    auto feKey1 = GetVfeInfoKey(fe1);
    if (feIdMap.find(feKey0) == feIdMap.end() && feIdMap.find(feKey1) != feIdMap.end() ||
        feIdMap.find(feKey0) != feIdMap.end() && feIdMap.find(feKey1) == feIdMap.end()) {
        UBSE_LOG_WARN << "There is only one fe in used, fe0 info: slotId=" << fe0.slotId << ", ubpuId=" << fe0.ubpuId
                      << ", iouId=" << fe0.iouId << ", entityId=" << fe0.entityId << ", fe1 info: slotId=" << fe1.slotId
                      << ", ubpuId=" << fe1.ubpuId << ", iouId=" << fe1.iouId << ", entityId=" << fe1.entityId;
        return true;
    }
    return feIdMap.find(feKey0) != feIdMap.end() && feIdMap.find(feKey1) != feIdMap.end();
}

UbseResult UbseUrmaControllerManager::ConstructNewUrmaInfo(const std::string &nodeId,
                                                           std::vector<std::vector<UbseMtiFeInfo>> &feInfos)
{
    if (!ValidateLcneFeInfo(feInfos)) {
        UBSE_LOG_ERROR
            << "Invalid feInfos, there must be at least two set of fes, and all fields must be convertible to uint32_t";
        return UBSE_ERROR_INVAL;
    }
    // 根据ubpuId和iouId对fe排序，使得(ubpuId, iouId)小的在前面，保证进程重启后能构建出相同的bounding
    std::sort(feInfos.begin(), feInfos.end(), UbseFeVecCmp());
    // 对每一组feInfo按Eid数量从小到大排序，如果Eid数量相同，按EntityId从小到大排序
    for (auto &feInfo : feInfos) {
        std::sort(feInfo.begin(), feInfo.end(), UbseFeInfoCmp());
        for (auto &fe : feInfo) {
            UBSE_LOG_DEBUG << "Fe info: slotId=" << fe.slotId << ", ubpuId=" << fe.ubpuId << ", iouId=" << fe.iouId
                           << ", entityId=" << fe.entityId << ", eid group size=" << fe.eidGroups.size();
        }
    }
    UBSE_LOG_INFO << "Begin to construct new bounding info for nodeId=" << nodeId;
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    // 目前每次只从两个区域各取一个fe组成bonding
    size_t maxBoundingCnt = std::min(feInfos[0].size(), feInfos[1].size());
    bool hasModified = false;
    for (size_t i = 0; i < maxBoundingCnt; ++i) {
        UbseMtiFeInfo &lcneFe0 = feInfos[0][i];
        UbseMtiFeInfo &lcneFe1 = feInfos[1][i];
        if (lcneFe0.eidGroups.empty() || lcneFe1.eidGroups.empty()) {
            UBSE_LOG_WARN << "Eid group is empty, skip it, fe0 info: slotId=" << lcneFe0.slotId
                          << ", ubpuId=" << lcneFe0.ubpuId << ", eid group size=" << lcneFe0.eidGroups.size()
                          << ", fe1 info: slotId=" << lcneFe1.slotId << ", ubpuId=" << lcneFe1.ubpuId
                          << ", eid group size=" << lcneFe1.eidGroups.size();
            continue;
        }
        if (IsLcneFeUsed(lcneFe0, lcneFe1)) {
            UBSE_LOG_INFO << "LcneFe is used, skip it, fe0 info: slotId=" << lcneFe0.slotId
                          << ", ubpuId=" << lcneFe0.ubpuId;
            continue;
        }
        if (CreateAndInsertUrmaInfo(nodeId, lcneFe0, lcneFe1) == UBSE_OK) {
            hasModified = true;
        }
    }
    if (hasModified) {
        UBSE_LOG_INFO << "Successfully constructed new URMA info for nodeId=" << nodeId;
        nodeInfos[nodeId].updateTimeStamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
    }
    return UBSE_OK;
}

UbseResult UbseUrmaControllerManager::ConstructNewUrmaInfo(const std::string &nodeId,
                                                           std::vector<std::vector<UbseMtiFeInfo>> &&feInfos)
{
    return ConstructNewUrmaInfo(nodeId, feInfos);
}

void UbseUrmaControllerManager::SetAllUrmaInfoToInactiveForNode(const std::string &nodeId)
{
    UBSE_LOG_INFO << "Set all URMA info to inactive for node=" << nodeId;
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << nodeId;
        return;
    }
    auto &nodeInfo = nodeInfos[nodeId];
    for (auto &urmaInfo : nodeInfo.urmaList) {
        urmaInfo.second.state = UrmaDevState::INACTIVED;
    }
}

void UbseUrmaControllerManager::SetAllUrmaInfoToActiveForNode(const std::string &nodeId)
{
    UBSE_LOG_INFO << "Set all URMA info to active for node=" << nodeId;
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "There is no urma info for node=" << nodeId;
        return;
    }
    auto &nodeInfo = nodeInfos[nodeId];
    for (auto &urmaInfo : nodeInfo.urmaList) {
        urmaInfo.second.state = UrmaDevState::ACTIVED;
    }
}

template <typename Func>
UbseResult RetryOperation(Func &&operation, uint32_t retryCount = 10, uint32_t retryInterval = 1)
{
    static_assert(std::is_same_v<std::invoke_result_t<Func>, UbseResult>,
                  "Func must return UbseResult and take no arguments.");
    for (int attempt = 0; attempt < retryCount; ++attempt) {
        if (g_globalStop || operation() == UBSE_OK) {
            return UBSE_OK;
        }
        std::this_thread::sleep_for(std::chrono::seconds(retryInterval));
    }
    return UBSE_ERROR;
}

UbseUrmaNodeInfo UbseUrmaControllerManager::GetUrmaNodeInfo(const std::string &nodeId)
{
    {
        ubse::utils::ReadLocker<utils::ReadWriteLock> readLock(&rwLock);
        if (nodeInfos.find(nodeId) != nodeInfos.end()) {
            return nodeInfos[nodeId];
        }
    }
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&rwLock);
    nodeInfos[nodeId].nodeId = nodeId;
    return nodeInfos[nodeId];
}
} // namespace ubse::urmaController
