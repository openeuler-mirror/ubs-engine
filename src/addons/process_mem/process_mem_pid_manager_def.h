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
#include <set>
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

// 槽生命周期单一状态: BORROWING(在途迁出) → COMPLETED(落地占用) → RETURNING(归还迁回中) → 删槽;
// FAILED 由超时处理置位后随即删除, 不落账本
enum class BorrowSlotStatus : uint8_t
{
    BORROWING,
    COMPLETED,
    RETURNING,
    FAILED,
};

enum class AtomicMigrateResult : uint8_t
{
    kOk,
    kFail,
    kVanish,
    kFaultNoMigrate,
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
};

struct BorrowState {
    uint64_t currentRemote{0};
    std::vector<BorrowSlot> slots;
    std::unordered_map<int, uint64_t> remoteNumaMigrated;
    // r2r 替换过的旧债务名集合: 远端迁远端为同步接口, 迁移完成后替换槽已置 COMPLETED(数据在远端 B),
    // 集合仅用于归还重播/孤儿归还识别旧债名, 防止重复建债重复迁移; 旧债删除成功后移除
    std::set<std::string> r2rReplacedDebts;
};

struct BorrowCandidate {
    pid_t pid{0};
    bool isChild{false};
    bool hasActiveBorrow{false};
    bool overCap{false};
    std::chrono::steady_clock::time_point lastMigrateTime{};
    uint64_t actual{0};
    uint64_t maxMemory{0};
    double remoteRatio{0.0};
    uint64_t currentRemote{0};
    uint64_t canMigrate{0};
};

enum class ProcessStatus
{
    IDLE,
    BORROWING,
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
} // namespace process_mem::def
#endif
