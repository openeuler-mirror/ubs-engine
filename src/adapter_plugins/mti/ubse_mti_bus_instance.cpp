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
#include "adapter_plugins/mti/ubse_mti_bus_instance.h"
#include "./out_of_band/ubse_mti_bus_instance_out_of_band.h"
namespace ubse::mti::bus_instance {
UbseMtiUbController::UbseMtiUbController(uint8_t chipId, uint8_t dieId) : chipId(chipId), dieId(dieId) {}

bool UbseMtiUbController::operator==(const UbseMtiUbController& other) const
{
    return slotId == other.slotId && chipId == other.chipId && dieId == other.dieId;
}

bool UbseMtiUbController::operator<(const UbseMtiUbController& other) const
{
    return std::tie(slotId, chipId, dieId) < std::tie(other.slotId, other.chipId, other.dieId);
}

UbseMtiBusInstance& UbseMtiBusInstance::GetInstance()
{
    static UbseMtiBusInstanceOutOfBand instance;
    return instance;
}
} // namespace ubse::mti::bus_instance
