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
#include "ubse_mem_scheduler_filter_manager.h"

#include "ubse_error.h"
#include "ubse_logger.h"

#include "scheduler_filter/ubse_mem_scheduler_config_consistency_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_free_memory_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_group_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_lend_count_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_lender_role_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_max_lent_size_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_node_state_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_provider_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_radius_borrow_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_radius_lender_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_region_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_requested_providers_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_role_conflict_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_socket_affinity_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_specified_lender_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_specified_link_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_topo_reachability_filter.h"

namespace ubse::mem::scheduler {
UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

SchedulerFilterManager::~SchedulerFilterManager() = default;

UbseResult SchedulerFilterManager::Init()
{
    RegisterFilter(std::make_unique<ConfigConsistencyFilter>());
    RegisterFilter(std::make_unique<RoleConflictFilter>());
    RegisterFilter(std::make_unique<LenderRoleFilter>());
    RegisterFilter(std::make_unique<SocketAffinityFilter>());
    RegisterFilter(std::make_unique<GroupFilter>());
    RegisterFilter(std::make_unique<ProviderFilter>());
    RegisterFilter(std::make_unique<NodeStateFilter>());
    RegisterFilter(std::make_unique<RequestedProvidersFilter>());
    RegisterFilter(std::make_unique<LendCountFilter>());
    RegisterFilter(std::make_unique<SpecifiedLenderFilter>());
    RegisterFilter(std::make_unique<SpecifiedLinkFilter>());
    RegisterFilter(std::make_unique<TopoReachabilityFilter>());
    RegisterFilter(std::make_unique<MaxLentSizeFilter>());
    RegisterFilter(std::make_unique<RadiusBorrowFilter>());
    RegisterFilter(std::make_unique<RadiusLenderFilter>());
    RegisterFilter(std::make_unique<RegionFilter>());
    RegisterFilter(std::make_unique<FreeMemoryFilter>());
    UBSE_LOG_INFO << "Register filters: " << filterMap_.size();

    return UBSE_OK;
}

void SchedulerFilterManager::RegisterFilter(std::unique_ptr<SchedulerFilter> filter)
{
    if (!filter) {
        return;
    }

    const auto& name = filter->GetName();
    filterMap_[name] = filter.get();
    ownedFilters_.push_back(std::move(filter));
}

SchedulerFilter* SchedulerFilterManager::FindFilterByName(const std::string& name) const
{
    auto it = filterMap_.find(name);
    return (it != filterMap_.end()) ? it->second : nullptr;
}

UbseResult SchedulerFilterManager::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerRequest& request)
{
    return FilterByNames(nodes, request.filterNames_, request);
}

UbseResult SchedulerFilterManager::FilterByNames(std::vector<NodeInfo>& nodes,
                                                 const std::vector<std::string>& filterNames,
                                                 const SchedulerRequest& request)
{
    UBSE_LOG_INFO << "[FilterChain] start"
                  << ", importNode=" << request.requestNodeId_ << ", requestSize=" << request.requestSize_
                  << ", initialNodes=" << nodes.size();

    for (const auto& name : filterNames) {
        SchedulerFilter* filter = FindFilterByName(name);
        if (!filter) {
            UBSE_LOG_WARN << "[FilterChain] filter '" << name << "' not registered, skipped";
            continue;
        }

        UbseResult ret = UBSE_OK;
        try {
            ret = filter->FilterNodes(nodes, *node_, *account_, request);
        } catch (const std::exception& e) {
            UBSE_LOG_ERROR << "[FilterChain] exception at " << filter->GetName() << ", error=" << e.what();
            filter->ClearLogRecords();
            return UBSE_SCHEDULER_ERROR_INVAL;
        }

        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[FilterChain] FAILED at " << filter->GetName() << ", reason: " << filter->GetErrorMsg()
                           << ", ret=" << static_cast<int>(ret);
            filter->ClearLogRecords();
            return ret;
        }

        for (const auto& rec : filter->GetWarnings()) {
            UBSE_LOG_WARN << "[" << filter->GetName() << "] warning: " << rec;
        }

        for (const auto& rec : filter->GetRejectedNodes()) {
            UBSE_LOG_WARN << "[" << filter->GetName() << "] node=" << rec.nodeId << ", removed: " << rec.reason;
        }

        filter->ClearLogRecords();
    }

    UBSE_LOG_INFO << "[FilterChain] finished, finalNodes=" << nodes.size() << ", requestName=" << request.name_;
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
