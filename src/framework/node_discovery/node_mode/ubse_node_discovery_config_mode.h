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

#ifndef UBS_ENGINE_UBSE_DISCOVERY_CONFIG_MODE_H
#define UBS_ENGINE_UBSE_DISCOVERY_CONFIG_MODE_H

#include <vector>

#include "../ubse_node_discovery_def.h"
#include "ubse_common_def.h"

namespace ubse::nodeDiscovery {
using namespace ubse::common::def;

class UbseNodeDiscoveryConfigMode {
public:
    static UbseNodeDiscoveryConfigMode &GetInstance()
    {
        static UbseNodeDiscoveryConfigMode instance;
        return instance;
    }

    UbseResult Init();

    void UnInit();

private:
    void GenerateClusterTopo(const std::vector<std::string> &clusterIpList);

    UbseResult InitCurNodeInfo(UbseNodeStaticInfo &node);

    uint32_t podCapability_ = DEFAULT_POD_CAPABILITY;

    bool isClos_ = false;
};
} // namespace ubse::nodeDiscovery

#endif // UBS_ENGINE_UBSE_DISCOVERY_CONFIG_MODE_H
