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

#ifndef UBSE_MTI_NPU_H
#define UBSE_MTI_NPU_H
#include <array>
#include <unordered_map>
#include <vector>
#include "ubse_common_def.h"
namespace ubse::mti::npu {
using namespace ubse::common::def;
using UbseMtiGuid = std::array<uint8_t, 16>; // 16 byte guid
using UbseMtiEid = std::array<uint8_t, 16>;  // 16 byte eid

struct UbseMtiUbController {
    uint8_t slotId;
    uint8_t chipId;
    uint8_t dieId;
};

struct UbseMtiIdevVfe {
    UbseMtiUbController ubController;
    uint8_t pfeId;
    UbseMtiGuid guid;
    uint8_t vfeId;
};

struct UbseMtiIdevPfe {
    UbseMtiUbController ubController;
    uint8_t pfeId;
    UbseMtiGuid guid;
    std::vector<UbseMtiIdevVfe> vfeList;
};

struct UbseMti1825Vf {
    uint8_t slotId;
    uint8_t chipId;
    uint8_t dieId;
    uint8_t pfId;
    UbseMtiGuid guid;
    uint8_t vfId;
};

struct UbseMti1825Pf {
    uint8_t slotId;
    uint8_t chipId;
    uint8_t dieId;
    uint8_t pfId;
    UbseMtiGuid guid;
    std::vector<UbseMti1825Vf> vfList;
};

struct UbseMtiDavid {
    uint8_t slotId;
    uint8_t chipId;
    uint8_t channelId{0xFF};

    bool operator==(const UbseMtiDavid &other) const
    {
        return slotId == other.slotId && chipId == other.chipId;
    }
};

struct UbseMtiDavidHash {
    std::size_t operator()(const UbseMtiDavid &d) const noexcept
    {
        auto key = static_cast<uint16_t>((static_cast<uint16_t>(d.slotId) << 8) | d.chipId);
        return std::hash<uint16_t>{}(key);
    }
};

using UbseMtiIdevVfeDavidPair = std::pair<UbseMtiIdevVfe, UbseMtiDavid>;
using UbseMtiIdevFeDavidMapping = std::unordered_map<UbseMtiDavid, UbseMtiIdevPfe, UbseMtiDavidHash>;

enum class UbseMtiBusInstanceType {
    HOST,
    VM
};

struct UbseMtiBusInstance {
    UbseMtiBusInstanceType type{UbseMtiBusInstanceType::VM};
    uint16_t upi;
    uint16_t vendor;
    UbseMtiGuid guid;
    UbseMtiEid eid;
};

class UbseMtiIdevFeOperation {
public:
    static UbseMtiIdevFeOperation &GetInstance();
    /**
    * @brief 获取1650 Fe列表
    * @param feList [out] Fe列表
    *
    * @return UBSE_OK代表获取成功，UBSE_ERROR代表获取失败
    */
    virtual UbseResult GetIdevFeList(std::vector<UbseMtiIdevPfe> &feList) = 0;
    /**
    * @brief 获取1650的pfe与David的对应关系
    * @param mapping [out] 1650的pfe与David的对应关系map
    *
    * @return UBSE_OK代表获取成功，UBSE_ERROR代表获取失败
    */
    virtual UbseResult GetIdevFeDavidMapping(UbseMtiIdevFeDavidMapping &mapping) = 0;
    /**
    * @brief 绑定1650 vfe与David
    * @param upi [in] upi
    * @param vfeList [in] vfe列表
    *
    * @return UBSE_OK代表绑定成功，UBSE_ERROR代表绑定失败
    */
    virtual UbseResult BindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList) = 0;
    /**
    * @brief 解绑定1650 vfe与David
    * @param upi [in] upi
    * @param vfeList [in] vfe列表
    *
    * @return UBSE_OK代表解绑成功，UBSE_ERROR代表解绑失败
    */
    virtual UbseResult UnBindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList) = 0;
    /**
    * @brief 将David 对应的Fe注册到Vm BusInstance
    * @param busInstance [in] UbseMtiBusInstance实例
    * @param vfeList [in] vfe列表
    *
    * @return UBSE_OK代表注册成功，UBSE_ERROR代表注册失败
    */
    virtual UbseResult RegDavidFeToVmBusInstance(const UbseMtiBusInstance &busInstance,
                                                 const std::vector<UbseMtiIdevVfe> &vfeList) = 0;
    /**
    * @brief 将David 对应的Fe从Vm BusInstance注销
    * @param vfeList [in] vfe列表
    *
    * @return UBSE_OK代表注销成功，UBSE_ERROR代表注销失败
    */
    virtual UbseResult UnRegDavidFeFromVmBusInstance(const std::vector<UbseMtiIdevVfe> &vfeList) = 0;
};

class UbseMti1825Operation {
public:
    static UbseMti1825Operation &GetInstance();
    /**
    * @brief 获取1825的Fe列表
    * @param pfList [out] pf列表
    *
    * @return UBSE_OK代表获取成功，UBSE_ERROR代表获取失败
    */
    virtual UbseResult Get1825FeList(std::vector<UbseMti1825Pf> &pfList) = 0;
    /**
    * @brief 将1825 vf注册到Host BusInstance
    * @param busInstance [in] UbseMtiBusInstance实例
    * @param vfList [in] vf列表
    *
    * @return UBSE_OK代表注册成功，UBSE_ERROR代表注册失败
    */
    virtual UbseResult Reg1825FeToHostBusInstance(const UbseMtiBusInstance &busInstance,
                                                  const std::vector<UbseMti1825Vf> &vfList) = 0;
    /**
    * @brief 将1825 vf从Host BusInstance注销
    * @param vfList [in] vf列表
    *
    * @return UBSE_OK代表注销成功，UBSE_ERROR代表注销失败
    */
    virtual UbseResult UnReg1825FeFromHostBusInstance(const std::vector<UbseMti1825Vf> &vfList) = 0;

    /**
    * @brief 将1825 vf注册到Vm BusInstance
    * @param busInstance [in] UbseMtiBusInstance实例
    * @param vfList [in] vf列表
    *
    * @return UBSE_OK代表注册成功，UBSE_ERROR代表注册失败
    */
    virtual UbseResult Reg1825FeToVmBusInstance(const UbseMtiBusInstance &busInstance,
                                                const std::vector<UbseMti1825Vf> &vfList) = 0;
    /**
    * @brief 将1825 vf从Vm BusInstance注销
    * @param vfList [in] vf列表
    *
    * @return UBSE_OK代表注销成功，UBSE_ERROR代表注销失败
    */
    virtual UbseResult UnReg1825FeFromVmBusInstance(const std::vector<UbseMti1825Vf> &vfList) = 0;
};

class UbseMtiBusInstanceOperation {
public:
    static UbseMtiBusInstanceOperation &GetInstance();

    /**
    * @brief 获取BusInstance列表，包含vm和host BusInstance
    * @param busInstanceList [out] BusInstance列表
    *
    * @return UBSE_OK代表获取成功，UBSE_ERROR代表获取失败
    */
    virtual UbseResult GetBusInstanceList(std::vector<UbseMtiBusInstance> &busInstanceList) = 0;
    /**
    * @brief 创建vm BusInstance
    * @param upi [in] upi
    * @param busInstance [out] UbseMtiBusInstance实例
    *
    * @return UBSE_OK代表创建成功，UBSE_ERROR代表创建失败
    */
    virtual UbseResult CreateVmBusInstance(uint16_t upi, UbseMtiBusInstance &busInstance) = 0;
    /**
    * @brief 销毁vm BusInstance
    * @param busInstance [in] UbseMtiBusInstance实例
    *
    * @return UBSE_OK代表销毁成功，UBSE_ERROR代表销毁失败
    */
    virtual UbseResult DestroyVmBusInstance(const UbseMtiBusInstance &busInstance) = 0;
};

} // namespace ubse::mti::npu
#endif // UBSE_MTI_NPU_H
