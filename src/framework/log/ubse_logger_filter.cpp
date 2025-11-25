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

#include "ubse_logger_filter.h"

#include <securec.h>
#include <algorithm>
#include <iostream>
#include <mutex>
#include "ubse_logger.h"
#include "ubse_logger_manager.h"

namespace ubse::log {
UbseLoggerFilter::UbseLoggerFilter(size_t maxHistory) : maxHistorySize(maxHistory), filterCycle(0) {}

UbseLoggerFilter::~UbseLoggerFilter() {}

UbseResult UbseLoggerFilter::SetMaxHistorySize(size_t size)
{
    if (size == 0) {
        std::cerr << "HistorySize cannot be 0." << std::endl;
        return UBSE_ERROR;
    }
    maxHistorySize.store(size, std::memory_order_relaxed);
    std::unique_lock<std::shared_mutex> lock(mutex);
    while (filterHistory.size() > size) {
        // 找到并删除最旧的记录
        auto oldest = std::min_element(filterHistory.begin(), filterHistory.end(), CompareTimestamp);
        filterHistory.erase(oldest);
    }
    return UBSE_OK;
}

UbseResult UbseLoggerFilter::SetFilterCycle(uint32_t cycle)
{
    if (cycle == 0) {
        std::cerr << "FilterCycle cannot be 0." << std::endl;
        return UBSE_ERROR;
    }
    if (cycle == UINT32_MAX) {
        std::cerr << "Invalid FilterCycle." << std::endl;
        return UBSE_ERROR;
    }
    filterCycle.store(cycle, std::memory_order_relaxed);
    return UBSE_OK;
}

bool UbseLoggerFilter::IsLogFilter(UbseLoggerEntry &ubseLoggerEntry)
{
    auto timeStamp = ubseLoggerEntry.GetEntryTimeStamp();
    std::chrono::system_clock::time_point curLogTime =
        std::chrono::system_clock::time_point(std::chrono::microseconds(timeStamp));
    LogFilterInfo info{ ubseLoggerEntry, curLogTime };

    std::chrono::system_clock::time_point lastLogTime;
    if (IsDuplicateLog(info, lastLogTime)) {
        auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(info.timestamp - lastLogTime);
        if (timeDiff.count() < filterCycle.load(std::memory_order_relaxed)) {
            UpdateFilterHistory(info, true, false); // 不更新时间戳
            return true;                            // 重复日志，被过滤
        }
    }

    UpdateFilterHistory(info, false, true); // 更新时间戳
    return false;                           // 允许日志通过
}

bool UbseLoggerFilter::IsDuplicateLog(LogFilterInfo &info, std::chrono::system_clock::time_point &lastLogTime)
{
    LogKey key{ info.ubseLoggerEntry.GetFile(), info.ubseLoggerEntry.GetLine() };
    size_t length{};
    char *buffer = info.ubseLoggerEntry.GetMessage(length);
    key.context = std::make_unique<char[]>(length);
    errno_t ret = memcpy_s(key.context.get(), length, buffer, length);
    if (EOK != ret) {
        return false;
    }
    key.length = length;

    auto it = filterHistory.find(key);
    if (it != filterHistory.end()) {
        lastLogTime = it->second.timestamp;
        return true;
    }
    return false;
}

void UbseLoggerFilter::UpdateFilterHistory(LogFilterInfo &info, bool filtered, bool updateTimestamp)
{
    LogKey key{ info.ubseLoggerEntry.GetFile(), info.ubseLoggerEntry.GetLine() };
    size_t length{};
    char *buffer = info.ubseLoggerEntry.GetMessage(length);
    key.context = std::make_unique<char[]>(length);
    errno_t ret = memcpy_s(key.context.get(), length, buffer, length);
    if (EOK != ret) {
        return;
    }
    key.length = length;

    auto [it, inserted] = filterHistory.try_emplace(std::move(key), info); // 将日志插入历史记录
    if (!inserted) {
        if (updateTimestamp) {
            it->second.timestamp = info.timestamp; // 只在日志通过时更新时间戳
        }
        if (filtered) {
            it->second.filterCount++;
        } else {
            it->second.filterCount = 0; // 重置计数器，如果日志通过
        }
    } else {
        it->second.filterCount = filtered ? 1 : 0;
    }

    // 如果超出最大大小，删除最旧的记录
    while (filterHistory.size() > maxHistorySize.load(std::memory_order_relaxed)) {
        auto oldest = std::min_element(filterHistory.begin(), filterHistory.end(), CompareTimestamp);
        filterHistory.erase(oldest);
    }
}

bool UbseLoggerFilter::CompareTimestamp(const std::pair<const LogKey, LogFilterInfo> &a,
    const std::pair<const LogKey, LogFilterInfo> &b)
{
    return a.second.timestamp < b.second.timestamp;
}
}