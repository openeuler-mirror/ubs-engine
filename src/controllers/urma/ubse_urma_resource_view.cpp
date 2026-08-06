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

#include "ubse_urma_resource_view.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <limits>
#include <map>
#include <unordered_set>
#include <utility>

#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mti_interface.h"
#include "ubse_node_controller.h"
#include "ubse_pointer_process.h"
#include "ubse_smbios.h"
#include "ubse_str_util.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_uvs_module.h"
#include "adapter_plugins/urma/ubse_urma_uvs.h"

namespace ubse::urmaController {
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::urma;

UBSE_DEFINE_THIS_MODULE("ubse");

namespace {
constexpr char URMA_NAME_PREFIX[] = "bonding_dev_";
constexpr uint64_t URMA_EID_SHARE_DEGREE = 192;
constexpr size_t URMA_LOGICAL_NAME_MAX_LENGTH = 31;
constexpr size_t URMA_BACKING_FE_COUNT = 2;

enum class UrmaProjectionMode
{
    ONE_TO_ONE,
    EID_SHARING,
};

using UrmaNameFilter = std::unordered_set<std::string>;

struct DeviceSummaries {
    std::vector<std::string> names;
    std::vector<uint32_t> states;
    std::vector<uint64_t> hwResIds;
};

UbseResult ParseLogicalId(const std::string& name, uint64_t& logicalId)
{
    constexpr size_t prefixLength = sizeof(URMA_NAME_PREFIX) - 1;
    if (name.size() > URMA_LOGICAL_NAME_MAX_LENGTH || name.compare(0, prefixLength, URMA_NAME_PREFIX) != 0) {
        UBSE_LOG_WARN << "Invalid URMA logical device name, name=" << name;
        return UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID;
    }

    // 逻辑设备名固定为 bonding_dev_<id>，这里只提取并转换数字后缀。
    const auto suffix = name.substr(prefixLength);
    const auto result = std::from_chars(suffix.data(), suffix.data() + suffix.size(), logicalId);
    if (suffix.empty() || (suffix.size() > 1 && suffix.front() == '0') || result.ec != std::errc{} ||
        result.ptr != suffix.data() + suffix.size()) {
        UBSE_LOG_WARN << "Invalid URMA logical device name, name=" << name;
        return UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID;
    }
    if (logicalId == 0) {
        UBSE_LOG_WARN << "URMA logical device does not exist, name=" << name;
        return UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST;
    }
    return UBSE_OK;
}

UbseResult FormatLogicalName(uint64_t logicalId, std::string& name)
{
    name.clear();
    if (logicalId == 0) {
        UBSE_LOG_ERROR << "Failed to format URMA logical device name, logicalId is zero";
        return UBSE_ERROR_INVAL;
    }
    name = URMA_NAME_PREFIX + std::to_string(logicalId);
    if (name.size() <= URMA_LOGICAL_NAME_MAX_LENGTH) {
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "Failed to format URMA logical device name, logicalId=" << logicalId
                   << ", nameLength=" << name.size() << ", maxLength=" << URMA_LOGICAL_NAME_MAX_LENGTH;
    name.clear();
    return UBSE_ERROR_INVAL;
}

UbseResult ValidateBacking(const UbseUrmaInfo& info)
{
    if (info.urmaDevEid.empty() || info.eidGroups.size() != URMA_BACKING_FE_COUNT) {
        UBSE_LOG_ERROR << "Invalid URMA backing metadata, bondingEid=" << info.urmaDevEid
                       << ", feGroupCount=" << info.eidGroups.size()
                       << ", expectedFeGroupCount=" << URMA_BACKING_FE_COUNT;
        return UBSE_ERROR_INVAL;
    }
    for (size_t index = 0; index < info.eidGroups.size(); ++index) {
        const auto& group = info.eidGroups[index];
        if (group.primaryEid.empty() || group.feInfo == nullptr) {
            UBSE_LOG_ERROR << "Invalid URMA FE metadata, bondingEid=" << info.urmaDevEid << ", feIndex=" << index
                           << ", primaryEid=" << group.primaryEid
                           << ", feInfoNull=" << static_cast<int>(group.feInfo == nullptr);
            return UBSE_ERROR_INVAL;
        }
    }
    return UBSE_OK;
}

UbseResult CalculateHostHwResId(const std::string& nodeId, const UbseUrmaUvsFe& plannedFe, uint64_t& hwResId)
{
    std::map<UbseMtiIouInfo, UbseMtiEidGroup> comEids;
    auto ret = UbseMtiInterface::GetInstance().GetMtiComEid(comEids);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query communication EIDs for host URMA hardware resource, nodeId=" << nodeId
                       << ", ret=" << ret;
        return ret;
    }
    // planning 不保存 iouId，按首个 FE 的现有字段匹配 MTI 当前数据并实时计算 hwResId。
    const auto match = std::find_if(comEids.begin(), comEids.end(), [&](const auto& entry) {
        return entry.first.slotId == nodeId && entry.first.ubpuId == plannedFe.ubpuId &&
               entry.second.entityId == plannedFe.entityId && entry.second.primaryEid == plannedFe.primaryEid;
    });
    if (match == comEids.end()) {
        UBSE_LOG_ERROR << "Failed to match host URMA FE in communication EIDs, nodeId=" << nodeId
                       << ", ubpuId=" << plannedFe.ubpuId << ", entityId=" << plannedFe.entityId
                       << ", primaryEid=" << plannedFe.primaryEid;
        return UBSE_ERROR_INVAL;
    }
    uint32_t iouId = 0;
    uint32_t entityId = 0;
    if (ubse::utils::ConvertStrToUint32(match->first.iouId, iouId) != UBSE_OK ||
        ubse::utils::ConvertStrToUint32(match->second.entityId, entityId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to convert host URMA hardware resource IDs, nodeId=" << nodeId
                       << ", iouId=" << match->first.iouId << ", entityId=" << match->second.entityId;
        return UBSE_ERROR_INVAL;
    }
    hwResId = (static_cast<uint64_t>(iouId) << NO_32) | entityId;
    return UBSE_OK;
}

UbseResult BuildHostBackingInfo(const std::string& nodeId, UbseUrmaInfo& info)
{
    std::vector<UbseUrmaUvsNodeInfo> planning;
    auto ret = UbseNodeController::GetInstance().GetPlanningHostBondingByNodeId(nodeId, planning);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query host URMA planning, nodeId=" << nodeId << ", ret=" << ret;
        return ret;
    }
    if (planning.size() != 1 || planning[0].nodeId != nodeId || planning[0].devList.size() != 1 ||
        planning[0].devList[0].feList.size() != URMA_BACKING_FE_COUNT) {
        UBSE_LOG_ERROR << "Invalid host URMA planning data, nodeId=" << nodeId << ", planningSize=" << planning.size();
        return UBSE_ERROR_INVAL;
    }

    const auto& planned = planning[0].devList[0];
    uint64_t hwResId = 0;
    if ((ret = CalculateHostHwResId(nodeId, planned.feList.front(), hwResId)) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to calculate host URMA hardware resource ID, nodeId=" << nodeId
                       << ", bondingEid=" << planned.urmaDevEid << ", ret=" << ret;
        return ret;
    }
    info = {};
    info.urmaDevEid = planned.urmaDevEid;
    info.urmaDevType = UrmaDevType::SHARED;
    info.state = UrmaDevState::UNKNOWN;
    info.hwResId = hwResId;
    info.eidGroups.reserve(planned.feList.size());
    for (const auto& plannedFe : planned.feList) {
        auto feInfo = SafeMakeShared<UbseFeInfo>();
        if (feInfo == nullptr) {
            UBSE_LOG_ERROR << "Failed to allocate host URMA FE metadata, nodeId=" << nodeId
                           << ", primaryEid=" << plannedFe.primaryEid;
            return UBSE_ERROR_NULLPTR;
        }
        feInfo->slotId = nodeId;
        feInfo->ubpuId = plannedFe.ubpuId;
        feInfo->entityId = plannedFe.entityId;
        info.eidGroups.push_back({plannedFe.primaryEid, plannedFe.portEid, std::move(feInfo)});
    }
    ret = ValidateBacking(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Host URMA backing metadata is incomplete, nodeId=" << nodeId
                       << ", bondingEid=" << planned.urmaDevEid << ", ret=" << ret;
    }
    return ret;
}

using LogicalBackingEntries = std::map<std::string, std::string, UrmaNameCompare>;

UbseResult BuildLogicalBackingEntries(const std::vector<std::string>& backingNames, LogicalBackingEntries& entries)
{
    entries.clear();
    uint64_t backingIndex = 0;
    for (const auto& backingName : backingNames) {
        if (backingName.empty()) {
            UBSE_LOG_ERROR << "Failed to map URMA backing with empty name";
            return UBSE_ERROR_INVAL;
        }
        if (backingIndex >= std::numeric_limits<uint64_t>::max() / URMA_EID_SHARE_DEGREE) {
            UBSE_LOG_ERROR << "URMA logical device index overflow, backingName=" << backingName;
            return UBSE_ERROR_INVAL;
        }
        const uint64_t firstLogicalId = backingIndex * URMA_EID_SHARE_DEGREE + 1;
        for (uint64_t offset = 0; offset < URMA_EID_SHARE_DEGREE; ++offset) {
            std::string logicalName;
            auto ret = FormatLogicalName(firstLogicalId + offset, logicalName);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to format URMA logical device name, backingName=" << backingName
                               << ", logicalId=" << firstLogicalId + offset << ", ret=" << ret;
                return ret;
            }
            entries.emplace(std::move(logicalName), backingName);
        }
        ++backingIndex;
    }
    return UBSE_OK;
}

bool IsNameAllowed(const UrmaNameFilter& allowed, const std::string& name)
{
    return allowed.empty() || allowed.find(name) != allowed.end();
}

bool QueryAllPortsDownWithFallback()
{
    bool isAllPortDown = false;
    // 简要列表延续原行为：端口查询失败时使用进程内最近一次成功结果。
    static std::atomic_bool lastQueryResult{false};
    if (auto ret = QueryAllPortsDown(isAllPortDown); ret != UBSE_OK) {
        isAllPortDown = lastQueryResult.load(std::memory_order_relaxed);
        UBSE_LOG_WARN << "Failed to query all ports status, use last query result=" << static_cast<int>(isAllPortDown)
                      << ", ret=" << ret;
    } else {
        lastQueryResult.store(isAllPortDown, std::memory_order_relaxed);
    }
    return isAllPortDown;
}

bool IsHostBackingCreated(const UrmaLogicalGroup& group)
{
    bool active = false;
    auto ret = UbseGetBondingActiveStateByEid(group.backingInfo.urmaDevEid, active);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query host URMA backing state, backingName=" << group.backingName
                      << ", bondingEid=" << group.backingInfo.urmaDevEid << ", ret=" << ret;
        return false;
    }
    if (!active) {
        UBSE_LOG_INFO << "Host URMA backing is inactive, backingName=" << group.backingName
                      << ", bondingEid=" << group.backingInfo.urmaDevEid;
        return false;
    }
    for (const auto& eidGroup : group.backingInfo.eidGroups) {
        bool feActive = false;
        ret = UbseGetBondingActiveStateByEid(eidGroup.primaryEid, feActive);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to query host URMA FE state, backingName=" << group.backingName
                          << ", primaryEid=" << eidGroup.primaryEid << ", ret=" << ret;
            return false;
        }
        if (!feActive) {
            UBSE_LOG_INFO << "Host URMA FE is inactive, backingName=" << group.backingName
                          << ", primaryEid=" << eidGroup.primaryEid;
            return false;
        }
    }
    return true;
}

bool IsGroupHealthy(const UrmaLogicalGroup& group, bool isAllPortDown)
{
    if (isAllPortDown) {
        return false;
    }
    return std::all_of(group.backingInfo.eidGroups.begin(), group.backingInfo.eidGroups.end(),
                       [](const auto& eidGroup) { return IsUdmaDevHealthy(eidGroup.primaryEid); });
}

void AppendGroupedSummary(const UrmaLogicalGroup& group, bool isAllPortDown, DeviceSummaries& summaries)
{
    const auto state =
        static_cast<uint32_t>(IsGroupHealthy(group, isAllPortDown) ? UrmaDevState::ACTIVED : UrmaDevState::INACTIVED);
    for (const auto& logicalName : group.logicalNames) {
        summaries.names.push_back(logicalName);
        summaries.states.push_back(state);
        summaries.hwResIds.push_back(group.backingInfo.hwResId);
    }
}

UrmaDevState GetGroupedDeviceState(const UrmaLogicalGroup& group, bool portQueryFailed, bool isAllPortDown,
                                   const UbseUrmaResourceView& view)
{
    if (portQueryFailed) {
        return UrmaDevState::UNKNOWN;
    }
    if (isAllPortDown) {
        return UrmaDevState::PORT_DOWN;
    }
    const bool created = group.isHostBonding ? IsHostBackingCreated(group) : view.IsBackingCreated(group.backingInfo);
    return created ? UrmaDevState::ACTIVED : UrmaDevState::INACTIVED;
}

void BuildGroupedDetailBase(const UrmaLogicalGroup& group, bool portQueryFailed, bool isAllPortDown,
                            const UbseUrmaResourceView& view, UbseUrmaDevBrief& detail)
{
    // 每个逻辑设备保留完整真实设备元数据，仅替换北向可见名称。
    detail.devEid = group.backingInfo.urmaDevEid;
    detail.bondingType = group.backingInfo.urmaDevType;
    for (const auto& eidGroup : group.backingInfo.eidGroups) {
        detail.feEids.push_back(eidGroup.primaryEid);
        std::string feName = eidGroup.feInfo->name;
        if (group.isHostBonding) {
            std::string actualFeName;
            const auto ret = UbseGetUrmaSubpathByEid(eidGroup.primaryEid, actualFeName);
            if (ret == UBSE_OK && !actualFeName.empty()) {
                feName = std::move(actualFeName);
            } else {
                UBSE_LOG_WARN << "Failed to query host URMA FE path, backingName=" << group.backingName
                              << ", primaryEid=" << eidGroup.primaryEid << ", ret=" << ret;
            }
        }
        detail.feNames.push_back(std::move(feName));
    }
    detail.state = GetGroupedDeviceState(group, portQueryFailed, isAllPortDown, view);
}

void AppendGroupedDetails(const UrmaLogicalGroup& group, bool portQueryFailed, bool isAllPortDown,
                          const UbseUrmaResourceView& view, std::vector<UbseUrmaDevBrief>& details)
{
    UbseUrmaDevBrief base{};
    BuildGroupedDetailBase(group, portQueryFailed, isAllPortDown, view, base);
    for (const auto& logicalName : group.logicalNames) {
        auto detail = base;
        detail.urmaName = logicalName;
        details.push_back(std::move(detail));
    }
}

void AppendDirectDetail(const std::pair<const std::string, UbseUrmaInfo>& backing,
                        std::vector<UbseUrmaDevBrief>& devInfos)
{
    if (backing.second.eidGroups.size() != URMA_BACKING_FE_COUNT) {
        UBSE_LOG_WARN << "Failed to get FE info for URMA backing, backingName=" << backing.first;
        return;
    }
    UbseUrmaDevBrief detail;
    detail.urmaName = backing.first;
    for (const auto& eidGroup : backing.second.eidGroups) {
        detail.feEids.push_back(eidGroup.primaryEid);
        detail.feNames.push_back(eidGroup.feInfo == nullptr ? "" : eidGroup.feInfo->name);
    }
    detail.state = backing.second.state;
    detail.devEid = backing.second.urmaDevEid;
    detail.bondingType = backing.second.urmaDevType;
    devInfos.push_back(std::move(detail));
}
} // namespace

bool IsUdmaDevHealthy(const std::string& feEid)
{
    std::string deviceName;
    const auto ret = UbseGetUrmaSubpathByEid(feEid, deviceName);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query URMA FE path while checking health, primaryEid=" << feEid << ", ret=" << ret;
        return false;
    }
    if (deviceName.empty()) {
        UBSE_LOG_INFO << "URMA FE path is empty while checking health, primaryEid=" << feEid;
        return false;
    }
    return true;
}

namespace {
UbseResult GetUrmaProjectionMode(UrmaProjectionMode& mode)
{
    mode = UrmaProjectionMode::ONE_TO_ONE;
    if (UbseSmbios::GetInstance().IsClosType()) {
        return UBSE_OK;
    }
    auto module = UbseContext::GetInstance().GetModule<UbseUrmaUvsModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "Failed to get URMA UVS module while selecting projection mode";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    if (module->IsEidSharingModeEnabled()) {
        mode = UrmaProjectionMode::EID_SHARING;
    }
    return UBSE_OK;
}
} // namespace

UbseResult UbseUrmaResourceView::CollectCurrentBackings(CurrentBackings& backings, bool hostOnly) const
{
    backings.clear();
    const auto curNode = UbseNodeController::GetInstance().GetCurNode();
    if (curNode.nodeId.empty()) {
        UBSE_LOG_ERROR << "Failed to get current node while collecting URMA backings";
        return UBSE_URMACONTRL_ERROR_GET_NODE_INFO_FAILED;
    }

    if (!hostOnly) {
        auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(curNode.nodeId);
        for (auto& [name, info] : nodeInfo.urmaList) {
            if (name != UBSE_HOST_URMA_DEV_NAME) {
                backings.emplace(name, CurrentBacking{std::move(info), false});
            }
        }
    }
    if (!UbseNodeController::GetInstance().IsHostBondingRegistered()) {
        // 没有占用，无需基于bonding_dev_0创建逻辑设备
        return UBSE_OK;
    }

    UbseUrmaInfo hostInfo;
    auto ret = BuildHostBackingInfo(curNode.nodeId, hostInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query host URMA backing, nodeId=" << curNode.nodeId << ", ret=" << ret;
        return ret;
    }
    backings.emplace(UBSE_HOST_URMA_DEV_NAME, CurrentBacking{std::move(hostInfo), true});
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::CollectAllocBackings(CurrentBackings& backings, bool hostOnly) const
{
    backings.clear();
    const auto curNode = UbseNodeController::GetInstance().GetCurNode();
    if (curNode.nodeId.empty()) {
        UBSE_LOG_ERROR << "Failed to get current node while collecting URMA allocation backings";
        return UBSE_URMACONTRL_ERROR_GET_NODE_INFO_FAILED;
    }
    if (!hostOnly) {
        auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(curNode.nodeId);
        for (auto& [name, info] : nodeInfo.urmaList) {
            if (name != UBSE_HOST_URMA_DEV_NAME) {
                backings.emplace(name, CurrentBacking{std::move(info), false});
            }
        }
    }
    // alloc 建立名称映射时不读取 b0 planning，命中后再进入 Node 所属的分配路径。
    if (UbseNodeController::GetInstance().IsHostBondingRegistered()) {
        backings.emplace(UBSE_HOST_URMA_DEV_NAME, CurrentBacking{{}, true});
    }
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::ValidateProjectionBackings(const CurrentBackings& backings) const
{
    // 设备列表负责保证 backing 间的 EID 关系，这里只校验每条记录的静态信息完整。
    for (const auto& [name, backing] : backings) {
        if (name.empty()) {
            UBSE_LOG_ERROR << "URMA backing name is empty";
            return UBSE_ERROR_INVAL;
        }
        auto ret = ValidateBacking(backing.info);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Invalid URMA backing metadata, backingName=" << name
                           << ", bondingEid=" << backing.info.urmaDevEid << ", ret=" << ret;
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::RefreshLogicalBackingMapLocked(const CurrentBackings& backings) const
{
    std::vector<std::string> backingNames;
    backingNames.reserve(backings.size());
    for (const auto& [name, _] : backings) {
        backingNames.push_back(name);
    }
    if (backingNames == mappedBackingNames) {
        return UBSE_OK;
    }
    LogicalBackingMap candidate;
    auto ret = BuildLogicalBackingEntries(backingNames, candidate);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to build URMA logical backing map, ret=" << ret;
        return ret;
    }
    mappedBackingNames.swap(backingNames);
    logicalBackingMap.swap(candidate);
    UBSE_LOG_INFO << "Refreshed URMA logical backing map, backingCount=" << mappedBackingNames.size()
                  << ", logicalCount=" << logicalBackingMap.size();
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::BuildProjectionGroups(const CurrentBackings& backings,
                                                       const std::vector<std::string>& filter,
                                                       UrmaLogicalProjection& projection) const
{
    const std::unordered_set<std::string> allowed(filter.begin(), filter.end());
    std::lock_guard<std::mutex> guard(mappingMutex);
    auto ret = RefreshLogicalBackingMapLocked(backings);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to refresh URMA logical backing map, ret=" << ret;
        return ret;
    }
    for (const auto& [logicalName, backingName] : logicalBackingMap) {
        if (!IsNameAllowed(allowed, logicalName)) {
            continue;
        }
        if (projection.groups.empty() || projection.groups.back().backingName != backingName) {
            const auto backing = backings.find(backingName);
            if (backing == backings.end()) {
                UBSE_LOG_ERROR << "Mapped URMA backing is missing, backingName=" << backingName;
                return UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST;
            }
            projection.groups.push_back({backingName, backing->second.info, backing->second.isHostBonding, {}});
        }
        projection.groups.back().logicalNames.push_back(logicalName);
        ++projection.logicalDeviceCount;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::BuildLogicalProjection(const std::vector<std::string>& filter,
                                                        UrmaLogicalProjection& projection) const
{
    return BuildLogicalProjection(filter, projection, false);
}

UbseResult UbseUrmaResourceView::BuildLogicalProjection(const std::vector<std::string>& filter,
                                                        UrmaLogicalProjection& projection, bool hostOnly) const
{
    projection = {};
    CurrentBackings current;
    // 现阶段共享 EID 模式只收集 bonding_dev_0
    auto ret = CollectCurrentBackings(current, hostOnly);
    if (ret != UBSE_OK || (ret = ValidateProjectionBackings(current)) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to validate current URMA backings, ret=" << ret;
        return ret;
    }
    UrmaLogicalProjection candidateProjection;
    ret = BuildProjectionGroups(current, filter, candidateProjection);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to build URMA logical projection groups, ret=" << ret;
        return ret;
    }
    projection = std::move(candidateProjection);
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::BuildGroupedSummaries(std::vector<std::string>& names, std::vector<uint32_t>& states,
                                                       std::vector<uint64_t>& hwResIds) const
{
    UrmaLogicalProjection projection;
    auto ret = BuildLogicalProjection({}, projection, true);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to build grouped URMA device summaries, ret=" << ret;
        return ret;
    }
    DeviceSummaries candidate;
    candidate.names.reserve(projection.logicalDeviceCount);
    candidate.states.reserve(projection.logicalDeviceCount);
    candidate.hwResIds.reserve(projection.logicalDeviceCount);
    const bool isAllPortDown = QueryAllPortsDownWithFallback();
    for (const auto& group : projection.groups) {
        AppendGroupedSummary(group, isAllPortDown, candidate);
    }
    names.swap(candidate.names);
    states.swap(candidate.states);
    hwResIds.swap(candidate.hwResIds);
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::BuildDirectSummaries(std::vector<std::string>& names, std::vector<uint32_t>& states,
                                                      std::vector<uint64_t>& hwResIds) const
{
    const auto currentNode = UbseNodeController::GetInstance().GetCurNode();
    if (currentNode.nodeId.empty()) {
        UBSE_LOG_WARN << "Failed to get current node while building URMA device summaries";
        return UBSE_ERROR;
    }
    const auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(currentNode.nodeId);
    const bool isAllPortDown = QueryAllPortsDownWithFallback();
    DeviceSummaries candidate;
    for (const auto& backing : nodeInfo.urmaList) {
        if (backing.first == UBSE_HOST_URMA_DEV_NAME) {
            continue;
        }
        candidate.names.push_back(backing.first);
        const bool healthy = !isAllPortDown &&
                             std::all_of(backing.second.eidGroups.begin(), backing.second.eidGroups.end(),
                                         [](const auto& group) { return IsUdmaDevHealthy(group.primaryEid); });
        candidate.states.push_back(static_cast<uint32_t>(healthy ? UrmaDevState::ACTIVED : UrmaDevState::INACTIVED));
        candidate.hwResIds.push_back(backing.second.hwResId);
    }
    names.swap(candidate.names);
    states.swap(candidate.states);
    hwResIds.swap(candidate.hwResIds);
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::GetDeviceSummaries(std::vector<std::string>& names, std::vector<uint32_t>& states,
                                                    std::vector<uint64_t>& hwResIds) const
{
    names.clear();
    states.clear();
    hwResIds.clear();
    UrmaProjectionMode mode = UrmaProjectionMode::ONE_TO_ONE;
    auto ret = GetUrmaProjectionMode(mode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to select URMA device summary projection mode, ret=" << ret;
        return ret;
    }
    // ResourceView 始终返回逻辑设备；非共享模式采用与真实设备一一对应的直接映射。
    ret = mode == UrmaProjectionMode::EID_SHARING ? BuildGroupedSummaries(names, states, hwResIds) :
                                                    BuildDirectSummaries(names, states, hwResIds);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to build URMA device summaries, projectionMode=" << static_cast<int>(mode)
                       << ", ret=" << ret;
    }
    return ret;
}

UbseResult UbseUrmaResourceView::BuildGroupedDetails(const std::vector<std::string>& filter,
                                                     std::vector<UbseUrmaDevBrief>& devInfos) const
{
    UrmaLogicalProjection projection;
    auto ret = BuildLogicalProjection(filter, projection, true);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to build grouped URMA device details, ret=" << ret;
        return ret;
    }
    bool isAllPortDown = false;
    const auto portRet = QueryAllPortsDown(isAllPortDown);
    const bool portQueryFailed = portRet != UBSE_OK;
    if (portQueryFailed) {
        UBSE_LOG_WARN << "Failed to query all ports status while building URMA device details, ret=" << portRet;
    }
    std::vector<UbseUrmaDevBrief> candidate;
    candidate.reserve(projection.logicalDeviceCount);
    for (const auto& group : projection.groups) {
        AppendGroupedDetails(group, portQueryFailed, isAllPortDown, *this, candidate);
    }
    devInfos.swap(candidate);
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::BuildDirectDetails(const std::vector<std::string>& filter,
                                                    std::vector<UbseUrmaDevBrief>& devInfos) const
{
    UbseRoleInfo currentNode{};
    const auto ret = UbseGetCurrentNodeInfo(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get current node while building URMA device details, ret=" << ret;
        return UBSE_OK;
    }
    RefreshAllUrmaDevsState(currentNode.nodeId);
    const auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(currentNode.nodeId);
    const UrmaNameFilter allowed(filter.begin(), filter.end());
    std::vector<UbseUrmaDevBrief> candidate;
    for (const auto& backing : nodeInfo.urmaList) {
        if (backing.first == UBSE_HOST_URMA_DEV_NAME || !IsNameAllowed(allowed, backing.first)) {
            continue;
        }
        AppendDirectDetail(backing, candidate);
    }
    devInfos.swap(candidate);
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::GetDeviceDetails(const std::vector<std::string>& filter,
                                                  std::vector<UbseUrmaDevBrief>& devInfos) const
{
    devInfos.clear();
    UrmaProjectionMode mode = UrmaProjectionMode::ONE_TO_ONE;
    auto ret = GetUrmaProjectionMode(mode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to select URMA device detail projection mode, ret=" << ret;
        return ret;
    }
    ret = mode == UrmaProjectionMode::EID_SHARING ? BuildGroupedDetails(filter, devInfos) :
                                                    BuildDirectDetails(filter, devInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to build URMA device details, projectionMode=" << static_cast<int>(mode)
                       << ", filterCount=" << filter.size() << ", ret=" << ret;
    }
    return ret;
}

bool UbseUrmaResourceView::IsBackingCreated(const UbseUrmaInfo& info) const
{
    if (info.subPath.empty()) {
        UBSE_LOG_INFO << "URMA backing is not created, subpath is empty";
        return false;
    }
    bool active = false;
    auto ret = UbseGetBondingActiveStateByEid(info.urmaDevEid, active);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query URMA backing state, bondingEid=" << info.urmaDevEid << ", ret=" << ret;
        return false;
    }
    if (!active) {
        UBSE_LOG_INFO << "URMA backing is inactive, bondingEid=" << info.urmaDevEid;
        return false;
    }
    if (info.eidGroups.empty()) {
        UBSE_LOG_INFO << "URMA backing is not created, FE groups are empty";
        return false;
    }
    for (const auto& group : info.eidGroups) {
        if (group.feInfo == nullptr || group.feInfo->name.empty()) {
            UBSE_LOG_INFO << "URMA backing FE metadata is incomplete, bondingEid=" << info.urmaDevEid
                          << ", primaryEid=" << group.primaryEid
                          << ", feInfoNull=" << static_cast<int>(group.feInfo == nullptr);
            return false;
        }
        ret = UbseGetBondingActiveStateByEid(group.primaryEid, active);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to query URMA backing FE state, bondingEid=" << info.urmaDevEid
                          << ", primaryEid=" << group.primaryEid << ", ret=" << ret;
            return false;
        }
        if (!active) {
            UBSE_LOG_INFO << "URMA backing FE is inactive, bondingEid=" << info.urmaDevEid
                          << ", primaryEid=" << group.primaryEid;
            return false;
        }
    }
    return true;
}

UbseResult UbseUrmaResourceView::ResolveGroupedAllocTarget(const std::string& logicalName,
                                                           UrmaAllocTarget& resolved) const
{
    resolved = UrmaAllocTarget{};
    uint64_t logicalId = 0;
    auto ret = ParseLogicalId(logicalName, logicalId);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to parse URMA logical device name for allocation, name=" << logicalName
                      << ", ret=" << ret;
        return ret;
    }

    CurrentBackings current;
    // grouped alloc 仅在共享 EID 模式调用，只收集 bonding_dev_0。
    if ((ret = CollectAllocBackings(current, true)) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to collect current URMA allocation backings, name=" << logicalName << ", ret=" << ret;
        return ret;
    }
    std::string backingName;
    {
        std::lock_guard<std::mutex> guard(mappingMutex);
        if ((ret = RefreshLogicalBackingMapLocked(current)) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to refresh URMA allocation mapping, name=" << logicalName << ", ret=" << ret;
            return ret;
        }
        const auto mapping = logicalBackingMap.find(logicalName);
        if (mapping != logicalBackingMap.end()) {
            backingName = mapping->second;
        }
    }
    if (backingName.empty()) {
        UBSE_LOG_WARN << "URMA logical device is outside current projection, name=" << logicalName
                      << ", logicalId=" << logicalId << ", backingCount=" << current.size();
        return UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID;
    }
    const auto backing = current.find(backingName);
    if (backing == current.end()) {
        UBSE_LOG_ERROR << "Mapped URMA allocation backing is missing, backingName=" << backingName;
        return UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST;
    }
    // 映射只返回真实名称，后续分配继续使用原有 manager/Node 流程。
    resolved.backingName = backingName;
    resolved.isHostBonding = backing->second.isHostBonding;
    return UBSE_OK;
}

UbseResult UbseUrmaResourceView::ResolveAllocTarget(const std::string& logicalName, UrmaAllocTarget& target) const
{
    target = UrmaAllocTarget{};
    UrmaProjectionMode mode = UrmaProjectionMode::ONE_TO_ONE;
    auto ret = GetUrmaProjectionMode(mode);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to query URMA projection mode, ret=" << ret;
        return ret;
    }
    if (mode == UrmaProjectionMode::EID_SHARING) {
        ret = ResolveGroupedAllocTarget(logicalName, target);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to resolve grouped URMA allocation target, name=" << logicalName
                          << ", ret=" << ret;
        }
        return ret;
    }
    // 非共享模式仍经过统一逻辑视图，逻辑名称与真实名称一一对应。
    target.backingName = logicalName;
    return UBSE_OK;
}
} // namespace ubse::urmaController
