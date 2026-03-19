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

#ifndef RS_MEM_ALGO_MEM_POOL_CONFIG_H
#define RS_MEM_ALGO_MEM_POOL_CONFIG_H

#include "mem_pool_strategy.h"

const int NUM_HOST_PER_ROW = 8;
const int NUM_HOST_PER_COL = 8;

namespace tc::rs::mem {
/** 系统链路时延信息 */
struct SysLatencyInfo {
    SysLatencyInfo() = default;

    int32_t maxSysLatency;                                             /* 所有链路时延的最大值 */
    int32_t minSysLatency;                                             /* 所有链路时延的最小值 */
    int32_t numaToNumaLatency[NUM_TOTAL_NUMA][NUM_TOTAL_NUMA];         /* [i][j]表示numa i到numa j的链路时延 */
    int32_t socketToNumaLatency[NUM_TOTAL_SOCKET][NUM_TOTAL_NUMA];     /* [i][j]表示socket i到numa j的平均时延 */
    int32_t socketToSocketLatency[NUM_TOTAL_SOCKET][NUM_TOTAL_SOCKET]; /* [i][j]表示socket i到socket j的平均时延 */
    int32_t socketToHostLatency[NUM_TOTAL_SOCKET][NUM_HOSTS];          /* [i][j]表示socket i到host j的平均时延 */
};

class MemPoolConfig {
public:
    /**
    * @brief 初始化静态参数，预计算socket间的链路时延信
    * @return 更新mStaticParam、LatencyInfo等成员变量
    */
    explicit MemPoolConfig(const StrategyParam &param);

    ~MemPoolConfig() = default;

    /** 算法权重参数归一化 */
    static BResult NormalizeStrategy(StrategyParam &param);

    /** 当enableCustomLatencies==false时, 检查hostMeshLoc参数的有效性 */
    bool IsHostMeshLocValid() const;

    /** 判断节点坐标是否全非负 */
    bool IsNonNegative(MeshLoc cord) const;

    /** 判断节点坐标是否全为负 */
    bool IsNonPositive(MeshLoc cord) const;

    /** 根据拓扑坐标，刷新Numa之间的时延 */
    BResult RefreshNumaDelays();

    /** 判断系统各host是否全连接 (1630代际, 或1650代际1P4) */
    bool CheckFullConnectivity() const;

    /** 根据初始化参数的hostMeshLoc, 记录host所在行列和hostIndex的对应关系 */
    void MapTopologyToIndices();

    /** 构建socket、numa全局index映射矩阵 */
    BResult BuildIndexMatrix();

    /** 根据numa位置获得numa全局index */
    int GetNumaIndex(MemLoc loc);

    /** 根据socket位置获得socket全局index*/
    int GetSocketIndex(MemLoc loc);

    /** 更新系统当前最小时延*/
    void UpdateMinLatency(int32_t latency);

    /** 更新系统当前最大时延*/
    void UpdateMaxLatency(int32_t latency);

    /** 计算平均时延*/
    void CalculateLatency(int32_t &latency, int num);

    /** 获得socket上所有numa全局index数组, -1为无效值 */
    int *GetNumaListInSocket(int32_t hostId, int32_t socketId);

    /** 获得host上所有numa全局index数组, -1为无效值 */
    int *GetNumaListInHost(int32_t hostId);

    /** 统计可用socket数量、可用socket列表 */
    BResult GetSockets();

    /** 统计系统链路时延信息 */
    BResult SysLatencyProcess();

    /** 统计所有节点借出内存上限的最大值 */
    BResult GetBorrowedMaxMem();

    /** 检查host和numa是否超上限 */
    void CheckNodeParameters(const StrategyParam &param);

    StrategyParam memStaticParam = {};                               /* 内存池算法静态配置参数 */
    SysLatencyInfo memLatencyInfo = {};                              /* 系统链路时延信息 */
    int memAvailSocketsCnt = 0;                                      /* 可用socket数量 */
    MemLoc memAvailSockets[NUM_TOTAL_SOCKET]{};                      /* 可用socket列表 */
    int32_t memMaxBorrowed = 0;                                      /* 所有节点借出内存上限的最大值 */
    bool memIsFullyConnected = false;                                /* 系统各节点是否全连接 */
    int memMeshLoc2HostIdx[NUM_HOST_PER_COL][NUM_HOST_PER_ROW] = {}; /* i行j列位置上的hostId, 若无host则记为-1 */

private:
    int memNumaLoc2Idx[NUM_HOSTS][NUM_NUMA_PER_HOST]{};
    int memSocketLoc2Idx[NUM_HOSTS][NUM_SOCKET_PER_HOST]{};
    int memNumaLocToIdx[NUM_HOSTS][NUM_SOCKET_PER_HOST][NUM_NUMA_PER_SOCKET]{};
};
} // namespace tc::rs::mem
#endif // RS_MEM_ALGO_MEM_POOL_CONFIG_H
