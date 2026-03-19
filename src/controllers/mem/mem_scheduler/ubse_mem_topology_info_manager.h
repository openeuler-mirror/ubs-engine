/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef UBSE_MEM_TOPOLOGY_INFO_MANAGER_H
#define UBSE_MEM_TOPOLOGY_INFO_MANAGER_H
#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "mem_pool_strategy.h"
#include "ubse_mem_constants.h"
#include "ubse_mem_meta_data.h"
#include "ubse_mem_types.h"
#include "ubse_node_controller.h"
#include "ubse_mem_configuration.h"

namespace ubse::mem::strategy {
struct SocketCnaTopoInfo {
    std::string importNodeIdSocketId{};
    std::string exportNodeIdSocketId{};
};

struct UbseMemCoordinateDesc {
    UbseMemCoordinateDesc() = default;
    std::set<NodeIndex> intersection1; // 创建一个新的set用于存储交集的结果，确定其中一条轴上的点
    std::set<NodeIndex> intersection2; // 创建一个新的set用于存储交集的结果，确定另外一条轴上的点
    std::array<uint8_t, MAX_X_NODE_NUM> nodeNumOnX = {0};                         // 对应轴上已放置节点个数
    std::array<uint8_t, MAX_X_NODE_NUM> nodeNumOnY = {0};                         // 对应轴上已放置节点个数
    std::array<std::array<NodeIndex, MAX_X_NODE_NUM>, MAX_X_NODE_NUM> coordinate; // 保存每个坐标上所在的节点的index
    uint32_t totalLocatedHostsNum = 0; // 坐标系中已放置节点的个数
};

/*
 * 维护每个numa上的状态
 *
 */
class UbseMemTopologyInfoManager {
public:
    UbseMemTopologyInfoManager(const UbseMemTopologyInfoManager &other) = delete;
    UbseMemTopologyInfoManager(UbseMemTopologyInfoManager &&other) = delete;
    UbseMemTopologyInfoManager &operator=(const UbseMemTopologyInfoManager &other) = delete;
    UbseMemTopologyInfoManager &operator=(UbseMemTopologyInfoManager &&other) noexcept = delete;

    inline static UbseMemTopologyInfoManager &GetInstance()
    {
        static UbseMemTopologyInfoManager instance;
        return instance;
    }

    UbseResult NodesInit(const std::vector<strategy::NodeDataWithNumaInfo> &nodeDatas); // 节点上线增加init

    void UpdateNodeMesgInfo(
        const std::vector<strategy::NodeDataWithNumaInfo> &nodeDatas); // 更新节点socket信息和numa信息

    // 算法参数的填充
    bool FillStrategyParam(tc::rs::mem::StrategyParam &strategyParam);
    bool SetAvailNumas(tc::rs::mem::StrategyParam &strategyParam,
                       const std::vector<std::shared_ptr<MemNumaInfo>> &numaList);
    bool SetNumaLatencies(tc::rs::mem::StrategyParam &strategyParam);
    bool SetNumaMemCapacities(tc::rs::mem::StrategyParam &strategyParam,
                              const std::vector<std::shared_ptr<MemNumaInfo>> &numaList);
    bool SetMaxMemParam(tc::rs::mem::StrategyParam &strategyParam,
                        const std::vector<std::shared_ptr<MemNumaInfo>> &numaList);
    bool SetMemOutHardLimit(tc::rs::mem::StrategyParam &strategyParam,
                            std::vector<std::shared_ptr<MemNumaInfo>> numaList);

    UbseResult GetNodePoolMemSize(const NodeId &nodeId, uint64_t &nodePoolMemSize,
                                  const std::vector<std::shared_ptr<MemNumaInfo>> &numaList);
    UbseResult GetActualNodeMemTotal(const NodeId &nodeId, uint64_t &nodeNumaMemTotal,
                                     const std::vector<std::shared_ptr<MemNumaInfo>> &numaList);
    UbseResult GetAttachNodeId(std::string &borrowNodeId, const std::string &exportNodeId, int exportSocketId,
                               uint32_t &attachSocketId);
    bool TransferTopoToCoordinate(tc::rs::mem::StrategyParam &strategyParam);
    bool UbseMemTransTopoToNeighborSet(std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> &neighborNodes,
                                       std::set<uint16_t> &nodeIndexList);
    bool GenerateCoordinate(std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> &neighborNodes,
                            tc::rs::mem::StrategyParam &strategyParam, const std::set<uint16_t> &nodeIndexList);
    void FillTopoNumaInfoByNumaLoc(const ubse::nodeController::UbseNumaInfo &numaInfo,
                                   ubse::nodeController::UbseAllocator allocator, uint32_t pmd_mapping);

    // 自规划topo和实际的topo的转换
    NodeIndex NodeIdToIndex(const NodeId &nodeId);
    NodeId NodeIndexToId(NodeIndex nodeIndex);
    std::shared_ptr<MemNodeInfo> GetNodeInfoById(const NodeId &nodeId);

    bool ConvertNumaIndex(const UbseMemNumaIndexLoc &ubseMemNumaIndexLoc, UbseMemNumaLoc &numaLoc);
    bool ConvertNumaLoc(const UbseMemNumaLoc &numaLoc, UbseMemNumaIndexLoc &ubseMemNumaIndexLoc);

    // 查询数据
    std::vector<std::shared_ptr<MemNumaInfo>> GetAllNumaInfo(const NodeId &nodeId);
    UbseResult GetSocketCnaInfo(const UbseMemNumaLoc &memIdLocBorrow, const UbseMemNumaLoc &memIdLocLend,
                                SocketCnaTopoInfo &socketCnaTopoInfo);

    std::shared_ptr<MemNumaInfo> GetNumaInfo(const NodeId &nodeId, NumaId numaId);

    UbseResult GetMemNumaLoc(const NodeId &nodeId, NumaId numaId, UbseMemNumaLoc &memIdLoc);

    UbseResult GetSocketTotalLentMem(const NodeId &nodeId, int socketId, uint64_t &socketTotalLentMem);

    UbseResult GetSocketTotalLentTimes(const NodeId &nodeId, int socketId, uint32_t &socketTotalLentTime);

    inline int32_t NumHosts()
    {
        return mNodeDataList_.size();
    }

    inline void ChangeNodeStatus(const NodeId &nodeId, bool status)
    {
        for (auto &nodeData : mNodeDataList_) {
            if (nodeData.nodeId == nodeId) {
                nodeData.status = status;
            }
        }
    }

    inline bool CheckIsNewNode(const NodeId &nodeId)
    {
        for (const auto &nodeData : mNodeDataList_) {
            if (nodeData.nodeId == nodeId) {
                return false;
            }
        }
        return true;
    }

    inline bool CheckNodeStatus(const NodeId &nodeId)
    {
        for (const auto &nodeData : mNodeDataList_) {
            if (nodeData.nodeId == nodeId) {
                return nodeData.status;
            }
        }
        return false;
    }

    void Clear();

private:
    void UpdateNumaMemoryInfo(std::shared_ptr<MemNumaInfo> numaPtr, const UbseNumaInfo &numaInfo, UbseAllocator allocator, uint32_t pmd_mapping);
    void HandleHugeTlbPud(const UbseNumaInfo &numaInfo, long double ratio, uint64_t &memTotal, uint64_t &memUsed, uint64_t &memFree);
    void HandleHugeTlbPmd(const UbseNumaInfo &numaInfo, long double ratio, uint64_t &memTotal, uint64_t &memUsed, uint64_t &memFree);
    void HandleDefault(const UbseNumaInfo &numaInfo, long double ratio, uint64_t &memTotal, uint64_t &memUsed, uint64_t &memFree);
    void LogNumaInfo(const UbseNumaInfo &numaInfo, UbseAllocator allocator, uint32_t pmd_mapping);

    std::vector<NodeData> mNodeDataList_{};
    std::unordered_map<NodeId, std::shared_ptr<MemNodeInfo>> mNodeIdMap_{};
    std::unordered_map<NodeIndex, std::shared_ptr<MemNodeInfo>> mNodeIndexMap_{};
    NodeIndex mCurNodeIndex_{0};
    GlobalNumaIndex mCurGlobalNumaIndex_{0};

    std::unordered_map<UbseMemNumaIndexLoc, UbseMemNumaLoc> mNumaLoc2IndexMap_{};
    std::unordered_map<UbseMemNumaLoc, UbseMemNumaIndexLoc> mNumaLoc2IdMap_{};
    std::unordered_map<std::string, std::string> mNodeId2HostName_{};

private:
    UbseMemTopologyInfoManager() = default;
    bool AllocOneNode(const NodeData &nodeData);
};
} // namespace ubse::mem::strategy
#endif