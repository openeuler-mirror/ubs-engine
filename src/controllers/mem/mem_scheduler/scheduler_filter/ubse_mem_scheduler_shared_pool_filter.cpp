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

#include "ubse_mem_scheduler_shared_pool_filter.h"

#include "ubse_logger.h"
#include "ubse_math_util.h"

namespace ubse::mem::scheduler {
UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

namespace {
constexpr uint64_t BYTES_PER_GB = 1073741824;
} // namespace

UbseResult SharedPoolFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                         const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto requestSize = request.requestSize_;

    for (auto& node : nodes) {
        auto* nodePtr = nodeInfo.GetNodeInfo(node.nodeId);
        if (nodePtr == nullptr) {
            RecordWarning(std::string("GetNodeInfo failed, node=") + node.nodeId);
            continue;
        }

        uint64_t poolBytes = static_cast<uint64_t>(nodePtr->GetNodeMaxLendGb()) * BYTES_PER_GB;
        if (poolBytes == 0) {
            continue; // 未配置，不限制此节点
        }

        uint64_t totalLent = nodePtr->GetLentSize();
        uint64_t projected = 0;
        if (!ubse::utils::SafeAdd(totalLent, requestSize, projected)) {
            RecordReject(node.nodeId, "lentSize + requestSize overflow");
            node.socketInfos.clear();
            continue;
        }

        if (projected > poolBytes) {
            RecordReject(node.nodeId, std::string("totalLent=") + std::to_string(totalLent / BYTES_PER_GB) + "GB" +
                                          " + request=" + std::to_string(requestSize / BYTES_PER_GB) + "GB" + " = " +
                                          std::to_string(projected / BYTES_PER_GB) + "GB" +
                                          " > nodePool=" + std::to_string(poolBytes / BYTES_PER_GB) + "GB");
            node.socketInfos.clear();
        } else {
            UBSE_LOG_DEBUG << "[SharedPoolFilter] node=" << node.nodeId << " lent_gb=" << totalLent / BYTES_PER_GB
                           << " pool_gb=" << poolBytes / BYTES_PER_GB
                           << " avail_gb=" << (poolBytes - totalLent) / BYTES_PER_GB;
        }
    }

    RemoveEmptyNodes(nodes);

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
