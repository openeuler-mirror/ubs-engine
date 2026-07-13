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

#include "topo_cases.h"

#include <gtest/gtest.h>

#include "it_assertion.h"
#include "it_cli_invoker.h"
#include "it_console_log.h"
#include "it_lcne_client.h"
#include "it_sdk_client.h"
#include "it_string_util.h"
#include "ubs_engine_topo.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>

namespace ubse::it::tests::topo {
namespace util = ubse::it::infra::util;

void RunQueryNodeTopo001(ubse::it::infra::ItCluster& cluster, const std::string& nodeId)
{
    auto& cli = cluster.GetCliInvoker(nodeId);

    // 正确参数：display topo -t cpu 应返回完整拓扑信息
    std::vector<ubse::it::infra::ItTopoCpuLink> topoLinks;
    auto ret = cli.QueryTopoCpu(topoLinks);
    EXPECT_EQ(ret, UBS_SUCCESS) << "display topo -t cpu should succeed";
    EXPECT_FALSE(topoLinks.empty()) << "display topo -t cpu should return topology entries";
    IT_LOG_INFO << "display topo -t cpu returned " << topoLinks.size() << " links";

    // 统计有效linkId（非空且非"-"）的数量，预期为2
    int validLinkCount = 0;
    for (const auto& link : topoLinks) {
        if (!link.linkId.empty() && link.linkId != "-") {
            validLinkCount++;
        }
    }
    EXPECT_EQ(validLinkCount, 2) << "display topo -t cpu should have 2 valid links, but got " << validLinkCount;
    IT_LOG_INFO << "display topo -t cpu valid link count: " << validLinkCount;

    // 错误参数：display topo -t cpu1 应返回失败
    std::string badOutput = cli.ExecCli("display topo -t cpu1");
    bool isFailed = (badOutput.find("fail") != std::string::npos || badOutput.find("error") != std::string::npos ||
                     badOutput.find("Error") != std::string::npos || badOutput.find("invalid") != std::string::npos ||
                     badOutput.find("Invalid") != std::string::npos || badOutput.empty());
    EXPECT_TRUE(isFailed) << "display topo -t cpu1 should fail, but got: " << badOutput;
    IT_LOG_INFO << "display topo -t cpu1 returned: " << badOutput;
}

namespace {

// Generate normalized link key: slotId-socketId-remote_slotId-remote_socketId
std::string MakeLinkKey(uint32_t slot1, uint32_t socket1, uint32_t slot2, uint32_t socket2)
{
    if (slot1 > slot2 || (slot1 == slot2 && socket1 > socket2)) {
        std::swap(slot1, slot2);
        std::swap(socket1, socket2);
    }
    return std::to_string(slot1) + "-" + std::to_string(socket1) + "-" + std::to_string(slot2) + "-" +
           std::to_string(socket2);
}

// Convert LCNE links to normalized key set
std::set<std::string> LcneLinksToKeys(const std::vector<ubse::it::infra::LcneLinkInfo>& links)
{
    std::set<std::string> keys;
    for (const auto& link : links) {
        keys.insert(MakeLinkKey(link.localSlot, link.localSocketId, link.remoteSlot, link.remoteSocketId));
    }
    return keys;
}

// Convert SDK links to normalized key set
std::set<std::string> SdkLinksToKeys(const ubs_topo_link_t* links, uint32_t count)
{
    std::set<std::string> keys;
    for (uint32_t i = 0; i < count; i++) {
        if (links[i].socket_id == 0xFFFFFFFF || links[i].peer_socket_id == 0xFFFFFFFF) {
            continue;
        }
        keys.insert(MakeLinkKey(links[i].slot_id, links[i].socket_id, links[i].peer_slot_id, links[i].peer_socket_id));
    }
    return keys;
}

// Collect socketId set per slot from LCNE nodes (ubpu -> socketId mapping)
std::map<uint32_t, std::set<uint32_t>> LcneNodesToSlotSocketMap(const std::vector<ubse::it::infra::LcneLinkInfo>& links)
{
    // Build ubpu -> socketId mapping from links
    std::map<uint32_t, uint32_t> ubpuToSocketId;
    for (const auto& link : links) {
        ubpuToSocketId[link.localUbpu] = link.localSocketId;
        ubpuToSocketId[link.remoteUbpu] = link.remoteSocketId;
    }
    // Build slot -> socketId set
    std::map<uint32_t, std::set<uint32_t>> slotSocketMap;
    for (const auto& link : links) {
        slotSocketMap[link.localSlot].insert(link.localSocketId);
        slotSocketMap[link.remoteSlot].insert(link.remoteSocketId);
    }
    return slotSocketMap;
}

// Collect socketId set per slot from SDK node list
std::map<uint32_t, std::set<uint32_t>> SdkNodesToSlotSocketMap(const ubs_topo_node_t* nodes, uint32_t count)
{
    std::map<uint32_t, std::set<uint32_t>> slotSocketMap;
    for (uint32_t i = 0; i < count; i++) {
        for (int j = 0; j < UBS_TOPO_SOCKET_NUM; j++) {
            uint32_t sid = nodes[i].socket_id[j];
            if (sid != 0xFFFFFFFF) {
                slotSocketMap[nodes[i].slot_id].insert(sid);
            }
        }
    }
    return slotSocketMap;
}

} // namespace

// LCNE vs SDK TopoNodeList：对比每个slot的socketId集合
void RunLcneVsSdkTopoNodeList001(ubse::it::infra::ItCluster& cluster)
{
    // 从LCNE获取拓扑连接（含socketId映射）
    std::vector<ubse::it::infra::LcneLinkInfo> lcneLinks;
    auto ret = cluster.GetAllLcneTopologyLinks(lcneLinks);
    EXPECT_IT_OK(ret);
    EXPECT_GT(lcneLinks.size(), 0);

    // 从SDK获取节点列表
    auto& sdkClient = cluster.GetSdkClient("1");
    ubs_topo_node_t* sdkNodes = nullptr;
    uint32_t sdkNodeCnt = 0;
    EXPECT_IT_OK(sdkClient.TopoNodeList(&sdkNodes, &sdkNodeCnt));
    EXPECT_GT(sdkNodeCnt, 0);
    EXPECT_NE(sdkNodes, nullptr);
    auto sdkNodeGuard = std::unique_ptr<ubs_topo_node_t, decltype(&free)>(sdkNodes, &free);

    // 对比每个slot的socketId集合
    auto lcneSlotSocket = LcneNodesToSlotSocketMap(lcneLinks);
    auto sdkSlotSocket = SdkNodesToSlotSocketMap(sdkNodes, sdkNodeCnt);

    IT_LOG_INFO << "=== LCNE vs SDK TopoNodeList Comparison ===";
    bool nodeMatch = true;
    for (const auto& [slot, socketIds] : lcneSlotSocket) {
        auto it = sdkSlotSocket.find(slot);
        if (it == sdkSlotSocket.end()) {
            IT_LOG_ERROR << "Slot " << slot << " found in LCNE but not in SDK";
            nodeMatch = false;
            continue;
        }
        if (socketIds != it->second) {
            IT_LOG_ERROR << "Slot " << slot << " socketId mismatch: LCNE=" << socketIds.size()
                         << " sockets, SDK=" << it->second.size() << " sockets";
            nodeMatch = false;
        } else {
            IT_LOG_INFO << "Slot " << slot << " socketId match: " << socketIds.size() << " sockets";
        }
    }
    EXPECT_TRUE(nodeMatch) << "LCNE and SDK node socketId should match";
}

// LCNE vs SDK TopoNodeLocalGet：本地节点socketId与LCNE对应slot一致
void RunLcneVsSdkTopoNodeLocalGet001(ubse::it::infra::ItCluster& cluster)
{
    // 从LCNE获取拓扑连接（含socketId映射）
    std::vector<ubse::it::infra::LcneLinkInfo> lcneLinks;
    auto ret = cluster.GetAllLcneTopologyLinks(lcneLinks);
    EXPECT_IT_OK(ret);
    EXPECT_GT(lcneLinks.size(), 0);

    // 从SDK获取本地节点
    auto& sdkClient = cluster.GetSdkClient("1");
    ubs_topo_node_t localNode;
    EXPECT_IT_OK(sdkClient.TopoNodeLocalGet(&localNode));

    // 对比本地节点socketId与LCNE对应slot
    auto lcneSlotSocket = LcneNodesToSlotSocketMap(lcneLinks);
    auto lcneIt = lcneSlotSocket.find(localNode.slot_id);
    ASSERT_NE(lcneIt, lcneSlotSocket.end()) << "Local slot " << localNode.slot_id << " not found in LCNE data";

    std::set<uint32_t> localSocketIds;
    for (int j = 0; j < UBS_TOPO_SOCKET_NUM; j++) {
        uint32_t sid = localNode.socket_id[j];
        if (sid != 0xFFFFFFFF) {
            localSocketIds.insert(sid);
        }
    }

    IT_LOG_INFO << "=== LCNE vs SDK TopoNodeLocalGet Comparison ===";
    EXPECT_EQ(localSocketIds, lcneIt->second)
        << "Local node slot " << localNode.slot_id << " socketId mismatch with LCNE";
    IT_LOG_INFO << "Local node slot " << localNode.slot_id << " socketId matches LCNE";
}

// LCNE vs SDK TopoLinkList：对比链路连接
void RunLcneVsSdkTopoLinkList001(ubse::it::infra::ItCluster& cluster)
{
    // 从LCNE获取拓扑连接
    std::vector<ubse::it::infra::LcneLinkInfo> lcneLinks;
    auto ret = cluster.GetAllLcneTopologyLinks(lcneLinks);
    EXPECT_IT_OK(ret);
    EXPECT_GT(lcneLinks.size(), 0);

    // 从SDK获取链路列表
    auto& sdkClient = cluster.GetSdkClient("1");
    ubs_topo_link_t* sdkLinks = nullptr;
    uint32_t sdkLinkCnt = 0;
    EXPECT_IT_OK(sdkClient.TopoLinkList(&sdkLinks, &sdkLinkCnt));
    EXPECT_GT(sdkLinkCnt, 0);
    EXPECT_NE(sdkLinks, nullptr);
    auto sdkLinkGuard = std::unique_ptr<ubs_topo_link_t, decltype(&free)>(sdkLinks, &free);

    // 对比链路key
    auto lcneKeys = LcneLinksToKeys(lcneLinks);
    auto sdkKeys = SdkLinksToKeys(sdkLinks, sdkLinkCnt);

    IT_LOG_INFO << "=== LCNE vs SDK TopoLinkList Comparison ===";
    IT_LOG_INFO << "LCNE unique links: " << lcneKeys.size();
    IT_LOG_INFO << "SDK unique links: " << sdkKeys.size();

    std::set<std::string> onlyInLcne;
    std::set<std::string> onlyInSdk;
    std::set_difference(lcneKeys.begin(), lcneKeys.end(), sdkKeys.begin(), sdkKeys.end(),
                        std::inserter(onlyInLcne, onlyInLcne.end()));
    std::set_difference(sdkKeys.begin(), sdkKeys.end(), lcneKeys.begin(), lcneKeys.end(),
                        std::inserter(onlyInSdk, onlyInSdk.end()));

    for (const auto& key : onlyInLcne) {
        IT_LOG_ERROR << "Only in LCNE: " << key;
    }
    for (const auto& key : onlyInSdk) {
        IT_LOG_ERROR << "Only in SDK: " << key;
    }

    EXPECT_TRUE(onlyInLcne.empty() && onlyInSdk.empty())
        << "LCNE and SDK topo links should match. LCNE: " << lcneKeys.size() << ", SDK: " << sdkKeys.size();

    if (onlyInLcne.empty() && onlyInSdk.empty()) {
        IT_LOG_INFO << "LCNE and SDK topo links match! Total: " << lcneKeys.size() << " unique links";
    }
}

// LCNE vs CLI display topo -t cpu：对比链路连接和接口名
void RunLcneVsCliTopoCpu001(ubse::it::infra::ItCluster& cluster)
{
    // 1. 从LCNE获取拓扑连接
    std::vector<ubse::it::infra::LcneLinkInfo> lcneLinks;
    auto ret = cluster.GetAllLcneTopologyLinks(lcneLinks);
    EXPECT_IT_OK(ret);
    EXPECT_GT(lcneLinks.size(), 0);

    // 2. 从CLI获取display topo -t cpu
    auto& cli = cluster.GetCliInvoker("1");
    std::vector<ubse::it::infra::ItTopoCpuLink> cliLinks;
    auto sdkRet = cli.QueryTopoCpu(cliLinks);
    EXPECT_IT_OK(sdkRet);
    EXPECT_GT(cliLinks.size(), 0);

    // 3. 生成对比key: node-socket-port -> peer-node-peer-socket-peer-port
    // LCNE: 归一化双向链路
    std::set<std::string> lcneKeys;
    for (const auto& link : lcneLinks) {
        std::string local = std::to_string(link.localSlot) + "-" + std::to_string(link.localSocketId) + "-" +
                            std::to_string(link.localPort);
        std::string remote = std::to_string(link.remoteSlot) + "-" + std::to_string(link.remoteSocketId) + "-" +
                             std::to_string(link.remotePort);
        lcneKeys.insert(local + " -> " + remote);
    }

    // CLI: node/socket/port 对应 slot/socketId/port，node格式如 "node-01(3)" 需提取ID
    std::set<std::string> cliKeys;
    for (const auto& link : cliLinks) {
        if (link.linkId.empty() || link.linkId == "-") {
            continue;
        }
        std::string local = util::ExtractNodeId(link.node) + "-" + link.socket + "-" + link.port;
        std::string remote = util::ExtractNodeId(link.peerNode) + "-" + link.peerSocket + "-" + link.peerPort;
        cliKeys.insert(local + " -> " + remote);
    }

    // 4. 对比
    IT_LOG_INFO << "=== LCNE vs CLI display topo -t cpu Comparison ===";
    IT_LOG_INFO << "LCNE links: " << lcneKeys.size();
    IT_LOG_INFO << "CLI links: " << cliKeys.size();

    std::set<std::string> onlyInLcne;
    std::set<std::string> onlyInCli;
    std::set_difference(lcneKeys.begin(), lcneKeys.end(), cliKeys.begin(), cliKeys.end(),
                        std::inserter(onlyInLcne, onlyInLcne.end()));
    std::set_difference(cliKeys.begin(), cliKeys.end(), lcneKeys.begin(), lcneKeys.end(),
                        std::inserter(onlyInCli, onlyInCli.end()));

    for (const auto& key : onlyInLcne) {
        IT_LOG_ERROR << "Only in LCNE: " << key;
    }
    for (const auto& key : onlyInCli) {
        IT_LOG_ERROR << "Only in CLI: " << key;
    }

    EXPECT_TRUE(onlyInLcne.empty() && onlyInCli.empty())
        << "LCNE and CLI topo links should match. LCNE: " << lcneKeys.size() << ", CLI: " << cliKeys.size();

    if (onlyInLcne.empty() && onlyInCli.empty()) {
        IT_LOG_INFO << "LCNE and CLI topo links match! Total: " << lcneKeys.size() << " links";
    }
}

// LCNE logic-entities vs CLI display cluster：按节点对比GUID
void RunLcneVsCliClusterGuid001(ubse::it::infra::ItCluster& cluster)
{
    // 1. 从CLI获取display cluster，构建 nodeId -> guid 映射
    auto& cli = cluster.GetCliInvoker("1");
    std::vector<ubse::it::infra::ItNodeInfo> nodeInfos;
    auto ret = cli.QueryClusterInfo(nodeInfos);
    EXPECT_IT_OK(ret);
    EXPECT_GT(nodeInfos.size(), 0);

    std::map<std::string, std::string> cliGuidMap; // nodeId -> guid
    for (const auto& info : nodeInfos) {
        std::string nodeId = util::ExtractNodeId(info.node);
        cliGuidMap[nodeId] = info.guid;
    }

    // 2. 逐节点从LCNE获取logic-entities，对比guid
    IT_LOG_INFO << "=== LCNE logic-entities vs CLI display cluster GUID Comparison ===";
    int mismatchCount = 0;
    for (const auto& nodeId : cluster.GetNodeIds()) {
        auto& lcneClient = cluster.GetLcneClient(nodeId);
        std::vector<ubse::it::infra::LcneLogicEntityInfo> entities;
        auto lcneRet = lcneClient.GetLogicEntities(entities);
        EXPECT_IT_OK(lcneRet);

        // 找到type=host的entity，取其guid
        std::string lcneGuid;
        for (const auto& entity : entities) {
            if (entity.type == "host") {
                lcneGuid = entity.guid;
                break;
            }
        }

        auto it = cliGuidMap.find(nodeId);
        if (it == cliGuidMap.end()) {
            IT_LOG_ERROR << "Node " << nodeId << " not found in CLI display cluster";
            mismatchCount++;
            continue;
        }

        const std::string& cliGuid = it->second;
        IT_LOG_INFO << "Node " << nodeId << ": LCNE guid=" << lcneGuid << ", CLI guid=" << cliGuid;

        if (cliGuid.empty() || cliGuid == "-") {
            IT_LOG_INFO << "Node " << nodeId << " CLI guid is empty or '-', skip comparison";
            continue;
        }

        if (lcneGuid != cliGuid) {
            IT_LOG_ERROR << "Node " << nodeId << " GUID mismatch: LCNE=" << lcneGuid << ", CLI=" << cliGuid;
            mismatchCount++;
        }
    }

    EXPECT_EQ(mismatchCount, 0)
        << "LCNE logic-entities and CLI display cluster GUID should match per node. Mismatches: " << mismatchCount;

    if (mismatchCount == 0) {
        IT_LOG_INFO << "All nodes GUID match between LCNE and CLI!";
    }
}

} // namespace ubse::it::tests::topo
