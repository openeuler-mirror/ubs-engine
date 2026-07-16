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

#ifndef UBSE_MEM_SCHEDULER_SPECIFIED_LENDER_FILTER_H
#define UBSE_MEM_SCHEDULER_SPECIFIED_LENDER_FILTER_H

#include <set>
#include <unordered_map>

#include "ubse_mem_scheduler_filter.h"

namespace ubse::mem::scheduler {

class SpecifiedLenderFilter : public SchedulerFilter {
public:
    UbseResult FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                           const SchedulerAccountManager& account, const SchedulerRequest& request) override;
    [[nodiscard]] std::string GetName() const override
    {
        return "SpecifiedLenderFilter";
    }

private:
    UbseResult ValidateLenderConsistency(const std::vector<adapter_plugins::mmi::UbseMemLenderInfo>& lenderInfos,
                                         const SchedulerNodeManager& nodeInfo, SocketId& outSocketId,
                                         std::set<NumaId>& outTargetNumaIds,
                                         std::unordered_map<NumaId, uint64_t>& outNumaToSize);

    UbseResult ValidatePortId(const adapter_plugins::mmi::UbseMemLenderInfo& lender,
                              const SchedulerNodeManager& nodeInfo, SocketId socketId);

    void FilterByNodeId(std::vector<NodeInfo>& nodes, const NodeId& targetNodeId);
    void FilterBySocketId(std::vector<NodeInfo>& nodes, SocketId targetSocketId, const NodeId& lenderNodeId);
    void FilterByNumaIds(std::vector<NodeInfo>& nodes, const std::set<NumaId>& targetNumaIds,
                         const NodeId& lenderNodeId);
    UbseResult CheckNumaCapacity(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                 const std::unordered_map<NumaId, uint64_t>& numaToSize,
                                 const SchedulerRequest& request);
    static void CleanupEmptyNodes(std::vector<NodeInfo>& nodes);
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SPECIFIED_LENDER_FILTER_H
