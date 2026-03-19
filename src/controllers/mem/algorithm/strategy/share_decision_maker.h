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

#ifndef RS_MEM_ALGO_MEM_POOL_SHARE_STRATEGY_H
#define RS_MEM_ALGO_MEM_POOL_SHARE_STRATEGY_H
#include <cstring>
#include "mem_pool_config.h"
#include "mem_pool_strategy.h"
#include "mem_pool_strategy_impl.h"
#include "ubse_logger.h"

namespace tc::rs::mem {
#define MODULE_LOG_NAME "ubse_mem_strategy"
const double SOCKET_COST_UPPER_BOUND = 10.0;

struct TmpResult {
    int socketIndex = 0;                                // 记录socket位置Temp值
    int optimalSocket = -1;                             // SocketCost数量
    double optimalSocketCost = SOCKET_COST_UPPER_BOUND; // SocketCost上限
    TargetSocket targetSocket{};
    // 下方为Greedy算法需要的Temp；
    int idxMaxFree = 0;
    uint64_t maxNumaFreeSizeBytes = 0;
};

enum class DebugStep {
    STEP1 = 1,
    STEP2 = 2,
};

class ShareDecisionMaker {
public:
    explicit ShareDecisionMaker(MemPoolStrategyImpl *strategyImpl)
    {
        mStrategyImpl_ = strategyImpl;
    };

    MemPoolStrategyImpl *mStrategyImpl_ = nullptr;
    MemPoolConfig *memConfig_{};

    /**
    * @brief 共享决策-自研算法
    *
    * @param shareRequest [IN] 共享请求方(指定共享节点、共享域，申请内存量，紧急程度)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    * @param result [OUT] 共享请求决策结果
    * @return
    */
    BResult MemoryShare(const ShareRequest &shareRequest, const UbseStatus &ubseStatus, ShareResult &result) const;

    /**
    * @brief 候选集筛选函数完成后，针对Socket拆分为Numa节点，进行分数的计算和结果的更新
    *
    * @param shareRequest [IN] 共享请求方(指定共享节点、共享域，申请内存量，紧急程度)
    * @param targetLoc [IN] 借出方numa位置
    * @param regionStatus [IN] 系统各域内存状态
    * @param tmpInfo [IN] 共享算法中间暂存结果以及Urgent Level、RequestSize
    * @param result [OUT] 共享请求决策结果
    * @return
    */
    BResult ShareScoreAndFilter(const ShareRequest &shareRequest, MemLoc targetLoc, const RegionStatus &regionStatus,
                                struct TmpResult &tmpInfo, ShareResult &result) const;

    /**
    * @brief 共享决策-Greedy算法
    *
    * @param shareRequest [IN] 共享请求方(指定共享节点、共享域，申请内存量，紧急程度)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    * @param result [OUT] 共享请求决策结果
    * @return
    */
    BResult MemoryShareGreedy(const ShareRequest &shareRequest, const UbseStatus &ubseStatus,
                              ShareResult &result) const;
    /**
    * @brief 获取单Host中的Numa个数
    *
    * @param numaList [IN] 根据Host ID获得的numa list
    * @return 单Host中Numa个数
    */
    int GetNumbNumaInHost(int *numaList) const;

    /**
    * @brief 获取单Host中的Socket个数
    *
    * @param hostId [IN] Host ID
    * @return 单Host中Numa个数
    */
    int GetNumbSocket(int16_t hostId) const;

    void ShareOperator(const ShareRequest &shareRequest, const RegionStatus &regionStatus, MemLoc targetLoc,
                       TmpResult &shareCurrentResult, ShareResult &result) const;
};
#undef MODULE_LOG_NAME
} // namespace tc::rs::mem
#endif // RS_MEM_ALGO_MEM_POOL_SHARE_STRATEGY_H
