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

#ifndef UBSE_MANAGER_UBSE_TOPY_UTIL_H
#define UBSE_MANAGER_UBSE_TOPY_UTIL_H
#include "ubse_node_controller.h"

namespace ubse::utils {
using ubse::nodeController::PortStatus;
using ubse::nodeController::UbseCpuInfo;
using ubse::nodeController::UbseNodeController;

struct ChipLocation {
    std::string slotId;
    std::string chipId;
    struct Hash {
        std::size_t operator()(const ChipLocation& loc) const
        {
            return std::hash<std::string>{}(loc.slotId) ^ (std::hash<std::string>{}(loc.chipId) << 1);
        }
    };

    struct Equal {
        bool operator()(const ChipLocation& lhs, const ChipLocation& rhs) const
        {
            return lhs.slotId == rhs.slotId && lhs.chipId == rhs.chipId;
        }
    };
};

bool IsMultiPortTopo(const UbseCpuInfo& cpuInfo);

bool IsSameSocketMultiPortTopo();
} // namespace ubse::utils

#endif // UBSE_MANAGER_UBSE_TOPY_UTIL_H
