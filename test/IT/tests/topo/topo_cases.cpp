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

#include <unistd.h>
#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

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

// 统计子串 text 中子串 sub 出现的次数
size_t CountOccurrences(const std::string& text, const std::string& sub)
{
    size_t count = 0;
    size_t pos = 0;
    while ((pos = text.find(sub, pos)) != std::string::npos) {
        ++count;
        pos += sub.length();
    }
    return count;
}

// 定位 ubsectl 的 Bash 补全脚本: <build>/scripts/cli_commands.sh
// 基于测试二进制 /proc/self/exe 推导构建目录（二进制位于 <build>/bin/ 下）
std::string LocateCliCompletionScript()
{
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len <= 0) {
        return "";
    }
    exePath[len] = '\0';
    std::string exe(exePath); // <build>/bin/<binary>
    size_t binSlash = exe.find_last_of('/');
    if (binSlash == std::string::npos) {
        return "";
    }
    std::string buildDir = exe.substr(0, binSlash); // <build>/bin
    size_t buildSlash = buildDir.find_last_of('/');
    if (buildSlash == std::string::npos) {
        return "";
    }
    buildDir = buildDir.substr(0, buildSlash); // <build>
    return buildDir + "/scripts/cli_commands.sh";
}

// 执行一段 bash 脚本并返回 stdout+stderr（不经过 ItCliInvoker，避免 CLI 超时/env 前缀干扰）
std::string RunBashScript(const std::string& script)
{
    std::string cmd = "bash -c '" + script + "' 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        IT_LOG_ERROR << "popen failed for bash script: " << cmd;
        return "";
    }
    std::ostringstream oss;
    std::array<char, 4096> buffer{};
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        oss << buffer.data();
    }
    pclose(pipe);
    return oss.str();
}

// 从输出中提取 "TAG=" 到行尾的内容
std::string ExtractTagValue(const std::string& output, const std::string& tag)
{
    size_t pos = output.find(tag + "=");
    if (pos == std::string::npos) {
        return "";
    }
    pos += tag.size() + 1;
    size_t eol = output.find('\n', pos);
    return output.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
}

// 判断空格分隔的候选词文本是否包含指定单词
bool ContainsWord(const std::string& text, const std::string& word)
{
    std::istringstream iss(text);
    std::string token;
    while (iss >> token) {
        if (token == word) {
            return true;
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

void RunP0CliTopoCpuOk01(ubse::it::infra::ItCluster& cluster)
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

    std::set<std::string> slotIds;
    std::map<std::string, std::string> cliGuidMap;
    for (const auto& info : nodeInfos) {
        std::string nodeId = util::ExtractNodeId(info.node);
        slotIds.insert(nodeId);
        cliGuidMap[nodeId] = info.guid;
    }

    // 各节点 slotId 不一致
    EXPECT_EQ(slotIds.size(), nodeInfos.size())
        << "All nodes should have unique slotId, but got " << slotIds.size() << " unique out of " << nodeInfos.size();

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

    // node 非空 (hostname(slotId) 格式)
    EXPECT_FALSE(nodeInfo.node.empty()) << "node field should not be empty";
    EXPECT_NE(nodeInfo.node.find('('), std::string::npos) << "node should contain '(' for slotId";
    EXPECT_NE(nodeInfo.node.find(')'), std::string::npos) << "node should contain ')' for slotId";

    // slot_id 与集群一致
    std::string cliSlotId = util::ExtractNodeId(nodeInfo.node);
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;
    EXPECT_EQ(cliSlotId, std::to_string(localSlotId))
        << "CLI slotId should match cluster spec, CLI=" << cliSlotId << ", cluster=" << localSlotId;

    // role 有效值
    EXPECT_FALSE(nodeInfo.role.empty()) << "role field should not be empty";
    EXPECT_TRUE(nodeInfo.role == "master" || nodeInfo.role == "standby" || nodeInfo.role == "agent")
        << "role should be master/standby/agent, actual: " << nodeInfo.role;

    // bondingEid 非空
    EXPECT_FALSE(nodeInfo.bondingEid.empty()) << "bondingEid field should not be empty";

    // guid 非空
    EXPECT_FALSE(nodeInfo.guid.empty()) << "guid field should not be empty";

    IT_LOG_INFO << "P0-CliNode-Ok-01 passed: node=" << nodeInfo.node << ", role=" << nodeInfo.role
                << ", bondingEid=" << nodeInfo.bondingEid << ", guid=" << nodeInfo.guid;
}

void RunP0CliNodeOk02(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    ASSERT_GE(nodeIds.size(), 2u) << "P0-CliNode-Ok-02 requires at least 2 nodes";

    // 查询节点2
    std::string targetNodeId = "2";
    auto& cli = cluster.GetCliInvoker("1");
    ubse::it::infra::ItNodeInfo nodeInfo;
    auto ret = cli.QueryNodeInfo(nodeInfo, targetNodeId);
    ASSERT_IT_OK(ret);

    // node 包含目标节点ID
    EXPECT_NE(nodeInfo.node.find('(' + targetNodeId + ')'), std::string::npos)
        << "node should contain target nodeId " << targetNodeId;

    // slot_id 与集群一致
    std::string cliSlotId = util::ExtractNodeId(nodeInfo.node);
    uint32_t targetSlotId = cluster.GetNode(targetNodeId).GetSpec().slotId;
    EXPECT_EQ(cliSlotId, std::to_string(targetSlotId))
        << "CLI slotId should match cluster spec, CLI=" << cliSlotId << ", cluster=" << targetSlotId;

    // role 有效值
    EXPECT_TRUE(nodeInfo.role == "master" || nodeInfo.role == "standby" || nodeInfo.role == "agent")
        << "role should be master/standby/agent, actual: " << nodeInfo.role;

    // bondingEid 非空
    EXPECT_FALSE(nodeInfo.bondingEid.empty()) << "bondingEid should not be empty";

    // guid 非空
    EXPECT_FALSE(nodeInfo.guid.empty()) << "guid should not be empty";

    IT_LOG_INFO << "P0-CliNode-Ok-02 passed: node=" << nodeInfo.node << ", role=" << nodeInfo.role;
}

void RunP0CliNodeBadParam01(ubse::it::infra::ItCluster& cluster)
{
    auto& cli = cluster.GetCliInvoker("1");
    ubse::it::infra::ItNodeInfo nodeInfo;
    auto ret = cli.QueryNodeInfo(nodeInfo, "999");
    EXPECT_NE(ret, UBS_SUCCESS) << "display node -n 999 should fail";
    IT_LOG_INFO << "P0-CliNode-BadParam-01 passed";
}

void RunP0CliNodeBadParam02(ubse::it::infra::ItCluster& cluster)
{
    auto& cli = cluster.GetCliInvoker("1");
    ubse::it::infra::ItNodeInfo nodeInfo;

    // -n 0: 超出范围
    auto ret = cli.QueryNodeInfo(nodeInfo, "0");
    EXPECT_NE(ret, UBS_SUCCESS) << "display node -n 0 should fail";

    // -n 256: 超出范围
    ret = cli.QueryNodeInfo(nodeInfo, "256");
    EXPECT_NE(ret, UBS_SUCCESS) << "display node -n 256 should fail";

    // -n abc: 非法字符
    ret = cli.QueryNodeInfo(nodeInfo, "abc");
    EXPECT_NE(ret, UBS_SUCCESS) << "display node -n abc should fail";

    IT_LOG_INFO << "P0-CliNode-BadParam-02 passed";
}

// P0-CliTopoCpu-LinkChange-01(四节点): 1-2 链路断链
void RunP0CliTopoCpuLinkChange01(ubse::it::infra::ItCluster& cluster)
{
    // 构造 1-2 ubpu=2 断链, 1、2 节点分别上报 link-down
    cluster.MarkPortDown({{"1", "2", {2}}});
    EXPECT_UBSE_OK(cluster.GetNode("1").NotifyLinkUpDown(true, "400GUB1/2/2"));
    EXPECT_UBSE_OK(cluster.GetNode("2").NotifyLinkUpDown(true, "400GUB2/2/2"));

    // 查询拓扑 CLI，校验 1-2 链路 down（链路减少一条）
    std::vector<ubse::it::infra::ItTopoCpuLink> topoLinks;
    EXPECT_IT_OK(cluster.GetCliInvoker("1").QueryTopoCpu(topoLinks));
    EXPECT_EQ(topoLinks.size(), 11);

    // 恢复断链, 1、2 节点分别上报 link-up
    cluster.GetNode("1").ClearLinkDowns();
    cluster.GetNode("2").ClearLinkDowns();
    EXPECT_UBSE_OK(cluster.GetNode("1").NotifyLinkUpDown(false, "400GUB1/2/2"));
    EXPECT_UBSE_OK(cluster.GetNode("2").NotifyLinkUpDown(false, "400GUB2/2/2"));

    // 再次查询拓扑 CLI，校验 1-2 链路恢复 up（链路增加一条）
    EXPECT_IT_OK(cluster.GetCliInvoker("1").QueryTopoCpu(topoLinks));
    EXPECT_EQ(topoLinks.size(), 12);
}

// P1-CliTopoCpu-LinkOneNodeOnline-01: 链路一端节点在线, 另一侧节点离线
void RunP1CliTopoCpuLinkOneNodeOnline01(ubse::it::infra::ItCluster& cluster)
{
    // 停止2节点后，查询拓扑 CLI，校验 1-2 链路 down（linkId == "-"）
    IT_LOG_INFO << "Stopping node 2...";
    EXPECT_UBSE_OK(cluster.GetNode("2").StopUBSE());
    std::vector<ubse::it::infra::ItTopoCpuLink> topoLinks;
    EXPECT_IT_OK(cluster.GetCliInvoker("1").QueryTopoCpu(topoLinks));
    EXPECT_EQ(topoLinks.size(), 2);
    EXPECT_TRUE(topoLinks[0].linkId == "-" && topoLinks[1].linkId == "-") << "1-2 link should be down";

    IT_LOG_INFO << "Restarting node 1...";
    EXPECT_UBSE_OK(cluster.GetNode("1").RestartUBSE());

    auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() -> bool {
            std::string masterNodeId;
            return cluster.GetMasterNodeId(masterNodeId) == UBSE_OK;
        },
        30000);
    EXPECT_UBSE_OK(ret) << "1 node should be running";
    EXPECT_IT_OK(cluster.GetCliInvoker("1").QueryTopoCpu(topoLinks));
    EXPECT_EQ(topoLinks.size(), 2);
    EXPECT_TRUE(topoLinks[0].linkId == "-" && topoLinks[1].linkId == "-") << "1-2 link should be down";

    // 重启2节点后，查询拓扑 CLI，校验 1-2 链路 up
    IT_LOG_INFO << "Restarting node 2...";
    ASSERT_IT_OK(cluster.RestartNode("2", true, 30000));
    EXPECT_IT_OK(cluster.GetCliInvoker("1").QueryTopoCpu(topoLinks));
    EXPECT_EQ(topoLinks.size(), 2);
    EXPECT_FALSE(topoLinks[0].linkId == "-" || topoLinks[1].linkId == "-") << "1-2 link should be up";
}

// P0-CliHelpInfo-Ok-01: ubsectl -h / --help 输出全量已注册指令和参数信息，且 -h 与 --help 内容一致
void RunP0CliHelpInfoOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& cli = cluster.GetCliInvoker("1");

    // 1. ubsectl -h：指令下发成功，输出全量已注册指令和参数信息
    std::string shortHelp = cli.ExecCli("-h");
    EXPECT_FALSE(shortHelp.empty()) << "ubsectl -h should produce output";
    EXPECT_EQ(shortHelp.find("ERROR:"), std::string::npos) << "ubsectl -h should not return error";

    // 帮助信息应包含已注册命令的 Usage 描述（全量指令）
    EXPECT_NE(shortHelp.find("Usage: ubsectl"), std::string::npos)
        << "help should contain 'Usage: ubsectl' for registered commands";
    EXPECT_GT(CountOccurrences(shortHelp, "Usage: ubsectl"), 1) << "help should list all registered commands";

    // 参数信息：每个命令应包含 OPTIONS 及选项描述
    EXPECT_NE(shortHelp.find("OPTIONS:"), std::string::npos) << "help should list options for registered commands";
    EXPECT_NE(shortHelp.find("--"), std::string::npos) << "help should show long option descriptions";

    // 2. ubsectl --help：指令下发成功，内容与 -h 完全一致
    std::string longHelp = cli.ExecCli("--help");
    EXPECT_FALSE(longHelp.empty()) << "ubsectl --help should produce output";
    EXPECT_EQ(longHelp.find("ERROR:"), std::string::npos) << "ubsectl --help should not return error";
    EXPECT_EQ(shortHelp, longHelp) << "ubsectl -h and --help should print identical help content";

    IT_LOG_INFO << "P0-CliHelpInfo-Ok-01 passed";
}

// P0-CliCompletion-Ok-01: ubsectl 命令自动补齐功能（Bash Tab 补全候选）
// 验证步骤：
//   1. ubsec + Tab                       -> 补全为 ubsectl（bash 命令名补全）
//   2. ubsectl dis + Tab                 -> ubsectl display
//   3. ubsectl display m + Tab           -> ubsectl display memory
//      ubsectl display t + Tab           -> ubsectl display topo
//   4. ubsectl display memory --t + Tab  -> ubsectl display memory --type
//   5. ubsectl display memory --n + Tab  -> ubsectl display memory --name
//   6. ubsectl display memory --b + Tab  -> ubsectl display memory --borrow-type
void RunP0CliCompletionOk01(ubse::it::infra::ItCluster& cluster)
{
    (void)cluster; // 补全用例不依赖集群节点，仅验证 CLI 自动补齐功能
    // 1. 定位补全脚本：<build>/scripts/cli_commands.sh
    std::string scriptPath = LocateCliCompletionScript();
    ASSERT_FALSE(scriptPath.empty()) << "failed to locate cli_commands.sh via /proc/self/exe";

    // 由补全脚本路径推导 ubsectl 所在目录 <build>/bin，供步骤1的命令名补全使用
    const std::string suffix = "/scripts/cli_commands.sh";
    std::string binDir;
    if (scriptPath.size() >= suffix.size() &&
        scriptPath.compare(scriptPath.size() - suffix.size(), suffix.size(), suffix) == 0) {
        binDir = scriptPath.substr(0, scriptPath.size() - suffix.size()) + "/bin";
    }
    ASSERT_FALSE(binDir.empty()) << "failed to derive <build>/bin from completion script path";

    // 2. 在 bash 中 source 补全脚本，模拟 Tab 补全（设置 COMP_WORDS/COMP_CWORD 后调用补全函数），收集候选词
    std::string script =
        "source \"" + scriptPath +
        "\"\n"
        "export PATH=\"" +
        binDir +
        ":$PATH\"\n"
        "echo \"REGISTER=$(complete -p ubsectl)\"\n"
        // 步骤1：ubsec + Tab，bash 命令名补全应得到 ubsectl
        // 注意：脚本由 RunBashScript 以 bash -c '<script>' 执行，脚本内禁止出现单引号，
        //       故用 xargs 将候选命令拼接为单行（不能用 tr '\n' ' '）。
        //       bin 目录含 ubse_it_daemon/ubse_it_node_launcher 等，前缀必须用 ubsec 才能唯一补全为 ubsectl
        "echo \"S1_CMD=$(compgen -c ubsec | xargs)\"\n"
        // 步骤2：ubsectl dis + Tab，object 部分字符补齐为完整 object
        "COMP_WORDS=(ubsectl dis); COMP_CWORD=1; _ubse_commond_completion; echo \"S2_DIS=${COMPREPLY[*]}\"\n"
        // 步骤3：ubsectl display m + Tab / display t + Tab，action 部分字符补齐为完整 action
        "COMP_WORDS=(ubsectl display m); COMP_CWORD=2; _ubse_commond_completion; echo \"S3_M=${COMPREPLY[*]}\"\n"
        "COMP_WORDS=(ubsectl display t); COMP_CWORD=2; _ubse_commond_completion; echo \"S3_T=${COMPREPLY[*]}\"\n"
        // 步骤4：ubsectl display memory --t + Tab，长名参数补齐为 --type
        "COMP_WORDS=(ubsectl display memory --t); COMP_CWORD=3; _ubse_commond_completion; echo "
        "\"S4_T=${COMPREPLY[*]}\"\n"
        // 步骤5：ubsectl display memory --n + Tab，长名参数补齐为 --name
        "COMP_WORDS=(ubsectl display memory --n); COMP_CWORD=3; _ubse_commond_completion; echo "
        "\"S5_N=${COMPREPLY[*]}\"\n"
        // 步骤6：ubsectl display memory --b + Tab，长名参数补齐为 --borrow-type
        "COMP_WORDS=(ubsectl display memory --b); COMP_CWORD=3; _ubse_commond_completion; echo "
        "\"S6_B=${COMPREPLY[*]}\"\n";
    std::string output = RunBashScript(script);
    ASSERT_FALSE(output.empty()) << "bash completion script produced no output";

    // 前置：补全函数已注册到 ubsectl（自动补齐已启用）
    std::string registered = ExtractTagValue(output, "REGISTER");
    EXPECT_NE(registered.find("_ubse_commond_completion"), std::string::npos)
        << "complete -p ubsectl should register _ubse_commond_completion, got: " << registered;
    EXPECT_NE(registered.find("ubsectl"), std::string::npos)
        << "completion should be bound to ubsectl, got: " << registered;

    // 步骤1：ubsec + Tab 应补全出 ubsectl
    std::string s1Cmd = ExtractTagValue(output, "S1_CMD");
    EXPECT_TRUE(ContainsWord(s1Cmd, "ubsectl")) << "step1: 'ubsec'+Tab should complete to 'ubsectl', got: " << s1Cmd;

    // 步骤2：ubsectl dis + Tab 应补齐为 ubsectl display
    EXPECT_EQ(ExtractTagValue(output, "S2_DIS"), "display") << "step2: prefix 'dis' should complete to 'display'";

    // 步骤3：ubsectl display m + Tab 应补齐为 memory；display t + Tab 应补齐为 topo
    EXPECT_EQ(ExtractTagValue(output, "S3_M"), "memory") << "step3: prefix 'm' should complete to 'memory'";
    EXPECT_EQ(ExtractTagValue(output, "S3_T"), "topo") << "step3: prefix 't' should complete to 'topo'";

    // 步骤4：ubsectl display memory --t + Tab 应补齐为 --type
    EXPECT_EQ(ExtractTagValue(output, "S4_T"), "--type") << "step4: prefix '--t' should complete to '--type'";

    // 步骤5：ubsectl display memory --n + Tab 应补齐为 --name
    EXPECT_EQ(ExtractTagValue(output, "S5_N"), "--name") << "step5: prefix '--n' should complete to '--name'";

    // 步骤6：ubsectl display memory --b + Tab 应补齐为 --borrow-type
    EXPECT_EQ(ExtractTagValue(output, "S6_B"), "--borrow-type")
        << "step6: prefix '--b' should complete to '--borrow-type'";

    IT_LOG_INFO << "P0-CliCompletion-Ok-01 passed";
}
} // namespace ubse::it::tests::topo