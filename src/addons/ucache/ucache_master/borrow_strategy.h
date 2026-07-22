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

#ifndef UCACHE_STRATEGY_H
#define UCACHE_STRATEGY_H

#include <vector>
#include "borrow_action.h"
#include "data_collect.h"
#include "mem_borrow.h"

namespace ucache {
namespace borrow_strategy {
using namespace ucache::mem_borrow;
using namespace ucache::borrow_action;
using namespace ucache::data_collect;

class Strategy {
public:
    Strategy() = default;
    virtual ~Strategy() = default;
    virtual int32_t Init() = 0;
    virtual void Start() = 0;
};

class BorrowStrategy : public Strategy {
public:
    // 构造策略需要初始化借用拓扑信息
    BorrowStrategy() = default;
    ~BorrowStrategy() override = default;

    // 获取动作集
    std::vector<BorrowAction> GetActionSet();

    // 获取平衡度
    double GetBalance();

    int32_t Init() override;

    // 执行策略
    void Start() override;

private:
    // 调度中的各节点信息
    std::vector<BorrowNodeStat*> nodeStats;
    // 调度中的借用拓扑信息
    MemBorrowTopo curBorrowTopo;
    // 输出的调度动作集
    std::vector<BorrowAction> borrowActionSet;

    // 判断系统是否需要继续调度
    bool ShouldBorrow();

    // 节点进行一个回收调度
    uint32_t ReclaimLoanedMemory(BorrowNodeStat* nodeStat);
    // 节点进行一个借用调度
    uint32_t BorrowMemory(BorrowNodeStat* nodeStat);

    // 添加回收动作
    uint32_t AddReturnAction(BorrowNodeStat* from, BorrowNodeStat* to);
    // 添加借用动作
    uint32_t AddBorrowAction(BorrowNodeStat* from, BorrowNodeStat* to);

    // 单次调度策略
    uint32_t CalBorrowStrategy();

    // 还原一次调度
    void RestoreActionSet();

    // 还原系统状态
    void ReclaimAllMemory();

    // 还原一个节点的状态
    uint32_t RaclaimMemoryOneNode(NodeMemBorrowInfo nodeMemBorrowInfo);

    // 判断当前节点借用内存是否超过上限
    uint32_t CheckBorrowSize(BorrowNodeStat* nodeStat);

    // 选出有瓶颈型应用而且稀缺度最高的机器
    BorrowNodeStat* GetMostScarcityNode();

    // 单次调度中是否增加了两个动作
    bool addTwoActions = false;
};

} // namespace borrow_strategy
} // namespace ucache

#endif
