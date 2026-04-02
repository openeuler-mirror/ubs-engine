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

#ifndef UBSE_SMBIOS_IMPL_H
#define UBSE_SMBIOS_IMPL_H

#include <memory>
#include "ubse_error.h"
#include "lock/ubse_lock.h"
#include "ubse_logger.h"
#include "ubse_smbios_def.h"

namespace ubse::adapter_plugins::smbios::impl {
using namespace ubse::common::def;
using namespace ubse::utils;
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");
class UbseSmbiosImpl {
public:
    UbseSmbiosImpl(const UbseSmbiosImpl &) = delete;
    UbseSmbiosImpl &operator=(const UbseSmbiosImpl &) = delete;
    static UbseSmbiosImpl &GetInstance()
    {
        static UbseSmbiosImpl instance;
        return instance;
    }

    /*
     * @brief 先加载SMBIOS 3.0入口点文件，获取DMI文件大小，再加载DMI文件、根据指定类型解析SMBIOS结构类型信息。
     *        为避免重复解析，先从缓存中查询是否已解析过该类型结构，若已解析，则直接返回缓存中的结构类型信息
     * @param type SMBIOS结构类型
     * @param typeInfo SMBIOS结构类型信息
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR_IO 表示读取SMBIOS 3.0入口点或DMI表文件失败
     * @return UBSE_ERR_NOT_SUPPORTED 表示SMBIOS版本不支持
     * @return UBSE_ERROR_INVAL 表示文件长度无效
     * @return UBSE_ERROR 表示失败
     */
    template <UbseSmbiosType type>
    std::shared_ptr<SmbiosStructType<type>> GetSmbiosTypeInfo();

private:
    UbseSmbiosImpl() = default;

private:
    std::map<UbseSmbiosType, std::shared_ptr<SmbiosStructure>> smbiosTypeInfoMap;
    ubse::utils::ReadWriteLock readWriteLock;
};

template <UbseSmbiosType type>
std::shared_ptr<SmbiosStructType<type>> UbseSmbiosImpl::GetSmbiosTypeInfo()
{
    {
        ReadLocker<ReadWriteLock> locker(&readWriteLock);
        if (smbiosTypeInfoMap.find(type) != smbiosTypeInfoMap.end()) {
            UBSE_LOG_INFO << "Smbios type " << static_cast<int>(type) << " found in cache";
            return std::dynamic_pointer_cast<SmbiosStructType<type>>(smbiosTypeInfoMap[type]);
        }
    }
    UBSE_LOG_INFO << "Smbios type " << static_cast<int>(type) << " not found in cache, will try to load from file";
    uint32_t size = ENTRY_POINT_LENGTH_MAX;
    auto entryBuf = LoadSysEntryFile(size);
    if (entryBuf.empty()) {
        UBSE_LOG_ERROR << "Failed to read " << SYS_ENTRY_FILE << ", errno=" << errno;
        return nullptr;
    }
    // 至少要包含结构表大小字段和结构表地址字段
    if (size != entryBuf.size() || size < STRUCTURE_TABLE_ADDRESS + sizeof(uint64_t)) {
        UBSE_LOG_ERROR << "Get invalid smbios entry point, size=" << size;
        return nullptr;
    }
    // 创建指定类型的SMBIOS结构对象
    std::shared_ptr<SmbiosStructure> typeInfo = CreateSmbiosStruct<type>();
    if (typeInfo == nullptr) {
        UBSE_LOG_ERROR << "Failed to create smbios struct type " << static_cast<int>(type);
        return nullptr;
    }
    constexpr std::string_view SMBIOS_3_ENTRY_SIGNATURE = "_SM3_";
    if (entryBuf.size() >= SMBIOS_3_ENTRY_SIGNATURE.size() &&
        std::equal(SMBIOS_3_ENTRY_SIGNATURE.begin(), SMBIOS_3_ENTRY_SIGNATURE.end(), entryBuf.begin())) {
        if (auto ret = typeInfo->DecodeSmbios3(entryBuf, SYS_TABLE_FILE.c_str(), FLAG_NO_FILE_OFFSET, type);
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to decode " << SYS_TABLE_FILE << ", ret=" << ret;
            return nullptr;
        }
        WriteLocker<ReadWriteLock> locker(&readWriteLock);
        smbiosTypeInfoMap[type] = typeInfo;
        UBSE_LOG_INFO << "Smbios type " << static_cast<int>(type) << " loaded from file";
        return std::dynamic_pointer_cast<SmbiosStructType<type>>(typeInfo);
    }
    UBSE_LOG_ERROR << "The Smbios version is not 3.0";
    return nullptr;
}
} // namespace ubse::adapter_plugins::smbios::impl

#endif // UBSE_SMBIOS_IMPL_H
