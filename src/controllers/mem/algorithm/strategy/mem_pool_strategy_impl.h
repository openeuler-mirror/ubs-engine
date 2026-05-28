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

#ifndef RS_MEM_ALGO_MEM_POOL_STRATEGY_IMPL_H
#define RS_MEM_ALGO_MEM_POOL_STRATEGY_IMPL_H

#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "ubse_common_def.h"
#include "mem_pool_config.h"
#include "mem_pool_strategy.h"

namespace tc::rs::mem {
#define LOG_LEVEL mStrategyImpl->GetLogLevel()

const int MB_TO_B = (1024 * 1024);
const int GB_TO_B = (1024 * 1024 * 1024);
const int HUNDRED = 100;
const int HALF_MIN_REQUEST_SIZE = (128.0 / 2);
const int TWO = 2;
const int PRECISION = 3;
const int TOOL_PRECISION = 7;

class BorrowDecisionMaker;
class ShareDecisionMaker;

/** 借用债务的统计信息 */
struct DebtInfo {
    DebtInfo() = default;
    int debtSize[NUM_HOSTS][NUM_HOSTS]; /* debtSize[i][j]表示节点 i向节点 j借用内存总量, 单位为MB */
    // std::unordered_map<uint16_t, std::vector<MemLoc>> borrowNodeToNuma;       // 借入节点和借出numa的关系
    std::unordered_map<MemLoc, std::unordered_set<uint16_t>, MemLoc::Hash>
        lenderNumaToBorrowNode; // 借出numa和借入节点的关系
};

/** 请求类型 */
enum class RequestMode
{
    BORROW,
    SHARE
};

/** 借出方numa信息 */
struct TargetSocket {
    TargetSocket() = default;
    int32_t resLen{0};                           /* 内存提供方numa数量 */
    MemLoc resLocs[MAX_NUM_SRC_PER_REQUEST]{};   /* 内存提供方numa列表 */
    int32_t resSizes[MAX_NUM_SRC_PER_REQUEST]{}; /* 内存提供量, 单位: MB */
    MemLoc socketLoc;
};

/** socket、host内存信息结构体 */
struct MemStatus {
    MemStatus() = default;
    MemLoc loc{};       /* socket(numaId=-1)或host的具体位置(socketId, numaId=-1) */
    time_t timeStamp{}; /* 采样时间戳 */
    uint64_t memLocal{}; /* 本地的内存容量，不包含借入内存、借出内存、共享内存，单位：Byte */
    uint64_t memUsed{};     /* memTotal 中已使用的内存容量，单位：Byte */
    uint64_t memFree{};     /* memTotal 中没有使用的内存容量，单位：Byte */
    uint64_t memBorrowed{}; /* 已借入的内存容量，单位：Byte */
    uint64_t memLent{};     /* 已借出的内存容量，单位：Byte */
    uint64_t memShared{};   /* 提供共享内存的容量，单位：Byte */
};

/** 域内存状态结构体 */
struct RegionStatus {
    RegionStatus() = default;
    // 数组下标表示域中心节点所在行和列, 复用NumaStatus结构记录各域内存信息. memTotal=-1表示该位置上域不存在
    NumaStatus memStatus[NUM_HOST_PER_COL][NUM_HOST_PER_ROW];
};

/** 系统状态信息结构体 */
struct SysStatus {
    SysStatus() = default;
    DebtInfo debtInfo{};
    MemStatus numaStatus[NUM_TOTAL_NUMA]{};
    MemStatus socketStatus[NUM_TOTAL_SOCKET]{};
    MemStatus hostStatus[NUM_HOSTS]{};
};

class MemPoolStrategyImpl : public MemPoolStrategy {
public:
    MemPoolStrategyImpl();
    ~MemPoolStrategyImpl() override;

    std::unique_ptr<MemPoolConfig> mConfig_; /* 内存池算法静态配置类 */
    SysStatus memSysStatus_ = {}; /* 系统状态信息, 决策前以socket、host为粒度完成内存状态统计 */
    DebtDetail mDebtDetail_ = {}; /* 系统详细借用账本, 决策前将输入参数的账本备份至成员变量中 */
    std::unique_ptr<BorrowDecisionMaker> mBorrowDecisionMaker_;
    std::unique_ptr<ShareDecisionMaker> mShareDecisionMaker_;
    float memUsedRatioWeight_[2]{0.5, 0.5}; /* 节点触发水线时, 考虑节点已用内存占比、socket已用内存占比的权重 */

    /**
    * @brief 决策单个借用请求
    * @param borrowRequest [IN] 借用请求方信息(借用请求方位置, 借用量大小, 借用紧急程度)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    * @param result [OUT] 借用请求决策结果(借出numa数量, 借出numa位置, 借入numa位置, 各numa内存借用量)
    * @return
    */
    BResult MemoryBorrow(const BorrowRequest& borrowRequest, const UbseStatus& ubseStatus,
                         BorrowResult& result) override;

    /**
    * @brief 决策共享请求
    * @param shareRequest [IN] 共享请求方(指定共享节点、共享域，申请内存量，紧急程度)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    * @param result [OUT] 共享请求决策结果
    * @return
    */
    BResult MemoryShare(const ShareRequest& shareRequest, const UbseStatus& ubseStatus, ShareResult& result) override;

    BResult Init(const StrategyParam& param) override;

    /**
    * @brief 将ubseStatus.debtDetails保存至成员变量中, 并将其转换为DebtInfo (每一对节点间的内存借用量)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    * @return
    */
    BResult InitDebtInfo(const UbseStatus& ubseStatus);

    /**
    * @brief 根据调用决策时各numa状态, 初始化各numa, socket, host状态(sysStatus成员变量)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    */
    BResult InitSysStatus(const UbseStatus& ubseStatus);

    /**
    * @brief 判断socket借出或共享内存后是否超过节点借用内存上限、共享内存上限、提供内存上限
    * @param targetLoc [IN] 目标socket位置
    * @param requestSize [IN] socket提供内存量
    * @param requestMode [IN] 请求类型, 仅支持借用和共享
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @return
    */
    bool MaxOutFilter(MemLoc targetLoc, int32_t requestSize, RequestMode requestMode, const SysStatus& sysStatus);

    /**
    * @brief 统计socket上各numa的剩余内存, 考虑水线、预留内存总量
    * @param targetLoc [IN] 目标socket位置
    * @param urgentLevel [IN] 请求紧急程度
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @return socket上的numa位置列表, 以及各numa内存余量, 单位MB
    */
    TargetSocket NumaMemFree(MemLoc targetLoc, RequestUrgentLevel urgentLevel, const SysStatus& sysStatus) const;

    /**
    * @brief 判断socket的剩余内存是否足够借用或共享 (综合考虑各numa水线下内存, 预留内存)
    * @param targetLoc [IN] 目标socket位置
    * @param requestSize [IN] socket提供内存量
    * @param urgentLevel [IN] 请求紧急程度
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @param numaList [OUT] socket上的numa位置列表, 以及各numa内存余量, 单位MB
    * @return
    */
    bool MemFreeFilter(MemLoc targetLoc, int32_t requestSize, RequestUrgentLevel urgentLevel,
                       const SysStatus& sysStatus, TargetSocket& numaList) const;

    /**
    * @brief 候选socket依据各numa的剩余内存, 确定各numa提供的内存量
    * @param numaList [IN] socket上的numa位置列表, 以及各numa内存余量, 单位MB
    * @param requestSize [IN] socket提供内存量
    * @return 提供内存的numa位置, 和numa提供的内存量
    */
    static TargetSocket TargetSocket2Numa(TargetSocket numaList, int32_t requestSize);

    /**
    * @brief 计算共享请求socket的时延评分,
    * @param requestSizeS [IN] 共享请求的内存申请量, 借用和归还请求无需该参数
    * @param targetSocket [IN] 目标socket. 借用, 共享请求的目标socket为numa拆分结果; 归还请求的目标socket为待归还借用债务
    * @return
    */
    double ComputeShareLatencyScore(int32_t requestSizeS, const TargetSocket& targetSocket) const;

    /**
    * @brief 候选socket依据各numa的剩余内存, 确定各numa提供的内存量
    * @param numaList[IN] socket上的numa位置列表,以及各numa内存余量, 单位MB
    * @param requestSize[IN] socket提供内存量
    * @return 提供内存的numa位置,和numa提供的内存
    */
    TargetSocket TargetSocket2NumaByReliable(const MemLoc& requestLoc, const TargetSocket& numaList,
                                             const SysStatus& sysStatus, int32_t requestSize);

    /**
    * @brief 计算请求方与目标socket的时延评分, 适用于借用、归还、共享请求.
    * @param requestLocBR [IN] 借用和归还请求的请求方, 共享请求无需该参数
    * @param requestSizeS [IN] 共享请求的内存申请量, 借用和归还请求无需该参数
    * @param targetSocket [IN] 目标socket. 借用, 共享请求的目标socket为numa拆分结果; 归还请求的目标socket为待归还借用债务
    * @param requestMode [IN] 请求类型, 借用、归还或共享
    * @return
    */
    double LatencyScore(MemLoc requestLocBR, int32_t requestSizeS, const TargetSocket& targetSocket,
                        RequestMode requestMode) const;

    /**
    * @brief 每条决策计算评分前, 统计各域的内存状态. 仅节点非全连接场景下需要计算.
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @param regionStatus [IN] 系统各域内存状态
    * @return
    */
    double GetRegionStatus(const SysStatus& sysStatus, RegionStatus& regionStatus) const;

    /**
    * @brief 计算域均衡性评分, 适用于借用、归还、共享请求
    * @param targetSocket [IN] 目标socket. 借用, 共享请求的目标socket为numa拆分结果; 归还请求的目标socket为待归还借用债务
    * @param requestMode [IN] 请求类型
    * @param regionStatus [IN] 系统各域内存状态
    * @return
    */
    double RegionBalanceScore(const TargetSocket& targetSocket, RequestMode requestMode,
                              const RegionStatus& regionStatus) const;

    /**
    * @brief 计算目标socket的均衡性评分, 适用于借用、归还、共享请求.
    * @param targetSocket [IN] 目标socket. 借用, 共享请求的目标socket为numa拆分结果; 归还请求的目标socket为待归还借用债务
    * @param requestMode [IN] 请求类型, 借用、归还或共享
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @return
    */
    double BalanceScore(const TargetSocket& targetSocket, RequestMode requestMode, const SysStatus& sysStatus) const;

    /**
    * @brief 计算目标socket的可靠性评分, 适用于借用、归还、共享请求.
    * @param requestLocBR [IN] 借用和归还请求的请求方, 共享请求无需该参数
    * @param targetSocket [IN] 目标socket. 借用, 共享请求的目标socket为numa拆分结果; 归还请求的目标socket为待归还借用债务
    * @param requestMode [IN] 请求类型, 借用、归还或共享
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @return
    */
    double ReliabilityScore(MemLoc requestLocBR, const TargetSocket& targetSocket, RequestMode requestMode,
                            const SysStatus& sysStatus) const;

    void SplitNumaWithVector(const std::vector<uint32_t>& numaVector, TargetSocket numaList, int32_t& lentSize,
                             std::unordered_set<uint32_t>& lentNumas, TargetSocket& tmpSocket);

    /**
    * @brief 计算目标socket的numa拆分评分, 适用于借用、共享请求, 归还请求无需该评分
    * @param requestSize [IN] 借用量、共享量
    * @param targetSocket [IN] 目标socket. 借用, 共享请求的目标socket为numa拆分结果
    * @return
    */
    static double DivideNumaScore(int32_t requestSize, const TargetSocket& targetSocket);

    /**
    * @brief 当请求紧急程度为L1、L2时, 若socket提供内存触发L0水线, 则借用代价增加惩罚项
    * @param requestSize [IN] 内存申请量
    * @param targetSocket [IN] socket的内存提供情况
    * @param sysStatus [IN] 系统numa, socket, host状态
    * @return
    */
    double PenaltyScore(int32_t requestSize, TargetSocket targetSocket, const SysStatus& sysStatus) const;

    /**
    * @brief 获得MemPoolStrategy的mLogLevel成员变量, 用于decision_maker的日志打印功能
    * @return
    */
    inline int GetLogLevel()
    {
        return mLogLevel;
    }

    /**
    * @brief 借用决策输入参数检查
    * @param borrowRequest [IN] 借用请求方信息
    * @return
    */
    BResult BorrowParamCheck(const BorrowRequest& borrowRequest);

    /**
    * @brief 共享请求输入参数检查
    * @param shareRequest [IN] 共享请求方信息
    * @return
    */
    BResult ShareParamCheck(const ShareRequest& shareRequest);

    /**
    * @brief BorrowDecisionMaker空指针检查
    * @param decisionMaker [IN] BorrowDecisionMaker指针
    * @return
    */
    static void CleanUpBorrowDecisionMaker(BorrowDecisionMaker* decisionMaker);

    /**
     * @brief 计算节点内存状态
     * @param ubseStatus
     * @param i
     * @param numa
     * @param timeStamp
     */
    void OperateMemStatus(const UbseStatus& ubseStatus, int i, MemLoc numa, time_t timeStamp);

    /**
     * @brief 计算节点借入借出内存情况
     * @param ubseStatus
     * @param i
     * @param numa
     */
    void OperateLedgerStatus(const UbseStatus& ubseStatus, int i, MemLoc numa);

    /**
     * @brief 根据紧急程度获得水线
     * @param targetLoc
     * @param urgentLevel
     * @return
     */
    int GetWaterLine(MemLoc targetLoc, RequestUrgentLevel urgentLevel) const;

    /**
     * @brief 获取节点间的时延
     * @param requestLocBR
     * @param targetIndex
     * @return
     */
    int GetLatency(MemLoc requestLocBR, int targetIndex) const;

    /**
     * @brief 获取节点的借出内存
     * @param targetSocket
     * @return
     */
    int32_t GetRequestSize(const TargetSocket& targetSocket) const;

    /**
     * @brief 获取节点的借出内存，单位Byte
     * @param targetSocket
     * @return
     */
    uint64_t GetRequestByteSize(const TargetSocket& targetSocket) const;

    /**
     * @brief 检查节点内存是否可以借出
     * @param memTotal
     * @param memUsed
     * @param memFree
     * @return
     */
    bool CheckMem(uint64_t memTotal, uint64_t memUsed, uint64_t memFree) const;

    /**
     * @brief 检查节点Id合法性
     * @param borrowRequest
     * @return
     */
    bool IsHostIdInvalid(const BorrowRequest& borrowRequest);

    /**
     * @brief 请求是否合法
     * @param borrowRequest
     * @return
     */
    bool IsInvalidRequest(const BorrowRequest& borrowRequest);

    /**
     * @brief 检查节点最大借入节点数设置
     * @param borrowRequest
     * @return
     */
    bool CheckMaxBorrowHosts(const BorrowRequest& borrowRequest);

    /**
     * @brief 域平衡检查边界
     * @param regionStatus
     * @param x
     * @param y
     * @return
     */
    bool CheckBoundary(const RegionStatus& regionStatus, int x, int y) const;

    /**
     * 检查重要参数是否为0
     */
    void IsArgumentZero();
};
} // namespace tc::rs::mem
#endif // RS_MEM_ALGO_MEM_POOL_STRATEGY_IMPL_H
