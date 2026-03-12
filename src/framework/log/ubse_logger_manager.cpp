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
bool UbseLoggerManager::gInited_ = false;
std::atomic<bool> UbseLoggerManager::threadRunning_;

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
        if (gInited_) {
            gInstance->Exit();
        }
        delete gInstance;
        gInstance = nullptr;
    }
}

UbseResult UbseLoggerManager::Init(const LoggerOptions &options, UbseLoggerWriter *logWriter)
{
    if (gInited_) {
        return UBSE_OK;
    }
    /* create */
    if (logWriter == nullptr) {
        return UBSE_ERROR;
    }
    this->minLogLevel_ = options.minLogLevel;
    this->syslogOpen_ = options.syslogOpen;
    this->syslogType_ = options.syslogType;
    gInstance->writer_ = logWriter;
    threadRunning_.store(true);
    try {
        logBuffer_ = std::make_unique<LogBuffer>(options.bufferMaxItem);
        loggingThread_ = std::thread([this] { UbseLoggerManager::Pop(); });
    } catch (...) {
        std::cerr << "Out of memory or create thread failed." << std::endl;
        return UBSE_ERROR;
    }

    gInited_ = true;
    return UBSE_OK;
}

void UbseLoggerManager::Exit()
{
    std::unique_lock<std::shared_mutex> lock(logBuffer_->mtx_);
    logBuffer_->stop_ = true;
    lock.unlock();
    threadRunning_.store(false);
    if (loggingThread_.joinable()) {
        loggingThread_.join();
    }
}

bool UbseLoggerManager::IsLog(UbseLogLevel level)
{
    return level >= minLogLevel_;
}

void UbseLoggerManager::Push(UbseLoggerEntry &&loggerEntry)
{
    logBuffer_->Push(std::move(loggerEntry));
}

void UbseLoggerManager::Pop()
{
    UbseLoggerEntry loggerEntry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    while (threadRunning_.load()) {
        if (logBuffer_->Pop(loggerEntry)) {
            writer_->Write(loggerEntry);
            // syslog打开且不被过滤
            if (syslogOpen_) {
                LogToSyslog(loggerEntry);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 环形缓冲区无数据则线程休眠1毫秒
        }
    }
    while (logBuffer_->Pop(loggerEntry)) {
        writer_->Write(loggerEntry);
        // syslog打开且不被过滤
        if (this->syslogOpen_) {
            LogToSyslog(loggerEntry);
        }
    }
}

void UbseLoggerManager::LogToSyslog(UbseLoggerEntry &loggerEntry)
{
    auto level = loggerEntry.GetLogLevel();
    auto syslogLevel = LogToSyslogLevel(level);
    openlog("ubse", 0, this->syslogType_);
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
    minLogLevel_ = level;
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