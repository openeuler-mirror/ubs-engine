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

#ifndef UBSE_MEM_SCHEDULER_NODE_STATE_FILTER_H
#define UBSE_MEM_SCHEDULER_NODE_STATE_FILTER_H

#include "ubse_mem_scheduler_filter.h"

namespace ubse::mem::scheduler {

class NodeStateFilter : public SchedulerFilter {
public:
    UbseResult FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                           const SchedulerAccountManager& account, const SchedulerRequest& request) override;
    [[nodiscard]] std::string GetName() const override
    {
        return "NodeStateFilter";
    }
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_FILTER_NODE_STATE_H
