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
#include "adapter_plugins/mti/ubse_mti_1825.h"
#include "./out_of_band/ubse_mti_1825_out_of_band.h"
namespace ubse::mti::_1825 {

UbseMti1825Vf::UbseMti1825Vf(uint8_t slotId, uint8_t chipId, uint8_t dieId, uint16_t pfId, uint16_t vfId)
    : slotId(slotId),
      chipId(chipId),
      dieId(dieId),
      pfId(pfId),
      vfId(vfId)
{
}

UbseMti1825Pf::UbseMti1825Pf(uint8_t slotId, uint8_t chipId, uint8_t dieId, uint16_t pfId)
    : slotId(slotId),
      chipId(chipId),
      dieId(dieId),
      pfId(pfId)
{
}

bool UbseMti1825Vf::operator==(const UbseMti1825Vf &other) const
{
    return slotId == other.slotId && chipId == other.chipId && dieId == other.dieId && pfId == other.pfId &&
           vfId == other.vfId;
}

bool UbseMti1825Vf::operator<(const UbseMti1825Vf &other) const
{
    return std::tie(slotId, chipId, dieId, pfId, vfId) <
           std::tie(other.slotId, other.chipId, other.dieId, other.pfId, other.vfId);
}

bool UbseMti1825Pf::operator==(const UbseMti1825Pf &other) const
{
    return slotId == other.slotId && chipId == other.chipId && dieId == other.dieId && pfId == other.pfId;
}

bool UbseMti1825Pf::operator<(const UbseMti1825Pf &other) const
{
    return std::tie(slotId, chipId, dieId, pfId) < std::tie(other.slotId, other.chipId, other.dieId, other.pfId);
}

UbseMti1825 &UbseMti1825::GetInstance()
{
    static UbseMti1825OutOfBand instance;
    return instance;
}
} // namespace ubse::mti::_1825
