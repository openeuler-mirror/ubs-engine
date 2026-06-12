/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_LOGGER_H
#define UBSE_LOGGER_H
#include <cstdint>
#include <cstring>
#include <string>

namespace ubse::log {

#ifndef UBSE_DEFINE_THIS_MODULE
#define UBSE_DEFINE_THIS_MODULE(mn, mid)                                                           \
    static const char* gModuleName = (mn);                                                         \
    _Pragma("GCC diagnostic push")                                                                 \
        _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") static uint32_t gModuleId = (mid); \
    _Pragma("GCC diagnostic pop")
#endif

#ifndef FILENAME
#define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define UBSE_LOG_INTERNAL(gModuleName, LEVEL) \
    ubse::log::UbseLog() == ubse::log::UbseLoggerEntry(gModuleName, LEVEL, FILENAME, __func__, __LINE__)

#define UBSE_LOGGER_CRIT(gModuleName, gModuleId) \
    if (true) {                                  \
    } else                                       \
        std::cout

#define UBSE_LOGGER_ERROR(gModuleName, gModuleId) \
    if (true) {                                   \
    } else                                        \
        std::cout

#define UBSE_LOGGER_WARN(gModuleName, gModuleId) \
    if (true) {                                  \
    } else                                       \
        std::cout

#define UBSE_LOGGER_INFO(gModuleName, gModuleId) \
    if (true) {                                  \
    } else                                       \
        std::cout

#define UBSE_LOGGER_DEBUG(gModuleName, gModuleId) \
    if (true) {                                   \
    } else                                        \
        std::cout

enum class UbseLogLevel : uint32_t
{
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    CRIT = 4
};

void UbseLogOutput(const char* moduleName, UbseLogLevel level, const char* msg);

std::string FormatRetCode(uint32_t retCode);

enum class UbseLoggerTypeId : uint8_t
{
    CHAR = 0,
    UINT32,
    UINT64,
    INT32,
    INT64,
    DOUBLE,
    STRING
};

class UbseLoggerEntry {
public:
    UbseLoggerEntry(const char* gModuleName, UbseLogLevel level, const char* file, const char* func, uint32_t line);
    ~UbseLoggerEntry() = default;
    UbseLoggerEntry() = default;

    void OutPutLog(std::ostream& os);

    UbseLoggerEntry& operator<<(char data);
    UbseLoggerEntry& operator<<(int32_t data);
    UbseLoggerEntry& operator<<(uint32_t data);
    UbseLoggerEntry& operator<<(int64_t data);
    UbseLoggerEntry& operator<<(uint64_t data);
    UbseLoggerEntry& operator<<(double data);
    UbseLoggerEntry& operator<<(const std::string& data);
    UbseLoggerEntry& operator<<(const char* data);
};

bool UbseIsLog(UbseLogLevel level);

struct UbseLog {
    bool operator==(UbseLoggerEntry& loggerEntry);
};
} // namespace ubse::log
#endif // UBSE_LOGGER_H
