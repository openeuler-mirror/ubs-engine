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

#include "../ubse_node_discovery.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_net_util.h"
#include "ubse_smbios.h"

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeDiscovery {
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::utils;

UbseResult UbseNodeDiscoveryConfigMode::InitCurNodeInfo(UbseNodeStaticInfo &node)
{
    auto &smbios = UbseSmbios::GetInstance();
    uint8_t slotId = 0;
    auto ret = smbios.GetSlotId(slotId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get slot id failed, " << FormatRetCode(ret);
        return ret;
    }
    if (!isClos_) {
        node.nodeId = std::to_string(slotId);
        node.podId = 0;
        node.superPodId = 0;
    } else {
        uint32_t serverIndex = 0;
        ret = smbios.GetServerIdx(serverIndex);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "get server index failed, " << FormatRetCode(ret);
            return ret;
        }
        node.nodeId = std::to_string(serverIndex + 1); // nodeId从1开始
        node.podId = serverIndex / podCapability_ + 1; // 逻辑机柜从1开始
        ret = smbios.GetSuperPodId(node.superPodId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "get super pod id failed, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult UbseNodeDiscoveryConfigMode::Init()
{
    const auto confModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_ERROR << "confModule nullptr";
        return UBSE_ERROR_NULLPTR;
    }

    std::string ipListStr;
    auto ret = confModule->GetConf<std::string>("ubse.rpc", "cluster.ipList", ipListStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get ips conf failed, " << FormatRetCode(ret);
        return ret;
    }
    isClos_ = UbseSmbios::GetInstance().IsClosType();
    if (isClos_) {
        ret = confModule->GetConf<uint32_t>("ubse.rpc", "cluster.pod.capability", podCapability_);
        if (ret != UBSE_OK || podCapability_ < MIN_POD_CAPABILITY || podCapability_ > MAX_CLUSTER_SIZE) {
            UBSE_LOG_WARN << "pod capability=" << podCapability_ << " invalid, will use default";
            podCapability_ = DEFAULT_POD_CAPABILITY;
        }
    }

    const std::vector<std::string> ipList = UbseNetUtil::ParseIpList(ipListStr);
    if (ipList.empty()) {
        UBSE_LOG_ERROR << "cluster ip empty";
        return UBSE_ERROR_NULLPTR;
    }

    UbseNodeStaticInfo node{};
    ret = InitCurNodeInfo(node);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init current node failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = UbseNetUtil::FindLocalIpByRemote(ipList[0], node.addr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init current node ip failed, " << FormatRetCode(ret);
        return ret;
    }
    const auto it = std::find(ipList.cbegin(), ipList.cend(), node.addr);
    if (it == ipList.end()) {
        UBSE_LOG_ERROR << "current node=" << node.addr << " not in cluster config";
        return UBSE_ERROR_NULLPTR;
    }
    UBSE_LOG_INFO << "cur node=" << node.nodeId << " podId=" << node.podId;
    auto &nodeDiscovery = UbseNodeDiscovery::GetInstance();
    nodeDiscovery.SetCurrentNode(node);
    GenerateClusterTopo(ipList);
    return UBSE_OK;
}

void UbseNodeDiscoveryConfigMode::GenerateClusterTopo(const std::vector<std::string> &clusterIpList)
{
    auto &nodeDiscovery = UbseNodeDiscovery::GetInstance();
    const auto &currentNode = nodeDiscovery.GetCurrentNode();
    std::vector<UbseNodeStaticInfo> clusterTopoList{};
    clusterTopoList.reserve(clusterIpList.size() - 1);
    for (size_t i = 0; i < clusterIpList.size(); ++i) {
        if (clusterIpList[i] == currentNode.addr) {
            continue;
        }
        UbseNodeStaticInfo node{};
        node.nodeId = std::to_string(i + 1);
        node.podId = isClos_ ? (i / podCapability_) + 1 : 0;
        node.superPodId = currentNode.superPodId;
        node.addr = clusterIpList[i];
        clusterTopoList.push_back(node);
    }
    nodeDiscovery.SetNodes(clusterTopoList);
}

void UbseNodeDiscoveryConfigMode::UnInit() {}
} // namespace ubse::nodeDiscovery
