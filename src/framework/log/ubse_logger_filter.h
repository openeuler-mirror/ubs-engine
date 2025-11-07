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

#ifndef UBSE_LOG_FILTER_H
#define UBSE_LOG_FILTER_H

#include <unordered_map>
#include <shared_mutex>
#include <atomic>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::log {
using namespace ubse::common::def;
struct LogFilterInfo {
    UbseLoggerEntry ubseLoggerEntry;
    std::chrono::system_clock::time_point timestamp;
    uint32_t filterCount = 0;
};

struct LogKey {
    const std::string fileName;
    uint32_t fileLine;
    std::unique_ptr<char[]> context;
    size_t length;

    bool operator == (const LogKey &other) const
    {
        return fileName == other.fileName && fileLine == other.fileLine && length == other.length &&
            (std::memcmp(context.get(), other.context.get(), length) == 0);
    }
};

struct LogKeyHash {
    std::size_t operator () (const LogKey &k) const
    {
        return (std::hash<std::string>{}(k.fileName)) ^ (std::hash<uint32_t>{}(k.fileLine) << 1) ^
            (std::hash<size_t>{}(k.length) << 2); // 左移2位增加哈希值复杂度
    }
};

class UbseLoggerFilter {
public:
    explicit UbseLoggerFilter(size_t maxHistory = 20); // 最大历史记录为20
    ~UbseLoggerFilter();

    UbseLoggerFilter(const UbseLoggerFilter &) = delete;
    UbseLoggerFilter &operator = (const UbseLoggerFilter &) = delete;
    UbseLoggerFilter(UbseLoggerFilter &&) = delete;
    UbseLoggerFilter &operator = (UbseLoggerFilter &&) = delete;

    /* *
     * @brief 设置日志过滤器历史记录的最大数量
     * @param[in] size 最大历史记录数量
     */
    UbseResult SetMaxHistorySize(size_t size);

    /* *
     * @brief 设置日志过滤周期
     * @param[in] cycle 过滤周期 单位秒
     */
    UbseResult SetFilterCycle(uint32_t cycle);

    /* *
     * @brief 检查是否应该记录此日志
     * @param[in] info 日志过滤信息
     */
    bool IsLogFilter(UbseLoggerEntry &ubseLoggerEntry);

private:
    struct ModuleFilterInfo {
        bool enabled = false;
        std::unordered_map<UbseLogLevel, bool> levelFilters;
    };

    std::unordered_map<uint16_t, ModuleFilterInfo> moduleFilters;
    std::unordered_map<LogKey, LogFilterInfo, LogKeyHash> filterHistory;
    std::atomic<size_t> maxHistorySize;
    std::atomic<int> filterCycle; // in seconds

    mutable std::shared_mutex mutex;

    /* *
     * @brief 检查给定的日志信息是否是重复的
     * @param info 要检查的日志信息
     * @param[out] lastLogTime 如果找到重复日志，存储其最后记录时间
     * @return 如果是重复日志返回 true，否则返回 false
     */
    bool IsDuplicateLog(LogFilterInfo &info, std::chrono::system_clock::time_point &lastLogTime);

    /* *
     * @brief 更新过滤历史记录
     * @param info 要更新的日志信息
     * @param filtered 是否被过滤
     * @param updateTimestamp 是否更新时间戳，默认为 true
     */
    void UpdateFilterHistory(LogFilterInfo &info, bool filtered, bool updateTimestamp = true);

    /* *
     * @brief 比较两个日志记录的时间戳
     * @param a 第一个日志记录
     * @param b 第二个日志记录
     * @return 如果 a 的时间戳早于 b 的时间戳，返回 true
     */
    static bool CompareTimestamp(const std::pair<const LogKey, LogFilterInfo> &a,
        const std::pair<const LogKey, LogFilterInfo> &b);
};
} // namespace ubse::log

#endif // UBSE_LOG_FILTER_H
