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

#ifndef UBS_ENGINE_UBSE_NODE_DISCOVERY_STATIC_MODE_H
#define UBS_ENGINE_UBSE_NODE_DISCOVERY_STATIC_MODE_H

#include "ubse_node_controller.h"
#include "ubse_node_mgr_def.h"

namespace ubse::nodeMgr {
using namespace ubse::nodeController;

class UbseNodeDiscoveryStaticMode {
public:
    static UbseNodeDiscoveryStaticMode &GetInstance()
    {
        static UbseNodeDiscoveryStaticMode instance;
        return instance;
    }

    uint32_t Init();

    void UnInit();

private:
    uint32_t GenerateClusterStaticInfo();

    uint32_t GenerateClosClusterStaticInfo();

    uint32_t GenerateClosStaticInfo(const UbseNodeStaticInfo &currentSuperNode, uint32_t serverIdx,
                                    UbseNodeStaticInfo &info);

    uint32_t podCapability_ = 0;

    bool isClos_ = false;
};
} // namespace ubse::nodeMgr

#endif // UBS_ENGINE_UBSE_NODE_DISCOVERY_STATIC_MODE_H
