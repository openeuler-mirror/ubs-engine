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

#include "ubse_mem_scheduler_lend_count_filter.h"
#include "ubse_math_util.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {

UbseResult LendCountFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                        const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto requestSizeMb = request.requestSize_ / ONE_M;

    for (auto& node : nodes) {
        auto currentNode = nodeInfo.GetNodeInfo(node.nodeId);
        if (!currentNode) {
            RecordWarning(std::string("GetNodeInfo failed, node=") + node.nodeId);
            node.socketInfos.clear();
            continue;
        }

        auto blockSize = currentNode->GetBlockSize();
        if (blockSize == 0) {
            node.socketInfos.clear();
            continue;
        }
        uint32_t requiredBlocks = (requestSizeMb + blockSize - 1) / blockSize;

        auto isOverMaxTimes = [&](const SocketInfo& socketInfo) {
            auto* socket = nodeInfo.GetSocketInfo(node.nodeId, socketInfo.socketId);
            if (socket == nullptr) {
                RecordWarning(std::string("GetSocketInfo failed, node=") + node.nodeId +
                              ", socket=" + std::to_string(socketInfo.socketId));
                return true;
            }
            uint64_t lentCount = socket->GetLentCount();
            uint32_t maxExportTimes = socket->GetMaxExportTimes();
            uint64_t total = 0;
            if (!ubse::utils::SafeAdd(lentCount, static_cast<uint64_t>(requiredBlocks), total)) {
                RecordReject(node.nodeId, std::string("socket=") + std::to_string(socketInfo.socketId) +
                                              " requestSize + lentCount overflow");
                return true;
            }
            if (total > static_cast<uint64_t>(maxExportTimes)) {
                RecordReject(node.nodeId, std::string("socket=") + std::to_string(socketInfo.socketId) +
                                              " lentCount=" + std::to_string(lentCount) +
                                              " + blocks=" + std::to_string(requiredBlocks) +
                                              " > MAX=" + std::to_string(maxExportTimes));
                return true;
            }
            return false;
        };
        EraseSocketsIf(node.socketInfos, isOverMaxTimes);
    }

    RemoveEmptyNodes(nodes);

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
