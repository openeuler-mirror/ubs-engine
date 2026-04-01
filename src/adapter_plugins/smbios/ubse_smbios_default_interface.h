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

#ifndef UBSE_SMBIOS_DEFAULT_INTERFACE_H
#define UBSE_SMBIOS_DEFAULT_INTERFACE_H

#include "ubse_smbios.h"

namespace ubse::adapter_plugins::smbios {
class UbseSmbiosDefaultInterface {
public:
    UbseSmbiosDefaultInterface(const UbseSmbiosDefaultInterface &) = delete;
    UbseSmbiosDefaultInterface &operator=(const UbseSmbiosDefaultInterface &) = delete;
    static UbseSmbiosDefaultInterface &GetInstance()
    {
        static UbseSmbiosDefaultInterface instance;
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

    /*
     * @brief 获取组网类型
     * @param networkType 组网类型
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR 表示失败
     */
    UbseResult GetMeshType(UbseMeshType &meshType);

    /*
     * @brief 获取组网类型，如果SMBIOS结构类型131获取失败，则返回默认值FULL_MESH
     * @param networkType 组网类型
     */
    UbseMeshType GetMeshType();

private:
    UbseSmbiosDefaultInterface() = default;
};

template <UbseSmbiosType type>
std::shared_ptr<SmbiosStructType<type>> UbseSmbiosDefaultInterface::GetSmbiosTypeInfo()
{
    return implement::UbseSmbios::GetInstance().GetSmbiosTypeInfo<type>();
}
} // namespace ubse::adapter_plugins::smbios

#endif // UBSE_SMBIOS_DEFAULT_INTERFACE_H
