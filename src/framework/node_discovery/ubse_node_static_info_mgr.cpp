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

#include "ubse_node_static_info_mgr.h"

#include <mutex>

#include "adapter_plugins/smbios/ubse_smbios.h"
#include "ubse_com_module.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_net_util.h"

namespace ubse::nodeDiscovery {
using namespace ubse::com;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::log;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::utils;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseNodeStaticInfoMgr::Init()
{
    std::unique_lock lock(nodeMutex_);
    auto ret = InitClusterMode();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Init cluster mode failed";
        return ret;
    }
    isUrma_ = mode_ == NodeDiscoveryMode::URMA_CLOS_MODE || mode_ == NodeDiscoveryMode::URMA_FULL_MESH_MODE;
    return UBSE_OK;
}

NodeMap UbseNodeStaticInfoMgr::GetAllNodes() const
{
    std::shared_lock lock(nodeMutex_);
    return nodes_;
}

std::unordered_map<std::string, UbseNodeStaticInfo> UbseNodeStaticInfoMgr::GetNodesByGroupId(uint16_t groupId)
{
    std::shared_lock lock(nodeMutex_);
    auto it = nodes_.find(groupId);
    if (it != nodes_.end()) {
        return it->second;
    }
    return {};
}

UbseNodeStaticInfo UbseNodeStaticInfoMgr::GetUbseNodeByAddr(const std::string &ip)
{
    std::shared_lock lock(nodeMutex_);
    for (const auto &pod : nodes_) {
        for (const auto &superNode : pod.second) {
            if (superNode.second.addr == ip) {
                return superNode.second;
            }
        }
    }
    return {};
}

UbseNodeStaticInfo UbseNodeStaticInfoMgr::GetUbseNodeById(const std::string &id)
{
    std::shared_lock lock(nodeMutex_);
    for (const auto &pod : nodes_) {
        auto it = pod.second.find(id);
        if (it != pod.second.end()) {
            return it->second; // 找到了直接返回对应的 value
        }
    }
    return {};
}

UbseNodeStaticInfo UbseNodeStaticInfoMgr::GetCurrentNode()
{
    std::shared_lock lock(nodeMutex_);
    return currentNode_;
}

void UbseNodeStaticInfoMgr::SetNodes(const std::vector<UbseNodeStaticInfo> &nodes)
{
    std::unique_lock lock(nodeMutex_);
    for (const auto &node : nodes) {
        nodes_[node.groupId][node.nodeId] = node;
    }
}

void UbseNodeStaticInfoMgr::SetCurrentNode(UbseNodeStaticInfo node)
{
    std::unique_lock lock(nodeMutex_);
    currentNode_ = node;
    nodes_[node.groupId][node.nodeId] = node;
}

NodeDiscoveryMode UbseNodeStaticInfoMgr::GetNodeDiscoveryMode()
{
    std::shared_lock lock(nodeMutex_);
    return mode_;
}

bool UbseNodeStaticInfoMgr::IsUrma()
{
    std::shared_lock lock(nodeMutex_);
    return isUrma_;
}

bool UbseNodeStaticInfoMgr::IsClos()
{
    std::shared_lock lock(nodeMutex_);
    return isClos_;
}

std::vector<std::string> UbseNodeStaticInfoMgr::GetRootIpList()
{
    std::shared_lock lock(nodeMutex_);
    return rootIps_;
}

std::vector<std::string> UbseNodeStaticInfoMgr::GetClusterIpList()
{
    std::shared_lock lock(nodeMutex_);
    return clusterIps_;
}

UbseResult UbseNodeStaticInfoMgr::InitClusterMode()
{
    isClos_ = UbseSmbios::GetInstance().IsClosType();
    auto confModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_ERROR << "confModule nullptr";
        return UBSE_ERROR_NULLPTR;
    }

    std::string rootIpStrs{};
    auto ret = confModule->GetConf<std::string>("ubse.rpc", "cluster.rootList", rootIpStrs);
    if (ret == UBSE_OK) {
        ret = UbseNetUtil::ParseIpList(rootIpStrs, rootIps_);
        if (ret != UBSE_OK || rootIps_.empty()) {
            return UBSE_ERROR_CONF_INVALID;
        }
    }

    std::string clusterIpStr{};
    ret = confModule->GetConf<std::string>("ubse.rpc", "cluster.ipList", clusterIpStr);
    if (ret == UBSE_OK) {
        ret = UbseNetUtil::ParseIpList(clusterIpStr, clusterIps_);
        if (ret != UBSE_OK || clusterIps_.empty()) {
            return UBSE_ERROR_CONF_INVALID;
        }
    }

    if (!isClos_) {
        mode_ =
            clusterIps_.empty() ? NodeDiscoveryMode::URMA_FULL_MESH_MODE : NodeDiscoveryMode::TCP_CONFIG_FULL_MESH_MODE;
        return UBSE_OK;
    }

    if (!rootIps_.empty()) {
        mode_ = NodeDiscoveryMode::TCP_ROOT_MODE;
    } else if (!clusterIps_.empty()) {
        mode_ = NodeDiscoveryMode::TCP_CONFIG_CLOS_MODE;
    } else {
        mode_ = NodeDiscoveryMode::URMA_CLOS_MODE;
    }
    return UBSE_OK;
}

uint32_t UbseNodeStaticInfoMgr::GetPodCapability()
{
    auto confModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_WARN << "confModule nullptr, use default pod capability=" << DEFAULT_POD_CAPABILITY;
        return DEFAULT_POD_CAPABILITY;
    }
    uint32_t podCapability = 0;
    auto ret = confModule->GetConf<uint32_t>("ubse.rpc", "cluster.pod.capability", podCapability);
    if (ret != UBSE_OK || podCapability < MIN_POD_CAPABILITY || podCapability > MAX_CLUSTER_SIZE) {
        UBSE_LOG_WARN << "pod capability=" << podCapability << " invalid, will use default pod capability="
            << DEFAULT_POD_CAPABILITY;
        return DEFAULT_POD_CAPABILITY;
    }
    return podCapability;
}

UbseResult UbseNodeStaticInfoMgr::InitCurNodeInfo(UbseNodeStaticInfo &node)
{
    bool isClos = UbseSmbios::GetInstance().IsClosType();
    auto &smbios = UbseSmbios::GetInstance();
    uint8_t slotId = 0;
    auto ret = smbios.GetSlotId(slotId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get slot id failed, " << FormatRetCode(ret);
        return ret;
    }
    if (!isClos) {
        node.nodeId = std::to_string(slotId);
        node.groupId = 0;
        node.superPodId = 0;
    } else {
        uint32_t serverIndex = 0;
        ret = smbios.GetServerIdx(serverIndex);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "get server index failed, " << FormatRetCode(ret);
            return ret;
        }
        node.nodeId = std::to_string(serverIndex + 1); // nodeId从1开始
        uint32_t podCapability = GetPodCapability();
        node.groupId = serverIndex / podCapability + 1; // 逻辑机柜从1开始
        ret = smbios.GetSuperPodId(node.superPodId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "get super pod id failed, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}
} // namespace ubse::nodeDiscovery