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

#ifndef UBSE_URMA_TOPO_CONFIG_H
#define UBSE_URMA_TOPO_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

#include "ubse_common_def.h"

namespace ubse::urma {
using namespace ubse::common::def;

struct UbseUrmaTopoPort {
    uint32_t chipId{0};
    uint32_t dieId{0};
    uint32_t portId{0};
};

struct UbseUrmaTopoLink {
    UbseUrmaTopoPort localPort;
    UbseUrmaTopoPort remotePort;
};

struct UbseUrmaTopoConfig {
    std::string version;
    std::string nodeType;
    std::string linkType;
    std::vector<UbseUrmaTopoPort> nodePorts;
    std::vector<UbseUrmaTopoLink> links;
};

enum class UbseUrmaTopoMode {
    NON_CROSS,
    HCCS_CROSS,
};

UbseUrmaTopoMode GetUrmaTopoMode();

UbseResult ParseUrmaTopoConfig(const std::string &topoFile, UbseUrmaTopoConfig &topoConfig);

UbseResult LoadUrmaTopoConfig(UbseUrmaTopoMode topoMode, UbseUrmaTopoConfig &topoConfig);
} // namespace ubse::urma

#endif // UBSE_URMA_TOPO_CONFIG_H
