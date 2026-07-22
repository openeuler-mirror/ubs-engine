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

// Collect socketId set per slot from LCNE nodes
std::map<uint32_t, std::set<uint32_t>> LcneNodesToSlotSocketMap(const std::vector<ubse::it::infra::LcneLinkInfo>& links)
{
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

// 获取 LCNE 拓扑数据, 失败则终止
std::vector<ubse::it::infra::LcneLinkInfo> GetLcneLinks(ubse::it::infra::ItCluster& cluster)
{
    std::vector<ubse::it::infra::LcneLinkInfo> lcneLinks;
    auto ret = cluster.GetAllLcneTopologyLinks(lcneLinks);
    EXPECT_IT_OK(ret);
    EXPECT_GT(lcneLinks.size(), 0);
    return lcneLinks;
}

// 校验节点 ips[] 中至少有一个合法 IP（IPv4: s_addr!=0, IPv6: 非全零）
bool HasValidIp(const ubs_topo_node_t& node)
{
    for (int j = 0; j < UBS_TOPO_IPADDR_NUM; j++) {
        const auto& ip = node.ips[j];
        if (ip.ipv4.s_addr != 0) {
            return true;
        }
        for (size_t k = 0; k < sizeof(ip.ipv6); k++) {
            if (reinterpret_cast<const uint8_t*>(&ip.ipv6)[k] != 0) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

// ==================== P0 用例 ====================

// P0-NodeList-Ok-01: 单节点查询 + LCNE 比对
void RunP0NodeListOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    ubs_topo_node_t* nodes = nullptr;
    uint32_t cnt = 0;

    int32_t ret = sdk.TopoNodeList(&nodes, &cnt);
    ASSERT_EQ(ret, UBS_SUCCESS) << "ubs_topo_node_list should succeed";
    ASSERT_NE(nodes, nullptr);
    auto guard = std::unique_ptr<ubs_topo_node_t, decltype(&free)>(nodes, &free);

    EXPECT_EQ(cnt, 1) << "single node cluster should have node_cnt==1";
    EXPECT_GT(nodes[0].slot_id, 0) << "slot_id should be > 0";
    EXPECT_GT(strlen(nodes[0].host_name), 0) << "host_name should not be empty";

    // IP 格式校验
    for (uint32_t i = 0; i < cnt; i++) {
        EXPECT_TRUE(HasValidIp(nodes[i]))
            << "Slot " << nodes[i].slot_id << " should have at least one valid IP address";
    }

    // LCNE 比对: SDK 每个节点的 socketId 应与 LCNE 一致
    auto lcneLinks = GetLcneLinks(cluster);
    auto lcneMap = LcneNodesToSlotSocketMap(lcneLinks);
    auto sdkMap = SdkNodesToSlotSocketMap(nodes, cnt);
    for (const auto& [slot, sdkSocketIds] : sdkMap) {
        auto lcneIt = lcneMap.find(slot);
        if (lcneIt == lcneMap.end()) {
            ADD_FAILURE() << "Slot " << slot << " not found in LCNE topology";
            continue;
        }
        EXPECT_EQ(sdkSocketIds, lcneIt->second) << "Slot " << slot << " socketId mismatch: SDK=" << sdkSocketIds.size()
                                                << ", LCNE=" << lcneIt->second.size();
    }

    IT_LOG_INFO << "P0-NodeList-Ok-01 passed: cnt=" << cnt << ", slot_id=" << nodes[0].slot_id;
}

// P0-NodeList-NullPtr-01: 空指针, 返回 NULL_POINTER
void RunP0NodeListNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    ubs_topo_node_t* nodes = nullptr;
    uint32_t cnt = 0;

    EXPECT_EQ(sdk.TopoNodeList(nullptr, &cnt), UBS_ERR_NULL_POINTER) << "node_list=null should return NULL_POINTER";
    EXPECT_EQ(sdk.TopoNodeList(&nodes, nullptr), UBS_ERR_NULL_POINTER) << "node_cnt=null should return NULL_POINTER";
    EXPECT_EQ(sdk.TopoNodeList(nullptr, nullptr), UBS_ERR_NULL_POINTER) << "both null should return NULL_POINTER";

    IT_LOG_INFO << "P0-NodeList-NullPtr-01 passed";
}

// P0-NodeList-Ok-02: 多节点查询 + LCNE 比对
void RunP0NodeListOk02(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    ubs_topo_node_t* nodes = nullptr;
    uint32_t cnt = 0;

    int32_t ret = sdk.TopoNodeList(&nodes, &cnt);
    ASSERT_EQ(ret, UBS_SUCCESS) << "ubs_topo_node_list should succeed";
    ASSERT_NE(nodes, nullptr);
    auto guard = std::unique_ptr<ubs_topo_node_t, decltype(&free)>(nodes, &free);

    EXPECT_EQ(cnt, cluster.GetNodeIds().size())
        << "node_cnt=" << cnt << " should match cluster size=" << cluster.GetNodeIds().size();

    // slot_id 互不相同
    std::set<uint32_t> slotIds;
    for (uint32_t i = 0; i < cnt; i++) {
        EXPECT_GT(nodes[i].slot_id, 0) << "node[" << i << "] slot_id should be > 0";
        EXPECT_TRUE(slotIds.insert(nodes[i].slot_id).second)
            << "node[" << i << "] slot_id=" << nodes[i].slot_id << " is duplicate";
    }

    // IP 格式校验
    for (uint32_t i = 0; i < cnt; i++) {
        EXPECT_TRUE(HasValidIp(nodes[i]))
            << "Slot " << nodes[i].slot_id << " should have at least one valid IP address";
    }

    // LCNE 比对: SDK 每个节点的 socketId 应与 LCNE 一致
    auto lcneLinks = GetLcneLinks(cluster);
    auto lcneMap = LcneNodesToSlotSocketMap(lcneLinks);
    auto sdkMap = SdkNodesToSlotSocketMap(nodes, cnt);
    for (const auto& [slot, sdkSocketIds] : sdkMap) {
        auto lcneIt = lcneMap.find(slot);
        if (lcneIt == lcneMap.end()) {
            ADD_FAILURE() << "Slot " << slot << " not found in LCNE topology";
            continue;
        }
        EXPECT_EQ(sdkSocketIds, lcneIt->second) << "Slot " << slot << " socketId mismatch: SDK=" << sdkSocketIds.size()
                                                << ", LCNE=" << lcneIt->second.size();
    }

    IT_LOG_INFO << "P0-NodeList-Ok-02 passed: cnt=" << cnt << ", unique slots=" << slotIds.size();
}

// P0-LocalGet-Ok-01: 查询本节点 + LCNE 比对
void RunP0LocalGetOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    ubs_topo_node_t node{};

    int32_t ret = sdk.TopoNodeLocalGet(&node);
    ASSERT_EQ(ret, UBS_SUCCESS) << "ubs_topo_node_local_get should succeed";

    EXPECT_GT(node.slot_id, 0) << "slot_id should be > 0";
    EXPECT_GT(strlen(node.host_name), 0) << "host_name should not be empty";

    // IP 格式校验
    EXPECT_TRUE(HasValidIp(node)) << "Local node slot " << node.slot_id << " should have at least one valid IP address";

    // LCNE 比对: 本节点 socketId 应与 LCNE 对应 slot 一致
    auto lcneLinks = GetLcneLinks(cluster);
    auto lcneMap = LcneNodesToSlotSocketMap(lcneLinks);
    auto lcneIt = lcneMap.find(node.slot_id);
    if (lcneIt == lcneMap.end()) {
        ADD_FAILURE() << "Local slot " << node.slot_id << " not found in LCNE data";
        return;
    }

    std::set<uint32_t> localSocketIds;
    for (int j = 0; j < UBS_TOPO_SOCKET_NUM; j++) {
        if (node.socket_id[j] != 0xFFFFFFFF) {
            localSocketIds.insert(node.socket_id[j]);
        }
    }
    EXPECT_EQ(localSocketIds, lcneIt->second) << "Local node slot " << node.slot_id << " socketId mismatch with LCNE";

    IT_LOG_INFO << "P0-LocalGet-Ok-01 passed: slot_id=" << node.slot_id;
}

// P0-LocalGet-NullPtr-01: 空指针, 返回 NULL_POINTER
void RunP0LocalGetNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    EXPECT_EQ(sdk.TopoNodeLocalGet(nullptr), UBS_ERR_NULL_POINTER) << "node=null should return NULL_POINTER";

    IT_LOG_INFO << "P0-LocalGet-NullPtr-01 passed";
}

// P0-LinkList-Ok-01: 双节点链路查询 + LCNE 比对
void RunP0LinkListOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    ubs_topo_link_t* links = nullptr;
    uint32_t cnt = 0;

    int32_t ret = sdk.TopoLinkList(&links, &cnt);
    ASSERT_EQ(ret, UBS_SUCCESS) << "ubs_topo_link_list should succeed";
    ASSERT_NE(links, nullptr);
    auto guard = std::unique_ptr<ubs_topo_link_t, decltype(&free)>(links, &free);

    EXPECT_GT(cnt, 0) << "cpu_link_cnt should be > 0";

    for (uint32_t i = 0; i < cnt; i++) {
        EXPECT_GT(links[i].slot_id, 0) << "link[" << i << "] slot_id should be > 0";
        EXPECT_GT(links[i].peer_slot_id, 0) << "link[" << i << "] peer_slot_id should be > 0";
        EXPECT_NE(links[i].slot_id, links[i].peer_slot_id) << "link[" << i << "] slot_id != peer_slot_id";
    }

    // LCNE 比对: SDK 有效链路(peer_socket_id 非 0xFFFFFFFF)应在 LCNE 中存在
    // LCNE 数据比 SDK 更完整(SDK 部分链路 peer_socket_id=0xFFFFFFFF 表示无效)
    auto lcneLinks = GetLcneLinks(cluster);
    auto lcneKeys = LcneLinksToKeys(lcneLinks);
    auto sdkValidKeys = SdkLinksToKeys(links, cnt);

    // SDK 有效链路都应在 LCNE 中
    bool allInLcne = true;
    for (const auto& key : sdkValidKeys) {
        if (lcneKeys.find(key) == lcneKeys.end()) {
            IT_LOG_ERROR << "SDK valid link not in LCNE: " << key;
            allInLcne = false;
        }
    }
    EXPECT_TRUE(allInLcne) << "All SDK valid links should exist in LCNE";

    IT_LOG_INFO << "P0-LinkList-Ok-01 passed: cnt=" << cnt;
}

// P0-LinkList-Fld-01: 字段校验, socket_id/port_id 等非空
void RunP0LinkListFld01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    ubs_topo_link_t* links = nullptr;
    uint32_t cnt = 0;

    ASSERT_EQ(sdk.TopoLinkList(&links, &cnt), UBS_SUCCESS);
    ASSERT_NE(links, nullptr);
    auto guard = std::unique_ptr<ubs_topo_link_t, decltype(&free)>(links, &free);

    for (uint32_t i = 0; i < cnt; i++) {
        // socket_id/peer_socket_id: 0xFFFFFFFF 表示无效值(SDK 规约), 不是错误
        // 只验非无效值时 > 0
        if (links[i].socket_id != 0xFFFFFFFF) {
            EXPECT_GT(links[i].socket_id, 0) << "link[" << i << "] valid socket_id should be > 0";
        }
        if (links[i].peer_socket_id != 0xFFFFFFFF) {
            EXPECT_GT(links[i].peer_socket_id, 0) << "link[" << i << "] valid peer_socket_id should be > 0";
        }
        EXPECT_GT(links[i].port_id, 0) << "link[" << i << "] port_id should be > 0";
        EXPECT_GT(links[i].peer_port_id, 0) << "link[" << i << "] peer_port_id should be > 0";
    }

    IT_LOG_INFO << "P0-LinkList-Fld-01 passed: validated " << cnt << " links";
}

// P0-LinkList-NullPtr-01: 空指针, 返回 NULL_POINTER
void RunP0LinkListNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    ubs_topo_link_t* links = nullptr;
    uint32_t cnt = 0;

    EXPECT_EQ(sdk.TopoLinkList(nullptr, &cnt), UBS_ERR_NULL_POINTER) << "cpu_links=null should return NULL_POINTER";
    EXPECT_EQ(sdk.TopoLinkList(&links, nullptr), UBS_ERR_NULL_POINTER)
        << "cpu_link_cnt=null should return NULL_POINTER";
    EXPECT_EQ(sdk.TopoLinkList(nullptr, nullptr), UBS_ERR_NULL_POINTER) << "both null should return NULL_POINTER";

    IT_LOG_INFO << "P0-LinkList-NullPtr-01 passed";
}

// ==================== CLI 用例 ====================

void RunP0CliTopoCpuOk01(ubse::it::infra::ItCluster& cluster, const std::string& nodeId)
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

// LCNE vs CLI display topo -t cpu：对比链路连接和接口名
void RunP1CliTopoCpuCrossConsist01(ubse::it::infra::ItCluster& cluster)
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

    // 3. 生成对比key
    std::set<std::string> lcneKeys;
    for (const auto& link : lcneLinks) {
        std::string local = std::to_string(link.localSlot) + "-" + std::to_string(link.localSocketId) + "-" +
                            std::to_string(link.localPort);
        std::string remote = std::to_string(link.remoteSlot) + "-" + std::to_string(link.remoteSocketId) + "-" +
                             std::to_string(link.remotePort);
        lcneKeys.insert(local + " -> " + remote);
    }

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
void RunP0CliClusterOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& cli = cluster.GetCliInvoker("1");
    std::vector<ubse::it::infra::ItNodeInfo> nodeInfos;
    auto ret = cli.QueryClusterInfo(nodeInfos);
    EXPECT_IT_OK(ret);
    EXPECT_GT(nodeInfos.size(), 0);

    std::map<std::string, std::string> cliGuidMap;
    for (const auto& info : nodeInfos) {
        std::string nodeId = util::ExtractNodeId(info.node);
        cliGuidMap[nodeId] = info.guid;
    }

    IT_LOG_INFO << "=== LCNE logic-entities vs CLI display cluster GUID Comparison ===";
    int mismatchCount = 0;
    for (const auto& nodeId : cluster.GetNodeIds()) {
        auto& lcneClient = cluster.GetLcneClient(nodeId);
        std::vector<ubse::it::infra::LcneLogicEntityInfo> entities;
        auto lcneRet = lcneClient.GetLogicEntities(entities);
        EXPECT_IT_OK(lcneRet);

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

void RunP0CliNodeOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& cli = cluster.GetCliInvoker("1");
    ubse::it::infra::ItNodeInfo nodeInfo;
    auto ret = cli.QueryNodeInfo(nodeInfo);
    ASSERT_IT_OK(ret);
    EXPECT_FALSE(nodeInfo.node.empty());
    EXPECT_FALSE(nodeInfo.role.empty());
    EXPECT_FALSE(nodeInfo.guid.empty());
    IT_LOG_INFO << "P0-CliNode-Ok-01 passed: node=" << nodeInfo.node << ", role=" << nodeInfo.role
                << ", guid=" << nodeInfo.guid;
}

void RunP0CliNodeBadParam01(ubse::it::infra::ItCluster& cluster)
{
    auto& cli = cluster.GetCliInvoker("1");
    ubse::it::infra::ItNodeInfo nodeInfo;
    auto ret = cli.QueryNodeInfo(nodeInfo, "999");
    EXPECT_NE(ret, UBS_SUCCESS) << "display node -n 999 should fail";
    IT_LOG_INFO << "P0-CliNode-BadParam-01 passed";
}

} // namespace ubse::it::tests::topo
