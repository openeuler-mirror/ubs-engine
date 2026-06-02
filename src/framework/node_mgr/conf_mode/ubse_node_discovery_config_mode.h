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

#include "ubse_node_mgr_def.h"

namespace ubse::nodeMgr {

class UbseNodeDiscoveryConfigMode {
public:
    static UbseNodeDiscoveryConfigMode &GetInstance()
    {
        static UbseNodeDiscoveryConfigMode instance;
        return instance;
    }

    UbseNodeDiscoveryConfigMode() = default;
    ~UbseNodeDiscoveryConfigMode() = default;
    UbseNodeDiscoveryConfigMode(const UbseNodeDiscoveryConfigMode &) = delete;
    UbseNodeDiscoveryConfigMode &operator=(const UbseNodeDiscoveryConfigMode &) = delete;

    uint32_t Init();

    void UnInit();

private:
    void GenerateClusterTopo(const std::vector<std::string> &clusterIpList);

    uint32_t podCapability_ = DEFAULT_POD_CAPABILITY;

    bool isClos_ = false;
};
} // namespace ubse::nodeMgr

#endif // UBS_ENGINE_UBSE_DISCOVERY_CONFIG_MODE_H
