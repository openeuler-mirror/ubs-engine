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

#ifndef MEM_POOL_STRATEGY_H
#define MEM_POOL_STRATEGY_H

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <array>

#include "ubse_error.h"
#include "ubse_mem_constants.h"

namespace tc::rs::mem {
using BResult = int32_t;

#ifdef UBSE_SERVER_1630
const int NUM_HOSTS = 4;
const int NUM_TOTAL_SOCKET = 8;
const int NUM_TOTAL_NUMA = 16;
const int NUM_SOCKET_PER_HOST = (NUM_TOTAL_SOCKET / NUM_HOSTS);
const int NUM_NUMA_PER_SOCKET = (NUM_TOTAL_NUMA / NUM_TOTAL_SOCKET);
const int NUM_NUMA_PER_HOST = (NUM_TOTAL_NUMA / NUM_HOSTS);
const int MAX_NUM_SRC_PER_REQUEST = 8; /* 单个借用/共享请求，内存提供方的数量上限 */
#else
const int NUM_HOSTS = TOPOLOGY_MAX_HOST_NUM;
const int NUM_TOTAL_SOCKET = TOPOLOGY_MAX_SOCKET_PER_HOST * NUM_HOSTS;
const int NUM_TOTAL_NUMA = TOPOLOGY_MAX_NUMA_PER_SOCKET * NUM_TOTAL_SOCKET;
const int NUM_SOCKET_PER_HOST = (NUM_TOTAL_SOCKET / NUM_HOSTS);
const int NUM_NUMA_PER_SOCKET = (NUM_TOTAL_NUMA / NUM_TOTAL_SOCKET);
const int NUM_NUMA_PER_HOST = (NUM_TOTAL_NUMA / NUM_HOSTS);
const int MAX_NUM_SRC_PER_REQUEST = 8; /* 单个借用/共享请求，内存提供方的数量上限 */
#endif
const int NUM_TOTAL_NUMA_FULLY_CONNECTED =
    32; /* 各节点全直连时系统最大可能numa数量, 用于填写初始化参数的链路时延矩阵 */
const float LENDER_BALANCE_RELIABILITY_COST = 0.53f;
const float LENDER_BALANCE_BALANCE_COST = 0.13f;

using ShmRegionType = enum class MemShmRegionType {
    ALL2ALL_SHARE = 0, /* *SHM域类型为域内任何节点提供内存都可以被域内所有节点共享访问 */
    ONE2ALL_SHARE      /* *SHM域类型为域内单一节点作为提供方都可以被域内所有节点共享访问 */
};

using WatermarkGrain = enum class MemWatermarkGrainType {
    HOST_WATERMARK = 0, /* 水线以host为粒度，触发内存借用或归还 */
    NUMA_WATERMARK      /* 水线以numa为粒度，触发内存借用或归还 */
};

enum class RequestUrgentLevel {
    LEVEL0 = 0, /* 紧急程度最低, 不允许内存提供方提供内存后超过memHighLineL0水线 */
    LEVEL1,     /* 紧急程度中等, 不允许内存提供方提供内存后超过memHighLineL1水线  */
    LEVEL2      /* 紧急程度最高, 允许内存提供方提供所有剩余内存 */
};

struct MemLoc {
    MemLoc() = default;
    int16_t hostId{-1};  /* 节点ID，编号从零开始 */
    int8_t socketId{-1}; /* socket(P)的ID，编号从零开始；如果不需要指定socket，设置为-1 */
    int8_t numaId{-1};   /* numa_node的ID，编号从零开始；如果不需要指定numa，设置为-1 */

    struct Hash {
        std::size_t operator()(const MemLoc &loc) const
        {
            std::size_t seed = 0;
            auto hashUint16 = std::hash<uint16_t>{};
            auto hashUint8 = std::hash<uint8_t>{};
            const uint32_t randValue = 0x9e3779b9;
            const uint8_t NO_6 = 6;
            const uint8_t NO_2 = 2;
            seed ^= hashUint16(loc.hostId) + randValue + (seed << NO_6) + (seed >> NO_2);
            seed ^= hashUint8(loc.socketId) + randValue + (seed << NO_6) + (seed >> NO_2);
            seed ^= hashUint8(loc.numaId) + randValue + (seed << NO_6) + (seed >> NO_2);
            return seed;
        }
    };

    friend bool operator==(const MemLoc &lhs, const MemLoc &rhs)
    {
        return (lhs.hostId == rhs.hostId && lhs.socketId == rhs.socketId && lhs.numaId == rhs.numaId);
    }

    friend std::ostream &operator<<(std::ostream &os, const MemLoc &obj)
    {
        return os << "hostId=" << obj.hostId << " socketId=" << static_cast<int32_t>(obj.socketId)
                  << " numaId=" << static_cast<int32_t>(obj.numaId);
    }
};

/* 1650代际 Ubseserver 拓扑坐标，用于标识 host/socket/numa 在拓扑中的位置 */
struct MeshLoc {
    MeshLoc() = default;
    int8_t x{-1}; /* 2D Full Mesh 拓扑中 X 轴方向的坐标，坐标值从零开始；1650代际要求X轴必须放置4台服务器 */
    int8_t y{-1}; /* 2D Full Mesh 拓扑中 Y 轴方向的坐标，坐标值从零开始；1650代际允许将Y轴的长度设置为1~4 */
    friend bool operator==(const MeshLoc &lhs, const MeshLoc &rhs)
    {
        return (lhs.x == rhs.x && lhs.y == rhs.y);
    };

    bool IsNeighbor(const MeshLoc &other) const
    {
        return ((x == other.x) || (y == other.y));
    }
};

struct DebtDetail {
    DebtDetail() = default;
    // 每个numa的内存借入账务情况，numa的globalIndex必须与 StrategyParam 的 availNumas 的 index 保持一致；
    // map<int16_t, uint64_t>的key为借出方numa的globalIndex，value为内存借用总量（不包含已经归还的内存，单位：Byte）；
    std::map<int16_t, uint64_t> numaDebts[NUM_TOTAL_NUMA]{};
};

struct NumaStatus {
    NumaStatus() = default;
    time_t timestamp{}; /* 数据采集的时间，格式：Unix时间戳，单位秒 */
    MemLoc numa{};      /* numa的具体位置，hostId、socketId、numaId 都是必填 */
    uint64_t memTotal{}; /* 本地的内存容量，不包含借入内存、借出内存、共享内存，单位：Byte */
    uint64_t memUsed{}; /* memTotal 中已使用的内存容量，单位：Byte */
    uint64_t memFree{}; /* memTotal 中没有使用的内存容量，单位：Byte */
};

struct NumaLedgerStatus {
    NumaLedgerStatus() = default;
    MemLoc numa{};          /* numa的具体位置，hostId、socketId、numaId 都是必填 */
    uint64_t memBorrowed{}; /* 已借入的内存容量，单位：Byte */
    uint64_t memLent{};     /* 已借出的内存容量，单位：Byte */
    uint64_t memShared{};   /* 提供共享内存的容量，单位：Byte */
};

struct BorrowAlgoParam {
    BorrowAlgoParam() = default;
    /* 参数初始化时进行归一化, 保证权重之和为1 */
    float wLatencyCost{0.13f};       /* 借用决策, 时延代价权重 */
    float wRegionBalanceCost{0.13f}; /* 借用决策, 域均衡性代价权重 */
    float wBalanceCost{0.53f};       /* 借用决策, 均衡性权重 */
    float wReliabilityCost{0.13f};   /* 借用决策, 可靠性权重 */
    float wDivideNumaCost{0.08f};    /* 借用决策, NUMA拆分代价权重 */
};

struct ShareAlgoParam {
    ShareAlgoParam() = default;
    /* 参数初始化时进行归一化, 保证权重之和为1 */
    float wLatencyCost{0.12f};       /* 共享决策, 时延代价权重 */
    float wRegionBalanceCost{0.12f}; /* 共享决策, 域均衡性代价权重 */
    float wBalanceCost{0.52f};       /* 共享决策, 均衡性权重 */
    float wReliabilityCost{0.12f};   /* 共享决策, 可靠性权重 */
    float wDivideNumaCost{0.12f};    /* 共享决策, NUMA拆分代价权重 */
};

enum class AlgoMode {
    GREEDY,        /* 贪心算法 */
    SELF_DEVELOPED /* 自研算法 */
};

enum class LenderNumaMode {
    NON_BALANCE,
    BALANCE,
};

struct StrategyParam {
    StrategyParam() = default;
    int32_t numHosts{};                  /* UbseServer可用host总数 */
    int32_t numAvailNumas{};             /* UbseServer可用numa总数 */
    MemLoc availNumas[NUM_TOTAL_NUMA]{}; /* 可用numa列表, 仅使用数组前numAvailNumas个位置 */
    /* 含义: 是否填写numaLatencies. true表示填写numaLatencies, 认为所有host全连接; false表示填写hostMeshLocs, 自动计算时延 */
    /* 配置建议: 1630代际必须配置为true, 填写numaLatencies; 1650代际建议配置为false, 填写hostMeshLocs, 自动计算链路时延信息 */
    bool enableCustomLatencies{true};
    std::array<std::set<int16_t>, tc::rs::mem::NUM_HOSTS> neighborNodes;  /* 每个节点的邻居节点的index集合 */
    MeshLoc hostMeshLocs
        [NUM_HOSTS]{}; /* 1650代际host拓扑坐标, 数组下标表示hostId: x表示host所在列编号, y表示host所在行编号 */
    /* 每两个numa之间的访存时延，index与availNumas一致 */
    int32_t numaLatencies[NUM_TOTAL_NUMA_FULLY_CONNECTED][NUM_TOTAL_NUMA_FULLY_CONNECTED]{};
    int32_t memHighLineL0[NUM_HOSTS]{
        0}; /* 低紧急程度下借用水线, 通常设定为内存子系统的借用水线，单位%，每个host可以单独配置 */
    /* 中等紧急程度下借用水线, 通常高于内存子系统的借用水线，单位%，每个host可以单独配置, memHighLineL1必须大于memHighLineL0 */
    int32_t memHighLineL1[NUM_HOSTS]{0};
    int32_t memLowLine[NUM_HOSTS]{0};             /* 归还内存的水线，单位%，每个host可以单独配置 */
    int32_t numaMemCapacities[NUM_TOTAL_NUMA]{0}; /* 每个numa的总内存容量，不考虑借入和借出，单位:MB */
    int32_t maxMemBorrowed[NUM_HOSTS]{};          /* host的借入内存容量上限,单位:MB */
    int32_t maxMemLent[NUM_HOSTS]{};              /* host的借出内存容量上限，单位:MB */
    int32_t maxMemShared[NUM_HOSTS]{};            /* host本地提供共享内存的容量上限，单位:MB */
    int32_t maxMemOut[NUM_HOSTS]{}; /* host能够提供的内存容量上限（用于借出或共享）, 单位: MB */
    int32_t maxBorrowHosts[NUM_HOSTS]{};        /* 借入内存的提供方host数量上限 */
    int32_t maxMemSizePerBorrow{};              /* 单笔借用的借用量上限，单位:MB */
    int32_t unitMemSize{128};                   /* 各numa借出、共享内存的最小单元, 单位:MB */
    int32_t memOutHardLimit[NUM_TOTAL_NUMA]{0}; /* [适配硬分区环境] 每个numa的预留内存池大小, 单位: MB */
    WatermarkGrain watermarkGrain{};            /* 水线触发的统计粒度 */
    AlgoMode algoMode{AlgoMode::SELF_DEVELOPED}; /* 算法选择开关, 默认选择自研算法 */
    LenderNumaMode lenderNumaMode{LenderNumaMode::NON_BALANCE};
    BorrowAlgoParam borrowParam{}; /* 借用算法参数 */
    ShareAlgoParam shareParam{};   /* 共享算法参数 */

    friend std::ostream &operator<<(std::ostream &os, const StrategyParam &obj)
    {
        return os << "numHosts: " << obj.numHosts << " usedHigh: " << obj.numAvailNumas;
    }
};

struct BorrowRequest {
    BorrowRequest() = default;
    MemLoc requestLoc{};   /* 借用请求方位置, hostId是必填 */
    int32_t requestSize{}; /* 借用量大小, 单位: MB */
    /* 借用紧急程度, 可选LEVEL0, LEVEL1, LEVEL2, 水线触发借用建议设置为LEVEL0, OOM场景建议设置为LEVEL2 */
    RequestUrgentLevel urgentLevel{};
};

struct BorrowResult {
    BorrowResult() = default;
    int32_t lenderLength{};                         /* 借出方numa个数 */
    MemLoc lenderLocs[MAX_NUM_SRC_PER_REQUEST]{};   /* 借出方numa位置 */
    MemLoc borrowerLocs[MAX_NUM_SRC_PER_REQUEST]{}; /* 借入方numa位置 */
    int32_t lenderSizes[MAX_NUM_SRC_PER_REQUEST]{}; /* 借用量, 单位: MB */
};

struct UbseStatus {
    UbseStatus() = default;
    NumaStatus numaStatus[NUM_TOTAL_NUMA]; /* 各numa的内存状态, 长度与StrategyParam.numAvailNumas保持一致 */
    NumaLedgerStatus
        numaLedgerStatus[NUM_TOTAL_NUMA]; /* 各numa的借入借出状态, 长度与StrategyParam.numAvailNumas保持一致 */
    DebtDetail debtDetail{};              /* 系统详细借用账本 */
};

using PerfLevel = enum class UbseMemPerLevel {
    L0, // 对应直连
    L1, // 对应1跳节点
    L2, // 对应超过一条节点
};

using SHMRegionDesc = struct TagUbseMemSHMRegionDesc {
    PerfLevel perfLevel;
    ShmRegionType type;    /* 共享请求类型 */
    int num;               /* 共享域节点数量, ALL2ALL申请必填, ONE2ALL申请可以不填 */
    int nodeId[NUM_HOSTS]; /* 共享域节点index, ALL2ALL申请必填, ONE2ALL申请可以不填 */
};

struct ShareRequest {
    MemLoc srcLoc{};                  /* 共享内存申请方, ONE2ALL申请必填, ALL2ALL申请可以不填 */
    SHMRegionDesc region{};           /* 共享请求类型 */
    int32_t requestSize{};            /* 共享内存申请量, 单位: MB */
    RequestUrgentLevel urgentLevel{}; /* 共享内存申请紧急程度 */
};

struct ShareResult {
    ShareResult() = default;
    int32_t numShareLocs{};                        /* 共享内存提供方numa数量 */
    MemLoc sharerLocs[MAX_NUM_SRC_PER_REQUEST]{};  /* 共享内存提供方numa位置 */
    int32_t shareSizes[MAX_NUM_SRC_PER_REQUEST]{}; /* 共享内存量, 单位: MB */
};

enum LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};

using ExternalLog = void (*)(int level, const char *msg);

class MemPoolStrategy {
public:
    static MemPoolStrategy &GetInstance();

    virtual BResult Init(const StrategyParam &param);

    /**
    * @brief 决策单个借用请求
    * @param borrowRequest [IN] 借用请求方信息(借用请求方位置, 借用量大小, 借用紧急程度)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    * @param result [OUT] 借用请求决策结果(借出numa数量, 借出numa位置, 借入numa位置, 各numa内存借用量)
    * @return
    */
    virtual BResult MemoryBorrow(const BorrowRequest &borrowRequest, const UbseStatus &ubseStatus,
                                 BorrowResult &result);

    /**
    * @brief 决策共享请求
    * @param shareRequest [IN] 共享请求方(指定共享节点、共享域，申请内存量，紧急程度)
    * @param ubseStatus [IN] 发起请求时系统状态信息(各numa内存状态, 各numa借用共享状态, 各节点间借用债务数)
    * @param result [OUT] 共享请求决策结果
    * @return
    */
    virtual BResult MemoryShare(const ShareRequest &shareRequest, const UbseStatus &ubseStatus, ShareResult &result);

    /** 配置日志打印级别, 默认OFF */
    inline void SetLogLevel(int level)
    {
        mLogLevel = level;
    }

    /** 配置日志打印函数, 默认打印控制台 */
    inline void SetLogFunction(ExternalLog func)
    {
        mLogFun = func;
    }

    /** 默认日志打印函数 */
    static inline void Print(int level, const char *msg)
    {
        const char *color = "\033[0m"; // 还原颜色
        if (level == LogLevel::INFO) {
            color = "\033[32m";
        }
        if (level == LogLevel::DEBUG) {
            color = "\033[34m";
        }
        if (level == LogLevel::ERROR) {
            color = "\033[31m";
        }

        std::string levels[OFF]{"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"};
        std::cout << color << "[" << levels[level] << "] "
                  << "\033[0m" << msg;
    }

    int mLogLevel = OFF;         /* 日志打印级别, 默认为OFF */
    ExternalLog mLogFun = Print; /* 日志打印函数, 默认打印控制台 */

    MemPoolStrategy(const MemPoolStrategy &other) = delete;
    MemPoolStrategy(MemPoolStrategy &&other) noexcept = delete;
    MemPoolStrategy &operator=(const MemPoolStrategy &other) = delete;
    MemPoolStrategy &operator=(MemPoolStrategy &&other) noexcept = delete;
    virtual ~MemPoolStrategy() = default;

protected:
    MemPoolStrategy() = default;
};
} // namespace tc::rs::mem
#endif
