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

#include "ubse_logger.h"
namespace ubse::log {

bool UbseIsLog(UbseLogLevel level)
{
    return true;
}

bool UbseLog::operator==(UbseLoggerEntry& loggerEntry)
{
    return false;
}

UbseLoggerEntry::UbseLoggerEntry(const char* gModuleName, UbseLogLevel level, const char* file, const char* func,
                                 uint32_t line)
{
}

UbseLoggerEntry& UbseLoggerEntry::operator<<(const std::string& data)
{
    return *this;
}

UbseLoggerEntry& UbseLoggerEntry::operator<<(uint64_t data)
{
    return *this;
}

UbseLoggerEntry& UbseLoggerEntry::operator<<(double data)
{
    return *this;
}

UbseLoggerEntry& UbseLoggerEntry::operator<<(int64_t data)
{
    return *this;
}

UbseLoggerEntry& UbseLoggerEntry::operator<<(uint32_t data)
{
    return *this;
}

UbseLoggerEntry& UbseLoggerEntry::operator<<(int32_t data)
{
    return *this;
}

UbseLoggerEntry& UbseLoggerEntry::operator<<(char data)
{
    return *this;
}

UbseLoggerEntry& UbseLoggerEntry::operator<<(const char* data)
{
    return *this;
}

std::string FormatRetCode(uint32_t retCode)
{
    return std::to_string(retCode);
}
} // namespace ubse::log
