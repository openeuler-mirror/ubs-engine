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

#include "process_mem_pid_migrate_ledger.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "ubse_error.h"
#include "ubse_logger.h"

namespace process_mem::ledger {
UBSE_DEFINE_THIS_MODULE("process_mem");

namespace {

// 行文本格式: "pid srcNumaId destNumaId bytes", 源/目标为 -1 表示无源/无目标
constexpr size_t kRowFieldCount = 4;

// (src,dest) 各按 int16 位域编码为 32 位 pairKey, 解码还原可保留 -1 哨兵
constexpr uint32_t EncodePairKey(int32_t srcNumaId, int32_t destNumaId)
{
    return (static_cast<uint32_t>(static_cast<uint16_t>(static_cast<int16_t>(srcNumaId))) << 16) |
           static_cast<uint32_t>(static_cast<uint16_t>(static_cast<int16_t>(destNumaId)));
}

int32_t DecodeKeySrc(uint32_t key)
{
    return static_cast<int32_t>(static_cast<int16_t>(key >> 16));
}

int32_t DecodeKeyDest(uint32_t key)
{
    return static_cast<int32_t>(static_cast<int16_t>(key & 0xFFFF));
}

bool ParseInt64(const std::string& token, int64_t& val)
{
    errno = 0;
    char* end = nullptr;
    val = strtoll(token.c_str(), &end, 10);
    return end != token.c_str() && *end == '\0' && errno == 0;
}

std::optional<PidMigrateRow> ParseRowLine(const std::string& line)
{
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    if (tokens.size() != kRowFieldCount) {
        return std::nullopt;
    }
    int64_t pid = 0;
    int64_t srcNumaId = 0;
    int64_t destNumaId = 0;
    int64_t bytes = 0;
    if (!ParseInt64(tokens[0], pid) || !ParseInt64(tokens[1], srcNumaId) || !ParseInt64(tokens[2], destNumaId) ||
        !ParseInt64(tokens[3], bytes)) {
        return std::nullopt;
    }
    if (pid <= 0 || pid > INT32_MAX || srcNumaId < -1 || srcNumaId > INT16_MAX || destNumaId < -1 ||
        destNumaId > INT16_MAX || bytes <= 0) {
        return std::nullopt;
    }
    PidMigrateRow row;
    row.pid = static_cast<int32_t>(pid);
    row.srcNumaId = static_cast<int32_t>(srcNumaId);
    row.destNumaId = static_cast<int32_t>(destNumaId);
    row.bytes = static_cast<uint64_t>(bytes);
    return row;
}

// 逐行容错加载: 坏行跳过并告警, 不影响其余行; 返回是否成功打开文件
bool LoadRowsFromFile(const std::string& filePath, MigrateLedgerRows& rows)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        UBSE_LOG_WARN << "[process_mem] migrate ledger: open " << filePath << " failed, persist disabled";
        return false;
    }
    std::string line;
    size_t lineNum = 0;
    while (std::getline(file, line)) {
        ++lineNum;
        auto row = ParseRowLine(line);
        if (!row.has_value()) {
            UBSE_LOG_WARN << "[process_mem] migrate ledger: skip bad line " << lineNum;
            continue;
        }
        rows[row->pid][EncodePairKey(row->srcNumaId, row->destNumaId)] = row->bytes;
    }
    return true;
}

// 序列化: 按 (pid, src, dest) 排序输出, 保证全表文本稳定可 diff
std::string BuildFileText(const MigrateLedgerRows& rows)
{
    struct KeyedRow {
        int32_t pid;
        uint32_t key;
        uint64_t bytes;
    };
    std::vector<KeyedRow> items;
    for (const auto& [pid, inner] : rows) {
        for (const auto& [key, bytes] : inner) {
            items.push_back({pid, key, bytes});
        }
    }
    std::sort(items.begin(), items.end(),
              [](const KeyedRow& a, const KeyedRow& b) { return a.pid != b.pid ? a.pid < b.pid : a.key < b.key; });
    std::string text;
    text.reserve(items.size() * 40);
    for (const auto& item : items) {
        text += std::to_string(item.pid);
        text += ' ';
        text += std::to_string(DecodeKeySrc(item.key));
        text += ' ';
        text += std::to_string(DecodeKeyDest(item.key));
        text += ' ';
        text += std::to_string(item.bytes);
        text += '\n';
    }
    return text;
}

// tmpfs 原子落盘: 整表写入 .tmp 后 rename 替换, 崩溃只丢最后一笔且被下一笔覆盖
uint32_t WriteFileAtomic(const std::string& filePath, const std::string& text)
{
    const std::string tmpPath = filePath + ".tmp";
    int fd = open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        UBSE_LOG_ERROR << "[process_mem] migrate ledger: open tmp " << tmpPath << " failed, errno=" << errno;
        return static_cast<uint32_t>(errno);
    }
    size_t offset = 0;
    while (offset < text.size()) {
        ssize_t written = write(fd, text.data() + offset, text.size() - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            break;
        }
        offset += static_cast<size_t>(written);
    }
    if (offset != text.size()) {
        uint32_t err = errno != 0 ? static_cast<uint32_t>(errno) : 1;
        UBSE_LOG_ERROR << "[process_mem] migrate ledger: write tmp failed, errno=" << errno;
        close(fd);
        unlink(tmpPath.c_str());
        return err;
    }
    if (close(fd) != 0 || rename(tmpPath.c_str(), filePath.c_str()) != 0) {
        UBSE_LOG_ERROR << "[process_mem] migrate ledger: rename " << tmpPath << " failed, errno=" << errno;
        unlink(tmpPath.c_str());
        return static_cast<uint32_t>(errno);
    }
    return UBSE_OK;
}

} // namespace

ProcessMemPidMigrateLedger& ProcessMemPidMigrateLedger::GetInstance()
{
    static ProcessMemPidMigrateLedger instance;
    return instance;
}

uint32_t ProcessMemPidMigrateLedger::Init(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rows_.clear();
    filePath_ = filePath;
    fileReady_ = false;

    const std::filesystem::path parent = std::filesystem::path(filePath_).parent_path();
    std::error_code ec;
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }
    if (ec) {
        UBSE_LOG_WARN << "[process_mem] migrate ledger: create dir " << parent.string()
                      << " failed, persist disabled, in-memory ledger only";
        return UBSE_OK;
    }
    fileReady_ = true;

    if (!std::filesystem::exists(filePath_, ec) || ec) {
        UBSE_LOG_INFO << "[process_mem] migrate ledger init: file not exist, start fresh";
        return UBSE_OK;
    }
    LoadRowsFromFile(filePath_, rows_);
    UBSE_LOG_INFO << "[process_mem] migrate ledger init: loaded rows=" << CountRowsLocked();
    return UBSE_OK;
}

void ProcessMemPidMigrateLedger::UnInit()
{
    std::lock_guard<std::mutex> lock(mutex_);
    PersistLocked();
}

void ProcessMemPidMigrateLedger::ReplacePidRows(pid_t pid, int32_t srcNumaId,
                                                const std::vector<std::pair<int32_t, uint64_t>>& destRows)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rows_.erase(pid);
    for (const auto& [destNumaId, bytes] : destRows) {
        if (bytes == 0) {
            continue;
        }
        rows_[pid][EncodePairKey(srcNumaId, destNumaId)] = bytes;
    }
    PersistLocked();
}

void ProcessMemPidMigrateLedger::MoveRowBytes(pid_t pid, int32_t srcNumaId, int32_t oldDestNumaId,
                                              int32_t newDestNumaId, uint64_t bytes)
{
    if (bytes == 0 || oldDestNumaId == newDestNumaId) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    int32_t rowSrc = srcNumaId;
    auto pidIt = rows_.find(pid);
    if (pidIt != rows_.end()) {
        auto oldIt = pidIt->second.find(EncodePairKey(rowSrc, oldDestNumaId));
        if (oldIt == pidIt->second.end() && rowSrc != 0) {
            // pid 行键按 migrate_out 声明源统一: 混源/旧账声明源退化 0, 与债务 usrInfo.srcNuma
            // 可不同, 按 0 兜底再找; 命中则新行沿用 0 键保持行键统一, 避免旧行残留造成双记
            auto fallbackIt = pidIt->second.find(EncodePairKey(0, oldDestNumaId));
            if (fallbackIt != pidIt->second.end()) {
                oldIt = fallbackIt;
                rowSrc = 0;
            }
        }
        if (oldIt != pidIt->second.end()) {
            if (oldIt->second > bytes) {
                oldIt->second -= bytes;
            } else {
                pidIt->second.erase(oldIt);
            }
        } else {
            UBSE_LOG_WARN << "[process_mem] migrate ledger: move row not found pid=" << pid << " src=" << srcNumaId
                          << " old_dest=" << oldDestNumaId << " new_dest=" << newDestNumaId << " bytes=" << bytes;
        }
    }
    rows_[pid][EncodePairKey(rowSrc, newDestNumaId)] += bytes;
    PersistLocked();
}

void ProcessMemPidMigrateLedger::ClearPid(pid_t pid)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (rows_.erase(pid) != 0) {
        PersistLocked();
    }
}

size_t ProcessMemPidMigrateLedger::CountRowsLocked() const
{
    size_t count = 0;
    for (const auto& [pid, inner] : rows_) {
        count += inner.size();
    }
    return count;
}

uint32_t ProcessMemPidMigrateLedger::PersistLocked()
{
    if (!fileReady_) {
        return UBSE_OK;
    }
    return WriteFileAtomic(filePath_, BuildFileText(rows_));
}

} // namespace process_mem::ledger
