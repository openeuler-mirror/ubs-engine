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

#ifndef UBSE_MEM_META_DATA_H
#define UBSE_MEM_META_DATA_H

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ubse_mem_resource.h"
#include "ubse_mem_constants.h"
#include "ubse_memoryfabric_types.h"
#include "ubse_node_controller.h"

namespace std {
template <>
struct hash<ubse::resource::mem::UbseMemNumaLoc> {
    std::size_t operator()(const ubse::resource::mem::UbseMemNumaLoc &key) const noexcept
    {
        static std::hash<int> intHash;
        static std::hash<int64_t> int64Hash;
        static std::hash<string> strHash;
        return strHash(key.nodeId) ^ intHash(key.socketId) ^ int64Hash(key.numaId);
    }
};
} // namespace std
namespace ubse::mem::strategy {
using namespace ubse::resource::mem;
struct NumaData {
    std::string nodeId;
    int socketId;
    int64_t numaId{};
};
struct SocketData {
    int socketId{};
    std::string nodeId{};
    std::vector<NumaData> numas;
};
struct NodeData {
    std::string nodeId{};
    std::vector<SocketData> sockets;
    bool status;
};

struct NodeDataWithNumaInfo {
    NodeData nodeData;
    std::vector<ubse::nodeController::UbseNumaInfo> numaInfo;
};

class MemWaterMarkHolder {
public:
    [[nodiscard]] int16_t GetUsedHigh() const
    {
        return usedHigh;
    }

    bool SetUsedHigh(const int16_t usedHigh0)
    {
        const auto change = this->usedHigh.load() != usedHigh0;
        this->usedHigh.store(usedHigh0);
        return change;
    }

    [[nodiscard]] int16_t GetUsedLow() const
    {
        return usedLow;
    }

    bool SetUsedLow(const int16_t usedLow0)
    {
        const auto change = this->usedLow.load() != usedLow0;
        this->usedLow.store(usedLow0);
        return change;
    }

    static MemWaterMarkHolder &GetInstance()
    {
        static MemWaterMarkHolder instance;
        return instance;
    }
    MemWaterMarkHolder(const MemWaterMarkHolder &other) = delete;
    MemWaterMarkHolder(MemWaterMarkHolder &&other) = delete;
    MemWaterMarkHolder &operator=(const MemWaterMarkHolder &other) = delete;
    MemWaterMarkHolder &operator=(MemWaterMarkHolder &&other) noexcept = delete;

private:
    std::atomic_int16_t usedHigh{0}; /* 标识使用量高 配置的百分比 单位%，numa配置按node一样 */
    std::atomic_int16_t usedLow{0};  /* *标识空闲量高 配置的百分比 */
    MemWaterMarkHolder() = default;
};

struct UbseMemNumaIndexLoc {
    NodeIndex nodeIndex{INVALID_META_ID};
    SocketIndex socketIndex{INVALID_META_ID}; /* Node内从0索引，非所有Socket的索引 */
    NumaIndex numaIndex{INVALID_META_ID};     /* Node内从0索引，非所有numa的索引 */
    bool operator==(const UbseMemNumaIndexLoc &other) const
    {
        return nodeIndex == other.nodeIndex && socketIndex == other.socketIndex && numaIndex == other.numaIndex;
    }
    // 重载 < 运算符
    bool operator<(const UbseMemNumaIndexLoc &other) const
    {
        if (nodeIndex != other.nodeIndex) {
            return nodeIndex < other.nodeIndex;
        }
        if (socketIndex != other.socketIndex) {
            return socketIndex < other.socketIndex;
        }
        return numaIndex < other.numaIndex;
    }
    friend std::ostream &operator<<(std::ostream &os, const UbseMemNumaIndexLoc &obj)
    {
        return os << "nodeIndex: " << obj.nodeIndex << " socketIndex: " << obj.socketIndex
                  << " numaIndex: " << obj.numaIndex;
    }
};

class MemNumaInfo {
public:
    MemNumaInfo(const UbseMemNumaLoc &ubseMemNumaLoc, const UbseMemNumaIndexLoc &ubseMemNumaIndexLoc,
                const GlobalNumaIndex globalIndex)
        : mUbseMemNumaLoc{ubseMemNumaLoc},
          mUbseMemNumaIndexLoc{ubseMemNumaIndexLoc},
          mGlobalIndex{globalIndex}
    {
    }
    void Update(const bool status, time_t timestamp, const uint64_t memTotal, const uint64_t memUsed,
                const uint64_t memFree, const std::vector<unsigned short> &cpuList)
    {
        this->mStatus.store(status);
        this->mTimestamp.store(timestamp);
        this->mMemTotal.store(memTotal, std::memory_order_relaxed);
        this->mMemUsed.store(memUsed, std::memory_order_relaxed);
        this->mMemFree.store(memFree, std::memory_order_relaxed);
        int16_t percent = memTotal == 0 ? 0 : memUsed * MAX_PERCENT / memTotal;
        this->mPercent.store(percent);
        mLock.lock();
        this->mCpuList = cpuList;
        mLock.unlock();
    }
    void UpdateActualMemTotal(const uint64_t &memTotal)
    {
        this->mActualMemTotal.store(memTotal, std::memory_order_relaxed);
    }

    const UbseMemNumaLoc mUbseMemNumaLoc{};
    const UbseMemNumaIndexLoc mUbseMemNumaIndexLoc{};
    const GlobalNumaIndex mGlobalIndex{INVALID_META_ID};
    std::atomic_bool mStatus{false};   /* 因数组管理，用于标识当前结构体是否有效 */
    std::atomic<time_t> mTimestamp{0}; /* 数据采集的时间，格式：Unix时间戳，单位秒 */
    std::atomic_uint_fast64_t mMemTotal{0}; /* 本地的内存容量，不包含借入内存、借出内存、共享内存，单位：Byte */
    std::atomic_uint_fast64_t mActualMemTotal{0}; /* 实际的内存总量， */
    std::atomic_uint_fast64_t mMemUsed{0};        /* memTotal 中已使用的内存容量，单位：Byte */
    std::atomic_uint_fast64_t mMemFree{0};        /* memTotal 中没有使用的内存容量，单位：Byte */
    std::vector<unsigned short> mCpuList;
    std::atomic_uint_fast64_t mMemBorrowed{0};      /* 已借入的内存容量，单位：Byte */
    std::atomic_uint_fast32_t mWaterBorrowCount{0}; /* 该numa水线借用的次数 */
    std::atomic_uint_fast64_t mMemLent{0};          /* 已借出的内存容量，单位：Byte */
    std::atomic_uint_fast64_t mMemShared{0};        /* 提供共享内存的容量，单位：Byte */
    std::atomic_int_fast16_t mPercent{0};           /* 标识当前实际使用比例 配置的百分比 */
    std::atomic_int_fast16_t mLastWarningCount{0}; // 每次触发告警后设为0， 超过5次后不管上次状态是什么，都触发

private:
    std::mutex mLock{};
};
} // namespace ubse::mem::strategy
// 哈希函数
template <>
struct std::hash<ubse::mem::strategy::UbseMemNumaIndexLoc> {
    std::size_t operator()(const ubse::mem::strategy::UbseMemNumaIndexLoc &key) const noexcept
    {
        static std::hash<int> intHash;
        static std::hash<int64_t> int64Hash;
        return intHash(key.nodeIndex) ^ intHash(key.socketIndex) ^ int64Hash(key.nodeIndex);
    }
};
namespace ubse::mem::strategy {
class MemNodeInfo {
public:
    MemNodeInfo(const NodeIndex nodeIndex, const NodeData &nodeData)
        : mNodeId(nodeData.nodeId),
          mNodeIndex(nodeIndex),
          mNodeData{nodeData} {};

    BResult Init(std::unordered_map<UbseMemNumaLoc, UbseMemNumaIndexLoc> &id2Index,
                 std::unordered_map<UbseMemNumaIndexLoc, UbseMemNumaLoc> &index2Id,
                 GlobalNumaIndex &beginGlobalIndex)
    {
        std::unique_lock<std::shared_mutex> writeLocker(mLock);
        mCurSocketIndex = 0;
        mCurNumaIndex = 0;
        if (mNodeData.nodeId == INVALID_META_STRID || mNodeData.sockets.empty() ||
            mNodeData.sockets.size() > TOPOLOGY_MAX_SOCKET_PER_HOST) {
            return E_CODE_MANAGER;
        }
        /* 创建socketInfo 和numa map */
        for (size_t i = 0; i < mNodeData.sockets.size(); i++) {
            auto curSocketData = mNodeData.sockets[i];
            if (curSocketData.numas.empty() || curSocketData.numas.size() > TOPOLOGY_MAX_NUMA_PER_SOCKET) {
                return E_CODE_MANAGER;
            }
            // 创建socket下面的numa
            for (size_t numaI = 0; numaI < curSocketData.numas.size(); ++numaI) {
                auto curNuma = curSocketData.numas[numaI];
                if (curNuma.numaId == INVALID_META_ID) {
                    return E_CODE_MANAGER;
                }

                ubse::mem::strategy::UbseMemNumaLoc ubseMemNumaLoc = {curNuma.nodeId, curNuma.socketId,
                    curNuma.numaId};
                ubse::mem::strategy::UbseMemNumaIndexLoc ubseMemNumaIndexLoc = {mNodeIndex, mCurSocketIndex,
                                                                                mCurNumaIndex};
                std::shared_ptr<MemNumaInfo> numa;
                try {
                    numa = std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, beginGlobalIndex);
                } catch (...) {
                    return E_CODE_MANAGER;
                }

                mNumaInfoIdMap[ubseMemNumaLoc] = numa;
                mNumaInfoIndexMap[ubseMemNumaIndexLoc] = numa;
                id2Index[ubseMemNumaLoc] = ubseMemNumaIndexLoc;
                index2Id[ubseMemNumaIndexLoc] = ubseMemNumaLoc;
                mCurNumaIndex++;
                ++beginGlobalIndex;
            }
            mCurSocketIndex++;
        }
        return 0;
    }

    std::shared_ptr<MemNumaInfo> GetNumaInfoById(const UbseMemNumaLoc &id)
    {
        std::shared_lock<std::shared_mutex> readLocker(mLock);
        auto find = mNumaInfoIdMap.find(id);
        if (find == mNumaInfoIdMap.end()) {
            return nullptr;
        }
        return find->second;
    }
    std::vector<std::shared_ptr<MemNumaInfo>> GetAllNumaInfo()
    {
        std::shared_lock<std::shared_mutex> readLocker(mLock);
        std::vector<std::shared_ptr<MemNumaInfo>> result{};
        for (const auto &pair : mNumaInfoIdMap) {
            result.push_back(pair.second);
        }
        return result;
    }

    std::shared_ptr<MemNumaInfo> GetNumaInfoByIndex(const UbseMemNumaIndexLoc &index)
    {
        std::shared_lock<std::shared_mutex> readLocker(mLock);
        auto find = mNumaInfoIndexMap.find(index);
        if (find == mNumaInfoIndexMap.end()) {
            return nullptr;
        }
        return find->second;
    }

    bool IsStatus()
    {
        std::shared_lock<std::shared_mutex> readLocker(mLock);
        return mStatus;
    }

    void SetStatus(const bool status)
    {
        std::unique_lock<std::shared_mutex> writeLocker(mLock);
        this->mStatus = status;
    }

    void ResetAccountInfo() noexcept
    {
        std::unique_lock<std::shared_mutex> writeLocker(mLock);
        for (auto &numa : mNumaInfoIdMap) {
            if (numa.second != nullptr) {
                numa.second->mMemBorrowed.store(0, std::memory_order_relaxed);
                numa.second->mWaterBorrowCount.store(0, std::memory_order_relaxed);
                numa.second->mMemLent.store(0, std::memory_order_relaxed);
                numa.second->mMemShared.store(0, std::memory_order_relaxed);
            }
        }
    }

    BResult GetNodeLeftShareMemSize(int64_t &nodeLeftShareMem)
    {
        return 0;
    }

    [[nodiscard]] NodeData GetMNodeData() const
    {
        return mNodeData;
    }

    // 节点的socket数量
    [[nodiscard]] SocketIndex GetMCurSocketIndex() const
    {
        return mCurSocketIndex;
    }

    // node的numa数量
    [[nodiscard]] NumaIndex GetMCurNumaIndex() const
    {
        return mCurNumaIndex;
    }

    const NodeId mNodeId{INVALID_META_STRID};
    const NodeIndex mNodeIndex{INVALID_META_ID};
    const NodeData mNodeData{};

private:
    std::shared_mutex mLock;
    SocketIndex mCurSocketIndex{0}; /* 节点内索引 */
    NumaIndex mCurNumaIndex{0};     /* 节点内索引 */
    std::map<UbseMemNumaLoc, std::shared_ptr<MemNumaInfo>> mNumaInfoIdMap{};
    std::map<UbseMemNumaIndexLoc, std::shared_ptr<MemNumaInfo>> mNumaInfoIndexMap{};
    bool mStatus{false}; /* 算法要等所有node收到agent资源上报后才初始化 */
};
} // namespace ubse::mem::strategy

#endif // UBSE_MEM_META_DATA_H