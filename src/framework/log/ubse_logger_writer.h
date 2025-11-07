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

#ifndef UBSE_LOGGER_WRITER_H
#define UBSE_LOGGER_WRITER_H
#include <iostream>
#include <utility>
#include "referable/ubse_ref.h"
#include "syslog.h"
#include "ubse_logger.h"

namespace ubse::log {
using namespace ubse::utils;

struct LoggerOptions {
    UbseLogLevel minLogLevel = UbseLogLevel::INFO;
    uint32_t maxFileSizeInMB = 2;   // 日志文件最大大小
    uint32_t rotationFileCount = 2; // 绕接个数
    uint32_t bufferMaxItem = 64;    // 日志缓冲区最大日志条目
    UbseLogLevel minSyslogLevel = UbseLogLevel::INFO;
    std::string logPath;
    bool syslogOpen = false;
    uint32_t syslogType = LOG_USER;
};

class UbseLoggerWriter {
public:
    virtual ~UbseLoggerWriter() = default;
    explicit UbseLoggerWriter() = default;

    virtual bool Write(UbseLoggerEntry &loggerEntry) = 0;
};

class UbseDefaultLoggerWriter : public UbseLoggerWriter {
public:
    explicit UbseDefaultLoggerWriter() = default;

    bool Write(UbseLoggerEntry &loggerEntry) override
    {
        loggerEntry.OutPutLog(std::cout);
        return true;
    }
};
} // namespace ubse::log
#endif // UBSE_LOGGER_WRITER_H
