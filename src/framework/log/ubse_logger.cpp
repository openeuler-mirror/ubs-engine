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

#include "ubse_logger.h"

#include <bits/chrono.h>         // for duration_cast, duration, high_resol...
#include <unistd.h>              // for getpid, pid_t
#include <algorithm>             // for max
#include <ctime>                 // for time_t, gmtime, strftime
#include <iomanip>               // for operator<<, setfill, setw
#include <iostream>              // for basic_ostream, operator<<, ostream
#include <new>                   // for nothrow
#include <utility>               // for move

#include "securec.h"             // for memcpy_s, EOK, errno_t
#include "ubse_logger_manager.h" // for UbseLoggerManager

namespace ubse::log {
static uint64_t GetTimeStamp()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::high_resolution_clock::now().time_since_epoch())
                                     .count());
}

static pid_t GetProcessId()
{
    static pid_t pid = getpid();
    return pid;
}

static std::thread::id GetThreadId()
{
    static thread_local const std::thread::id tid = std::this_thread::get_id();
    return tid;
}

static void FormatTimestamp(std::ostringstream &oss, uint64_t timestamp)
{
    std::time_t timet = static_cast<std::time_t>(timestamp / 1000000) + 8 * 60 * 60; // 时区8小时为8*60*60秒
    std::tm time;
    gmtime_r(&timet, &time);
    char buffer[32]; // 设置缓冲区大小为32
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %T.", &time);
    uint64_t milliseconds = (timestamp % 1000000) / 1000; // timestamp % 1000000提取时间戳微秒部分
    oss << '[' << buffer << std::setw(3) << std::setfill('0') << milliseconds << " +0800]"; // 毫秒格式化3位
}

static const char *LogLevelToString(UbseLogLevel level)
{
    switch (level) {
        case UbseLogLevel::DEBUG:
            return "DEBUG";
        case UbseLogLevel::INFO:
            return "INFO";
        case UbseLogLevel::WARN:
            return "WARN";
        case UbseLogLevel::ERROR:
            return "ERROR";
        case UbseLogLevel::CRIT:
            return "CRIT";
        default:
            return "UNKNOWN";
    }
}

void UbseLogOutput(const char *moduleName, UbseLogLevel level, const char *msg)
{
    UbseIsLog(level) && UbseLog() == (UbseLoggerEntry(moduleName, level, FILENAME, nullptr, __LINE__) << msg);
}

std::string FormatRetCode(uint32_t retCode)
{
    std::string retCodeString = "ErrorCode=" + std::to_string(retCode);
    return retCodeString;
}

UbseLoggerEntry::UbseLoggerEntry(const char *gModuleName, UbseLogLevel level, const char *file, const char *func,
                                 uint32_t line)
    : moduleName(gModuleName),
      level(level),
      file(file),
      func(func),
      line(line),
      maxSize(sizeof(logEntryBuffer)),
      currentSize(0)
{
    timeStamp = GetTimeStamp();
    pid = GetProcessId();
    tid = GetThreadId();
}

UbseLoggerEntry::UbseLoggerEntry(const UbseLoggerEntry &other)
    : moduleName(other.moduleName),
      timeStamp(other.timeStamp),
      pid(other.pid),
      tid(other.tid),
      level(other.level),
      file(other.file),
      func(other.func),
      line(other.line),
      maxSize(other.maxSize),
      currentSize(other.currentSize)
{
    if (other.heapBuffer) {
        heapBuffer = std::make_unique<char[]>(maxSize);
        errno_t ret = memcpy_s(heapBuffer.get(), currentSize, other.heapBuffer.get(), currentSize);
        if (EOK != ret) {
            heapBuffer.reset();
            std::cerr << "Failed to copy heapBuffer." << std::endl;
        }
    } else {
        errno_t ret = memcpy_s(logEntryBuffer, currentSize, other.logEntryBuffer, currentSize);
        if (EOK != ret) {
            std::cerr << "Failed to copy logEntryBuffer." << std::endl;
        }
    }
}

UbseLoggerEntry &UbseLoggerEntry::operator=(const UbseLoggerEntry &other)
{
    if (this == &other) {
        return *this;
    }

    moduleName = other.moduleName;
    timeStamp = other.timeStamp;
    pid = other.pid;
    tid = other.tid;
    level = other.level;
    file = other.file;
    func = other.func;
    line = other.line;
    maxSize = other.maxSize;
    currentSize = other.currentSize;

    if (other.heapBuffer) {
        heapBuffer = std::make_unique<char[]>(maxSize);
        errno_t ret = memcpy_s(heapBuffer.get(), currentSize, other.heapBuffer.get(), currentSize);
        if (EOK != ret) {
            heapBuffer.reset();
            std::cerr << "ERROR: failed to copy." << std::endl;
        }
    } else {
        heapBuffer.reset();
        errno_t ret = memcpy_s(logEntryBuffer, currentSize, other.logEntryBuffer, currentSize);
        if (EOK != ret) {
            std::cerr << "ERROR: failed to copy." << std::endl;
        }
    }

    return *this;
}

void UbseLoggerEntry::OutPutLog(std::ostream &os)
{
    std::ostringstream oss;
    char *start = !heapBuffer ? &logEntryBuffer[0] : &(heapBuffer.get())[0];
    const char *const end = start + currentSize;

    FormatTimestamp(oss, timeStamp);
    oss << '[' << LogLevelToString(level) << "][" << pid << "][" << tid << "]";
    if (func) {
        oss << "[" << file << ':' << func << ':' << line << "] ";
    }
    DecodeData(oss, start, end);
    os << oss.str() << std::endl;
}
void UbseLoggerEntry::FormatSyslog(std::ostream &os)
{
    std::ostringstream oss;
    char *start = !heapBuffer ? &logEntryBuffer[0] : &(heapBuffer.get())[0];
    const char *const end = start + currentSize;
    if (func) {
        oss << "[" << file << ':' << func << ':' << line << "] ";
    }
    DecodeData(oss, start, end);
    os << oss.str() << std::endl;
}
const char *UbseLoggerEntry::GetModuleName()
{
    return moduleName;
}

const char *UbseLoggerEntry::GetFile()
{
    return file;
}

uint32_t UbseLoggerEntry::GetLine()
{
    return line;
}

UbseLogLevel UbseLoggerEntry::GetLogLevel()
{
    return level;
}
uint64_t UbseLoggerEntry::GetEntryTimeStamp()
{
    return timeStamp;
}

char *UbseLoggerEntry::GetMessage(size_t &length)
{
    length = currentSize;
    return !heapBuffer ? &logEntryBuffer[0] : &(heapBuffer.get())[0];
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(char data)
{
    EncodeData<char>(UbseLoggerTypeId::CHAR, data);
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(int32_t data)
{
    EncodeData<int32_t>(UbseLoggerTypeId::INT32, data);
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(uint32_t data)
{
    EncodeData<uint32_t>(UbseLoggerTypeId::UINT32, data);
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(int64_t data)
{
    EncodeData<int64_t>(UbseLoggerTypeId::INT64, data);
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(uint64_t data)
{
    EncodeData<uint64_t>(UbseLoggerTypeId::UINT64, data);
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(double data)
{
    EncodeData<double>(UbseLoggerTypeId::DOUBLE, data);
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(const std::string &data)
{
    EncodeString(data.c_str(), data.length());
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(const char *data)
{
    EncodeData(data);
    return *this;
}

uint32_t UbseLoggerEntry::ResizeBuffer(size_t addSize)
{
    size_t const newSize = currentSize + addSize;
    if (newSize <= maxSize) {
        return UBSE_OK;
    }

    maxSize = std::max(static_cast<size_t>(2 * maxSize), newSize); // 重新分配2倍大小buffer
    if (!heapBuffer) {
        heapBuffer.reset(new (std::nothrow) char[maxSize]);
        if (heapBuffer == nullptr) {
            return UBSE_ERROR;
        }
        memcpy_s(heapBuffer.get(), currentSize, logEntryBuffer, currentSize);
        return UBSE_OK;
    } else {
        std::unique_ptr<char[]> newHeapBuffer = std::make_unique<char[]>(maxSize);
        if (newHeapBuffer == nullptr) {
            return UBSE_ERROR;
        }
        memcpy_s(newHeapBuffer.get(), currentSize, heapBuffer.get(), currentSize);
        heapBuffer.swap(newHeapBuffer);
        return UBSE_OK;
    }
}

char *UbseLoggerEntry::GetBuffer()
{
    if (!heapBuffer) {
        return &logEntryBuffer[currentSize];
    }
    return &(heapBuffer.get())[currentSize];
}

void UbseLoggerEntry::EncodeString(const char *data, size_t length)
{
    if (length == 0) {
        return;
    }
    UbseResult ret = ResizeBuffer(sizeof(UbseLoggerTypeId) + length + 1);
    if (ret != UBSE_OK) {
        return;
    }
    char *buffer = GetBuffer();
    *reinterpret_cast<UbseLoggerTypeId *>(buffer++) = UbseLoggerTypeId::STRING;
    memcpy_s(buffer, length + 1, data, length + 1);
    currentSize += sizeof(UbseLoggerTypeId) + length + 1;
}

void UbseLoggerEntry::EncodeData(const char *data)
{
    if (data != nullptr) {
        EncodeString(data, strlen(data));
    }
}

char *UbseLoggerEntry::DecodeChar(std::ostream &os, char *buffer)
{
    char data = *reinterpret_cast<char *>(buffer);
    os << data;
    return buffer + sizeof(char);
}

char *UbseLoggerEntry::DecodeUint(std::ostream &os, char *buffer)
{
    uint32_t data = *reinterpret_cast<uint32_t *>(buffer);
    os << data;
    return buffer + sizeof(uint32_t);
}

char *UbseLoggerEntry::DecodeUlong(std::ostream &os, char *buffer)
{
    uint64_t data = *reinterpret_cast<uint64_t *>(buffer);
    os << data;
    return buffer + sizeof(uint64_t);
}

char *UbseLoggerEntry::DecodeInt(std::ostream &os, char *buffer)
{
    int32_t data = *reinterpret_cast<int32_t *>(buffer);
    os << data;
    return buffer + sizeof(int32_t);
}

char *UbseLoggerEntry::DecodeLong(std::ostream &os, char *buffer)
{
    int64_t data = *reinterpret_cast<int64_t *>(buffer);
    os << data;
    return buffer + sizeof(int64_t);
}
char *UbseLoggerEntry::DecodeDouble(std::ostream &os, char *buffer)
{
    double data = *reinterpret_cast<double *>(buffer);
    os << data;
    return buffer + sizeof(double);
}

char *UbseLoggerEntry::DecodeString(std::ostream &os, char *buffer)
{
    while (*buffer != '\0') {
        os << *buffer;
        ++buffer;
    }
    return ++buffer;
}

void UbseLoggerEntry::DecodeData(std::ostream &os, char *start, const char *end)
{
    while (start < end) {
        auto id = static_cast<UbseLoggerTypeId>(*start);
        start++;
        switch (id) {
            case UbseLoggerTypeId::CHAR:
                start = DecodeChar(os, start);
                break;
            case UbseLoggerTypeId::UINT32:
                start = DecodeUint(os, start);
                break;
            case UbseLoggerTypeId::UINT64:
                start = DecodeUlong(os, start);
                break;
            case UbseLoggerTypeId::INT32:
                start = DecodeInt(os, start);
                break;
            case UbseLoggerTypeId::INT64:
                start = DecodeLong(os, start);
                break;
            case UbseLoggerTypeId::DOUBLE:
                start = DecodeDouble(os, start);
                break;
            case UbseLoggerTypeId::STRING:
                start = DecodeString(os, start);
                break;
            default:
                return;
        }
    }
}

bool UbseIsLog(UbseLogLevel level)
{
    if (!ubse::log::UbseLoggerManager::gInstance) {
        return false;
    }
    return ubse::log::UbseLoggerManager::gInstance->IsLog(level);
}
bool UbseLog::operator==(UbseLoggerEntry &loggerEntry)
{
    ubse::log::UbseLoggerManager::gInstance->Push(std::move(loggerEntry));
    return true;
}
} // namespace ubse::log