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

#ifndef UBSE_MEM_SCHEDULER_SUB_HEALTH_FILTER_H
#define UBSE_MEM_SCHEDULER_SUB_HEALTH_FILTER_H

#include "ubse_mem_scheduler_filter.h"

namespace ubse::mem::scheduler {

// 链路亚健康过滤插件（EXCLUDE 模式生效）。
// 剔除当前处于亚健康的 socket 对；socket 被全部剔除的 node 一并移除。
// fail-closed：IsSocketPairSubHealthy 返回 false（含查询失败/无数据）时视为健康，不剔除。
class SubHealthFilter : public SchedulerFilter {
public:
    UbseResult FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                           const SchedulerAccountManager& account, const SchedulerRequest& request) override;
    [[nodiscard]] std::string GetName() const override
    {
        return "SubHealthFilter";
    }
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SUB_HEALTH_FILTER_H
