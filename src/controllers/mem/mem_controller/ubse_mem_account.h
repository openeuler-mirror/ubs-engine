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

#ifndef MXE_MEM_ACCOUNT_H
#define MXE_MEM_ACCOUNT_H

#include <string>
#include <unordered_map>
#include <vector>

namespace ubse::mem::account {
// NUMA节点信息结构体
struct UbseNumaNodeInfo {
    std::string nodeId;             // 该numa的nodeId
    std::string hostName;           // 主机名
    uint32_t numaId;                // numa id
    uint32_t socketId;              // socket id
    std::vector<uint16_t> mCpuList; // CPU列表（逗号分隔）
    uint64_t mReservedMemRatio;     // 预留内存比例
    uint64_t mMemTotal;             // 总内存量（字节）
    uint64_t mMemFree;              // 空闲内存量（字节）
    unsigned int nrHugepages;       // 2M大页数量
    unsigned int freeHugepages;     // 2M大页空闲数量
    uint64_t mTimestamp;            // 数据采集的时间戳（秒级）
    uint64_t mMemBorrowed;          // 借入内存量（字节）
    uint64_t mMemLent;              // 借出内存量（字节）
    uint64_t mMemShared;            // 共享内存量（字节）
};

/**
 * @brief 展示整个机架内的所有节点具体到每个numa的内存相关信息，包括节点本身的内存信息以及借用账本信息;现有使用方：控制面sdk
 * @param[in/out] numaNodeInfoList
 * @return uint32_t, 成功返回0, 失败返回非0
 */
uint32_t UbseAllNumaInfo(std::vector<UbseNumaNodeInfo>& numaNodeInfoList);

// 借用类型
enum class LedgerType
{
    APP,
    WATER, // water 和 app组成numa
    FD,
    SHARE,
    ADDR
};

// 内存借用信息结构体
struct UbseMemoryBorrowInfo {
    std::string name; // 资源名称标识
    LedgerType type = LedgerType::FD;
    std::string borrowNodeId;
    std::vector<int> borrowSocketIdList;
    std::vector<uint32_t> borrowNumaIdList; // 水线借用时未远端numa id
    std::vector<uint64_t> borrowNumaSizeList;
    std::vector<uint64_t> borrowMemId;
    std::string lentNodeId;
    std::vector<int> lentSocketIdList;
    std::vector<uint32_t> lentNumaIdList;
    std::vector<uint64_t> lentNumaSizeList;
    std::vector<uint64_t> lentMemId;
    std::string lentObmmDesc; // hccs场景下导出的obmm描述
    uint64_t size;            // 总借用内存大小（字节）
    int64_t remoteNumaId = -1;
    int16_t borrowNumaId = -1;
};

// numa静态信息中间结构
class NumaStaticInfo {
public:
    std::string nodeId;
    uint32_t soketId{0};
    uint32_t numaId{0};
    std::string hostName;
    std::vector<uint16_t> cpuList;
    uint64_t totalMemSize{0};
    uint64_t freeMemSize{0};
    uint32_t nrHugePages{0};
    uint32_t freeHugePages{0};
    uint64_t timeStamp{0};
    uint32_t ratio{0};
};

// obmm描述中间结构
#ifdef UB_ENVIRONMENT
class ObmmDesc {
public:
    uint64_t addr{0};
    uint64_t length{0};
    uint32_t seid{0};
    uint32_t deid{0};
    uint32_t tokenId{0};
    uint32_t scna{0};
    uint32_t dcna{0};
    uint16_t marId{0};
};
#endif

// 账本动态信息中间结构
class LedgerDymaticInfo {
public:
    std::string resourceId;
    LedgerType type{LedgerType::FD};
    // 共享借用下可以有多个导入节点
    std::vector<std::string> borrowNodeId;
    std::unordered_map<std::string, std::vector<int>> borrowSocketIdList;      // nodeId: socketIds
    std::unordered_map<std::string, std::vector<uint32_t>> borrowNumaIdList;   // nodeId: numaIds
    std::unordered_map<std::string, std::vector<uint64_t>> borrowNumaSizeList; // nodeId: numaSizes
    std::unordered_map<std::string, std::vector<uint64_t>> borrowMemId;        // nodeId: memIds
    std::string lentNodeId;
    std::vector<int> lentSocketIdList;
    std::vector<uint32_t> lentNumaIdList;
    std::vector<uint64_t> lentNumaSizeList;
    std::vector<uint64_t> lentMemId;
    std::vector<ObmmDesc> obmmDesc; // obmm描述信息
    int64_t remoteNumaId{-1};
    int16_t borrowNumaId;
};

// key表示借用的资源标识name_borrowNodeId
using UbseBorrowAccountMap = std::unordered_map<std::string, UbseMemoryBorrowInfo>;

/**
 * @brief 查询账本明细信息;现有使用方：控制面sdk
 * @param[in] nodeId 为空则查询所有节点的账本明细信息，不为空则查询此nodeId节点的账本信息
 * @param[in/out] accountMap
 * @return uint32_t, 成功返回0, 失败返回非0
 */
uint32_t UbseAllBorrowAccountInfo(const std::string& nodeId, UbseBorrowAccountMap& accountMap);

// 共享内存账户信息结构体
struct UbseShmAccountInfo {
    std::vector<uint64_t> exportMemId;                                  // 导出内存ID列表
    std::string exportNode;                                             // 导出节点ID
    std::unordered_map<std::string, std::vector<uint64_t>> importMap{}; // <nodeId:<memIds>>
    uint64_t size;                                                      // 共享内存大小（字节）
};

// key表示借用的资源标识name
using UbseShmAccountMap = std::unordered_map<std::string, UbseShmAccountInfo>;

/**
 * @brief 展示整个机架内的所有节点具体到每个numa的内存相关信息，包括节点本身的内存信息以及借用账本信息;现有使用方：控制面sdk
 * @param[in/out] outMap
 * @return uint32_t, 成功返回0, 失败返回非0
 */
uint32_t UbseAllShmAccountInfo(UbseShmAccountMap& outMap);

// 单个借入/借出项结构体
struct UbseBorrowLentItem {
    std::string nodeId; // 对方节点ID
    uint32_t numaId;    // NUMA节点ID
    uint64_t size;      // 内存大小（字节）
};

// 节点借入借出信息结构体
struct UbseNodeBorrowLentInfo {
    std::string nodeId;                           // 当前节点ID
    std::vector<UbseBorrowLentItem> borrowedItem; // 借入项列表
    std::vector<UbseBorrowLentItem> lentItem;     // 借出项列表
};

// 整体借入借出信息结构
using UbseBorrowedLentInfoList = std::vector<UbseNodeBorrowLentInfo>;

/**
 * @brief 查询每个节点上每个numa的借用信息，单位是字节;现有使用方：控制面sdk
 * @param[in] nodeId
 * @param[in/out] outList
 * @return uint32_t, 成功返回0, 失败返回非0
 */
uint32_t UbseGetBorrowedLentInfo(const std::string& nodeId, UbseBorrowedLentInfoList& outList);

/**
* @brief 调用内部模块获得numa静态信息和账本动态信息
* @param numaInfo [out] numa静态信息
* @param ledgerInfo [out] 账本动态信息
*/
uint32_t GetMemInfoFromInner(std::vector<NumaStaticInfo>& numaInfo, std::vector<LedgerDymaticInfo>& ledgerInfo);
} // namespace ubse::mem::account
#endif // MXE_MEM_ACCOUNT_H
