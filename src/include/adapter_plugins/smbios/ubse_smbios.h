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

#ifndef UBSE_SMBIOS_H
#define UBSE_SMBIOS_H

#include "ubse_smbios_def.h"

namespace ubse::adapter_plugins::smbios {
class UbseSmbios {
public:
    UbseSmbios(const UbseSmbios &) = delete;
    UbseSmbios &operator=(const UbseSmbios &) = delete;
    static UbseSmbios &GetInstance()
    {
        static UbseSmbios instance;
        return instance;
    }

    /*
     * @brief 获取type 131结构类型信息
     * @param typeInfo SMBIOS结构类型信息
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR_IO 表示读取SMBIOS 3.0入口点或DMI表文件失败
     * @return UBSE_ERR_NOT_SUPPORTED 表示SMBIOS版本不支持
     * @return UBSE_ERROR_INVAL 表示文件长度无效
     * @return UBSE_ERROR 表示失败
     */
    std::shared_ptr<SmbiosStructureType131> GetSmbiosType131Info();

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
    UbseSmbios() = default;
};
} // namespace ubse::adapter_plugins::smbios

#endif // UBSE_SMBIOS_H
