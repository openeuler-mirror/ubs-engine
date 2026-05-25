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
    FULL_MESH = 1,
    CLOS = 8,
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

    /**
     * @brief 判断组网是否为CLOS类型
     */
    bool IsClosType();

    /**
     * @brief 获取二层超节点Id
     * @param superPodId 二层超节点Id
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR 表示失败
     */
    uint32_t GetSuperPodId(uint16_t &superPodId);

    /**
     * @brief 获取节点所在框号/超节点ID
     * @param podId 超节点ID
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR 表示失败
     */
    uint32_t GetPodId(uint16_t &podId);

    /**
     * @brief 获取节点所在槽位ID
     * @param slotId 槽位ID
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR 表示失败
     */
    uint32_t GetSlotId(uint8_t &slotId);

    /**
     * @brief 获取服务器索引
     * @param serverIdx 服务器索引
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR 表示失败
     */
    uint32_t GetServerIdx(uint32_t &serverIdx);

private:
    UbseSmbios() = default;
};
} // namespace ubse::adapter_plugins::smbios

#endif // UBSE_SMBIOS_H
