/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_LOGGER_H
#define UBSE_LOGGER_H
#include <sys/types.h> // for pid_t
#include <cstdint>     // for uint32_t, uint64_t, int32_t, int64_t, uint8_t
#include <cstring>     // for size_t, strrchr
#include <iomanip>
#include <iosfwd> // for ostream
#include <iostream>
#include <memory>      // for unique_ptr
#include <string>      // for string
#include <thread>      // for thread
#include <type_traits> // for enable_if, is_same

namespace ubse::log {

/**
 * @brief 定义Module名，为调试日志写入的文件名
 * @param mn [in] Module名，如ubse
 */
#ifndef UBSE_DEFINE_THIS_MODULE
#define UBSE_DEFINE_THIS_MODULE(mn, mid)                                                           \
    static const char* gModuleName = (mn);                                                         \
    _Pragma("GCC diagnostic push")                                                                 \
        _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") static uint32_t gModuleId = (mid); \
    _Pragma("GCC diagnostic pop")
#endif

/**
 * @brief 文件名的宏
 */
#ifndef FILENAME
#define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

/**
 * @brief 创建单条日志，写到终端或写入文件
 * @param gModuleName [in] Module名，为调试日志写入的文件名
 * @param LEVEL [in] 单条日志级别
 */
#define UBSE_LOG_INTERNAL(gModuleName, LEVEL) \
    ubse::log::UbseLog() == ubse::log::UbseLoggerEntry(gModuleName, LEVEL, FILENAME, __func__, __LINE__)

/**
 * @brief 创建单条CRIT级别日志，写到终端或写入文件
 * @param gModuleName [in] Module名，为调试日志写入的文件名
 */
#define UBSE_LOGGER_CRIT(gModuleName, gModuleId) \
    if (true) {                                  \
    } else                                       \
        std::cout

/**
 * @brief 创建单条ERROR级别日志，写到终端或写入文件
 * @param gModuleName [in] Module名，为调试日志写入的文件名
 */
#define UBSE_LOGGER_ERROR(gModuleName, gModuleId) \
    if (true) {                                   \
    } else                                        \
        std::cout

/**
 * @brief 创建单条WARN级别日志，写到终端或写入文件
 * @param gModuleName [in] Module名，为调试日志写入的文件名
 */
#define UBSE_LOGGER_WARN(gModuleName, gModuleId) \
    if (true) {                                  \
    } else                                       \
        std::cout

/**
 * @brief 创建单条INFO级别日志，写到终端或写入文件
 * @param gModuleName [in] Module名，为调试日志写入的文件名
 */
#define UBSE_LOGGER_INFO(gModuleName, gModuleId) \
    if (true) {                                  \
    } else                                       \
        std::cout

/**
 * @brief 创建单条DEBUG级别日志，写到终端或写入文件
 * @param gModuleName [in] Module名，为调试日志写入的文件名
 */
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

/**
 * @brief 三方库注册日志函数接口
 * @param moduleName [in] Module名，为调试日志写入的文件名
 * @param level [in] 日志级别
 * @param msg [in] 日志内容
 */
void UbseLogOutput(const char* moduleName, UbseLogLevel level, const char* msg);

/**
 * @brief 格式化错误码
 * @param retCode [in] 错误码
 * @return 格式化后的错误码
 */
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
    UbseLoggerEntry(const UbseLoggerEntry& other);
    UbseLoggerEntry& operator=(const UbseLoggerEntry& other);
    UbseLoggerEntry(UbseLoggerEntry&&) = default;
    UbseLoggerEntry& operator=(UbseLoggerEntry&&) = default;

    void OutPutLog(std::ostream& os);
    void FormatSyslog(std::ostream& os);

    UbseLogLevel GetLogLevel();
    const char* GetModuleName();
    const char* GetFile();
    uint32_t GetLine();
    char* GetMessage(size_t& length);
    uint64_t GetEntryTimeStamp();

    UbseLoggerEntry& operator<<(char data);
    UbseLoggerEntry& operator<<(int32_t data);
    UbseLoggerEntry& operator<<(uint32_t data);
    UbseLoggerEntry& operator<<(int64_t data);
    UbseLoggerEntry& operator<<(uint64_t data);
    UbseLoggerEntry& operator<<(double data);
    UbseLoggerEntry& operator<<(const std::string& data);
    UbseLoggerEntry& operator<<(const char* data);

private:
    uint32_t ResizeBuffer(size_t addSize);

    char* GetBuffer();

    void EncodeString(const char* data, size_t length);

    template <typename T>
    void EncodeData(T data)
    {
        *reinterpret_cast<T*>(GetBuffer()) = data;
        currentSize += sizeof(T);
    }

    template <typename T>
    void EncodeData(UbseLoggerTypeId id, T data)
    {
        uint32_t ret = ResizeBuffer(sizeof(UbseLoggerTypeId) + sizeof(T));
        if (ret != 0) {
            return;
        }
        EncodeData<UbseLoggerTypeId>(id);
        EncodeData<T>(data);
    }

    void EncodeData(const char* data);
    char* DecodeChar(std::ostream& os, char* buffer);
    char* DecodeUint(std::ostream& os, char* buffer);
    char* DecodeUlong(std::ostream& os, char* buffer);
    char* DecodeInt(std::ostream& os, char* buffer);
    char* DecodeLong(std::ostream& os, char* buffer);
    char* DecodeDouble(std::ostream& os, char* buffer);
    char* DecodeString(std::ostream& os, char* buffer);
    void DecodeData(std::ostream& os, char* start, const char* end);

    const char* moduleName;
    uint64_t timeStamp;
    pid_t pid;
    std::thread::id tid;
    UbseLogLevel level;
    std::string traceId;
    const char* file;
    const char* func;
    uint32_t line;

    size_t maxSize;
    char logEntryBuffer[512] = {0}; // 缓冲区大小为512
    std::unique_ptr<char[]> heapBuffer;
    size_t currentSize;
};

bool UbseIsLog(UbseLogLevel level);

struct UbseLog {
    bool operator==(UbseLoggerEntry& loggerEntry);
};
} // namespace ubse::log
#endif // UBSE_LOGGER_H