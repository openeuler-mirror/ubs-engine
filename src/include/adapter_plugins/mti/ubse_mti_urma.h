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

#ifndef UBSE_MTI_URMA_H
#define UBSE_MTI_URMA_H
#include <array>
#include <unordered_map>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_mti_bus_instance.h"
namespace ubse::mti::urma {
using namespace ubse::mti::bus_instance;

struct UbseMtiIdevVfe {
    UbseMtiUbController ubController{};
    uint8_t pfeId{0xFF};
    UbseMtiGuid guid{0xFF};
    uint8_t vfeId{0xFF};

    UbseMtiIdevVfe() = default;

    UbseMtiIdevVfe(const UbseMtiUbController &ubController, uint8_t pfeId, uint8_t vfeId);

    bool operator==(const UbseMtiIdevVfe &other) const;

    bool operator<(const UbseMtiIdevVfe &other) const;
};

struct UbseMtiIdevPfe {
    UbseMtiUbController ubController{};
    uint8_t pfeId{0xFF};
    UbseMtiGuid guid{0xFF};
    std::vector<UbseMtiIdevVfe> vfeList{};

    UbseMtiIdevPfe() = default;

    UbseMtiIdevPfe(const UbseMtiUbController &ubController, uint8_t pfeId);

    void AddVfe(const UbseMtiIdevVfe &vfe);

    bool operator==(const UbseMtiIdevPfe &other) const;

    bool operator<(const UbseMtiIdevPfe &other) const;
};

struct UbseMtiDavid {
    uint8_t slotId{0xFF};
    uint8_t chipId{0xFF};
    uint8_t channelId{0xFF};

    UbseMtiDavid() = default;

    UbseMtiDavid(uint8_t slotId, uint8_t chipId);

    bool operator==(const UbseMtiDavid &other) const;

    bool operator<(const UbseMtiDavid &other) const;
};

struct UbseMtiDavidHash {
    std::size_t operator()(const UbseMtiDavid &d) const noexcept;
};

using UbseMtiIdevVfeDavidPair = std::pair<UbseMtiIdevVfe, UbseMtiDavid>;
using UbseMtiIdevFeDavidMapping = std::unordered_map<UbseMtiDavid, UbseMtiIdevPfe, UbseMtiDavidHash>;

class UbseMtiUrma {
public:
    static UbseMtiUrma &GetInstance();
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
    * @param resList [out] 每个vfe绑定的结果，与vfeDavidList一一对应
    *
    * @return UBSE_OK代表绑定成功，UBSE_ERROR代表绑定失败
    */
    virtual UbseResult BindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList,
                                 std::vector<bool> &resList) = 0;
    /**
    * @brief 解绑定1650 vfe与David
    * @param upi [in] upi
    * @param vfeList [in] vfe列表
    * @param resList [out] 每个vfe解绑定的结果，与vfeDavidList一一对应
    *
    * @return UBSE_OK代表解绑成功，UBSE_ERROR代表解绑失败
    */
    virtual UbseResult UnBindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &vfeDavidList,
                                   std::vector<bool> &resList) = 0;
    /**
    * @brief 将David 对应的Fe注册到Vm BusInstance
    * @param busInstance [in] UbseMtiBusInst实例
    * @param vfeList [in] vfe列表
    * @param resList [out] 每个vfe注测的结果，与vfeList一一对应
    *
    * @return UBSE_OK代表注册成功，UBSE_ERROR代表注册失败
    */
    virtual UbseResult RegDavidFeToVmBusInstance(const UbseMtiBusInst &busInstance,
                                                 const std::vector<UbseMtiIdevVfe> &vfeList,
                                                 std::vector<bool> &resList) = 0;
    /**
    * @brief 将David 对应的Fe从Vm BusInstance注销
    * @param busInstance [in] UbseMtiBusInst实例
    * @param vfeList [in] vfe列表
    * @param resList [out] 每个vfe注销的结果，与vfeList一一对应
    *
    * @return UBSE_OK代表注销成功，UBSE_ERROR代表注销失败
    */
    virtual UbseResult UnRegDavidFeFromVmBusInstance(const UbseMtiBusInst &busInstance,
                                                     const std::vector<UbseMtiIdevVfe> &vfeList,
                                                     std::vector<bool> &resList) = 0;
};
} // namespace ubse::mti::urma
#endif // UBSE_MTI_URMA_H