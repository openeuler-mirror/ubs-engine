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

#include "ubse_mem_scheduler_config_consistency_filter.h"

namespace ubse::mem::scheduler {

UbseResult ConfigConsistencyFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                                const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    bool foundFirst = false;
    nodeController::UbseAllocator firstAllocator = nodeController::UbseAllocator::BUDDY_HIGHMEM;
    uint32_t firstBlockSize = 0;

    for (const auto& node : nodes) {
        auto nodePtr = nodeInfo.GetNodeInfo(node.nodeId);
        if (!nodePtr) {
            RecordWarning(std::string("GetNodeInfo failed, node=") + node.nodeId);
            return UBSE_SCHEDULER_ERROR_INVAL;
        }
        if (!nodePtr->IsLender()) {
            continue;
        }
        auto allocator = nodePtr->GetAllocator();
        auto blockSize = nodePtr->GetBlockSize();
        if (!foundFirst) {
            firstAllocator = allocator;
            firstBlockSize = blockSize;
            foundFirst = true;
        } else {
            if (allocator != firstAllocator) {
                RecordError(std::string("allocator mismatch on lender node: ") + node.nodeId +
                            std::string(", allocator=") + std::to_string(static_cast<int>(allocator)) +
                            std::string(", expected=") + std::to_string(static_cast<int>(firstAllocator)));
                return UBSE_ERROR_CONF_INVALID;
            }
            if (blockSize != firstBlockSize) {
                RecordError(std::string("blockSize mismatch on lender node: ") + node.nodeId +
                            std::string(", blockSize=") + std::to_string(blockSize) + std::string(", expected=") +
                            std::to_string(firstBlockSize));
                return UBSE_ERROR_CONF_INVALID;
            }
        }
    }

    if (!foundFirst) {
        return UBSE_OK;
    }
    if (firstBlockSize < 4 || firstBlockSize > 4096 || firstBlockSize % 2 != 0) {
        RecordError(std::string("blockSize invalid: ") + std::to_string(firstBlockSize) +
                    std::string(", range [4, 4096], must be multiple of 2"));
        return UBSE_ERROR_CONF_INVALID;
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
