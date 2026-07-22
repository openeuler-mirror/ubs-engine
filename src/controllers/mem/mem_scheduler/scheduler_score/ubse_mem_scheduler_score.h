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

#ifndef UBSE_MEM_SCHEDULER_SCORE_H
#define UBSE_MEM_SCHEDULER_SCORE_H

#include <string>
#include <vector>

#include "../ubse_mem_scheduler_account_manager.h"
#include "../ubse_mem_scheduler_node_manager.h"
#include "../ubse_mem_scheduler_request.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {

struct ScoredNode {
    NodeId nodeId;
    SocketId socketId{};
    std::vector<double> componentScores;
    double totalCost{0.0};
};

class SchedulerScore {
public:
    SchedulerScore() = default;
    virtual ~SchedulerScore() = default;

    virtual UbseResult ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                  const SchedulerAccountManager& account, const SchedulerRequest& request,
                                  std::vector<double>& scores) = 0;

    [[nodiscard]] virtual std::string GetName() const = 0;

    // ---- 日志收集接口 (ScoreManager 调用) ----
    struct ScoreRecord {
        std::string nodeId;
        std::string detail;
    };
    [[nodiscard]] const std::vector<ScoreRecord>& GetScoreRecords() const
    {
        return scoreRecords_;
    }
    [[nodiscard]] const std::vector<std::string>& GetWarnings() const
    {
        return warnMsgs_;
    }
    [[nodiscard]] const std::string& GetErrorMsg() const
    {
        return errorMsg_;
    }
    void ClearLogRecords()
    {
        scoreRecords_.clear();
        warnMsgs_.clear();
        errorMsg_.clear();
    }

protected:
    void RecordScore(const std::string& nodeId, const std::string& detail)
    {
        scoreRecords_.push_back({nodeId, detail});
    }
    void RecordWarning(const std::string& msg)
    {
        warnMsgs_.push_back(msg);
    }
    void RecordError(const std::string& msg)
    {
        errorMsg_ = msg;
    }

private:
    std::vector<ScoreRecord> scoreRecords_;
    std::vector<std::string> warnMsgs_;
    std::string errorMsg_;
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SCORE_H
