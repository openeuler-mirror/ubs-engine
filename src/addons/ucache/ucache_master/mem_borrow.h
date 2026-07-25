/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UCACHE_MEM_BORROW_H
#define UCACHE_MEM_BORROW_H

#include <ubse_node.h>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include "data_collect.h"

namespace ucache {
namespace mem_borrow {

using namespace ubse::nodeController;
using namespace ucache::data_collect;

class BorrowNodeStat {
public:
    BorrowNodeStat() = default;
    explicit BorrowNodeStat(BorrowStrategyRawData& borrowRawData);

    uint64_t GetUsedMem() const
    {
        return usedMem;
    }

    uint64_t GetFreeMem() const
    {
        return freeMem;
    }

    uint64_t GetPageCacheMem() const
    {
        return usedPagecache;
    }

    uint64_t GetFreeMemMin() const
    {
        return freeMemMin;
    }

    double GetBorrowedMemUsage() const
    {
        return borrowedMemUsage;
    }

    int GetPagecacheAppNums() const
    {
        return pagecacheAppNums;
    }

    std::string GetNodeId() const
    {
        return nodeId;
    }

    double GetScarcity() const
    {
        return scarcity;
    }

    void ReturnMem(uint64_t size);
    void LendMem(uint64_t size);
    void BorrowMem(uint64_t size);
    void RestoreMem(uint64_t size);
    void PrintNodeStat();

    std::map<int, MemInfo> GetRemoteNumaMemInfo();

    static std::map<std::string, BorrowNodeStat> nodeIdToNodeStatMap;
    static uint32_t InitBorrowNodeStat();
    static void ClearBorrowNodeStat();
    static void SortNodeByScarcity(std::vector<BorrowNodeStat*>& nodeStats);
    static void InsertBorrowNodeStat(BorrowNodeStat* nodeStat, std::vector<BorrowNodeStat*>& nodeStats);
    static void EraseBorrowNodeStat(BorrowNodeStat* nodeStat, std::vector<BorrowNodeStat*>& nodeStats);

private:
    // raw data
    std::string nodeId;
    int pagecacheAppNums;                     // pagecache紧缺型app数量
    uint64_t freeMemMin;                      // 节点空闲内存最小值
    MemInfo localMemInfo;                     // 节点本地内存信息
    std::map<int, MemInfo> remoteNumaMemInfo; // 节点远端numa内存信息

    // calculated data
    uint64_t usedMem = 0;          // 已使用内存
    uint64_t usedPagecache = 0;    // in bytes
    uint64_t freeMem = 0;          // in bytes，已减去节点空闲内存最小值
    double borrowedMemUsage = 0.0; // percentage
    double scarParam = 0.0;        // 内存稀缺度加权参数scarParam
    double scarcity = 0.0;         // 内存稀缺度

    void CalScarcity();
    void CalScarParam();
};

// 每个node的内存情况，借用策略执行时修改
struct BorrowMemInfoPerNode {
    std::map<int, uint64_t> numaNodeSize; // 每个numa node上还有多少内存
    uint64_t freeMem;                     // 当前还能借出多少
    uint64_t totalMem;                    // 总共可借用的内存
};

// 借用topo
class MemBorrowTopo {
public:
    MemBorrowTopo() = default;

    // 节点是否有远端内存
    bool HasRemoteMem(const std::string& nodeId);
    // 节点是否有借出内存
    bool HasLentMem(const std::string& nodeId);
    // 节点是否有借入内存
    bool HasBorrowedMem(const std::string& nodeId);
    // 节点向目标节点外是否还有借出内存
    bool HasLendMemExceptDst(const std::string& src, const std::string& dst);
    // 节点能否借出内存
    bool HasAvailableMemToBorrow(const std::string& nodeId);

    /* 删除一个借用关系
     * @from：入参，借出节点Id
     * @to：入参，借入节点Id
     */
    uint32_t DeleteNodeMemBorrowInfo(const std::string& from, const std::string& to);

    /* 增加一个借用关系
     * @from：入参，借出节点Id
     * @to：入参，借入节点Id
     */
    uint32_t AddNodeMemBorrowInfo(const std::string& from, const std::string& to);

    /* 查询可借用内存来源
     * @node：入参，需要借出内存的节点Id
     * @sockeId：出参，需要借出的内存所在的sockeId
     * @numaId: 出参，需要借出的内存所在的numaId
     */
    uint32_t GetBorrowableNumaInfo(const std::string& node, int& socketId, int& numaId);

    /* 查询可归还内存
     * @from：入参，需要归还内存的借出节点Id
     * @to：入参，需要归还内存的借入节点Id
     * @memName：出参，需要归还的内存标识符
     * @numaId: 出参，需要归还的内存所在的numaId
     */
    uint32_t GetReturnableMem(const std::string& from, const std::string& to, std::string& memName, int& numaId);

    /* 保存借用记录
     * @memName：入参，内存标识符
     * @from：入参，借出节点Id
     * @to：入参，借入节点Id
     * @fromNumaId：入参，借出内存在借出节点的numaId
     * @toNumaId: 入参，借出内存在借入节点的numaId
     */
    uint32_t SetNumaNodeBorrowSize(const std::string& memName, const std::string& from, const std::string& to,
                                   int fromNumaId, int toNumaId);

    /* 删除借用记录
     * @memName：入参，内存标识符
     * @from：入参，借出节点Id
     * @to：入参，借入节点Id
     * @fromNumaId：入参，借出内存在借出节点的numaId
     */
    uint32_t DelNumaNodeBorrowSize(const std::string& memName, const std::string& from, const std::string& to,
                                   int fromNumaId);

    // 借出列表
    std::map<std::string, std::vector<NodeMemBorrowInfo>> lendMap;
    // 借入列表
    std::map<std::string, std::vector<NodeMemBorrowInfo>> borrowMap;
    // 每个node的每个numa node的内存情况
    std::map<std::string, BorrowMemInfoPerNode> borrowMemInfoPerNodeMap;

    // 设置物理拓扑
    static void SetTopology(std::map<std::string, std::vector<std::string>>& topo);
    // 物理拓扑
    static std::map<std::string, std::vector<std::string>> physicalTopo;
    // 初始化借用topo，根据rack的topo生成借用topo，并保存rack topo
    static uint32_t InitGlobalMemBorrowTopo();
    // 全局借用topo
    static MemBorrowTopo globalMemBorrowTopo;

private:
    /* 根据物理拓扑检查边节点存在性、节点连通性
     * @from：入参，借出节点Id
     * @to：入参，借入节点Id
     */
    bool PhysicalTopoCheck(const std::string& from, const std::string& to);

    /* 根据lendMap、borrowMap检查节点存在性
     * @from：入参，借出节点Id
     * @to：入参，借入节点Id
     */
    bool LendBorrowMapCheck(const std::string& from, const std::string& to);

    /* 根据物理拓扑、借入借出map检查节点合法性
     * @from：入参，借出节点Id
     * @to：入参，借入节点Id
     */
    bool MemBorrowTopoCheck(const std::string& from, const std::string& to);

    /* 增加节点numa上的可借用内存大小
     * @nodeId：入参，节点Id
     * @numaId：入参，numaId
     * @size：入参，增加的内存大小
     */
    void AddNumaSize(const std::string& nodeId, const int numaId, const uint64_t size);

    /* 减少节点numa上的可借用内存大小
     * @nodeId：入参，节点Id
     * @numaId：入参，numaId
     * @size：入参，增加的内存大小
     */
    void SubNumaSize(const std::string& nodeId, const int numaId, const uint64_t size);
};

} // end namespace mem_borrow
} // end namespace ucache

#endif // UCACHE_MEM_BORROW_H
