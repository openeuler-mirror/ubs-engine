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
#ifndef UBSE_MEM_SCHEDULER_NODE_INFO_H
#define UBSE_MEM_SCHEDULER_NODE_INFO_H

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_types.h"
#include "scheduler_cache/ubse_mem_scheduler_node_info.h"

#include "ubse_noncopyable.h"

namespace ubse::mem::scheduler {

class SchedulerAccountManager;

class SchedulerNodeManager : Noncopyable {
public:
    SchedulerNodeManager() = default;
    ~SchedulerNodeManager() = default;

    std::vector<NodeInfo> GetAllNodes();
    UbseResult UpdateNodeInfo(const UbseNodeInfo& nodeInfo);
    UbseResult UpdateAllLinkInfo(const std::unordered_map<NodeId, UbseNodeInfo>& nodeMap);
    UbseResult UpdateAllNumaMemInfo(const std::unordered_map<NodeId, UbseNodeInfo>& nodeMap);
    void UpdateNumaMemInfo(const UbseNumaInfo& numaInfo, UbseAllocator allocator, uint32_t pmdMapping,
                           SchedulerNumaInfo* numaPtr);

    // Filter support
    SchedulerNodeInfo* GetNodeInfo(const NodeId& nodeId) const;
    SchedulerSocketInfo* GetSocketInfo(const NodeId& nodeId, SocketId socketId) const;
    SchedulerNumaInfo* GetNumaInfo(const NodeId& nodeId, NumaId numaId) const;
    SocketId GetSocketIdByNumaId(const NodeId& nodeId, NumaId numaId) const;
    bool IsFullyConnected() const;
    std::set<std::pair<NodeId, SocketId>> GetReachablePeers(const NodeId& nodeId) const;
    std::set<std::pair<NodeId, SocketId>> GetReachablePeers(const NodeId& nodeId, SocketId socketId) const;
    uint64_t GetMaxBorrowSize() const;
    void InitPageSize();
    void InitRadiusConfig();
    void InitLenderBalance();

    uint16_t GetRadiusBorrow() const
    {
        return radiusBorrow_;
    }
    uint16_t GetRadiusLender() const
    {
        return radiusLender_;
    }
    bool GetLenderBalance() const
    {
        return lenderBalance_;
    }

    void UpdateProviderNodeList(const NodeId& nodeId, const std::string& hostName);
    void UpdateGroupNodeList(const NodeId& nodeId, const std::string& hostName);
    const std::set<NodeId>& GetProviderNodeList() const;
    const std::set<NodeId>& GetGroupNodes(const NodeId& nodeId) const;

    void Clear();

public:
    // 非拥有语义：account_ 的生命周期由 SchedulerImpl 管理。
    // C++ 成员析构顺序为逆序：scoreManager_ → filterManager_ → nodeInfo_ → account_。
    // SchedulerAccountManager 析构不访问 node_，因此安全。
    void SetAccountManager(SchedulerAccountManager* account)
    {
        account_ = account;
    }
    SchedulerAccountManager* GetAccountManager() const
    {
        return account_;
    }

private:
    bool InitOneNodeData(const UbseNodeInfo& nodeInfo);
    void UpdateNodeInfoCache(const NodeId& nodeId);

    std::unordered_map<NodeId, std::unique_ptr<SchedulerNodeInfo>> nodeMap_;
    std::vector<NodeInfo> nodeInfos_;
    std::set<NodeId> providerNodes_;
    std::vector<std::set<NodeId>> groupNodes_;

    // chip 和 socket的双向映射
    std::map<NodeId, std::map<ChipId, SocketId>> chipToSocket_;
    std::map<NodeId, std::map<SocketId, ChipId>> socketToChip_;
    SchedulerAccountManager* account_{nullptr};
    bool isPageSize64K_{false};
    bool lenderBalance_{false};
    uint16_t radiusBorrow_{UINT16_MAX};
    uint16_t radiusLender_{UINT16_MAX};
};

} // namespace ubse::mem::scheduler
#endif // UBSE_MEM_SCHEDULER_NODE_INFO_H
