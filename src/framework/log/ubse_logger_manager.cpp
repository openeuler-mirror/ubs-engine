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
#include <mutex>

#include "ubse_error.h"
#include "ubse_logger_ringbuffer.h"
#include "sys/syslog.h"

namespace ubse::log {
std::atomic<UbseLoggerManager*> UbseLoggerManager::gInstance{nullptr};
bool UbseLoggerManager::gInited_ = false;
std::atomic<bool> UbseLoggerManager::threadRunning_;
constexpr uint32_t kDefaultBufferMaxItem = 4096; // 未初始化时启动缓冲容量，与默认配置 log.queue.maxItem 一致
static std::mutex g_instanceMutex;

UbseLoggerManager::UbseLoggerManager()
{
    // 构造时即分配缓冲，保证配置模块/日志模块初始化之前的日志也能先缓存，不因依赖未就绪而丢失
    logBuffer_ = std::make_unique<LogBuffer>(kDefaultBufferMaxItem);
}

UbseLoggerManager* UbseLoggerManager::Instance()
{
    /* already created */
    auto* instance = gInstance.load(std::memory_order_acquire);
    if (instance != nullptr) {
        return instance;
    }
    std::lock_guard<std::mutex> lock(g_instanceMutex);
    instance = gInstance.load(std::memory_order_relaxed);
    if (instance == nullptr) {
        try {
            instance = new (std::nothrow) UbseLoggerManager();
        } catch (...) {
            instance = nullptr;
        }
        if (instance == nullptr) {
            std::cerr << "Failed to new UbseLogger object, probably out of memory" << std::endl;
            return nullptr;
        }
        gInstance.store(instance, std::memory_order_release);
    }
    return instance;
}

void UbseLoggerManager::Destroy()
{
    /* un-initialize and delete logger */
    auto* instance = gInstance.load(std::memory_order_acquire);
    if (instance != nullptr) {
        if (gInited_) {
            instance->Exit();
        } else {
            uint32_t pending = instance->logBuffer_->PendingCount();
            if (pending > 0) {
                std::cerr << "Discard " << pending << " buffered log entries: logger sink is not initialized."
                          << std::endl;
            }
        }
        // 重置初始化标记，支持 Stop/Start 周期或测试场景下重新初始化
        gInited_ = false;
        std::lock_guard<std::mutex> lock(g_instanceMutex);
        gInstance.store(nullptr, std::memory_order_release);
        delete instance;
    }
}

UbseResult UbseLoggerManager::Init(const LoggerOptions& options, UbseLoggerWriter* logWriter)
{
    if (gInited_) {
        return UBSE_OK;
    }
    /* create */
    if (logWriter == nullptr) {
        return UBSE_ERROR;
    }
    // 配置容量与默认不同时，按配置重建缓冲并迁移启动期日志，保证配置生效且早期日志不丢
    if (options.bufferMaxItem != kDefaultBufferMaxItem) {
        ResizeBuffer(options.bufferMaxItem);
    }
    this->minLogLevel_ = options.minLogLevel;
    this->syslogOpen_ = options.syslogOpen;
    this->syslogType_ = options.syslogType;
    this->writer_ = logWriter;
    threadRunning_.store(true);
    try {
        loggingThread_ = std::thread([this] { UbseLoggerManager::Pop(); });
    } catch (...) {
        std::cerr << "Out of memory or create thread failed." << std::endl;
        return UBSE_ERROR;
    }

    gInited_ = true;
    return UBSE_OK;
}

void UbseLoggerManager::ResizeBuffer(uint32_t newCapacity)
{
    auto oldBuffer = std::move(logBuffer_);
    auto newBuffer = std::make_unique<LogBuffer>(newCapacity);
    // 启动期无消费者，条目都积在writeBuffer_；复用Swap()换入readBuffer_后统一按FIFO迁移
    oldBuffer->Swap();
    UbseLoggerEntry entry(nullptr, UbseLogLevel::INFO, nullptr, nullptr, 0);
    while (oldBuffer->Pop(entry)) {
        newBuffer->Push(std::move(entry)); // 容量不足时丢最新并复用"缓冲满"告警
    }
    logBuffer_ = std::move(newBuffer);
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

void UbseLoggerManager::Push(UbseLoggerEntry&& loggerEntry)
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

void UbseLoggerManager::LogToSyslog(UbseLoggerEntry& loggerEntry)
{
    auto level = loggerEntry.GetLogLevel();
    auto syslogLevel = LogToSyslogLevel(level);
    openlog("ubse", 0, this->syslogType_);
    std::ostringstream oss;
    loggerEntry.FormatSyslog(oss);
    syslog(syslogLevel, "%s", oss.str().c_str());
    closelog();
}

uint32_t UbseLoggerManager::LogToSyslogLevel(UbseLogLevel& level)
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

UbseLogLevel UbseLoggerManager::StringToLogLevel(const std::string& level)
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
