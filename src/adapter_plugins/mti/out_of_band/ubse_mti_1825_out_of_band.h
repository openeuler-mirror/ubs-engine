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

#ifndef UBSE_MTI_1825_OUT_OF_BAND_H
#define UBSE_MTI_1825_OUT_OF_BAND_H
#include "adapter_plugins/mti/ubse_mti_1825.h"
namespace ubse::mti::_1825 {
class UbseMti1825OutOfBand : public UbseMti1825 {
    UbseResult Get1825FeList(std::vector<UbseMti1825Pf>& pfList) override;

    UbseResult Reg1825FeToHostBusInstance(const UbseMtiBusInst& busInstance, const std::vector<UbseMti1825Vf>& vfList,
                                          std::vector<bool>& resList) override;

    UbseResult UnReg1825FeFromHostBusInstance(const UbseMtiBusInst& busInstance,
                                              const std::vector<UbseMti1825Vf>& vfList,
                                              std::vector<bool>& resList) override;

    UbseResult Reg1825FeToVmBusInstance(const UbseMtiBusInst& busInstance, const std::vector<UbseMti1825Vf>& vfList,
                                        std::vector<bool>& resList) override;

    UbseResult UnReg1825FeFromVmBusInstance(const UbseMtiBusInst& busInstance, const std::vector<UbseMti1825Vf>& vfList,
                                            std::vector<bool>& resList) override;
};
} // namespace ubse::mti::_1825
#endif // UBSE_MTI_1825_OUT_OF_BAND_H
