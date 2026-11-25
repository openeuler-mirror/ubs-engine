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

#include "ubse_mem_cabinet_ledger_util.h"

#include <algorithm>
#include <limits>

#include "ubse_node_mgr.h"

namespace ubse::mem::controller {
namespace {
using ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;

// 与既有 CollectRelatedNodeIds 完全同源：每条记录只取首元素代表其对外关系
bool NumaInfosHitFilter(const std::vector<UbseMemDebtNumaInfo>& infos, const std::set<std::string>& filterSet)
{
    if (infos.empty()) {
        return false;
    }
    const auto& nodeId = infos.front().nodeId;
    return !nodeId.empty() && filterSet.count(nodeId) != 0;
}

template <typename Obj>
bool ObjPeerHitFilter(const Obj& obj, const std::set<std::string>& filterSet)
{
    return NumaInfosHitFilter(obj.algoResult.importNumaInfos, filterSet) ||
           NumaInfosHitFilter(obj.algoResult.exportNumaInfos, filterSet);
}

bool ObjPeerHitFilter(const UbseMemShareBorrowImportObj& obj, const std::set<std::string>& filterSet)
{
    const bool importHit = !obj.importNodeId.empty() && filterSet.count(obj.importNodeId) != 0;
    return importHit || NumaInfosHitFilter(obj.algoResult.exportNumaInfos, filterSet);
}

void InsertNumaInfoPeers(const std::vector<UbseMemDebtNumaInfo>& infos, std::set<std::string>& peerIds)
{
    if (infos.empty()) {
        return;
    }
    const auto& nodeId = infos.front().nodeId;
    if (!nodeId.empty()) {
        peerIds.insert(nodeId);
    }
}

template <typename Obj>
void AppendObjPeers(const Obj& obj, std::set<std::string>& peerIds)
{
    InsertNumaInfoPeers(obj.algoResult.importNumaInfos, peerIds);
    InsertNumaInfoPeers(obj.algoResult.exportNumaInfos, peerIds);
}

void AppendObjPeers(const UbseMemShareBorrowImportObj& obj, std::set<std::string>& peerIds)
{
    if (!obj.importNodeId.empty()) {
        peerIds.insert(obj.importNodeId);
    }
    InsertNumaInfoPeers(obj.algoResult.exportNumaInfos, peerIds);
}

bool ShouldKeep(PeerFilterMode mode, bool hit)
{
    return mode == PeerFilterMode::KEEP_IN_SET ? hit : !hit;
}

template <typename ObjMap>
void ProjectObjMap(const ObjMap& src, ObjMap& dst, const std::set<std::string>& peerNodeIds, PeerFilterMode mode)
{
    for (const auto& item : src) {
        if (ShouldKeep(mode, ObjPeerHitFilter(item.second, peerNodeIds))) {
            dst.emplace(item.first, item.second);
        }
    }
}
} // namespace

bool GetCabinetGroupIdByNodeId(const std::string& nodeId, uint32_t& groupId)
{
    if (nodeId.empty()) {
        return false;
    }
    const auto staticInfo = ubse::nodeMgr::GetUbseNodeById(nodeId);
    if (staticInfo.nodeId.empty()) {
        return false;
    }
    groupId = static_cast<uint32_t>(staticInfo.groupId);
    return true;
}

std::vector<std::string> GetCabinetMemberNodeIds(uint32_t groupId)
{
    std::vector<std::string> nodeIds;
    if (groupId > std::numeric_limits<uint16_t>::max()) {
        return nodeIds;
    }
    const auto members = ubse::nodeMgr::GetNodesByGroupId(static_cast<uint16_t>(groupId));
    nodeIds.reserve(members.size());
    for (const auto& node : members) {
        if (!node.nodeId.empty()) {
            nodeIds.push_back(node.nodeId);
        }
    }
    std::sort(nodeIds.begin(), nodeIds.end());
    return nodeIds;
}

bool IsConfirmedFaultClusterState(nodeController::UbseNodeClusterState state)
{
    return state == nodeController::UbseNodeClusterState::UBSE_NODE_FAULT ||
           state == nodeController::UbseNodeClusterState::UBSE_NODE_PRE_BMC ||
           state == nodeController::UbseNodeClusterState::UBSE_NODE_UNKNOWN;
}

std::set<std::string> GetDebtPeerNodeIds(const NodeMemDebtInfo& info)
{
    std::set<std::string> peerIds;
    for (const auto& item : info.fdImportObjMap) {
        AppendObjPeers(item.second, peerIds);
    }
    for (const auto& item : info.fdExportObjMap) {
        AppendObjPeers(item.second, peerIds);
    }
    for (const auto& item : info.numaImportObjMap) {
        AppendObjPeers(item.second, peerIds);
    }
    for (const auto& item : info.numaExportObjMap) {
        AppendObjPeers(item.second, peerIds);
    }
    for (const auto& item : info.shareImportObjMap) {
        AppendObjPeers(item.second, peerIds);
    }
    for (const auto& item : info.shareExportObjMap) {
        AppendObjPeers(item.second, peerIds);
    }
    for (const auto& item : info.addrImportObjMap) {
        AppendObjPeers(item.second, peerIds);
    }
    for (const auto& item : info.addrExportObjMap) {
        AppendObjPeers(item.second, peerIds);
    }
    return peerIds;
}

NodeMemDebtInfo FilterDebtInfoByPeerNodeIds(const NodeMemDebtInfo& src, const std::set<std::string>& peerNodeIds,
                                            PeerFilterMode mode)
{
    NodeMemDebtInfo filtered;
    ProjectObjMap(src.fdImportObjMap, filtered.fdImportObjMap, peerNodeIds, mode);
    ProjectObjMap(src.fdExportObjMap, filtered.fdExportObjMap, peerNodeIds, mode);
    ProjectObjMap(src.numaImportObjMap, filtered.numaImportObjMap, peerNodeIds, mode);
    ProjectObjMap(src.numaExportObjMap, filtered.numaExportObjMap, peerNodeIds, mode);
    ProjectObjMap(src.shareImportObjMap, filtered.shareImportObjMap, peerNodeIds, mode);
    ProjectObjMap(src.shareExportObjMap, filtered.shareExportObjMap, peerNodeIds, mode);
    ProjectObjMap(src.addrImportObjMap, filtered.addrImportObjMap, peerNodeIds, mode);
    ProjectObjMap(src.addrExportObjMap, filtered.addrExportObjMap, peerNodeIds, mode);
    return filtered;
}
} // namespace ubse::mem::controller
