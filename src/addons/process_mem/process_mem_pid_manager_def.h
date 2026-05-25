/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef PROCESS_MEM_PID_MANAGER_DEF_H
#define PROCESS_MEM_PID_MANAGER_DEF_H
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "ubse_error.h"
#include "ubse_mem_controller.h"
#include "ubse_serial_util.h"
namespace process_mem::def {
const uint32_t PROCESS_MEM_MODULE_CODE = 123;
const uint32_t PROCESS_MEM_SERVICE_ID = 11;

/**
 * @brief 进程运行时状态
 */
enum class ProcessStatus {
    IDLE,        // 无借用
    WAIT_BORROW, // 等待借用
    BORROWING,   // 借用中
    REPAYING,    // 归还中
    INACTIVE,    // 已停止
    FAULT,       // 故障隔离中（借出节点故障，暂停业务处理）
};

struct ProcessMemPidConfigInfo {
    pid_t pid;
    int evictThreshold;
    int targetEvictThreshold;
    int reclaimThreshold;
    uint64_t expectedMemoryUsage;
    std::optional<uint64_t> srcNumaId; // redis进程存在的numaId，可选
    inline ubse::common::def::UbseResult SerializeConfigInfo(ubse::serial::UbseSerialization& serializer) const
    {
        serializer << pid << evictThreshold << targetEvictThreshold << reclaimThreshold << expectedMemoryUsage;
        uint64_t hasSrcNuma = srcNumaId.has_value() ? 1 : 0;
        serializer << hasSrcNuma;
        if (srcNumaId.has_value()) {
            serializer << srcNumaId.value();
        }
        return serializer.Check() ? UBSE_OK : UBSE_ERROR;
    }

    inline ubse::common::def::UbseResult DeserializeConfigInfo(ubse::serial::UbseDeSerialization& deserializer)
    {
        deserializer >> pid >> evictThreshold >> targetEvictThreshold >> reclaimThreshold >> expectedMemoryUsage;
        uint64_t hasSrcNuma = 0;
        deserializer >> hasSrcNuma;
        if (hasSrcNuma != 0) {
            uint64_t val = 0;
            deserializer >> val;
            srcNumaId = val;
        } else {
            srcNumaId = std::nullopt;
        }
        return deserializer.Check() ? UBSE_OK : UBSE_ERROR;
    }
};

enum class BorrowStatus {
    COMPLETED,
    CREATING,
};

struct DebtInfo {
    std::chrono::steady_clock::time_point borrowStartTime{}; // 借用开始时间，自定义超时时间，借用中的账本需要关注
    BorrowStatus status{BorrowStatus::COMPLETED};
    ubse::mem::controller::UbseMemNumaDesc numaDesc{};
};

struct BorrowInfo {
    int32_t remoteNumaId{-1};
    int32_t exportSlotId{-1};                            // 导出的节点Id
    int32_t importSocketId{-1};                          // 导入的socketId
    std::unordered_map<std::string, DebtInfo> debtInfos; // key-value: name-debtInfo
};

struct ProcessMemPidInfo {
    pid_t ppid{0};     // 父进程 ID（0 表示无父进程）
    long startTime{0}; // 进程启动时间

    // ========== 配置信息（持久化） ==========
    ProcessMemPidConfigInfo configInfo{}; // 配置信息

    // ========== 运行时状态（不持久化） ==========
    ProcessStatus processStatus{ProcessStatus::IDLE}; // 进程状态

    // ========== 内存借用信息（持久化于ubse） ==========
    BorrowInfo memBorrowInfo; // 借用内存的信息

    // ========== 子进程信息 (不持久化)==========
    // std::unordered_map<pid_t, ProcessMemPidInfo> childrenInfo; // 子进程信息映射

    inline ubse::common::def::UbseResult SerializePidInfo(ubse::serial::UbseSerialization& serializer) const
    {
        auto ret = configInfo.SerializeConfigInfo(serializer);
        if (ret != UBSE_OK) {
            return ret;
        }
        serializer << startTime;
        return serializer.Check() ? UBSE_OK : UBSE_ERROR;
    }

    inline ubse::common::def::UbseResult DeserializePidInfo(ubse::serial::UbseDeSerialization& deserializer)
    {
        auto ret = configInfo.DeserializeConfigInfo(deserializer);
        if (ret != UBSE_OK) {
            return ret;
        }
        deserializer >> startTime;
        return deserializer.Check() ? UBSE_OK : UBSE_ERROR;
    }
};

struct PidCollectInfo {
    std::unordered_map<uint32_t, size_t> numaMemDistribution{}; // 每个numa上内存分布单位为 byte.
    std::vector<pid_t> childrenInfo{};                          // 子进程信息
};

enum class UsrInfoPluginType : uint32_t {
    PROCESS_MEM = 0,
};

inline const std::string PROCESS_MEM_NAME_PREFIX = "ProcessMem_";

struct ProcessMemUsrInfo {
    UsrInfoPluginType pluginId{UsrInfoPluginType::PROCESS_MEM};
    int32_t pid{};
    int64_t startTime{};
};
static_assert(sizeof(ProcessMemUsrInfo) <= ubse::mem::controller::UBSE_MAX_USR_INFO_LEN,
              "ProcessMemUsrInfo must fit within usrInfo buffer");
} // namespace process_mem::def
#endif
