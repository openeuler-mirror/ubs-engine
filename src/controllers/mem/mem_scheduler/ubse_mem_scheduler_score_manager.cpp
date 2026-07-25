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
#include "ubse_mem_scheduler_score_manager.h"

#include <algorithm>

#include "ubse_logger.h"

#include "scheduler_score/ubse_mem_scheduler_balance_score.h"
#include "scheduler_score/ubse_mem_scheduler_borrow_reliability_score.h"
#include "scheduler_score/ubse_mem_scheduler_divide_numa_score.h"
#include "scheduler_score/ubse_mem_scheduler_latency_score.h"
#include "scheduler_score/ubse_mem_scheduler_region_balance_score.h"
#include "scheduler_score/ubse_mem_scheduler_reliability_balance_score.h"
#include "scheduler_score/ubse_mem_scheduler_share_reliability_score.h"

namespace ubse::mem::scheduler {

UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

SchedulerScoreManager::SchedulerScoreManager(SchedulerNodeManager* node, SchedulerAccountManager* account)
    : node_(node),
      account_(account)
{
}

UbseResult SchedulerScoreManager::Init()
{
    RegisterScore(std::make_unique<LatencyScore>());
    RegisterScore(std::make_unique<RegionBalanceScore>());
    RegisterScore(std::make_unique<BalanceScore>());
    RegisterScore(std::make_unique<ReliabilityBalanceScore>());
    RegisterScore(std::make_unique<BorrowReliabilityScore>());
    RegisterScore(std::make_unique<ShareReliabilityScore>());
    RegisterScore(std::make_unique<DivideNumaScore>());
    UBSE_LOG_INFO << "Register scores: " << scoreMap_.size();
    return UBSE_OK;
}

void SchedulerScoreManager::RegisterScore(std::unique_ptr<SchedulerScore> score)
{
    if (!score) {
        return;
    }
    auto name = score->GetName();
    scoreMap_[name] = score.get();
    ownedScores_.push_back(std::move(score));
}

SchedulerScore* SchedulerScoreManager::FindScoreByName(const std::string& name) const
{
    auto it = scoreMap_.find(name);
    return (it != scoreMap_.end()) ? it->second : nullptr;
}

double SchedulerScoreManager::GetWeightFor(const std::string& name, const ScoreWeights& weights) const
{
    if (name == "LatencyScore") {
        return weights.wLatency;
    }
    if (name == "RegionBalanceScore") {
        return weights.wRegionBalance;
    }
    if (name == "BalanceScore") {
        return weights.wBalance;
    }
    if (name == "ReliabilityBalanceScore") {
        return weights.wReliability;
    }
    if (name == "BorrowReliabilityScore") {
        return weights.wReliability;
    }
    if (name == "ShareReliabilityScore") {
        return weights.wReliability;
    }
    if (name == "DivideNumaScore") {
        return weights.wDivideNuma;
    }
    return 0.0;
}

UbseResult SchedulerScoreManager::ComputeWeightedScores(const std::vector<NodeInfo>& nodes,
                                                        const SchedulerRequest& request,
                                                        std::vector<ScoredNode>& results)
{
    ScoreWeights weights = request.weights_;
    size_t totalSockets = 0;
    for (const auto& node : nodes) {
        totalSockets += node.socketInfos.size();
    }
    results.resize(totalSockets);
    size_t idx = 0;
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            results[idx].nodeId = node.nodeId;
            results[idx].socketId = socketInfo.socketId;
            results[idx].totalCost = 0.0;
            ++idx;
        }
    }

    for (const auto& name : request.scoreNames_) {
        auto* scorer = FindScoreByName(name);
        if (!scorer) {
            UBSE_LOG_WARN << "[ScoreManager] scorer not registered: " << name << ", skipped";
            continue;
        }
        std::vector<double> componentScores;
        componentScores.resize(totalSockets, 0.0);
        UbseResult ret = scorer->ScoreNodes(nodes, *node_, *account_, request, componentScores);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[ScoreManager] scorer failed: " << name << ", reason: " << scorer->GetErrorMsg()
                           << ", ret=" << static_cast<int>(ret);
            scorer->ClearLogRecords();
            return ret;
        }

        for (const auto& rec : scorer->GetScoreRecords()) {
            UBSE_LOG_INFO << "[" << scorer->GetName() << "] node=" << rec.nodeId << " " << rec.detail;
        }

        double w = GetWeightFor(name, weights);
        for (size_t i = 0; i < totalSockets; ++i) {
            results[i].componentScores.push_back(componentScores[i]);
            results[i].totalCost += w * componentScores[i];
        }

        UBSE_LOG_INFO << "[" << scorer->GetName() << "] weight=" << w << ", scored " << totalSockets << " nodes";
        scorer->ClearLogRecords();
    }

    return UBSE_OK;
}

UbseResult SchedulerScoreManager::ScoreAndRank(const std::vector<NodeInfo>& nodes, const SchedulerRequest& request,
                                               std::vector<ScoredNode>& results, size_t topK)
{
    std::vector<ScoredNode> scoredNodes;
    auto ret = ComputeWeightedScores(nodes, request, scoredNodes);
    if (ret != UBSE_OK) {
        return ret;
    }
    if (scoredNodes.empty()) {
        return UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND;
    }

    std::sort(scoredNodes.begin(), scoredNodes.end(),
              [](const ScoredNode& a, const ScoredNode& b) { return a.totalCost < b.totalCost; });

    size_t k = (topK == 0 || topK > scoredNodes.size()) ? scoredNodes.size() : topK;
    results.assign(scoredNodes.begin(), scoredNodes.begin() + k);

    UBSE_LOG_INFO << "[ScoreManager] ranked " << results.size() << " nodes, best cost=" << results.front().totalCost;
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
