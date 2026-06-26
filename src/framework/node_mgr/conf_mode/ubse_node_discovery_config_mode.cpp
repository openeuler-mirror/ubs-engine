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

#include "ubse_node_discovery_config_mode.h"

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_net_util.h"
#include "ubse_node_static_info_mgr.h"
#include "adapter_plugins/mti/ubse_smbios.h"

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeMgr {
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::utils;

UbseResult UbseNodeDiscoveryConfigMode::Init()
{
    auto &nodeStaticInfoMgr = UbseNodeStaticInfoMgr::GetInstance();
    isClos_ = nodeStaticInfoMgr.IsClos();
    if (isClos_) {
        podCapability_ = nodeStaticInfoMgr.GetPodCapability();
    }

    const std::vector<std::string> ipList = UbseNodeStaticInfoMgr::GetInstance().GetClusterIpList();

    UbseNodeStaticInfo node{};
    auto ret = UbseNodeStaticInfoMgr::GetInstance().InitCurNodeInfo(node);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init current node failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = UbseNetUtil::FindLocalIpInIpList(ipList, node.addr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init current node ip failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "cur node=" << node.nodeId << " groupId=" << node.groupId;
    UbseNodeStaticInfoMgr::GetInstance().SetCurrentNode(node);
    GenerateClusterTopo(ipList);
    return UBSE_OK;
}

void UbseNodeDiscoveryConfigMode::GenerateClusterTopo(const std::vector<std::string> &clusterIpList)
{
    auto &nodeStaticInfoMgr = UbseNodeStaticInfoMgr::GetInstance();
    const auto &currentNode = nodeStaticInfoMgr.GetCurrentNode();
    std::vector<UbseNodeStaticInfo> clusterTopoList{};
    clusterTopoList.reserve(clusterIpList.size() - 1);
    for (size_t i = 0; i < clusterIpList.size(); ++i) {
        if (clusterIpList[i] == currentNode.addr) {
            continue;
        }
        UbseNodeStaticInfo node{};
        node.nodeId = std::to_string(i + 1);
        node.groupId = isClos_ ? (i / podCapability_) + 1 : 0;
        node.superPodId = currentNode.superPodId;
        node.addr = clusterIpList[i];
        clusterTopoList.push_back(node);
    }
    nodeStaticInfoMgr.SetNodes(clusterTopoList);
}

void UbseNodeDiscoveryConfigMode::UnInit() {}
} // namespace ubse::nodeMgr
