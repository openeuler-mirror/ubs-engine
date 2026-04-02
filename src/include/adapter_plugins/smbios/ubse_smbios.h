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

#include <cstdint>
enum class UbseMeshType {
    FULL_MESH = 0,
    CLOS = 1,
};
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
     * @brief 获取组网类型
     * @param networkType 组网类型
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR 表示失败
     */
    uint32_t GetMeshType(UbseMeshType &meshType);

    static bool IsClosType();

private:
    UbseSmbios() = default;
};
} // namespace ubse::adapter_plugins::smbios

#endif // UBSE_SMBIOS_H
