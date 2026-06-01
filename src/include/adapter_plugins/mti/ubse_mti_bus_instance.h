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

#ifndef UBSE_MTI_BUS_INSTANCE_H
#define UBSE_MTI_BUS_INSTANCE_H
#include <array>
#include <unordered_map>
#include <vector>
#include "ubse_common_def.h"
namespace ubse::mti::bus_instance {
using namespace ubse::common::def;
using UbseMtiGuid = std::array<uint8_t, 16>; // 16 byte guid
using UbseMtiEid = std::array<uint8_t, 16>;  // 16 byte eid

struct UbseMtiUbController {
    uint8_t slotId{0xFF};
    uint8_t chipId{0xFF};
    uint8_t dieId{0xFF};

    UbseMtiUbController() = default;

    UbseMtiUbController(uint8_t chipId, uint8_t dieId);

    bool operator==(const UbseMtiUbController& other) const;

    bool operator<(const UbseMtiUbController& other) const;
};

enum class UbseMtiBusInstanceType {
    HOST,
    VM
};

struct UbseMtiBusInst {
    UbseMtiBusInstanceType type{UbseMtiBusInstanceType::VM};
    uint16_t upi;
    uint16_t vendor;
    UbseMtiGuid guid;
    UbseMtiEid eid;
    std::vector<UbseMtiGuid> subDeviceGuids;
};

class UbseMtiBusInstance {
public:
    static UbseMtiBusInstance& GetInstance();

    /**
    * @brief 获取BusInstance列表，包含vm和host BusInstance
    * @param busInstanceList [out] BusInstance列表
    *
    * @return UBSE_OK代表获取成功，UBSE_ERROR代表获取失败
    */
    virtual UbseResult GetBusInstanceList(std::vector<UbseMtiBusInst>& busInstanceList) = 0;
    /**
    * @brief 创建vm BusInstance
    * @param upi [in] upi
    * @param busInstance [out] UbseMtiBusInst实例
    *
    * @return UBSE_OK代表创建成功，UBSE_ERROR代表创建失败
    */
    virtual UbseResult CreateVmBusInstance(uint16_t upi, UbseMtiBusInst& busInstance) = 0;
    /**
    * @brief 销毁vm BusInstance
    * @param busInstance [in] UbseMtiBusInst实例
    *
    * @return UBSE_OK代表销毁成功，UBSE_ERROR代表销毁失败
    */
    virtual UbseResult DestroyVmBusInstance(const UbseMtiBusInst& busInstance) = 0;

    /**
    * @brief 查询D2H Ub Memory内存信息
    * @param busInstance [in] UbseMtiBusInst实例
    * @param tid [out] tid
    * @param uba [out] uba
    * @param size [out] 内存大小
    *
    * @return UBSE_OK代表查询成功，UBSE_ERROR代表查询失败
    */
    virtual UbseResult GetD2hMemory(const UbseMtiBusInst& busInstance, uint32_t& tid, uint64_t& uba,
                                    uint64_t& size) = 0;
};
} // namespace ubse::mti::bus_instance
#endif // UBSE_MTI_BUS_INSTANCE_H
