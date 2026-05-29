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

#include "ubse_ipc_log.h"
#include <sys/time.h>
#include <array>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sys/time.h>

namespace ubse::ipc {
UbseIpcLogFunc UbseIpcLog::logFunc_ = nullptr;

UbseIpcLogEntry::UbseIpcLogEntry(UbseIpcLogLevel level, const char* file, const char* func, uint32_t line)
{
    this->level_ = level;
    oss_ << "[" << file << ':' << func << ':' << line << "] ";
}

void UbseIpcLogEntry::OutPutLog()
{
    std::string msg = oss_.str();
    UbseIpcLog::OutPutLog(level_, msg.c_str());
}

void UbseIpcLog::SetLogFunc(UbseIpcLogFunc func)
{
    logFunc_ = func;
}

void UbseIpcLog::Print(uint32_t level, const char* msg)
{
    constexpr std::array<std::string_view, 4> levelStr = {"DEBUG", "INFO", "WARN", "ERROR"};
    constexpr int microsecondWith = 3;
    constexpr int NO_1000 = 1000;

    if (level >= levelStr.size()) {
        std::cerr << "Invalid log level: " << level << '\n'; // 非法的日志级别
        return;
    }

    struct timeval tv {
    };
    char strTime[24] = {};

    int ret = gettimeofday(&tv, nullptr);
    if (ret != 0) {
        std::cout << "Fail to get the current system time, " << ret << ".\n";
    }
    time_t timeStamp = tv.tv_sec;
    struct tm localTime {
    };
    struct tm* resultTime = localtime_r(&timeStamp, &localTime);
    if ((resultTime != nullptr) && (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", resultTime) != 0)) {
        std::cout << strTime << std::setw(microsecondWith) << std::setfill('0') << (tv.tv_usec / NO_1000) << " "
                  << levelStr[level] << " " << msg << '\n';
    } else {
        std::cout << "Invalid time trace " << tv.tv_usec << " " << levelStr[level] << " " << msg << '\n';
    }
}

void UbseIpcLog::OutPutLog(UbseIpcLogLevel level, const char* msg)
{
    if (logFunc_) {
        logFunc_(static_cast<uint32_t>(level), msg);
    } else {
        Print(static_cast<uint32_t>(level), msg);
    }
}

bool Log::operator==(UbseIpcLogEntry& loggerEntry)
{
    loggerEntry.OutPutLog();
    return true;
}
} // namespace ubse::ipc