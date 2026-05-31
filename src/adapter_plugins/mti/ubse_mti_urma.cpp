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
#include "adapter_plugins/mti/ubse_mti_urma.h"
#include "./out_of_band/ubse_mti_urma_out_of_band.h"
namespace ubse::mti::urma {

UbseMtiIdevVfe::UbseMtiIdevVfe(const UbseMtiUbController& ubController, uint8_t pfeId, uint8_t vfeId)
    : ubController(ubController),
      pfeId(pfeId),
      vfeId(vfeId)
{
}

bool UbseMtiIdevVfe::operator==(const UbseMtiIdevVfe& other) const
{
    return ubController == other.ubController && pfeId == other.pfeId && vfeId == other.vfeId;
}

bool UbseMtiIdevVfe::operator<(const UbseMtiIdevVfe& other) const
{
    return std::tie(ubController, pfeId, vfeId) < std::tie(other.ubController, other.pfeId, other.vfeId);
}

UbseMtiIdevPfe::UbseMtiIdevPfe(const UbseMtiUbController& ubController, uint8_t pfeId)
    : ubController(ubController),
      pfeId(pfeId)
{
}

void UbseMtiIdevPfe::AddVfe(const UbseMtiIdevVfe& vfe)
{
    vfeList.emplace_back(vfe);
}

bool UbseMtiIdevPfe::operator==(const UbseMtiIdevPfe& other) const
{
    return ubController == other.ubController && pfeId == other.pfeId;
}

bool UbseMtiIdevPfe::operator<(const UbseMtiIdevPfe& other) const
{
    return std::tie(ubController, pfeId) < std::tie(other.ubController, other.pfeId);
}

UbseMtiDavid::UbseMtiDavid(uint8_t slotId, uint8_t chipId) : slotId(slotId), chipId(chipId) {}

bool UbseMtiDavid::operator==(const UbseMtiDavid& other) const
{
    return slotId == other.slotId && chipId == other.chipId;
}

bool UbseMtiDavid::operator<(const UbseMtiDavid& other) const
{
    return std::tie(slotId, chipId) < std::tie(other.slotId, other.chipId);
}

std::size_t UbseMtiDavidHash::operator()(const UbseMtiDavid& d) const noexcept
{
    auto key = static_cast<uint16_t>((static_cast<uint16_t>(d.slotId) << 8) | d.chipId);
    return std::hash<uint16_t>{}(key);
}

UbseMtiUrma& UbseMtiUrma::GetInstance()
{
    static UbseMtiUrmaOutOfBand instance;
    return instance;
}

} // namespace ubse::mti::urma
