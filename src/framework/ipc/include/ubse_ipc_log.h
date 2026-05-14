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

#ifndef UBSE_IPC_LOG_H
#define UBSE_IPC_LOG_H

#include <cstdint>
#include <sstream>

#ifndef FILENAME
#define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define IPC_LOG_ERROR \
    ubse::ipc::Log() == ubse::ipc::UbseIpcLogEntry(ubse::ipc::UbseIpcLogLevel::ERROR, FILENAME, __func__, __LINE__)

#define IPC_LOG_WARN \
    ubse::ipc::Log() == ubse::ipc::UbseIpcLogEntry(ubse::ipc::UbseIpcLogLevel::WARN, FILENAME, __func__, __LINE__)

#define IPC_LOG_INFO \
    ubse::ipc::Log() == ubse::ipc::UbseIpcLogEntry(ubse::ipc::UbseIpcLogLevel::INFO, FILENAME, __func__, __LINE__)

#define IPC_LOG_DEBUG \
    ubse::ipc::Log() == ubse::ipc::UbseIpcLogEntry(ubse::ipc::UbseIpcLogLevel::DEBUG, FILENAME, __func__, __LINE__)

namespace ubse::ipc {
using UbseIpcLogFunc = void (*)(uint32_t, const char*);

enum class UbseIpcLogLevel : uint32_t {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class UbseIpcLogEntry {
public:
    UbseIpcLogEntry(UbseIpcLogLevel level, const char* file, const char* func, uint32_t line);
    ~UbseIpcLogEntry() = default;

    template <typename T>
    UbseIpcLogEntry& operator<<(T data)
    {
        oss_ << data;
        return *this;
    }

    void OutPutLog();

private:
    UbseIpcLogLevel level_{};
    std::ostringstream oss_;
};

class UbseIpcLog {
public:
    static void SetLogFunc(UbseIpcLogFunc func);

    static void Print(uint32_t level, const char* msg);

    static void OutPutLog(UbseIpcLogLevel level, const char* msg);

private:
    static UbseIpcLogFunc logFunc_;
};

struct Log {
    bool operator==(UbseIpcLogEntry& loggerEntry);
};
} // namespace ubse::ipc

#endif // UBSE_IPC_LOG_H
