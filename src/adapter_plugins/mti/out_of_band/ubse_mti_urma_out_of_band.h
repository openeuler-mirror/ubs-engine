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

#ifndef UBSE_MTI_URMA_OUT_OF_BAND_H
#define UBSE_MTI_URMA_OUT_OF_BAND_H
#include "adapter_plugins/mti/ubse_mti_urma.h"
namespace ubse::mti::urma {
class UbseMtiUrmaOutOfBand : public UbseMtiUrma {
    UbseResult GetIdevFeList(std::vector<UbseMtiIdevPfe>& feList) override;

    UbseResult GetIdevFeDavidMapping(UbseMtiIdevFeDavidMapping& mapping) override;

    UbseResult BindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair>& vfeDavidList,
                         std::vector<bool>& resList) override;

    UbseResult UnBindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair>& vfeDavidList,
                           std::vector<bool>& resList) override;
    UbseResult RegDavidFeToVmBusInstance(const UbseMtiBusInst& busInstance, const std::vector<UbseMtiIdevVfe>& vfeList,
                                         std::vector<bool>& resList) override;

    UbseResult UnRegDavidFeFromVmBusInstance(const UbseMtiBusInst& busInstance,
                                             const std::vector<UbseMtiIdevVfe>& vfeList,
                                             std::vector<bool>& resList) override;
};
} // namespace ubse::mti::urma
#endif // UBSE_MTI_URMA_OUT_OF_BAND_H
