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

#include "ubse_node_mgr_module.h"

#include "conf_mode/ubse_node_discovery_config_mode.h"
#include "root_mode/ubse_node_discovery_common.h"
#include "static_mode/ubse_node_discovery_static_mode.h"
#include "ubse_error.h"
#include "ubse_node_static_info_mgr.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::nodeMgr {
using namespace ubse::urma;
OPTIONAL_MODULE_IMPL(UbseNodeMgrModule, UbseUrmaUvsModule);

UbseResult UbseNodeMgrModule::Initialize()
{
    return UbseNodeStaticInfoMgr::GetInstance().Init();
}

UbseResult UbseNodeMgrModule::Start()
{
    NodeDiscoveryMode mode = UbseNodeStaticInfoMgr::GetInstance().GetNodeDiscoveryMode();
    switch (mode) {
        case NodeDiscoveryMode::TCP_CONFIG_CLOS_MODE:
        case NodeDiscoveryMode::TCP_CONFIG_FULL_MESH_MODE:
            return UbseNodeDiscoveryConfigMode::GetInstance().Init();
        case NodeDiscoveryMode::TCP_ROOT_MODE:
            // todo: support root
            return UBSE_OK;
        case NodeDiscoveryMode::URMA_CLOS_MODE:
        case NodeDiscoveryMode::URMA_FULL_MESH_MODE:
            return UbseNodeDiscoveryStaticMode::GetInstance().Init();
        default:
            return UBSE_ERROR_INVAL;
    }
}

void UbseNodeMgrModule::Stop()
{
    // Do Nothing
}

void UbseNodeMgrModule::UnInitialize()
{
    // Do Nothing
}
}