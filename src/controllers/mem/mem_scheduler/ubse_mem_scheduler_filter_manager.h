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
#ifndef UBSE_MEM_SCHEDULER_FILTER_MANAGER_H
#define UBSE_MEM_SCHEDULER_FILTER_MANAGER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ubse_mem_scheduler_account_manager.h"
#include "ubse_mem_scheduler_node_manager.h"
#include "ubse_mem_scheduler_request.h"
#include "ubse_mem_types.h"
#include "ubse_noncopyable.h"
#include "scheduler_filter/ubse_mem_scheduler_filter.h"

namespace ubse::mem::scheduler {

class SchedulerFilterManager : public Noncopyable {
public:
    explicit SchedulerFilterManager(SchedulerNodeManager* node, SchedulerAccountManager* account)
        : node_(node),
          account_(account)
    {
    }
    ~SchedulerFilterManager();

    UbseResult Init();
    void RegisterFilter(std::unique_ptr<SchedulerFilter> filter);

    UbseResult FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerRequest& request);

    UbseResult FilterByNames(std::vector<NodeInfo>& nodes, const std::vector<std::string>& filterNames,
                             const SchedulerRequest& request);

    SchedulerFilter* FindFilterByName(const std::string& name) const;

private:
    std::unordered_map<std::string, SchedulerFilter*> filterMap_;
    std::vector<std::unique_ptr<SchedulerFilter>> ownedFilters_;
    SchedulerNodeManager* node_;
    SchedulerAccountManager* account_;
};

} // namespace ubse::mem::scheduler
#endif // UBSE_MEM_SCHEDULER_FILTER_MANAGER_H
