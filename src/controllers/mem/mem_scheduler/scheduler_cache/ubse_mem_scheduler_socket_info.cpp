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

#include "ubse_mem_scheduler_socket_info.h"
#include "ubse_mem_types.h"
#include "ubse_str_util.h"

namespace ubse::mem::scheduler {

UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

void SchedulerSocketInfo::Init(const UbseNumaInfo& ubseNumaInfo)
{
    SchedulerNumaInfo* numaInfo;
    auto it = numaInfoMap_.find(ubseNumaInfo.location.numaId);
    if (it != numaInfoMap_.end()) {
        numaInfo = it->second.get();
    } else {
        auto [newIt, _] = numaInfoMap_.emplace(ubseNumaInfo.location.numaId,
                                               std::make_unique<SchedulerNumaInfo>(ubseNumaInfo.location.numaId));
        numaInfo = newIt->second.get();
        UBSE_LOG_INFO << "Create memNumaInfo successful, numaId=" << ubseNumaInfo.location.numaId;
    }
    numaInfo->Init(ubseNumaInfo);
}

SchedulerNumaInfo* SchedulerSocketInfo::GetNumaInfo(NumaId numaId) const
{
    auto numaIter = numaInfoMap_.find(numaId);
    if (numaIter != numaInfoMap_.end()) {
        return numaIter->second.get();
    }
    return nullptr;
}

SocketInfo SchedulerSocketInfo::GetSocketInfo() const
{
    SocketInfo socketInfo;
    socketInfo.socketId = socketId_;
    for (const auto& [_, numaInfo] : numaInfoMap_) {
        socketInfo.numaInfos.emplace_back(numaInfo->GetNumaId());
    }
    return socketInfo;
}

void SchedulerSocketInfo::UpdateLinkInfo(const nodeController::UbseCpuInfo& cpuInfo)
{
    if (cpuInfo.socketId != socketId_) {
        UBSE_LOG_WARN << "UpdateLinkInfo: cpuInfo.socketId != socketId_, socketId_=" << socketId_
                      << ", cpuInfo.socketId=" << cpuInfo.socketId;
        return;
    }
    ports_.clear();
    rawPortInfos_.clear();
    for (const auto& [_, portInfo] : cpuInfo.portInfos) {
        if (portInfo.portStatus != nodeController::PortStatus::UP) {
            continue;
        }
        PortId portId{};
        auto ret = ubse::utils::ConvertStrToUint32(portInfo.portId, portId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to convert portId from string to uint32_t, portId=" << portInfo.portId;
            continue;
        }
        ports_.insert(portId);
        if (portInfo.remoteSlotId.empty() || portInfo.remoteSlotId == "-") {
            continue;
        }

        ChipId peerChipId{};
        ret = ubse::utils::ConvertStrToUint32(portInfo.remoteChipId, peerChipId);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to convert remoteChipId from string to uint32_t, remoteChipId="
                          << portInfo.remoteChipId;
            continue;
        }
        rawPortInfos_.push_back({portId, portInfo.remoteSlotId, peerChipId});
    }
}

std::set<std::pair<NodeId, SocketId>> SchedulerSocketInfo::ResolveRawPorts(
    const std::map<std::string, std::map<ChipId, SocketId>>& socketToChip) const
{
    std::set<std::pair<NodeId, SocketId>> resolved;
    for (const auto& raw : rawPortInfos_) {
        auto chipMapIt = socketToChip.find(raw.remoteSlotId);
        if (chipMapIt == socketToChip.end()) {
            continue;
        }
        auto socketIt = chipMapIt->second.find(raw.remoteChipId);
        if (socketIt == chipMapIt->second.end()) {
            continue;
        }
        resolved.insert({raw.remoteSlotId, socketIt->second});
    }
    return resolved;
}

} // namespace ubse::mem::scheduler
