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

#include "ubse_smbios_def.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_security_module.h"

namespace ubse::adapter_plugins::smbios {
using namespace ubse::common::def;
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");
const uint32_t RAW_DATA_SIZE = 1024;
const uint32_t ROW_SIZE = 16;

UbseResult ReadFromFileDescriptor(int fd, uint8_t* buf, size_t maxLen, const char* filePath)
{
    int64_t readLen = 1;
    uint64_t offset = 0;
    while (offset < maxLen && readLen != 0) {
        readLen = read(fd, buf + offset, maxLen - offset);
        if (readLen == -1) {
            UBSE_LOG_WARN << "Failed to read " << filePath << ", errno=" << errno << ", readLen=" << readLen;
            if (errno != EINTR) {
                return UBSE_ERROR_IO;
            }
        } else {
            offset += static_cast<size_t>(readLen); // readLen要么为-1，要么为非负数
        }
    }
    if (offset != maxLen) {
        UBSE_LOG_ERROR << "Failed to read " << filePath << ", offset=" << offset << ", maxLen=" << maxLen;
        return UBSE_ERROR_IO;
    }
    return UBSE_OK;
}

inline void CloseFd(int fd, const char* filePath)
{
    if (close(fd) == -1) {
        UBSE_LOG_ERROR << "Failed to close " << filePath << ", errno=" << errno;
    }
}

UbseResult OpenSmbiosFile(const char* filePath, int& fd)
{
    std::vector<__u32> caps{CAP_DAC_OVERRIDE};
    auto result = ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    if (result != UBSE_OK) {
        ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        UBSE_LOG_ERROR << "Modify Effective Capabilities failed. ret=" << result;
        return UBSE_ERROR;
    }
    auto realPath = realpath(filePath, nullptr);
    if (realPath == nullptr) {
        ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        UBSE_LOG_ERROR << "Failed to get real path of " << filePath << ", errno=" << errno;
        return UBSE_ERROR_IO;
    }
    if ((fd = open(realPath, O_RDONLY)) < 0) {
        ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        UBSE_LOG_ERROR << "Failed to open " << realPath << ", errno=" << errno;
        free(realPath);
        return UBSE_ERROR_IO;
    }
    free(realPath);
    ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    return UBSE_OK;
}

/*
 * @brief 从SMBIOS文件中读取数据，保存至buf中，并返回读取到的数据指针
 * @param base 偏移量
 * @param filePath 文件路径
 * @param maxLen 最大读取长度，小于等于从文件读取的buf大小
 * @return std::vector<uint8_t> 读取到的数据
 */
constexpr uint32_t MAX_READ_SIZE = 256 * 1024 * 1024; // 256MB
std::vector<uint8_t> ReadSmbiosFile(off_t base, const char* filePath, uint32_t& maxLen)
{
    struct stat statbuf;
    int fd;
    uint32_t bufSize = 0;
    uint8_t* ptr = nullptr;
    auto ret = OpenSmbiosFile(filePath, fd);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to open " << filePath << ", ret=" << ret;
        return {};
    }
    auto statRet = fstat(fd, &statbuf);
    if ((statRet == 0 && base >= statbuf.st_size) || statRet == -1) {
        UBSE_LOG_ERROR << "base=" << base << " is out of file size=" << statbuf.st_size << ", or fstat failed";
        CloseFd(fd, filePath);
        return {};
    }
    bufSize = static_cast<uint32_t>(statbuf.st_size - base);
    maxLen = std::min(maxLen, bufSize);
    if (maxLen == 0 || maxLen > MAX_READ_SIZE) {
        UBSE_LOG_ERROR << "maxLen is 0 or maxLen > MAX_READ_SIZE, maxLen=" << maxLen;
        CloseFd(fd, filePath);
        return {};
    }
    if (ptr = new (std::nothrow) uint8_t[maxLen]; ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate " << maxLen << " bytes";
        CloseFd(fd, filePath);
        return {};
    }
    std::vector<uint8_t> buf(ptr, ptr + maxLen);
    if (lseek(fd, base, SEEK_SET) == -1) {
        UBSE_LOG_ERROR << "Failed to lseek " << filePath << ", errno=" << errno;
        delete[] ptr;
        CloseFd(fd, filePath);
        return {};
    }
    if (auto ret = ReadFromFileDescriptor(fd, buf.data(), maxLen, filePath); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse " << filePath << ", ret=" << ret;
        delete[] ptr;
        CloseFd(fd, filePath);
        return {};
    }
    delete[] ptr;
    CloseFd(fd, filePath);
    return buf;
}

inline uint16_t WORD(uint8_t* buf)
{
    return *reinterpret_cast<const uint16_t*>(buf);
}

inline uint32_t DWORD(uint8_t* buf)
{
    return *reinterpret_cast<const uint32_t*>(buf);
}

inline SmbiosOffset QWORD(uint8_t* buf)
{
    return *reinterpret_cast<const SmbiosOffset*>(buf);
}

void SmbiosStructureType1::LogSmbiosStructTypeInfo()
{
    UBSE_LOG_WARN << "Smbios structure type 1 info not supported";
}

void SmbiosSuperPodBasicInfo::LogSmbiosStructTypeInfo()
{
    UBSE_LOG_INFO << "SmbiosSuperPodBasicInfo: "
                  << "flag=" << static_cast<int>(flag) << ", podId=" << static_cast<int>(podId)
                  << ", slotId=" << static_cast<int>(slotId) << ", meshType=" << static_cast<int>(meshType)
                  << ", superPodId=" << static_cast<int>(superPodId) << ", serverIdx=" << static_cast<int>(serverIdx);
}

std::vector<uint8_t> GetDmiTable(off_t base, const char* tableFile, uint32_t flags, uint32_t& len)
{
    // 从文件开头读取，不加入偏移
    if (flags & FLAG_NO_FILE_OFFSET) {
        auto table = ReadSmbiosFile(0, tableFile, len);
        if (table.empty()) {
            UBSE_LOG_ERROR << "Failed to read " << tableFile << ", errno=" << errno;
        }
        return table;
    }
    UBSE_LOG_ERROR << "Get invalid flag, flags=" << flags << ", expect FLAG_NO_FILE_OFFSET";
    return {};
}

void SmbiosHeader::FillHeaderFromBuf(uint8_t* buf)
{
    type = buf[NO_0];
    length = buf[NO_1];
    handle = WORD(buf + NO_2);
    data = buf;
}

UbseResult SmbiosStructureType1::FillSmbiosStructFromBuf()
{
    return UBSE_ERR_NOT_SUPPORTED;
}

const size_t NO_24 = 24;
UbseResult SmbiosSuperPodBasicInfo::FillSmbiosStructFromBuf()
{
    if (header.data == nullptr) {
        UBSE_LOG_ERROR << "Smbios structure data is null";
        return UBSE_ERROR_NULLPTR;
    }
    // 除去header长度，至少要有9个字节
    if (static_cast<uint32_t>(header.length) - SMBIOS_HEADER_SIZE < NO_7) {
        UBSE_LOG_ERROR << "Smbios structure length " << header.length << " is smaller than "
                       << SMBIOS_HEADER_SIZE + NO_9;
        return UBSE_ERROR_INVAL;
    }
    std::array<uint8_t, RAW_DATA_SIZE> rawData{};
    rawData.fill(0xff);
    for (uint32_t row = 0; row < ((header.length - 1) >> NO_4) + 1; ++row) { // 按16字节对齐
        for (uint32_t i = 0; i < ROW_SIZE && i < header.length - (row << NO_4); ++i) {
            rawData[row * ROW_SIZE + i] = header.data[row * ROW_SIZE + i];
        }
    }
    this->flag = rawData[NO_4]; // 前4字节是header
    if (this->flag == 0xff) {
        // 0xff表示未从BMC获取数据成功
        UBSE_LOG_ERROR << "Get invalid flag, flag=0xff";
        return UBSE_ERROR_AGAIN;
    }
    this->podId = static_cast<uint16_t>(rawData[NO_6] << NO_8) | rawData[NO_5]; // 小端存储
    this->slotId = rawData[NO_7];
    this->meshType = rawData[NO_8];
    this->superPodId = static_cast<uint32_t>((rawData[NO_10] << NO_8) | rawData[NO_9]);
    this->serverIdx = static_cast<uint32_t>((rawData[NO_14] << NO_8) | rawData[NO_13]);
    LogSmbiosStructTypeInfo();
    return UBSE_OK;
}

/*
 * dmiBuf存储格式如下：
 * ------------------------------------------
 * 1. header   format-body     unformatted-data（非结构化字符串，以两个'\0'结尾）
 * 2. header   format-body     unformatted-data
 * 3. header   format-body     unformatted-data
 * ...
 * ------------------------------------------
 * 按以上格式遍历dmiBuf，找到符合type的结构体，填充到this中
 */
const size_t SMBIOS_TYPE_END = 127;
UbseResult SmbiosStructure::DecodeDmiTable(std::vector<uint8_t>& dmiBuf, uint32_t flags, UbseSmbiosType type)
{
    uint8_t* cursor = dmiBuf.data();
    uint32_t len = dmiBuf.size();
    while (cursor - dmiBuf.data() + SMBIOS_HEADER_SIZE < len) {
        // 获取header
        this->header.FillHeaderFromBuf(cursor);
        if (this->header.length < SMBIOS_HEADER_SIZE) {
            UBSE_LOG_ERROR << "Get invalid smbios entry length, length=" << static_cast<int>(this->header.length);
            return UBSE_ERROR_INVAL;
        }
        if (this->header.type == SMBIOS_TYPE_END && (flags & FLAG_STOP_AT_EOT)) {
            UBSE_LOG_ERROR << "SMBIOS v3 required, stop at type " << static_cast<int>(SMBIOS_TYPE_END);
            return UBSE_ERROR;
        }
        // 当前的type不匹配，跳到下一个type
        if (this->header.type != static_cast<uint8_t>(type)) {
            // 跳转到字符串表
            uint8_t* next = cursor + this->header.length;
            // 遍历字符串表，直到连续两个符号都是'\0'，表示字符串表结束
            while (next - dmiBuf.data() + 1 < len && (*next != '\0' || *(next + 1) != '\0')) {
                next++;
            }
            next += NO_2; // 跳过字符串表的两个'\0'，指向下一个结构体的header
            if (next - dmiBuf.data() >= len) {
                UBSE_LOG_ERROR << "Cannot find next smbios entry";
                return UBSE_ERROR;
            }
            cursor = next;
            continue;
        }
        UBSE_LOG_INFO << "Find smbios type " << static_cast<int>(type)
                      << ", len=" << static_cast<int>(this->header.length);
        // 计算剩下的长度，确保足够解析当前结构体
        if (static_cast<uint32_t>(this->header.length) > len - (cursor - dmiBuf.data())) {
            UBSE_LOG_ERROR << "Smbios structure length " << this->header.length << " is larger than "
                           << len - (cursor - dmiBuf.data());
            return UBSE_ERROR_INVAL;
        }
        // 调用子类的解析函数，如果返回成功，直接返回
        auto ret = this->FillSmbiosStructFromBuf();
        if (ret == UBSE_OK) {
            UBSE_LOG_INFO << "Successfully parse smbios type " << static_cast<int>(type);
        } else {
            UBSE_LOG_ERROR << "Failed to parse smbios type " << static_cast<int>(type) << ", ret=" << ret;
        }
        return ret;
    }
    UBSE_LOG_ERROR << "Failed to find smbios type " << static_cast<int>(type) << ", len=" << len;
    return UBSE_ERROR;
}

/*
 * struct SMBIOS_Entry_Point_64 {
 *   char anchor[5];          // "_SM3_" (0x5F 0x53 0x4D 0x33 0x5F)
 *   uint8_t checksum;        // 校验和
 *   uint8_t length;          // 结构长度（通常为0x18=24字节）
 *   uint8_t major_version;   // 主版本号（3）
 *   uint8_t minor_version;   // 次版本号
 *   uint8_t docrev;          // 文档修订版
 *   uint8_t entry_point_revision; // 入口点修订版
 *   uint8_t reserved;        // 保留
 *   uint32_t structure_table_max_size; // 结构表最大大小
 *   uint64_t structure_table_address; // 结构表64位物理地址
 * };
 */
UbseResult SmbiosStructure::DecodeSmbios3(std::vector<uint8_t>& entryBuf, const char* tableFile, uint32_t flags,
                                          UbseSmbiosType type)
{
    if (entryBuf[ENTRY_POINT_LENGTH] > ENTRY_POINT_LENGTH_MAX) {
        UBSE_LOG_ERROR << "Get invalid smbios entry point length, length="
                       << static_cast<int8_t>(entryBuf[ENTRY_POINT_LENGTH]);
        return UBSE_ERROR;
    }

    SmbiosOffset offset = QWORD(entryBuf.data() + STRUCTURE_TABLE_ADDRESS);
    // 检查系统是否支持64位文件偏移
    if (!(flags & FLAG_NO_FILE_OFFSET) && offset.high && sizeof(off_t) < OFF_T_SIZE) {
        UBSE_LOG_ERROR << "64-bit address not supported";
        return UBSE_ERR_NOT_SUPPORTED;
    }
    uint32_t len = DWORD(entryBuf.data() + STRUCTURE_TABLE_MAXIMUM_SIZE);
    auto table = GetDmiTable(static_cast<off_t>(offset.address64), tableFile, flags | FLAG_STOP_AT_EOT, len);
    if (table.empty()) {
        UBSE_LOG_ERROR << "Failed to read " << tableFile << ", errno=" << errno;
        return UBSE_ERROR_IO;
    }
    return this->DecodeDmiTable(table, flags | FLAG_STOP_AT_EOT, type);
}

std::vector<uint8_t> LoadSysEntryFile(uint32_t& maxLen)
{
    return ReadSmbiosFile(0, SYS_ENTRY_FILE.c_str(), maxLen);
}
} // namespace ubse::adapter_plugins::smbios
