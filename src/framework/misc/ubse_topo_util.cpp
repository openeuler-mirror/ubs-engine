/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_topo_util.h"

namespace ubse::utils {
using namespace ubse::nodeController;

bool IsMultiPortTopo(const UbseCpuInfo& cpuInfo)
{
    std::unordered_map<ChipLocation, std::string, ChipLocation::Hash, ChipLocation::Equal> map;
    for (const auto& portInfo : cpuInfo.portInfos) {
        if (portInfo.second.portStatus == PortStatus::UP) {
            ChipLocation location{.slotId = portInfo.second.remoteSlotId, .chipId = portInfo.second.remoteChipId};
            if (map.find(location) != map.end()) {
                return true;
            }
            map[location] = location.slotId;
        }
    }
    return false;
}

bool IsSameSocketMultiPortTopo()
{
    auto nodeInfo = UbseNodeController::GetInstance().GetCurNode();
    auto cpuInfos = nodeInfo.cpuInfos;
    for (const auto& cpuInfo : cpuInfos) {
        if (IsMultiPortTopo(cpuInfo.second)) {
            return true;
        }
    }
    return false;
}
} // namespace ubse::utils