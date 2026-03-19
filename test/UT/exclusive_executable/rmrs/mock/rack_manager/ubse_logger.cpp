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

#include "ubse_logger.h"
namespace ubse::log {

void UbseLogOutput(const char *moduleName, UbseLogLevel level, const char *msg);

bool UbseIsLog(UbseLogLevel level)
{
    return true;
}

bool UbseLog::operator==(UbseLoggerEntry &loggerEntry)
{
    return false;
}

void UbseLoggerEntry::DecodeData(std::ostream &os, char *start, const char *end)
{
}

char *UbseLoggerEntry::DecodeString(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *UbseLoggerEntry::DecodeDouble(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *UbseLoggerEntry::DecodeLong(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *UbseLoggerEntry::DecodeInt(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *UbseLoggerEntry::DecodeUlong(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *UbseLoggerEntry::DecodeUint(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *UbseLoggerEntry::DecodeChar(std::ostream &os, char *buffer)
{
    return nullptr;
}

void UbseLoggerEntry::EncodeData(const char *data)
{
}

void UbseLoggerEntry::EncodeString(const char *data, size_t length)
{
}

char *UbseLoggerEntry::GetBuffer()
{
    return nullptr;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(const std::string &data)
{
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(uint64_t data)
{
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(double data)
{
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(int64_t data)
{
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(uint32_t data)
{
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(int32_t data)
{
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(char data)
{
    return *this;
}

UbseLoggerEntry &UbseLoggerEntry::operator<<(const char *data)
{
    return *this;
}

char *UbseLoggerEntry::GetMessage(size_t &length)
{
    return nullptr;
}

uint32_t UbseLoggerEntry::GetLine()
{
    return 0;
}

const char *UbseLoggerEntry::GetFile()
{
    return nullptr;
}

const char *UbseLoggerEntry::GetModuleName()
{
    return nullptr;
}

void UbseLoggerEntry::OutPutLog(std::ostream &os)
{
}

UbseLoggerEntry::UbseLoggerEntry(const char *gModuleName, UbseLogLevel level, const char *file, const char *func,
                                 uint32_t line)
{
}
}  // namespace ubse::log