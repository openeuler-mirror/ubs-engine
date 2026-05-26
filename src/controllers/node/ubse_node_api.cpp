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

#include "ubs_engine.h"
#include "ubse_api_server_module.h"
#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_node.h"
#include "ubse_node_api_convert.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_query_api.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"
#include "ubse_election_module.h"
#include "ubse_smbios.h"


namespace ubse::node::api {
using namespace ubse::context;
using namespace ubse::serial;
using namespace ubse::log;
using namespace ::api::server;
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::smbios;

UBSE_DEFINE_THIS_MODULE("ubse");

const std::string TOPO_PERMISSION = "topo";
const std::string MEM_STAT_PERMISSION = "mem.stat";

uint32_t UbseNodeApi::UbseServerNodeGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UbseNode node{};
    UbseNodeGet(node);
    UbseIpcMessage resp{};
    auto ret = UbseNodePack(node, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNode pack failed," << FormatRetCode(ret);
        return ret;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        delete[] resp.buffer;
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseNodeGet response send failed," << FormatRetCode(ret);
    }
    delete[] resp.buffer;
    resp.buffer = nullptr;
    return ret;
}

uint32_t UbseNodeApi::UbseServerNodeList(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    std::vector<UbseNode> nodeList{};
    UbseNodeList(nodeList);
    UbseIpcMessage resp{};
    auto ret = UbseNodeListPack(nodeList, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNodeList pack failed," << FormatRetCode(ret);
        return ret;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        delete[] resp.buffer;
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    delete[] resp.buffer;
    resp.buffer = nullptr;
    return ret;
}

uint32_t UbseNodeApi::UbseServerCpuTopoList(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    std::vector<UbseCpuLink> linkList{};
    UbseNodeCpuTopoList(linkList);
    UbseIpcMessage resp{};
    auto ret = UbseCpuLinkListPack(linkList, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseCpuLinkList pack failed," << FormatRetCode(ret);
        return ret;
    }

    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        delete[] resp.buffer;
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    delete[] resp.buffer;
    resp.buffer = nullptr;
    return ret;
}

uint32_t UbseNodeApi::UbseServerNodeNumaMemGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    uint32_t slotId{};
    auto ret = UbseSlotIdUnpack(req, slotId);
    if (ret != UBSE_OK) {
        return ret;
    }
    std::vector<UbseNumaNodeInfo> numaNodeInfoList{};
    ret = UbseNodeNumaMemGet(std::to_string(slotId), numaNodeInfoList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get numa info failed," << FormatRetCode(ret);
        return ret;
    }
    UbseIpcMessage resp{};
    ret = UbseNumaInfoListPack(numaNodeInfoList, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNumaInfoList pack failed," << FormatRetCode(ret);
        return ret;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        delete[] resp.buffer;
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseNumaMemGet response send failed," << FormatRetCode(ret);
    }
    delete[] resp.buffer;
    resp.buffer = nullptr;
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

std::unordered_map<std::string, std::string> UbseGetRoleMap(const UbseRequestContext &context)
{
    std::unordered_map<std::string, std::string> roleMap{};
    roleMap.reserve(NO_2);
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "requestId=" << context.requestId << ", election module not load";
        return roleMap;
    }

    Node masterNode{};
    auto ret = module->UbseGetMasterNode(masterNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "requestId=" << context.requestId << ", get master node failed, " << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << UBSE_ROLE_MASTER << "=" << masterNode.id;
        roleMap[masterNode.id] = UBSE_ROLE_MASTER;
    }

    Node standbyNode{};
    ret = module->UbseGetStandbyNode(standbyNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "requestId=" << context.requestId << ", get standby node failed, " << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << UBSE_ROLE_SLAVE << "=" << standbyNode.id;
        roleMap[standbyNode.id] = UBSE_ROLE_SLAVE;
    }
    return roleMap;
}

uint32_t UbseNodeApi::UbseQueryClusterInfo(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "requestId=" << context.requestId << ", Cluster IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::vector<UbseNodeInfo> nodeList;
    UbseClusterList(nodeList);
    std::unordered_map<std::string, std::string> roleMap = UbseGetRoleMap(context);
    UbseSerialization ubseSerial;
    ubseSerial << (right_v<std::size_t>(nodeList.size()));
    // hostName(slotId), role, bondingEid;
    for (const auto &node : nodeList) {
        UBSE_LOG_INFO << "requestId=" << context.requestId << ", hostname=" << node.hostName
                      << ", slotId=" << node.slotId << ", clusterState=" << static_cast<uint32_t>(node.clusterState);
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
        ubseSerial << (UbseSmbios::GetInstance().IsClosType() ? "-" : node.bondingEid);
        ubseSerial << (!isOnline || node.guid.empty() ? "-" : node.guid);
    }
    if (!ubseSerial.Check()) {
        UBSE_LOG_ERROR << "requestId=" << context.requestId << ", Serialization of cluster response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseIpcMessage res{ubseSerial.GetBuffer(), static_cast<uint32_t>(ubseSerial.GetLength())};
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "requestId=" << context.requestId << ", Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "requestId=" << context.requestId << ", ClusterInfoQuery response send failed,"
                       << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t SendErrorResponse(uint32_t errorCode, uint64_t requestId, const std::string& errorMsg)
{
    UBSE_LOG_ERROR << errorMsg << ", error: " << FormatRetCode(errorCode);
    auto ubseApiModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubseApiModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server module";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage errorResp{};
    errorResp.buffer = nullptr;
    errorResp.length = 0;
    auto ret = ubseApiModule->SendResponse(errorCode, requestId, errorResp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send error response failed: " << FormatRetCode(ret);
    }
    return ret;
}

static uint32_t ParseNodeIdFromRequestStrict(const UbseIpcMessage &req,
                                             std::string &targetNodeId,
                                             const UbseRequestContext &context)
{
    // 允许空请求（查询本地节点）
    if (req.buffer != nullptr && req.length > 0) {
        try {
            serial::UbseDeSerialization inStream(req.buffer, req.length);
            inStream >> targetNodeId;

            if (!inStream.Check()) {
                UBSE_LOG_ERROR << "Deserialize failed";
                return SendErrorResponse(UBSE_ERROR_DESERIALIZE_FAILED,
                                         context.requestId,
                                         "Deserialize failed");
            }
        } catch (const std::exception& e) {
            UBSE_LOG_ERROR << "Deserialize exception: " << e.what();
            return SendErrorResponse(UBSE_ERROR_DESERIALIZE_FAILED,
                                     context.requestId,
                                     "Deserialize exception");
        }
    } else if (req.buffer == nullptr || req.length == 0) {
        UBSE_LOG_INFO << "Empty request, will query local node";
    } else {
        UBSE_LOG_ERROR << "Invalid request state";
        return SendErrorResponse(UBSE_ERROR_DESERIALIZE_FAILED,
                                 context.requestId,
                                 "Invalid request");
    }
    return UBSE_OK;
}

static uint32_t EnsureTargetNodeIdStrict(std::string &targetNodeId,
                                         const UbseRequestContext &context)
{
    if (!targetNodeId.empty()) {
        if (!UbseSmbios::GetInstance().IsClosType()) {
            UBSE_LOG_INFO << "Querying remote node: " << targetNodeId;
            // 获取组网类型失败时，默认为FULL_MESH组网类型
            return UBSE_OK;
        } else {
            return SendErrorResponse(UBSE_ERR_NOT_SUPPORTED, context.requestId,
                                     " param -n is not supported in current topo");
        }
    }

    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "Election module not loaded";
        return SendErrorResponse(UBSE_ERROR_NULLPTR,
                                 context.requestId,
                                 "Election module not loaded");
    }

    Node currentNode;
    auto ret = module->GetCurrentNode(currentNode);
    if (ret != UBSE_OK || currentNode.id.empty()) {
        UBSE_LOG_ERROR << "Failed to get current node: " << ret;
        return SendErrorResponse(ret,
                                 context.requestId,
                                 "Failed to get current node");
    }

    targetNodeId = currentNode.id;
    UBSE_LOG_INFO << "Querying local node, ID: " << targetNodeId;
    return UBSE_OK;
}

static bool FindTargetNodeStrict(const std::vector<UbseNodeInfo> &nodeList,
                                 const std::string &targetNodeId,
                                 UbseNodeInfo &targetNode)
{
    // 将targetNodeId转换为uint32_t
    uint32_t targetSlotId = 0;
    try {
        targetSlotId = static_cast<uint32_t>(std::stoul(targetNodeId));
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Invalid targetNodeId format: " << targetNodeId
                       << ", error: " << e.what();
        return false;
    }

    for (const auto &node : nodeList) {
        if (node.slotId == targetSlotId) {
            targetNode = node;
            return true;
        }
    }
    return false;
}

static bool IsOnlineStrict(const UbseNodeInfo &node)
{
    return node.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
           node.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING;
}

static void SerializeNodeNotFoundStrict(UbseSerialization &ubseSerial,
                                        const std::string &targetNodeId)
{
    uint32_t errorCode = UBSE_ERR_NODE_NOT_FOUND;
    ubseSerial << errorCode;
    ubseSerial << targetNodeId;
    ubseSerial << "-";
    ubseSerial << "";  // bonding-eid
    ubseSerial << "-";  // guid
}

static void SerializeNodeFoundStrict(UbseSerialization &ubseSerial,
                                     const UbseNodeInfo &targetNode,
                                     const std::unordered_map<std::string, std::string> &roleMap,
                                     std::string &nodeNameOut,
                                     std::string &roleStrOut)
{
    const bool isOnline = IsOnlineStrict(targetNode);

    std::string slotIdStr = std::to_string(targetNode.slotId);
    std::string nodeName = isOnline ? (targetNode.hostName + "(" + slotIdStr + ")") : ("-(" + slotIdStr + ")");
    nodeNameOut = nodeName;

    std::string roleStr = "-";
    if (isOnline) {
        auto it = roleMap.find(slotIdStr);
        roleStr = (it != roleMap.end()) ? it->second : UBSE_ROLE_AGENT;
    }
    roleStrOut = roleStr;

    std::string bondingEid = (UbseSmbios::GetInstance().IsClosType() ? "-" : targetNode.bondingEid);

    // 获取guid，如果离线或guid为空则显示"-"
    std::string guid = (!isOnline || targetNode.guid.empty()) ? "-" : targetNode.guid;

    uint32_t errorCode = UBSE_OK;
    if (!isOnline) {
        if (targetNode.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
            errorCode = UBSE_ERR_NODE_UNREACHABLE;
        } else if (targetNode.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
            errorCode = UBSE_ERR_NODE_NOT_RESPONDING;
        } else {
            errorCode = UBSE_ERR_NODE_NOT_ACTIVE;
        }
    }

    ubseSerial << errorCode;
    ubseSerial << nodeName;
    ubseSerial << roleStr;
    ubseSerial << bondingEid;
    ubseSerial << guid;
}

static uint32_t CheckSerializationStrict(UbseSerialization &ubseSerial,
                                         const UbseRequestContext &context)
{
    if (!ubseSerial.Check()) {
        UBSE_LOG_ERROR << "Serialization failed";
        return SendErrorResponse(UBSE_ERROR_SERIALIZE_FAILED,
                                 context.requestId,
                                 "Serialization failed");
    }
    return UBSE_OK;
}

static uint32_t SendSerializedResponseStrict(UbseSerialization &ubseSerial,
                                             const UbseRequestContext &context)
{
    uint8_t* responseBuffer = ubseSerial.GetBuffer();
    uint32_t responseLength = static_cast<uint32_t>(ubseSerial.GetLength());
    if (responseBuffer == nullptr && responseLength > 0) {
        UBSE_LOG_ERROR << "Buffer is null but length > 0";
        return SendErrorResponse(UBSE_ERROR_SERIALIZE_FAILED,
                                 context.requestId,
                                 "Buffer allocation failed");
    }

    UbseIpcMessage res{responseBuffer, responseLength};
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "API server module not found";
        return UBSE_ERROR_NULLPTR;
    }

    auto ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send response failed: " << FormatRetCode(ret);
    }

    return UBSE_OK;
}

uint32_t UbseNodeApi::UbseQueryNodeInfo(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "UbseQueryNodeInfo START";
    UBSE_LOG_INFO << "Request ID: " << context.requestId;

    // 解析请求
    std::string targetNodeId;
    auto ret = ParseNodeIdFromRequestStrict(req, targetNodeId, context);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 确定 targetNodeId（必要时查 election）
    ret = EnsureTargetNodeIdStrict(targetNodeId, context);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 获取节点列表 + roleMap
    std::vector<UbseNodeInfo> nodeList;
    UbseClusterList(nodeList);
    std::unordered_map<std::string, std::string> roleMap = UbseGetRoleMap(context);

    // 查找目标节点
    UbseNodeInfo targetNode;
    bool found = FindTargetNodeStrict(nodeList, targetNodeId, targetNode);

    // 序列化响应
    UbseSerialization ubseSerial;
    if (!found) {
        SerializeNodeNotFoundStrict(ubseSerial, targetNodeId);
        UBSE_LOG_INFO << "Node not found: " << targetNodeId;
    } else {
        std::string nodeName;
        std::string roleStr;
        SerializeNodeFoundStrict(ubseSerial, targetNode, roleMap, nodeName, roleStr);
        UBSE_LOG_INFO << "Found node: " << nodeName << ", role: " << roleStr;
    }

    ret = CheckSerializationStrict(ubseSerial, context);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 发送响应
    ret = SendSerializedResponseStrict(ubseSerial, context);

    UBSE_LOG_INFO << "UbseQueryNodeInfo END";
    return ret;
}

UbseResult UbseNodeApi::Register()
{
    auto ubse_api_server_module = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubse_api_server_module == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_NODE_GET,
                                                          UbseServerNodeGet, TOPO_PERMISSION);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_NODE_LIST,
                                                      UbseServerNodeList, TOPO_PERMISSION);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE,
                                                      UBSE_NODE_NUMA_MEM_GET, UbseServerNodeNumaMemGet,
                                                      MEM_STAT_PERMISSION);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE,
                                                      UBSE_NODE_CPU_TOPO_LIST, UbseServerCpuTopoList,
                                                      TOPO_PERMISSION);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_CLUSTER_INFO, UbseQueryClusterInfo);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_NODE_CLI_CPU_TOPO_LIST, UbseQueryCpuTopo);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_NODE_CLI_NODE_INFO, UbseQueryNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of Node IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult GetPhysicalLinkOffset(CliPhysicalLink link, UbseSerialization &outStream)
{
    outStream << link.node << link.socketId << link.portId
              << link.interfaceName << link.peerNode << link.peerSocketId << link.peerPortId
              << link.peerInterfaceName << link.linkId;
    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "Ubse serialize cpu link info failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t SerializePhysicalLinks(std::vector<CliPhysicalLink> links, uint8_t *&buffer, size_t &size)
{
    UbseResult ret = UBSE_OK;
    UbseSerialization outStream;
    outStream << (right_v<size_t>(links.size()));
    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "Ubse serialize cpu link info failed";
        return UBSE_ERROR;
    }
    for (size_t i = 0; i < links.size(); i++) {
        UbseSerialization item;
        ret = GetPhysicalLinkOffset(links[i], item);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Ubse serialize cpulinkinfo list[" << i
                           << "] failed at serialization step, " << FormatRetCode(ret);
            return ret;
        }
        outStream << item;
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "Ubse serialize cpulinkinfo list[" << i
                           << "] failed at writing to stream, " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
    }

    size = outStream.GetLength();
    buffer = outStream.GetBuffer(true);
    return UBSE_OK;
}

void ProcessNodeInfo(const std::unordered_map<std::string, UbseNodeInfo>& allNodesInfo,
    std::unordered_map<uint32_t, std::string>& hostMap,
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>>& socketMap)
{
    for (auto &node : allNodesInfo) {
        auto &info = node.second;
        std::ostringstream oss;
        oss << "After mapping nodeId=" << node.first;
        std::for_each(info.cpuInfos.begin(), info.cpuInfos.end(),
                      [&oss](const std::pair<UbseCpuLocation, UbseCpuInfo> &cpuInfo) {
                          oss << ", chipId=" << cpuInfo.second.chipId << " maps to socketId=" << cpuInfo.second.socketId;
                      });
        UBSE_LOG_INFO << oss.str();

        std::stringstream ss;
        ss << info.hostName << "(" << info.slotId << ")";
        if (info.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING) {
            continue;
        }
        hostMap[info.slotId] = ss.str();
        for (auto &cpu : info.cpuInfos) {
            auto cpuInfo = cpu.second;
            try {
                socketMap[cpuInfo.slotId][std::stoi(cpuInfo.chipId)] = std::to_string(cpuInfo.socketId);
            } catch (const std::invalid_argument &e) {
                UBSE_LOG_ERROR << "cpuInfo.chipId is not number: " << cpuInfo.chipId;
                continue;
            } catch (const std::out_of_range &e) {
                UBSE_LOG_ERROR << "cpuInfo.chipId out of int range:" << cpuInfo.chipId;
                continue;
            }
        }
    }
}

bool checkIdSocketMap(const PhysicalLink &dev,
                      std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>> &socketMap,
                      const std::string &flag)
{
    if (flag == "both") {
        return socketMap.count(dev.slotId) && socketMap.at(dev.slotId).count(dev.chipId) &&
               socketMap.count(dev.peerSlotId) && socketMap.at(dev.peerSlotId).count(dev.peerChipId);
    }
    if (flag == "host") {
        return socketMap.count(dev.slotId) && socketMap.at(dev.slotId).count(dev.chipId);
    }
    if (flag == "peer") {
        return socketMap.count(dev.peerSlotId) && socketMap.at(dev.peerSlotId).count(dev.peerChipId);
    }
    UBSE_LOG_ERROR << "Invalid flag value: " << flag << ", expected 'both', 'host' or 'peer'";
    return false;
}

static void PrepareHostSocketMaps(
    std::unordered_map<uint32_t, std::string>& hostMap,
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>>& socketMap)
{
    auto allNodesInfo = UbseNodeController::GetInstance().GetAllNodes();
    if (allNodesInfo.empty()) {
        UBSE_LOG_WARN << "all nodes info is empty";
    }
    ProcessNodeInfo(allNodesInfo, hostMap, socketMap);
}

static std::string MakeUndirectedKey(const PhysicalLink& dev)
{
    uint32_t slot1 = dev.slotId;
    uint32_t chip1 = dev.chipId;
    uint32_t port1 = dev.portId;
    uint32_t slot2 = dev.peerSlotId;
    uint32_t chip2 = dev.peerChipId;
    uint32_t port2 = dev.peerPortId;

    // 确保(slot1,chip1,port1) <= (slot2,chip2,port2)
    bool needSwap = false;
    if (slot1 > slot2) {
        needSwap = true;
    } else if (slot1 == slot2) {
        if (chip1 > chip2) {
            needSwap = true;
        } else if (chip1 == chip2 && port1 > port2) {
            needSwap = true;
        }
    }

    if (needSwap) {
        std::swap(slot1, slot2);
        std::swap(chip1, chip2);
        std::swap(port1, port2);
    }

    return std::to_string(slot1) + "_" + std::to_string(chip1) + "_" +
           std::to_string(port1) + "__" +
           std::to_string(slot2) + "_" + std::to_string(chip2) + "_" +
           std::to_string(port2);
}

static std::tuple<std::string, std::string, std::string, std::string> GetEndInfo(
    uint32_t slot, uint32_t chip, uint32_t port, const std::string& ifName,
    const std::unordered_map<uint32_t, std::string>& hostMap,
    const std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>>& socketMap)
{
    std::string host = "-";
    std::string socket = "-";
    std::string portStr = "-";
    std::string interface = "-";
    auto hostIt = hostMap.find(slot);
    if (hostIt != hostMap.end())
        host = hostIt->second;
    if (host != "-" && !ifName.empty()) {
        interface = ifName;
        if (socketMap.count(slot) && socketMap.at(slot).count(chip)) {
            socket = socketMap.at(slot).at(chip);
        }
        portStr = std::to_string(port);
    }
    return {host, socket, portStr, interface};
}

static void ProcessOneDeviceLink(const PhysicalLink& dev, const std::unordered_map<uint32_t, std::string>& hostMap,
                                 std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>>& socketMap,
                                 std::map<std::string, CliPhysicalLink>& linkMap)
{
    // 生成无向linkKey
    std::string linkKey = MakeUndirectedKey(dev);

    // 使用独立的辅助函数获取两端信息
    auto [host1, socket1, port1, if1] =
        GetEndInfo(dev.slotId, dev.chipId, dev.portId, dev.interfaceName, hostMap, socketMap);

    auto [host2, socket2, port2, if2] =
        GetEndInfo(dev.peerSlotId, dev.peerChipId, dev.peerPortId,
                   dev.peerInterfaceName, hostMap, socketMap);
    // 确定小端在前
    uint32_t smallSlot = dev.slotId;
    uint32_t largeSlot = dev.peerSlotId;
    std::string smallHost = host1;
    std::string smallSocket = socket1;
    std::string smallPort = port1;
    std::string smallIf = if1;
    std::string largeHost = host2;
    std::string largeSocket = socket2;
    std::string largePort = port2;
    std::string largeIf = if2;
    if (dev.slotId > dev.peerSlotId) {
        std::swap(smallSlot, largeSlot);
        std::swap(smallHost, largeHost);
        std::swap(smallSocket, largeSocket);
        std::swap(smallPort, largePort);
        std::swap(smallIf, largeIf);
    }
    // 生成linkId
    std::string linkId = (smallSocket != "-" && largeSocket != "-") ?
                          std::to_string(smallSlot) + "/" + smallSocket + "/" + smallPort + "-" +
                          std::to_string(largeSlot) + "/" + largeSocket + "/" + largePort : "-";
    // 创建或合并记录
    auto it = linkMap.find(linkKey);
    if (it == linkMap.end()) {
        linkMap[linkKey] = CliPhysicalLink(smallHost, smallSocket, smallPort,
                                           smallIf, largeHost, largeSocket,
                                           largePort, largeIf, linkId);
    } else {
        if (it->second.interfaceName == "-" && smallIf != "-")
            it->second.interfaceName = smallIf;
        if (it->second.peerInterfaceName == "-" && largeIf != "-")
            it->second.peerInterfaceName = largeIf;
    }
}

// 从node字符串提取槽位号
static int ExtractSlotFromNode(const std::string& nodeStr)
{
    if (nodeStr == "-") return 9999;
    size_t start = nodeStr.find('(');
    size_t end = nodeStr.find(')', start);
    if (start == std::string::npos || end == std::string::npos) {
        return 9999;
    }

    try {
        return std::stoi(nodeStr.substr(start + 1, end - start - 1));
    } catch (...) {
        return 9999;
    }
}

// 安全转换字符串为int
static int SafeStoi(const std::string& str)
{
    if (str == "-") return 9999;
    try {
        return std::stoi(str);
    } catch (...) {
        return 9999;
    }
}

static bool CompareCliPhysicalLinks(const CliPhysicalLink& a, const CliPhysicalLink& b)
{
    // 提取排序信息
    int aSlot = ExtractSlotFromNode(a.node);
    int bSlot = ExtractSlotFromNode(b.node);
    int aSocket = SafeStoi(a.socketId);
    int bSocket = SafeStoi(b.socketId);
    int aPort = SafeStoi(a.portId);
    int bPort = SafeStoi(b.portId);
    bool aValid = (a.linkId != "-");
    bool bValid = (b.linkId != "-");

    // 首先按node的槽位号排序
    if (aSlot != bSlot) {
        return aSlot < bSlot;
    }
    // node相同，按socket排序
    if (aSocket != bSocket) {
        return aSocket < bSocket;
    }
    // socket相同，按port排序
    if (aPort != bPort) {
        return aPort < bPort;
    }
    // node+socket+port都相同，有效链路排在前面
    if (aValid && !bValid) return true;
    if (!aValid && bValid) return false;
    // 都有效或都无效，按linkId排序
    return a.linkId < b.linkId;
}

void TransToPhysicalLinks(std::vector<PhysicalLink> &devDirConnectVec,
                          std::vector<CliPhysicalLink> &cpuTopoLinks)
{
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>> socketMap{};
    std::unordered_map<uint32_t, std::string> hostMap{};
    PrepareHostSocketMaps(hostMap, socketMap);

    std::map<std::string, CliPhysicalLink> linkMap;
    for (const auto &dev : devDirConnectVec) {
        ProcessOneDeviceLink(dev, hostMap, socketMap, linkMap);
    }

    cpuTopoLinks.clear();
    cpuTopoLinks.reserve(linkMap.size());
    for (const auto& pair : linkMap) {
        cpuTopoLinks.push_back(pair.second);
    }

    // 使用辅助函数进行排序
    std::sort(cpuTopoLinks.begin(), cpuTopoLinks.end(), CompareCliPhysicalLinks);
}

uint32_t GetCpuTopoLink(std::vector<CliPhysicalLink> &cpuTopoLinks)
{
    std::map<std::string, PhysicalLink> devDirConnectMap{};
    std::vector<PhysicalLink> devDirConnectVec{};
    devDirConnectMap = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    if (devDirConnectMap.empty()) {
        UBSE_LOG_WARN << "DirConnectInfo is empty";
        return UBSE_OK;
    }
    auto allNodesInfo = UbseNodeController::GetInstance().GetAllNodes();
    std::unordered_set<uint32_t> activeNodeSlots;
    for (const auto& nodePair : allNodesInfo) {
        const auto& node = nodePair.second;

        if (node.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING) {
            activeNodeSlots.insert(node.slotId);
            UBSE_LOG_INFO << "Active node: slot=" << node.slotId << ", host=" << node.hostName;
        } else {
            UBSE_LOG_INFO << "Inactive node: slot=" << node.slotId << ", host=" << node.hostName
                           << ", state=" << static_cast<int>(node.clusterState);
        }
    }
    devDirConnectVec.reserve(devDirConnectMap.size());
    for (const auto& pair : devDirConnectMap) {
        const auto& link = pair.second;
        bool sourceActive = activeNodeSlots.count(link.slotId) > 0;
        bool peerActive = activeNodeSlots.count(link.peerSlotId) > 0;
        if (sourceActive || peerActive) {
            devDirConnectVec.push_back(link);
        }
    }
    UBSE_LOG_INFO << "Original links: " << devDirConnectMap.size() << ", after filtering: " << devDirConnectVec.size();
    if (devDirConnectVec.empty()) {
        return UBSE_OK;
    }
    std::sort(devDirConnectVec.begin(), devDirConnectVec.end(),
              [](const PhysicalLink &lhs, const PhysicalLink &rhs) {
                  if (lhs.slotId == rhs.slotId && lhs.chipId == rhs.chipId) {
                      return lhs.portId < rhs.portId;
                  } else if (lhs.slotId == rhs.slotId) {
                      return lhs.chipId < rhs.chipId;
                  } else {
                      return lhs.slotId < rhs.slotId;
                  }
              });
    TransToPhysicalLinks(devDirConnectVec, cpuTopoLinks);
    return UBSE_OK;
}

uint32_t UbseNodeApi::UbseQueryCpuTopo(const UbseIpcMessage &request, const UbseRequestContext &context)
{
    (void)request;
    UBSE_LOG_INFO << "enter UbseQueryCpuTopo";
    if (UbseSmbios::GetInstance().IsClosType()) {
        return SendErrorResponse(UBSE_ERR_NOT_SUPPORTED, context.requestId,
                                 "the command is not supported with Current topo ");
    }
    std::vector<CliPhysicalLink> cpuTopoLinks{};
    auto result = GetCpuTopoLink(cpuTopoLinks);
    if (result != UBSE_OK) {
        return SendErrorResponse(result, context.requestId, "get cpu topo link failed");
    }
    UBSE_LOG_INFO << "cpuTopoLinks size:" << cpuTopoLinks.size();
    if (cpuTopoLinks.empty()) {
        UBSE_LOG_INFO << "cpu topology links is empty";
    } else {
        for (const auto &link : cpuTopoLinks) {
            std::ostringstream oss;
            oss << link.node << "," << link.socketId << "," << link.portId << "," << link.interfaceName << ","
                << link.peerNode << "," << link.peerSocketId << "," << link.peerPortId << "," <<  link.peerInterfaceName
                << "," << link.linkId;
            UBSE_LOG_INFO << oss.str();
        }
    }
    UbseIpcMessage resp{};
    resp.buffer = nullptr;
    resp.length = 0;
    size_t respLen = 0;
    auto ret = SerializePhysicalLinks(cpuTopoLinks, resp.buffer, respLen);
    resp.length = static_cast<uint32_t>(respLen);

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Serialize physical links failed. " << FormatRetCode(ret);
        SafeDeleteArray(resp.buffer);
        return SendErrorResponse(ret, context.requestId, "serialize physical links failed");
    }
    auto ubseApiModule = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubseApiModule == nullptr) {
        UBSE_LOG_ERROR << "Get ubse api server module failed";
        SafeDeleteArray(resp.buffer);
        return UBSE_ERROR_NULLPTR;
    }
    ret = ubseApiModule->SendResponse(result, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Send response failed, " << FormatRetCode(ret);
    }
    SafeDeleteArray(resp.buffer);
    return UBSE_OK;
}
} // namespace ubse::nodeController