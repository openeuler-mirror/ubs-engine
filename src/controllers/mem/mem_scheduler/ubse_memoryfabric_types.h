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

#ifndef UBSE_MEMORYFABRIC_TYPES_H
#define UBSE_MEMORYFABRIC_TYPES_H

#include <regex.h>
#include <securec.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>
#include <fstream>
#include <list>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "ubse_mem_constants.h"

namespace ubse::mem::strategy {
using BResult = uint32_t;
using NodeId = std::string; /* 节点真实ID，agent的nodeid，不算manager */
using SocketId = int;
using NumaId = int64_t;

using NodeIndex = int16_t;
using NumaIndex = int16_t;       // 节点内编号
using SocketIndex = int16_t;     // 节点内编号
using GlobalNumaIndex = int32_t; // 全局编号
using IpAddress = std::pair<std::string, uint16_t>;
using UbseResult = uint32_t;
using mem_id = uint64_t;
#ifndef MAX_NUMA_NODES
#define MAX_NUMA_NODES 16
#endif

#ifdef UB_ENVIRONMENT
#define INVALID_MEM_ID 0
#endif

enum class BorrowSceneType : uint16_t {
    APP_APPLY,
    WATER_MARK,
    OOM_EVENTS,
    APP_PRI_APPLY
};

inline void GeneKey(std::string &keyPrefix, std::string &key, int optType, std::string nodeId)
{
    std::string scene = "vm";
    // 获取当前时间点
    auto currentTime = std::chrono::system_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime.time_since_epoch());
    auto curTime = seconds.count();

    auto oneHourAgo = std::chrono::system_clock::now() - std::chrono::hours(1);
    auto hours = std::chrono::duration_cast<std::chrono::hours>(oneHourAgo.time_since_epoch());
    auto oneHourAgoTime = hours.count();
    keyPrefix = "/ubse." + scene + ".memOpt." + std::to_string(oneHourAgoTime);
    key = "." + std::to_string(optType) + "." + std::to_string(curTime) + "_" + nodeId;
}

enum class ObmmDevType : uint32_t {
    REMOTE_NUMA,    // 水线场景，create numa
    APP_BORROW_DEV, // 共享，本地设备呈现
    APP_SHARE_DEV   // 共享，本地设备呈现
};

struct UbseMemTierProcessTubseing {
    int pid{};      // 进程ID
    int scanTime{}; // 扫描周期
    int type{};     // 扫描类型，0--水线扫描、1--确定性迁移扫描
};

struct MemSocketInfo {
    int memTotal;
    int memUsed;
    int memExport;
    int memImport;
};

struct MemHostInfo {
    char hostName[MAX_HOSTNAME_LENGTH];
    int num;
    MemSocketInfo socket[TOPOLOGY_MAX_SOCKET_PER_HOST];
};

struct MemClusterInfo {
    int num; // 集群可用节点数量
    MemHostInfo host[TOPOLOGY_MAX_HOST_NUM];
};
struct BaseNetContext {
    uint32_t pid = 0;                                     // process id
    uid_t uid = 0;                                        // user id
    gid_t gid = 0;                                        // group id
    ObmmDevType obmmDevType{ObmmDevType::APP_BORROW_DEV}; // obmm 请求类型
    friend std::ostream &operator<<(std::ostream &os, const BaseNetContext &obj)
    {
        return os << "pid: " << obj.pid << " uid: " << obj.uid << " gid: " << obj.gid
                  << " obmmDevType: " << static_cast<uint32_t>(obj.obmmDevType);
    }
};
class SpinLock {
public:
    SpinLock() = default;
    ~SpinLock() = default;

    SpinLock(const SpinLock &) = delete;
    SpinLock &operator=(const SpinLock &) = delete;
    SpinLock(SpinLock &&) = delete;
    SpinLock &operator=(SpinLock &&) = delete;

    inline void TryLock()
    {
        mFlag.test_and_set(std::memory_order_acquire);
    }

    inline void Lock()
    {
        while (mFlag.test_and_set(std::memory_order_acquire)) {}
    }

    inline void UnLock()
    {
        mFlag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag mFlag = ATOMIC_FLAG_INIT;
};

class UbseMemSpinLocker {
public:
    explicit UbseMemSpinLocker(SpinLock *lock) : mLock(lock)
    {
        if (mLock != nullptr) {
            mLock->Lock();
        }
    }

    ~UbseMemSpinLocker()
    {
        if (mLock != nullptr) {
            mLock->UnLock();
        }
    }

    UbseMemSpinLocker(const UbseMemSpinLocker &) = delete;
    UbseMemSpinLocker &operator=(const UbseMemSpinLocker &) = delete;
    UbseMemSpinLocker(UbseMemSpinLocker &&) = delete;
    UbseMemSpinLocker &operator=(UbseMemSpinLocker &&) = delete;

private:
    SpinLock *mLock = nullptr;
};
} // namespace ubse::mem::strategy

#endif