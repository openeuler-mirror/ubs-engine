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

#ifndef UBSE_SMBIOS_DEF_H
#define UBSE_SMBIOS_DEF_H

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::adapter_plugins::smbios {
using namespace ubse::common::def;

const uint32_t TYPE_1_MAX_LEN = 32;
const uint32_t ENTRY_POINT_LENGTH = 0x06;
const uint32_t ENTRY_POINT_LENGTH_MAX = 0x20;
const uint32_t SMBIOS_MAJOR_VERSION = 0x07;
const uint32_t SMBIOS_MINOR_VERSION = 0x08;
const uint32_t STRUCTURE_TABLE_MAXIMUM_SIZE = 0x0c;
const uint32_t STRUCTURE_TABLE_ADDRESS = 0x10;
const uint32_t OFF_T_SIZE = 8;
const uint32_t PRODUCT_NAME_MAX_LEN = 64;
const uint32_t SUPER_POD_BASIC_T_RESERVED_SIZE = 64;
const uint32_t SMBIOS_HEADER_SIZE = 4;
const std::string SYS_FIRMWARE_DIR = "/sys/firmware/dmi/tables";
const std::string SYS_ENTRY_FILE = SYS_FIRMWARE_DIR + "/smbios_entry_point";
const std::string SYS_TABLE_FILE = SYS_FIRMWARE_DIR + "/DMI";
const uint32_t FLAG_NO_FILE_OFFSET = 1 << 0;
const uint32_t FLAG_STOP_AT_EOT = 1 << 1;

std::vector<uint8_t> LoadSysEntryFile(uint32_t &maxLen);
enum class UbseSmbiosType {
    TYPE_1 = 1,
    SUPER_POD_BASIC_INFO_T = 131,
    TYPE_INVALID
};

union SmbiosOffset {
    uint64_t address64;
    struct {
        uint32_t low;  // 低32位
        uint32_t high; // 高32位
    };
};

class SmbiosHeader {
public:
    void FillHeaderFromBuf(uint8_t *buf);

public:
    uint8_t type;    // SMBIOS结构类型
    uint8_t length;  // SMBIOS结构长度
    uint16_t handle; // SMBIOS结构句柄
    uint8_t *data;   // SMBIOS结构数据指针
};

class SmbiosStructure {
public:
    static constexpr UbseSmbiosType type = UbseSmbiosType::TYPE_INVALID;
    virtual ~SmbiosStructure() = default;
    virtual void LogSmbiosStructTypeInfo(){};

    /*
     * @brief 基于SMBIOS 3.0入口点和DMI表文件，解析指定类型的SMBIOS结构，存储到this指针指向的对象中
     * @param entryBuf SMBIOS 3.0入口点数据
     * @param tableFile SMBIOS 3.0 DMI表文件路径
     * @param flags 解析标志位
     * @param type SMBIOS结构类型
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR_IO 表示读取SMBIOS 3.0入口点或DMI表文件失败
     * @return UBSE_ERR_NOT_SUPPORTED 表示SMBIOS结构类型不支持
     * @return UBSE_ERROR_INVAL 表示文件长度无效
     * @return UBSE_ERROR 表示失败
     */
    UbseResult DecodeSmbios3(std::vector<uint8_t> &entryBuf, const char *tableFile, uint32_t flags,
                             UbseSmbiosType type);

protected:
    SmbiosHeader header; // SMBIOS结构头

protected:
    SmbiosStructure() = default;
    UbseResult DecodeDmiTable(std::vector<uint8_t> &dmiBuf, uint32_t flags, UbseSmbiosType type);
    virtual UbseResult FillSmbiosStructFromBuf()
    {
        return UBSE_ERR_NOT_SUPPORTED;
    }
};

class SmbiosStructureType1 : public SmbiosStructure {
public:
    static constexpr UbseSmbiosType type = UbseSmbiosType::TYPE_1;
    void LogSmbiosStructTypeInfo() override;

public:
    std::array<char, TYPE_1_MAX_LEN> manufacturer;
    std::array<char, PRODUCT_NAME_MAX_LEN> productName;
    std::array<char, TYPE_1_MAX_LEN> version;
    std::array<char, TYPE_1_MAX_LEN> serialNumber;
    std::array<char, TYPE_1_MAX_LEN> uuid;
    std::array<char, TYPE_1_MAX_LEN> wakeupType;
    std::array<char, TYPE_1_MAX_LEN> skuNumber;
    std::array<char, TYPE_1_MAX_LEN> family;

protected:
    UbseResult FillSmbiosStructFromBuf() override;
};

class SmbiosSuperPodBasicInfo : public SmbiosStructure {
public:
    static constexpr UbseSmbiosType type = UbseSmbiosType::SUPER_POD_BASIC_INFO_T;
    void LogSmbiosStructTypeInfo() override;

public:
    uint8_t flag{0xff};  // 标志位，用于判断是否从SMBIOS中读取到该结构，取0x00时数据有效
    uint16_t podId;      // 机架号、超节点Id
    uint8_t slotId;      // 槽位Id
    uint8_t meshType;    // 组网类型：clos组网或fullmesh组网
    uint16_t superPodId; // 二层超节点Id
    uint32_t serverIdx;  // 服务器索引
    std::array<uint8_t, SUPER_POD_BASIC_T_RESERVED_SIZE> reserved; // 保留字段

protected:
    UbseResult FillSmbiosStructFromBuf() override;
};

/**
 * @brief 编译期类型映射，将UbseSmbiosType枚举值映射到对应的类型
 * @tparam Type UbseSmbiosType枚举值
 */
template <UbseSmbiosType Type>
struct SmbiosTypeMap {};

/**
 * @brief SmbiosTypeMap的特化，将TYPE_1映射到SmbiosStructureType1
 */
template <>
struct SmbiosTypeMap<UbseSmbiosType::TYPE_1> {
    using Type = SmbiosStructureType1;
};

/**
 * @brief SmbiosTypeMap的特化，将SUPER_POD_BASIC_INFO_T映射到SmbiosSuperPodBasicInfo
 */
template <>
struct SmbiosTypeMap<UbseSmbiosType::SUPER_POD_BASIC_INFO_T> {
    using Type = SmbiosSuperPodBasicInfo;
};

/**
 * @brief 便捷的类型别名，用于获取与UbseSmbiosType对应的类型
 * @tparam Type UbseSmbiosType枚举值
 */
template <UbseSmbiosType Type>
using SmbiosStructType = typename SmbiosTypeMap<Type>::Type;

/*
 * @brief 根据SMBIOS类型枚举（模板参数）创建对应的SMBIOS结构对象
 * @tparam Type SMBIOS结构类型枚举值
 * @return 指向创建的SMBIOS结构对象的智能指针
 */
template <UbseSmbiosType Type>
std::shared_ptr<SmbiosStructType<Type>> CreateSmbiosStruct()
{
    static_assert(std::is_base_of<SmbiosStructure, SmbiosStructType<Type>>::value,
                  "SmbiosStructType<Type> must be a subclass of SmbiosStructure");
    return std::make_shared<SmbiosStructType<Type>>();
}
} // namespace ubse::adapter_plugins::smbios

#endif // UBSE_SMBIOS_DEF_H
