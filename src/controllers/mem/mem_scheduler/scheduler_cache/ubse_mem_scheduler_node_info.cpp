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

#include "ubse_mem_scheduler_node_info.h"
#include "ubse_mem_types.h"
#include "ubse_str_util.h"

namespace ubse::mem::scheduler {

UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

SchedulerNodeInfo::SchedulerNodeInfo(const UbseNodeInfo& nodeInfo)
{
    nodeId_ = nodeInfo.nodeId;
    hostName_ = nodeInfo.hostName;
    allocator_ = nodeInfo.allocator;
    pmdMapping_ = nodeInfo.pmdMapping;
    blockSize_ = nodeInfo.blockSize;
    isLender_ = nodeInfo.isLender;
    for (const auto& [_, numaInfo] : nodeInfo.numaInfos) {
        SchedulerSocketInfo* socketInfo;
        auto it = socketInfoMap_.find(numaInfo.socketId);
        if (it != socketInfoMap_.end()) {
            socketInfo = it->second.get();
        } else {
            auto [newIt, _] =
                socketInfoMap_.emplace(numaInfo.socketId, std::make_unique<SchedulerSocketInfo>(numaInfo.socketId));
            socketInfo = newIt->second.get();
            UBSE_LOG_INFO << "Create memSocketInfo successful, socketId=" << numaInfo.socketId;
        }
        socketInfo->Init(numaInfo);
        socketInfo->SetMaxExportTimes(nodeInfo.exportTotalTimes);
    }
    for (const auto& [_, cpuInfo] : nodeInfo.cpuInfos) {
        auto socketIter = socketInfoMap_.find(cpuInfo.socketId);
        if (socketIter != socketInfoMap_.end()) {
            ChipId chipId = 0;
            if (ubse::utils::ConvertStrToUint32(cpuInfo.chipId, chipId) == UBSE_OK) {
                socketIter->second->SetChipId(chipId);
            } else {
                UBSE_LOG_ERROR << "Failed to convert chipId, nodeId=" << nodeInfo.nodeId
                               << ", chipId=" << cpuInfo.chipId;
            }
        }
    }
    UBSE_LOG_INFO << "Create memNodeInfo successful, nodeId=" << nodeInfo.nodeId;
}

SchedulerSocketInfo* SchedulerNodeInfo::GetSocketInfo(SocketId socketId) const
{
    auto socketIt = socketInfoMap_.find(socketId);
    if (socketIt != socketInfoMap_.end()) {
        return socketIt->second.get();
    }
    return nullptr;
}

SchedulerNumaInfo* SchedulerNodeInfo::GetNumaInfo(SocketId socketId, NumaId numaId) const
{
    auto socketIter = socketInfoMap_.find(socketId);
    if (socketIter != socketInfoMap_.end()) {
        return socketIter->second->GetNumaInfo(numaId);
    }
    return nullptr;
}

NodeInfo SchedulerNodeInfo::GetNodeInfo() const
{
    NodeInfo nodeInfo;
    nodeInfo.nodeId = nodeId_;
    for (const auto& [_, socketInfo] : socketInfoMap_) {
        nodeInfo.socketInfos.emplace_back(socketInfo->GetSocketInfo());
    }
    return nodeInfo;
}

} // namespace ubse::mem::scheduler
