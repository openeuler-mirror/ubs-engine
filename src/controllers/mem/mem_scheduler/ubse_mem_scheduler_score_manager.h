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

#ifndef UBSE_MEM_SCHEDULER_SCORE_MANAGER_H
#define UBSE_MEM_SCHEDULER_SCORE_MANAGER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ubse_noncopyable.h"
#include "scheduler_score/ubse_mem_scheduler_score.h"
#include "scheduler_score/ubse_mem_scheduler_score_weight.h"

namespace ubse::mem::scheduler {

class SchedulerScoreManager : public Noncopyable {
public:
    explicit SchedulerScoreManager(SchedulerNodeManager* node, SchedulerAccountManager* account);
    ~SchedulerScoreManager() = default;

    UbseResult Init();
    void RegisterScore(std::unique_ptr<SchedulerScore> score);

    UbseResult ScoreAndRank(const std::vector<NodeInfo>& nodes, const SchedulerRequest& request,
                            std::vector<ScoredNode>& results, size_t topK = 1);

    SchedulerScore* FindScoreByName(const std::string& name) const;

private:
    UbseResult ComputeWeightedScores(const std::vector<NodeInfo>& nodes, const SchedulerRequest& request,
                                     std::vector<ScoredNode>& results);

    double GetWeightFor(const std::string& name, const ScoreWeights& weights) const;

    std::unordered_map<std::string, SchedulerScore*> scoreMap_;
    std::vector<std::unique_ptr<SchedulerScore>> ownedScores_;
    SchedulerNodeManager* node_;
    SchedulerAccountManager* account_;
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SCORE_MANAGER_H
