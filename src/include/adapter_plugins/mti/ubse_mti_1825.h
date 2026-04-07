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

#ifndef UBSE_MTI_1825_H
#define UBSE_MTI_1825_H
#include <array>
#include <unordered_map>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_mti_bus_instance.h"
namespace ubse::mti::_1825 {
using namespace ubse::common::def;
using namespace ubse::mti::bus_instance;

struct UbseMti1825Vf {
    uint8_t slotId{0xFF};
    uint8_t chipId;
    uint8_t dieId;
    uint16_t pfId;
    UbseMtiUbController affinityUbController;
    UbseMtiGuid guid;
    uint16_t vfId;

    UbseMti1825Vf() = default;

    UbseMti1825Vf(uint8_t slotId, uint8_t chipId, uint8_t dieId, uint16_t pfId, uint16_t vfId);

    bool operator==(const UbseMti1825Vf &other) const;

    bool operator<(const UbseMti1825Vf &other) const;
};

struct UbseMti1825Pf {
    uint8_t slotId{0xFF};
    uint8_t chipId;
    uint8_t dieId;
    uint16_t pfId;
    UbseMtiUbController affinityUbController;
    UbseMtiGuid guid;
    std::vector<UbseMti1825Vf> vfList;

    UbseMti1825Pf() = default;

    UbseMti1825Pf(uint8_t slotId, uint8_t chipId, uint8_t dieId, uint16_t pfId);

    bool operator==(const UbseMti1825Pf &other) const;

    bool operator<(const UbseMti1825Pf &other) const;
};

class UbseMti1825 {
public:
    static UbseMti1825 &GetInstance();
    /**
    * @brief 获取1825的Fe列表
    * @param pfList [out] pf列表
    *
    * @return UBSE_OK代表获取成功，UBSE_ERROR代表获取失败
    */
    virtual UbseResult Get1825FeList(std::vector<UbseMti1825Pf> &pfList) = 0;
    /**
    * @brief 将1825 vf注册到Host BusInstance
    * @param busInstance [in] UbseMtiBusInst实例
    * @param vfList [in] vf列表
    * @param resList [out] 每个vf注册的结果，与vfList一一对应
    *
    * @return UBSE_OK代表注册成功，UBSE_ERROR代表注册失败
    */
    virtual UbseResult Reg1825FeToHostBusInstance(const UbseMtiBusInst &busInstance,
                                                  const std::vector<UbseMti1825Vf> &vfList,
                                                  std::vector<bool> &resList) = 0;
    /**
    * @brief 将1825 vf从Host BusInstance注销
    * @param busInstance [in] UbseMtiBusInst实例
    * @param vfList [in] vf列表
    * @param resList [out] 每个vf注销的结果，与vfList一一对应
    *
    * @return UBSE_OK代表注销成功，UBSE_ERROR代表注销失败
    */
    virtual UbseResult UnReg1825FeFromHostBusInstance(const UbseMtiBusInst &busInstance,
                                                      const std::vector<UbseMti1825Vf> &vfList,
                                                      std::vector<bool> &resList) = 0;

    /**
    * @brief 将1825 vf注册到Vm BusInstance
    * @param busInstance [in] UbseMtiBusInst实例
    * @param vfList [in] vf列表
    * @param resList [out] 每个vf注销的结果，与vfList一一对应
    *
    * @return UBSE_OK代表注册成功，UBSE_ERROR代表注册失败
    */
    virtual UbseResult Reg1825FeToVmBusInstance(const UbseMtiBusInst &busInstance,
                                                const std::vector<UbseMti1825Vf> &vfList,
                                                std::vector<bool> &resList) = 0;
    /**
    * @brief 将1825 vf从Vm BusInstance注销
    * @param busInstance [in] UbseMtiBusInst实例
    * @param vfList [in] vf列表
    * @param resList [out] 每个vf注销的结果，与vfList一一对应
    *
    * @return UBSE_OK代表注销成功，UBSE_ERROR代表注销失败
    */
    virtual UbseResult UnReg1825FeFromVmBusInstance(const UbseMtiBusInst &busInstance,
                                                    const std::vector<UbseMti1825Vf> &vfList,
                                                    std::vector<bool> &resList) = 0;
};
} // namespace ubse::mti::_1825
#endif // UBSE_MTI_1825_H