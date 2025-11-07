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

#ifndef RS_MEM_ALGO_BORROW_DECISION_MAKER_H
#define RS_MEM_ALGO_BORROW_DECISION_MAKER_H

#include <cstring>
#include "mem_pool_config.h"
#include "mem_pool_strategy.h"
#include "mem_pool_strategy_impl.h"

namespace tc::rs::mem {

const int MAX_DEBT_COST = 10;
const int HALF_MAX_DEBT_COST = (MAX_DEBT_COST / 2);

/** 合并请求时，系统中间状态结构体 */
struct BorrowTreeNodeStatus {
    BorrowTreeNodeStatus() = default;
    SysStatus sysStatus{};         /* 决策组合对应的系统状态 */
    BorrowResult *borrowResults{}; /* 批量借用决策结果 */
};

class BorrowDecisionMaker {
public:
    explicit BorrowDecisionMaker(MemPoolStrategyImpl *strategyImpl)
    {
        mStrategyImpl = strategyImpl;
    };
    MemPoolStrategyImpl *mStrategyImpl = nullptr;
    MemPoolConfig *memConfig = nullptr;
    BorrowTreeNodeStatus *mParentStat = nullptr;
    BorrowTreeNodeStatus *mChildStat = nullptr;

    /**
    * @brief 贪心策略, 筛选满足借用约束的剩余内存最多的numa
    * @param borrowRequest [IN] 借用请求方信息(借用请求方位置, 借用量大小, 借用紧急程度)
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @param idxMaxFree [OUT] 最优numa的index
    * @param maxNumaFreeSizeBytes [OUT] 最优numa的剩余内存容量
    * @return
    */
    BResult SelectOptimalNumaGreedy(const BorrowRequest &borrowRequest, const SysStatus &sysStatus, int &idxMaxFree,
                                    uint64_t &maxNumaFreeSizeBytes) const;

    /**
    * @brief 基于贪心策略选择借入方和借出方
    * @param borrowRequest [IN] 借用请求方信息(借用请求方位置, 借用量大小, 借用紧急程度)
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @param borrowResult [OUT] 借用请求决策结果(借出numa数量, 借出numa位置, 借入numa位置, 各numa内存借用量)
    * @return
    */
    BResult DetermineLenderGreedy(const BorrowRequest &borrowRequest, const SysStatus &sysStatus,
                                  BorrowResult &borrowResult) const;

    /**
     * @brief 基于时延选择借入方numa位置
     * @param borrowRequest
     * @param borrowResult
     */
    void GetBorrowerNuma(const BorrowRequest &borrowRequest, BorrowResult &borrowResult) const;

    /**
     * @brief 获取节点的numa列表
     * @param borrowRequest
     * @return
     */
    int *GetNumaList(const BorrowRequest &borrowRequest) const;

    /**
     * @brief 检查socket和host是否直连
     * @param borrowRequest
     * @param socketIdx
     * @return
     */
    bool CheckDirectConnect(const BorrowRequest &borrowRequest, int socketIdx) const;

    /**
     * @brief 检查是否同host
     * @param lendHost
     * @param borrowHost
     * @return
     */
    bool CheckSameHost(int lendHost, int borrowHost) const;

    /**
     * @brief 获取已借入节点数量
     * @param requestLoc
     * @param sysStatus
     * @return
     */
    int GetBorrowedNum(MemLoc requestLoc, const SysStatus &sysStatus) const;

    /**
     * @brief 检查是否超过借入节点限制
     * @param hasBorrowed
     * @param requestLoc
     * @return
     */
    bool IsBorrowedMax(int hasBorrowed, MemLoc requestLoc) const;

    /**
     * @brief 检查是否从该节点借入过内存
     * @param requestLoc
     * @param targetLoc
     * @param sysStatus
     * @return
     */
    bool IsNeverBorrowed(MemLoc requestLoc, MemLoc targetLoc, const SysStatus &sysStatus) const;

    /**
    * @brief 独立借用请求决策器, 贪心策略
    * @param borrowRequest [IN] 借用请求方信息(借用请求方位置, 借用量大小, 借用紧急程度)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    * @param borrowResult [OUT] 借用请求决策结果(借出numa数量, 借出numa位置, 借入numa位置, 各numa内存借用量)
    * @return
    */
    BResult MemoryBorrowGreedy(const BorrowRequest &borrowRequest, const UbseStatus &ubseStatus,
                               BorrowResult &borrowResult) const;

    /**
    * @brief 判断目标socket是否在候选集内, 仅适用于借用请求
    * @param requestLoc [IN] 借用请求的请求方
    * @param targetLoc [IN] 目标socket
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @return
    */
    bool LenderFilter(MemLoc requestLoc, MemLoc targetLoc, const SysStatus &sysStatus) const;

    /**
    * @brief 计算所有候选socket的借用代价
    * @param borrowRequest [IN] 借用请求方信息
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @param numAvailSockets [IN] 系统socket总数, memConfig->mNumAvailSockets
    * @param targetSockets [IN] 系统所有socket列表(resLen=0表示socket不可借), 调用时预留空间mConfig->mNumAvailSockets
    * @param socketCosts [OUT] 各socket的借用代价, 调用时预留空间mConfig->mNumAvailSockets
    * @return
    */
    BResult ComputeSocketCosts(const BorrowRequest &borrowRequest, const SysStatus &sysStatus, int numAvailSockets,
                               std::vector<TargetSocket> &targetSockets, std::vector<double> &socketCosts) const;

    /**
    * @brief 系统socket初筛, 并计算所有候选socket的借用代价(ComputeSocketCosts)
    * @param borrowRequest [IN] 借用请求方信息
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @param socketResults [OUT] 各socket的numa拆分结果(未确定借入方位置), 调用时预留空间mConfig->mNumAvailSockets
    * @param socketCosts [OUT] 各socket的借用代价, 调用时预留空间mConfig->mNumAvailSockets
    * @return
    */
    BResult GetSocketBorrowCost(const BorrowRequest &borrowRequest, const SysStatus &sysStatus,
                                std::vector<BorrowResult> &socketResults, std::vector<double> &socketCosts) const;

    /**
    * @brief 从所有socket中选择借用代价最低的topK个socket, 保存结果(以拆分numa形式存储)
    * @param borrowRequest [IN] 借用请求的请求方信息
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @param topK [IN] 目标topK
    * @param borrowResults [IN] 借用决策的topK最优决策结果(未确定借入方numa位置), borrowResults.lenderLength=0表示socket不可借
    * @return
    */
    BResult SelectTopKBorrow(const BorrowRequest &borrowRequest, const SysStatus &sysStatus, int topK,
                             BorrowResult *borrowResults) const;

    /**
    * @brief 确定借出方socket对应的借入方numa位置, 并保存决策结果(borrowResult)
    * @param requestLoc [IN] 借用请求的请求方信息
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @param borrowResult [IN] 借用决策的决策结果(已确定借入方numa位置), borrowResults.lenderLength=0表示socket不可借
    * @return
    */
    BResult Borrower2Numa(MemLoc requestLoc, const SysStatus &sysStatus, BorrowResult &borrowResult) const;

    /**
    * @brief 独立借用请求决策器, 评分策略
    * @param borrowRequest [IN] 借用请求方信息(借用请求方位置, 借用量大小, 借用紧急程度)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    * @param borrowResult [OUT] 借用请求决策结果(借出numa数量, 借出numa位置, 借入numa位置, 各numa内存借用量)
    * @return
    */
    BResult SingleMemBorrow(const BorrowRequest &borrowRequest, const UbseStatus &ubseStatus,
                            BorrowResult &borrowResult);
};
} // namespace tc::rs::mem
#endif // RS_MEM_ALGO_BORROW_DECISION_MAKER_H
