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

#include "ubse_mem_scheduler_free_memory_filter.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {

UbseResult FreeMemoryFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                         const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto requestSize = request.requestSize_;
    auto highWatermarkOpt = request.GetParamOpt<size_t>("highWatermark");
    size_t highWatermark = highWatermarkOpt.value_or(MAX_PERCENT);

    for (auto& node : nodes) {
        if (requestSize == 0) {
            continue;
        }
        auto nodePtr = nodeInfo.GetNodeInfo(node.nodeId);
        if (nodePtr == nullptr) {
            RecordWarning(std::string("GetNodeInfo failed, node=") + node.nodeId);
            continue;
        }
        uint64_t blockSize = static_cast<uint64_t>(nodePtr->GetBlockSize()) * ONE_M;

        auto isFreeMemoryInsufficient = [&](const SocketInfo& socketInfo) {
            auto* socket = nodeInfo.GetSocketInfo(node.nodeId, socketInfo.socketId);
            if (socket == nullptr) {
                RecordWarning(std::string("GetSocketInfo failed, node=") + node.nodeId +
                              ", socket=" + std::to_string(socketInfo.socketId));
                return true;
            }
            uint64_t available = socket->GetAvailableLendSize(static_cast<uint64_t>(highWatermark), blockSize);
            if (available < requestSize) {
                RecordReject(node.nodeId, std::string("socket=") + std::to_string(socketInfo.socketId) +
                                              " available=" + std::to_string(available / ONE_M) + "MB" +
                                              " < requestSize=" + std::to_string(requestSize / ONE_M) + "MB" +
                                              ", highWatermark=" + std::to_string(highWatermark) +
                                              ", freeSize=" + std::to_string(socket->GetFreeSize() / ONE_M) + "MB" +
                                              ", lentSize=" + std::to_string(socket->GetLentSize() / ONE_M) + "MB");
                return true;
            }
            return false;
        };
        EraseSocketsIf(node.socketInfos, isFreeMemoryInsufficient);
    }

    RemoveEmptyNodes(nodes);

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
