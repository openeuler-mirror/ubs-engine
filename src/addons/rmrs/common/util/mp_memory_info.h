/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MP_MEMORY_INFO_H
#define MP_MEMORY_INFO_H

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "ubse_str_util.h"
#include "mp_json_util.h"

namespace mempooling {
using namespace ubse::utils;

const uint64_t KB = 1024;
const uint64_t MB = KB * KB;
const uint64_t TB = MB * KB;

struct RackNumaMemInfo {
    // numa节点编号，如0、1等。
    uint16_t numaId;

    // socket编号，如0、1等。
    uint16_t socketId;

    // 总内存大小，单位kB
    uint64_t memTotal;

    // 空闲内存大小，单位kB
    uint64_t memFree;

    // 使用内存大小，单位kB
    uint64_t memUsed;

    // nrHugepages 单位kB
    uint64_t vmMemTotal;

    // freeHugepages 单位kB
    uint64_t vmMemFree;

    // nrHugepages -  freeHugepages 单位kB
    uint64_t vmUsedMem;

    // 新增字段：预留内存大小 单位kB
    uint64_t reservedMem;

    // 新增字段：借出内存大小 单位kB
    uint64_t lentMem;

    // 新增字段：共享内存大小 单位kB
    uint64_t sharedMem;

    // 新增字段: UB可借内存大小 单位KB
    uint64_t canBorrowMem;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "RackNumaMemInfo(numaId=" << numaId << ", socketId=" << socketId << ", memTotal=" << memTotal
            << ", memFree=" << memFree << ", memUsed=" << memUsed << ", vmMemTotal=" << vmMemTotal
            << ", vmMemFree=" << vmMemFree << ", vmUsedMem=" << vmUsedMem << ", reservedMem=" << reservedMem
            << ", lentMem=" << lentMem << ", sharedMem=" << sharedMem << ", canBorrowMem=" << canBorrowMem << ")";
        return oss.str();
    }
};

struct RackMemNumaPair {
    // 节点ID，节点唯一标识。
    std::string nodeId;

    // numa节点编号，如0、1等。
    int16_t numaId;

    // 内存大小 单位kB
    uint64_t memorySize;
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "RackMemNumaPair(nodeId=" << nodeId << ", numaId=" << numaId << ", memorySize=" << memorySize << ")";
        return oss.str();
    }
};

/**
// 节点内存借用关系
 */
struct RackBorrowedAndLentInfo {
    // 借入内存节点信息
    std::vector<RackMemNumaPair> borrowedItem;

    // 借出内存节点信息
    std::vector<RackMemNumaPair> lentItem;
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "RackBorrowedAndLentInfo(\n  borrowedItem=[";
        for (const auto& item : borrowedItem) {
            oss << "\n    " << item.ToString();
        }
        oss << "\n  ],\n  lentItem=[";
        for (const auto& item : lentItem) {
            oss << "\n    " << item.ToString();
        }
        oss << "\n  ]\n)";
        return oss.str();
    }
};

/**
// 节点内存信息
 */
struct NodeMemoryInfo {
    // 时间戳。
    time_t timestamp;

    // 节点ID，节点唯一标识。
    std::string nodeId;

    // 总内存大小，单位kB
    uint64_t totalMemory;

    // 已使用内存大小，单位kB
    uint64_t usedMemory;

    // 空余内存大小，单位kB
    uint64_t freeMemory;

    // 借出内存大小，单位kB
    uint64_t lentMemory;

    // numa层面内存信息
    std::vector<RackNumaMemInfo> numaMemInfo;
};

uint64_t StringToKB(std::string& str);

struct NodeMemoryInfoList {
    std::vector<NodeMemoryInfo> nodeMemoryInfoList;
    bool FromJson(const std::string& jsonString);
    bool ParseNodeMemoryInfoMap(const JSON_VEC& nodeMemoryInfoListVec, const int& i, JSON_MAP& nodeMemoryInfoMap);
    bool ParseNumaMemInfoMap(const JSON_VEC& numaMemInfoVec, const int& i, const int& j, JSON_MAP& numaMemInfoMap);
    bool CreateNodeMemoryInfoListVec(const std::string& jsonString, JSON_MAP& NodeMemoryInfoListMAP,
                                     JSON_VEC& nodeMemoryInfoListVec);
    bool CreateNumaMemInfoVec(JSON_VEC& numaMemInfoVec, const int& i, JSON_MAP& nodeMemoryInfoMap);
};

struct NodeMemoryInfoWithReservedMem : NodeMemoryInfo {
    uint64_t reservedMem;
    uint64_t sharedMem;
    uint16_t socketId;
    uint64_t canBorrowMem;

    NodeMemoryInfoWithReservedMem() : reservedMem(0), sharedMem(0), socketId(0), canBorrowMem(0) {}
    NodeMemoryInfoWithReservedMem(const NodeMemoryInfo& nodeMemoryInfo, uint64_t reservedMem_, uint64_t sharedMem_)
        : NodeMemoryInfo(nodeMemoryInfo),
          reservedMem(reservedMem_),
          sharedMem(sharedMem_),
          socketId(0),
          canBorrowMem(0)
    {
    }
    NodeMemoryInfoWithReservedMem& operator=(const NodeMemoryInfo& nodeMemoryInfo)
    {
        if (this != &nodeMemoryInfo) {
            NodeMemoryInfo::operator=(nodeMemoryInfo);
        }
        return *this;
    }
    std::string ToString();
};

} // namespace mempooling
#endif // MP_MEMORY_INFO_H