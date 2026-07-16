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

#ifndef UBSE_MEM_SCHEDULER_FILTER_H
#define UBSE_MEM_SCHEDULER_FILTER_H

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "../ubse_mem_scheduler_account_manager.h"
#include "../ubse_mem_scheduler_node_manager.h"
#include "../ubse_mem_scheduler_request.h"
#include "../ubse_mem_types.h"
#include "../ubse_noncopyable.h"

namespace ubse::mem::scheduler {

class SchedulerFilter {
public:
    SchedulerFilter() = default;
    virtual ~SchedulerFilter() = default;

    /**
     * @brief 执行过滤
     * @param nodes    [IN/OUT] 候选节点列表, 不合格节点从中移除
     * @param nodeInfo 节点信息管理器 (只读)
     * @param account  账户管理器 (只读)
     * @param request  请求参数 (包含所有运行时数据)
     * @return UBSE_OK 继续, 其他值终止过滤链
     */
    virtual UbseResult FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                   const SchedulerAccountManager& account, const SchedulerRequest& request) = 0;

    [[nodiscard]] virtual std::string GetName() const = 0;

    // ---- 日志收集接口 (FilterManager 调用) ----
    struct RejectRecord {
        std::string nodeId;
        std::string reason;
    };
    [[nodiscard]] const std::vector<RejectRecord>& GetRejectedNodes() const
    {
        return rejectNodes_;
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
        rejectNodes_.clear();
        warnMsgs_.clear();
        errorMsg_.clear();
    }

protected:
    // 按谓词删除 nodes 中的匹配元素，返回删除数量
    template <typename Predicate>
    size_t EraseNodesIf(std::vector<NodeInfo>& nodes, Predicate pred)
    {
        auto it = std::remove_if(nodes.begin(), nodes.end(), std::move(pred));
        auto count = static_cast<size_t>(std::distance(it, nodes.end()));
        nodes.erase(it, nodes.end());
        return count;
    }

    // 按谓词删除 socketInfos 中的匹配元素，返回删除数量
    template <typename Predicate>
    size_t EraseSocketsIf(std::vector<SocketInfo>& socketInfos, Predicate pred)
    {
        auto it = std::remove_if(socketInfos.begin(), socketInfos.end(), std::move(pred));
        auto count = static_cast<size_t>(std::distance(it, socketInfos.end()));
        socketInfos.erase(it, socketInfos.end());
        return count;
    }

    // 删除 socketInfos 为空的 node，返回删除数量
    size_t RemoveEmptyNodes(std::vector<NodeInfo>& nodes)
    {
        auto it = std::remove_if(nodes.begin(), nodes.end(), [](const NodeInfo& n) { return n.socketInfos.empty(); });
        auto count = static_cast<size_t>(std::distance(it, nodes.end()));
        nodes.erase(it, nodes.end());
        return count;
    }

    void RecordReject(const std::string& nodeId, std::string reason)
    {
        rejectNodes_.push_back({nodeId, std::move(reason)});
    }
    void RecordWarning(std::string msg)
    {
        warnMsgs_.push_back(std::move(msg));
    }
    void RecordError(std::string msg)
    {
        errorMsg_ = std::move(msg);
    }

private:
    std::vector<RejectRecord> rejectNodes_;
    std::vector<std::string> warnMsgs_;
    std::string errorMsg_;
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_FILTER_H
