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

#include "ubse_mem_scheduler_max_lent_size_filter.h"
#include "ubse_math_util.h"

namespace ubse::mem::scheduler {

UbseResult MaxLentSizeFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                          const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto requestSize = request.requestSize_;

    for (auto& node : nodes) {
        auto isOverLentLimit = [&](const SocketInfo& socketInfo) {
            auto* socketInfoPtr = nodeInfo.GetSocketInfo(node.nodeId, socketInfo.socketId);
            if (!socketInfoPtr) {
                RecordWarning(std::string("GetSocketInfo failed, node=") + node.nodeId +
                              ", socket=" + std::to_string(socketInfo.socketId));
                return true;
            }
            auto maxLentSize = socketInfoPtr->GetMaxLentSize();
            uint64_t totalLent = 0;
            if (!ubse::utils::SafeAdd(requestSize, socketInfoPtr->GetLentSize(), totalLent)) {
                RecordReject(node.nodeId, std::string("requestSize + lentSize overflow, socket=") +
                                              std::to_string(socketInfo.socketId));
                return true;
            }
            if (maxLentSize != 0 && maxLentSize < totalLent) {
                RecordReject(node.nodeId, std::string("socket=") + std::to_string(socketInfo.socketId) +
                                              " maxLentSize=" + std::to_string(maxLentSize) +
                                              " < requestSize=" + std::to_string(requestSize) +
                                              " + lentSize=" + std::to_string(socketInfoPtr->GetLentSize()));
                return true;
            }
            return false;
        };
        EraseSocketsIf(node.socketInfos, isOverLentLimit);
    }

    RemoveEmptyNodes(nodes);

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
