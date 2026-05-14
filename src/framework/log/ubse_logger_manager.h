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

#ifndef UBSE_LOGGER_MANAGER_H
#define UBSE_LOGGER_MANAGER_H

#include "ubse_common_def.h"
#include "ubse_logger.h"
#include "ubse_logger_filesink.h"
#include "ubse_logger_ringbuffer.h"
#include "ubse_logger_writer.h"
#include "sys/syslog.h"

namespace ubse::log {
using namespace ubse::common::def;
using namespace ubse::utils;

class UbseLoggerManager {
public:
    static UbseLoggerManager* Instance();

    static void Destroy();

    explicit UbseLoggerManager() = default;

    ~UbseLoggerManager() = default;

    UbseResult Init(const LoggerOptions& options, UbseLoggerWriter* logWriter);

    void Exit();

    bool IsLog(UbseLogLevel level);

    void Push(UbseLoggerEntry&& loggerEntry);

    void Pop();
    void LogToSyslog(UbseLoggerEntry& loggerEntry);

    static uint32_t LogToSyslogLevel(UbseLogLevel& level);

    void SetLogLevel(UbseLogLevel level);

    static UbseLogLevel StringToLogLevel(const std::string& level);

    static UbseLoggerManager* gInstance;

private:
    static bool gInited_;
    UbseLogLevel minLogLevel_ = UbseLogLevel::INFO;
    bool syslogOpen_ = false;
    uint32_t syslogType_ = LOG_USER;
    static std::atomic<bool> threadRunning_;
    std::unique_ptr<LogBuffer> logBuffer_;
    std::thread loggingThread_;
    UbseLoggerWriter* writer_ = nullptr;
};
} // namespace ubse::log

#endif // UBSE_LOGGER_MANAGER_H