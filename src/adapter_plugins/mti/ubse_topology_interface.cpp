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

#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "ubse_conf.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_module.h"

namespace ubse::mti {
using namespace ubse::context;
using namespace ubse::config;

UBSE_DEFINE_THIS_MODULE("ubse");
uint32_t UbseGetLocalNodeInfo(MtiNodeInfo& ubseNodeInfo)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "mti module not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return module->UbseGetLocalNodeInfo(ubseNodeInfo);
}

uint32_t UbseGetAllNodeInfos(std::vector<MtiNodeInfo>& ubseNodeInfos)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "mti module not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    bool ubEnable = true;
    UbseGetBool("ubse.rpc", "urma.enable", ubEnable);
    if (ubEnable) {
        return module->UbseGetAllNodeInfos(ubseNodeInfos);
    }

    std::vector<std::string> ipList = module->GetClusterIpList();
    for (auto& ip : ipList) {
        MtiNodeInfo nodeInfo{};
        nodeInfo.eid = ip;
        ubseNodeInfos.emplace_back(nodeInfo);
    }
    return UBSE_OK;
}
} // namespace ubse::mti