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
struct ProcessMemNewConfigInfo {
    bool isPid{true};
    std::string identifier{};
    uint64_t maxMemory{0};
    double remoteRatio{0.0};
    uint64_t startTime{0};

    inline ubse::common::def::UbseResult Serialize(ubse::serial::UbseSerialization& serializer) const
    {
        uint8_t isPidFlag = static_cast<uint8_t>(isPid ? 1 : 0);
        serializer << isPidFlag << identifier << maxMemory << remoteRatio << startTime;
        return serializer.Check() ? UBSE_OK : UBSE_ERROR;
    }

    inline ubse::common::def::UbseResult Deserialize(ubse::serial::UbseDeSerialization& deserializer)
    {
        uint8_t isPidFlag = 0;
        deserializer >> isPidFlag >> identifier >> maxMemory >> remoteRatio >> startTime;
        isPid = (isPidFlag != 0);
        return deserializer.Check() ? UBSE_OK : UBSE_ERROR;
    }
};

struct ProcessMemDisplayEntry {
    int32_t pid{0};
    std::string name{};
    uint64_t maxMemory{0};
    double remoteRatio{0.0};

    inline ubse::common::def::UbseResult Serialize(ubse::serial::UbseSerialization& serializer) const
    {
        serializer << pid << name << maxMemory << remoteRatio;
        return serializer.Check() ? UBSE_OK : UBSE_ERROR;
    }

    inline ubse::common::def::UbseResult Deserialize(ubse::serial::UbseDeSerialization& deserializer)
    {
        deserializer >> pid >> name >> maxMemory >> remoteRatio;
        return deserializer.Check() ? UBSE_OK : UBSE_ERROR;
    }
};

inline const std::string PROC_MEM_PID_KEY_PREFIX = "procMem_pid_";
inline const std::string PROC_MEM_NAME_KEY_PREFIX = "procMem_name_";
inline const std::string PROC_MEM_FOSSIL_KEY_PREFIX = "procMem_fossil_";

struct FossilPidConfigInfo {
    std::string name{};
    uint64_t maxMemory{0};
    double remoteRatio{0.0};
    uint64_t startTime{0};

    inline ubse::common::def::UbseResult Serialize(ubse::serial::UbseSerialization& serializer) const
    {
        serializer << name << maxMemory << remoteRatio << startTime;
        return serializer.Check() ? UBSE_OK : UBSE_ERROR;
    }

    inline ubse::common::def::UbseResult Deserialize(ubse::serial::UbseDeSerialization& deserializer)
    {
        deserializer >> name >> maxMemory >> remoteRatio >> startTime;
        return deserializer.Check() ? UBSE_OK : UBSE_ERROR;
    }
};

enum class ConfigSource : uint8_t
{
    PID_CONFIG = 1 << 0,
    NAME_CONFIG = 1 << 1,
};

enum class BorrowSlotStatus : uint8_t
{
    BORROWING,
    COMPLETED,
    FAILED,
};

enum class AtomicMigrateResult : uint8_t
{
    kOk,
    kFail,
    kVanish,
};

enum class ReturnStatus : uint8_t
{
    NONE,
    RETURNING,
    RETURNED,
};

struct ReturnRequestItem {
    std::string name{};
    uint64_t size{0};
};

struct BorrowSlot {
    uint64_t capacity{0};
    uint64_t migratedBytes{0};
    int32_t exportSlotId{-1};
    std::string debtId{};
    int srcNumaId{-1};
    int remoteNumaId{-1};
    std::chrono::steady_clock::time_point borrowTime{};
    BorrowSlotStatus status{BorrowSlotStatus::BORROWING};
    ReturnStatus returnStatus{ReturnStatus::NONE};
};

struct BorrowState {
    uint64_t currentRemote{0};
    std::vector<BorrowSlot> slots;
    std::unordered_map<int, uint64_t> remoteNumaMigrated;
};

struct BorrowCandidate {
    pid_t pid{0};
    bool isChild{false};
    bool hasActiveBorrow{false};
    std::chrono::steady_clock::time_point lastMigrateTime{};
    uint64_t actual{0};
    uint64_t maxMemory{0};
    double remoteRatio{0.0};
    uint64_t currentRemote{0};
    uint64_t canMigrate{0};
};

enum class ProcessStatus
{
    IDLE,        // 无借用
    WAIT_BORROW, // 等待借用
    BORROWING,   // 借用中
    REPAYING,    // 归还中
    INACTIVE,    // 已停止
    FAULT,       // 故障隔离中（借出节点故障，暂停业务处理）
    BORROWED,
};

struct ManagedPidEntry {
    pid_t pid{0};
    pid_t parentPid{0};
    bool isChild{false};
    uint8_t sources{0};
    std::string nameConfigName{};
    uint64_t maxMemory{0};
    double remoteRatio{0.0};

    uint64_t vmRss{0};

    // 进程启动时刻（/proc/<pid>/stat 第22字段），用于识别退出后 pid 被复用
    uint64_t startTime{0};

    std::chrono::steady_clock::time_point lastMigrateTime{};

    ProcessStatus processStatus{ProcessStatus::IDLE};
    BorrowState borrow{};
};

inline std::optional<pid_t> ParsePidFromIdentifier(const std::string& identifier)
{
    if (identifier.empty() || identifier.find_first_not_of("0123456789") != std::string::npos) {
        return std::nullopt;
    }
    try {
        pid_t pid = static_cast<pid_t>(std::stoi(identifier));
        if (pid > 0) {
            return pid;
        }
    } catch (...) {
    }
    return std::nullopt;
}

enum class UsrInfoPluginType : uint32_t
{
    PROCESS_MEM = 123,
};

struct ProcessMemUsrInfo {
    UsrInfoPluginType pluginId{UsrInfoPluginType::PROCESS_MEM};
    int32_t pid{};
    int64_t startTime{};
    int32_t srcNuma{-1};
};
static_assert(sizeof(ProcessMemUsrInfo) <= ubse::mem::controller::UBSE_MAX_USR_INFO_LEN,
              "ProcessMemUsrInfo must fit within usrInfo buffer");

// ========== 兼容层: 旧模型符号(将在后续提交移除) ==========
const uint32_t PROCESS_MEM_MODULE_CODE = 123;
const uint32_t PROCESS_MEM_SERVICE_ID = 11;

struct ProcessMemPidConfigInfo {
    pid_t pid{};
    int evictThreshold{};
    int targetEvictThreshold{};
    int reclaimThreshold{};
    uint64_t expectedMemoryUsage{};
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

enum class BorrowStatus
{
    COMPLETED,
    CREATING,
};

struct DebtInfo {
    std::chrono::steady_clock::time_point borrowStartTime{}; // 借用开始时间，自定义超时时间，借用中的账本需要关注
    BorrowStatus status{BorrowStatus::COMPLETED};
    ubse::mem::controller::UbseMemNumaDesc numaDesc{};
};

constexpr uint16_t INVALID_REMOTE_NUMA = 0;

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
inline const std::string PROCESS_MEM_NAME_PREFIX = "ProcessMem_";
} // namespace process_mem::def
#endif
