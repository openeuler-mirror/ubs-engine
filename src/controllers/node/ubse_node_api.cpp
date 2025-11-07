/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * UbseEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_node_api.h"

#include "src/sdk/include/ubs_engine.h"
#include "ubse_api_server_module.h"
#include "ubse_context.h"
#include "ubse_logger_inner.h"
#include "ubse_node_api_convert.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_query_api.h"
#include "ubse_node_module.h"
#include "ubse_node_topology.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"

namespace ubse::node::api {
using namespace ubse::context;
using namespace ubse::serial;
using namespace ubse::log;
using namespace ubse::node;
using namespace ::api::server;
using namespace ubse::nodeController;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_API_SERVER_MID)
UbseResult UbseSplitStringByHyphen(const std::string &ubse_node_socket_id, UbseSerialization &ubse_serial)
{
    size_t hyphen_pos = ubse_node_socket_id.find('-');
    if (hyphen_pos == std::string::npos) {
        UBSE_LOG_ERROR << "Invalid node-socket ID format.";
        return UBSE_ERROR;
    }
    std::string ubse_node_id = ubse_node_socket_id.substr(0, hyphen_pos);
    std::string ubse_socket_id = ubse_node_socket_id.substr(hyphen_pos + 1);
    if (ubse_node_id.empty() || ubse_socket_id.empty()) {
        UBSE_LOG_ERROR << "Node ID or socket ID is empty.";
        return UBSE_ERROR_NULL_INFO;
    }
    ubse_serial << std::string(ubse_node_id + "(" + ubse_node_id + ")") << ubse_socket_id;
    return ubse_serial.Check() ? UBSE_OK : UBSE_ERROR_SERIALIZE_FAILED;
}

uint32_t UbseNodeApi::UbseQueryTopologyInfoHandle(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Node topo IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::unordered_map<std::string, std::vector<MemNodeData>> ubse_node_topology;
    uint32_t ret;
    ret = UbseMemGetTopologyInfo(ubse_node_topology);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to invoke the query method.";
        return ret;
    }
    UbseSerialization ubse_serial;
    ubse_serial << right_v<std::size_t>(ubse_node_topology.size());
    for (auto &ubse_node_topo : ubse_node_topology) {
        std::string ubse_node_socket_id = ubse_node_topo.first;
        if (std::count(ubse_node_socket_id.begin(), ubse_node_socket_id.end(), '-') > 1) {
            UBSE_LOG_WARN << "Got abnormal socket information, " << ubse_node_socket_id;
            continue;
        }
        ret = UbseSplitStringByHyphen(ubse_node_socket_id, ubse_serial);
        if (ret != UBSE_OK) {
            return ret;
        }
        ubse_serial << right_v<std::size_t>(ubse_node_topo.second.size());
        for (auto &ubse_node : ubse_node_topo.second) {
            ubse_serial << std::string(ubse_node.nodeId + "(" + ubse_node.nodeId + ")") << ubse_node.socket.socketId;
        }
    }
    if (!ubse_serial.Check()) {
        UBSE_LOG_ERROR << "Serialization of topo response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseIpcMessage res{ubse_serial.GetBuffer(), static_cast<uint32_t>(ubse_serial.GetLength())};
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseNodeApi::UbseServerNodeGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UbseNode node{};
    UbseNodeGet(node);
    UbseIpcMessage resp{};
    resp.length = UBSE_NODE_SIZE;
    auto ptr = new (std::nothrow) uint8_t[resp.length];
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "malloc resp failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    resp.buffer = static_cast<uint8_t *>(ptr);
    auto ret = UbseNodePack(node, resp.buffer);
    if (ret != IPC_SUCCESS) {
        UBSE_LOG_ERROR << "UbseNode pack failed," << FormatRetCode(ret);
        SafeDeleteArray(resp.buffer);
        return ret;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        SafeDeleteArray(resp.buffer);
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseNodeGet response send failed," << FormatRetCode(ret);
    }
    SafeDeleteArray(resp.buffer);
    return ret;
}

uint32_t UbseNodeApi::UbseServerNodeList(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    std::vector<def::UbseNode> nodeList{};
    UbseNodeList(nodeList);
    uint32_t nodeCnt = nodeList.size();
    UbseIpcMessage resp{};
    resp.length = sizeof(uint32_t) + nodeCnt * UBSE_NODE_SIZE;
    auto ptr = new (std::nothrow) uint8_t[resp.length];
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "malloc resp failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    resp.buffer = static_cast<uint8_t *>(ptr);
    auto ret = UbseNodeListPack(nodeList, resp.buffer);
    if (ret != IPC_SUCCESS) {
        UBSE_LOG_ERROR << "UbseNodeList pack failed," << FormatRetCode(ret);
        SafeDeleteArray(resp.buffer);
        return ret;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        SafeDeleteArray(resp.buffer);
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    SafeDeleteArray(resp.buffer);
    return ret;
}

uint32_t UbseNodeApi::UbseServerCpuTopoList(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    std::vector<def::UbseCpuLink> linkList{};
    UbseNodeCpuTopoList(linkList);
    uint32_t linkCnt = linkList.size();
    UbseIpcMessage resp{};
    resp.length = sizeof(uint32_t) + linkCnt * UBSE_CPU_LINK_SIZE;
    auto ptr = malloc(resp.length);
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "malloc resp failed. ";
        return UBSE_ERROR_NULLPTR;
    }
    resp.buffer = static_cast<uint8_t *>(ptr);
    auto ret = UbseCpuLinkListPack(linkList, resp.buffer);
    if (ret != IPC_SUCCESS) {
        UBSE_LOG_ERROR << "UbseCpuLinkList pack failed," << FormatRetCode(ret);
        free(resp.buffer);
        return ret;
    }

    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        free(resp.buffer);
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    free(resp.buffer);
    return ret;
}

void UbseClusterList(std::vector<UbseNodeInfo> &nodeList)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    if (nodeInfos.empty()) {
        return;
    }
    std::vector<UbseNodeInfo> staticNodeInfos = UbseNodeController::GetInstance().GetStaticNodeInfo();
    nodeList.reserve(std::max(nodeInfos.size(), staticNodeInfos.size()));
    std::transform(nodeInfos.begin(), nodeInfos.end(), std::back_inserter(nodeList),
                   [](auto &kv) { return kv.second; });
    // 加入静态节点信息
    for (auto &nodeInfo : staticNodeInfos) {
        if (nodeInfos.find(nodeInfo.nodeId) == nodeInfos.end()) {
            ConvertStrToUint32(nodeInfo.nodeId, nodeInfo.slotId);
            nodeList.emplace_back(nodeInfo);
        }
    }
    std::sort(nodeList.begin(), nodeList.end(), [](UbseNodeInfo &l, UbseNodeInfo &r) { return l.slotId < r.slotId; });
}

std::unordered_map<std::string, std::string> UbseGetRoleMap()
{
    std::unordered_map<std::string, std::string> roleMap{};
    roleMap.reserve(NO_2);
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load";
        return roleMap;
    }

    Node masterNode{};
    auto ret = module->UbseGetMasterNode(masterNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get master node failed, " << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << UBSE_ROLE_MASTER << "=" << masterNode.id;
        roleMap[masterNode.id] = UBSE_ROLE_MASTER;
    }

    Node standbyNode{};
    ret = module->UbseGetStandbyNode(standbyNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get standby node failed, " << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << UBSE_ROLE_SLAVE << "=" << standbyNode.id;
        roleMap[standbyNode.id] = UBSE_ROLE_SLAVE;
    }
    return roleMap;
}

uint32_t UbseNodeApi::UbseQueryClusterInfo(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Cluster IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::vector<UbseNodeInfo> nodeList;
    UbseClusterList(nodeList);
    std::unordered_map<std::string, std::string> roleMap = UbseGetRoleMap();
    UbseSerialization ubseSerial;
    ubseSerial << (right_v<std::size_t>(nodeList.size()));
    // hostName(slotId), role, bondingEid;
    for (const auto &node : nodeList) {
        UBSE_LOG_INFO << "hostname=" << node.hostName << ", slotId=" << node.slotId
                      << ", clusterState=" << static_cast<uint32_t>(node.clusterState);
        bool isOnline = node.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
                        node.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING;
        std::string slotIdStr = std::to_string(node.slotId);
        ubseSerial << ((!isOnline || node.hostName.empty() ? "-" : node.hostName) + "(" + slotIdStr + ")");
        if (!isOnline) {
            ubseSerial << "-";
        } else {
            auto it = roleMap.find(slotIdStr);
            if (it != roleMap.end()) {
                ubseSerial << it->second;
            } else {
                ubseSerial << UBSE_ROLE_AGENT;
            }
        }
        ubseSerial << node.bondingEid;
    }
    if (!ubseSerial.Check()) {
        UBSE_LOG_ERROR << "Serialization of cluster response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseIpcMessage res{ubseSerial.GetBuffer(), static_cast<uint32_t>(ubseSerial.GetLength())};
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ClusterInfoQuery response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseNodeApi::Register()
{
    auto ubse_api_server_module = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubse_api_server_module == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret =
        ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_NODE_CLI_TOPO_LIST, UbseQueryTopologyInfoHandle);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_NODE_GET, UbseServerNodeGet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_NODE_LIST, UbseServerNodeList);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_NODE_CPU_TOPO_LIST, UbseServerCpuTopoList);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_CLUSTER_INFO, UbseQueryClusterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of TopoInfoQuery IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::node::api