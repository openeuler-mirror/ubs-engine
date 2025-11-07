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

#include "ubse_logger_manager.h"
#include <iostream>

#include "sys/syslog.h"
#include "ubse_error.h"
#include "ubse_logger_ringbuffer.h"

namespace ubse::log {
UbseLoggerManager *UbseLoggerManager::gInstance = nullptr;
bool UbseLoggerManager::gInited = false;
std::atomic<bool> UbseLoggerManager::threadRunning;

UbseLoggerManager *UbseLoggerManager::Instance()
{
    /* already created */
    if (gInstance != nullptr) {
        return gInstance;
    }
    gInstance = new (std::nothrow) UbseLoggerManager();
    if (gInstance == nullptr) {
        std::cerr << "Failed to new UbseLogger object, probably out of memory";
        return nullptr;
    }
    return gInstance;
}

void UbseLoggerManager::Destroy()
{
    /* un-initialize and delete logger */
    if (gInstance != nullptr) {
        if (gInited) {
            gInstance->Exit();
        }
        delete gInstance;
        gInstance = nullptr;
    }
}

UbseResult UbseLoggerManager::Init(const LoggerOptions &options, UbseLoggerWriter *logWriter)
{
    if (gInited) {
        return UBSE_OK;
    }
    /* create */
    if (logWriter == nullptr) {
        return UBSE_ERROR;
    }
    this->minLogLevel = options.minLogLevel;
    this->syslogOpen = options.syslogOpen;
    this->syslogType = options.syslogType;
    gInstance->writer = logWriter;
    threadRunning.store(true);
    ubseLoggerFilter.SetFilterCycle(5); // 设置过滤周期为5秒
    try {
        logBuffer = std::make_unique<LogBuffer>(options.bufferMaxItem);
        loggingThread = std::thread([this] { UbseLoggerManager::Pop(); });
    } catch (...) {
        std::cerr << "Out of memory or create thread failed." << std::endl;
        return UBSE_ERROR;
    }

    gInited = true;
    return UBSE_OK;
}

void UbseLoggerManager::Exit()
{
    std::unique_lock<std::shared_mutex> lock(logBuffer->mtx);
    logBuffer->stop = true;
    lock.unlock();
    threadRunning.store(false);
    if (loggingThread.joinable()) {
        loggingThread.join();
    }
}

bool UbseLoggerManager::IsLog(UbseLogLevel level)
{
    return level >= minLogLevel;
}

void UbseLoggerManager::Push(UbseLoggerEntry &&loggerEntry)
{
    logBuffer->Push(std::move(loggerEntry));
}

void UbseLoggerManager::Pop()
{
    UbseLoggerEntry loggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    while (threadRunning.load()) {
        if (logBuffer->Pop(loggerEntry)) {
            if (ubseLoggerFilter.IsLogFilter(loggerEntry)) {
                continue;
            }
            writer->Write(loggerEntry);
            // syslog打开且不被过滤
            if (syslogOpen) {
                LogToSyslog(loggerEntry);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 环形缓冲区无数据则线程休眠1毫秒
        }
    }
    while (logBuffer->Pop(loggerEntry)) {
        if (ubseLoggerFilter.IsLogFilter(loggerEntry)) {
            continue;
        }
        writer->Write(loggerEntry);
        // syslog打开且不被过滤
        if (this->syslogOpen) {
            LogToSyslog(loggerEntry);
        }
    }
}

void UbseLoggerManager::LogToSyslog(UbseLoggerEntry &loggerEntry)
{
    auto level = loggerEntry.GetLogLevel();
    auto syslogLevel = LogToSyslogLevel(level);
    openlog("ubse", 0, this->syslogType);
    std::ostringstream oss;
    loggerEntry.FormatSyslog(oss);
    syslog(syslogLevel, "%s", oss.str().c_str());
    closelog();
}

uint32_t UbseLoggerManager::LogToSyslogLevel(UbseLogLevel &level)
{
    if (level == UbseLogLevel::DEBUG) {
        return LOG_DEBUG;
    }
    if (level == UbseLogLevel::INFO) {
        return LOG_INFO;
    }
    if (level == UbseLogLevel::WARN) {
        return LOG_WARNING;
    }
    if (level == UbseLogLevel::ERROR) {
        return LOG_ERR;
    }
    if (level == UbseLogLevel::CRIT) {
        return LOG_CRIT;
    }
    return LOG_INFO;
}

void UbseLoggerManager::SetLogLevel(UbseLogLevel level)
{
    minLogLevel = level;
}

UbseLogLevel UbseLoggerManager::StringToLogLevel(const std::string &level)
{
    if (level == "DEBUG") {
        return UbseLogLevel::DEBUG;
    }
    if (level == "INFO") {
        return UbseLogLevel::INFO;
    }
    if (level == "WARN") {
        return UbseLogLevel::WARN;
    }
    if (level == "ERROR") {
        return UbseLogLevel::ERROR;
    }
    if (level == "CRIT") {
        return UbseLogLevel::CRIT;
    }
    return UbseLogLevel::INFO;
}
} // namespace ubse::log