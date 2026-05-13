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

#include "test_ubs_engine_topo.h"

#include <arpa/inet.h>
#include <securec.h>
#include <cstring>
#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_node_api_convert.h"
#include "ubs_engine_topo.h"
#include "ubs_error.h"

namespace ubse::sdk::ut {
using namespace ubse::node::api;
void TestUbsEngineTopo::SetUp()
{
    Test::SetUp();
}

void TestUbsEngineTopo::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

// 1. 非法参数
TEST_F(TestUbsEngineTopo, UbsTopoNodeListWhenInvalidParameters)
{
    // node_list为nullptr
    ubs_topo_node_t** nullList = nullptr;
    uint32_t cnt = 0;

    int32_t ret = ubs_topo_node_list(nullList, &cnt);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);

    // node_cnt为nullptr
    ubs_topo_node_t* list = nullptr;
    ret = ubs_topo_node_list(&list, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 2. ubse_invoke_call 失败
TEST_F(TestUbsEngineTopo, UbsTopoNodeListWhenInvokeCallFailed)
{
    ubs_topo_node_t* list = nullptr;
    uint32_t cnt = 0;

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    int32_t ret = ubs_topo_node_list(&list, &cnt);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

// 3. 解包失败
TEST_F(TestUbsEngineTopo, UbsTopoNodeListWhenUnpackFailed)
{
    ubs_topo_node_t* list = nullptr;
    uint32_t cnt = 0;
    // 构造异常数据
    ubse_api_buffer_t respBuffer{};
    respBuffer.length = 2; // 数据长度2
    respBuffer.buffer = static_cast<uint8_t*>(malloc(respBuffer.length));
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));

    int32_t ret = ubs_topo_node_list(&list, &cnt);
    EXPECT_EQ(ret, UBS_ERR_BUFFER_TOO_SMALL);
}

UbseNode BuildNode()
{
    UbseNode node;
    node.slotId = 1;
    node.socketId[0] = 1;   // socketId是1
    node.socketId[1] = 2;   // socketId是2
    node.numaIds[0][0] = 1; // numaId是1
    node.numaIds[0][1] = 2; // numaId是2
    node.numaIds[1][0] = 3; // numaId是3
    node.numaIds[1][1] = 4; // numaId是4
    // 初始化 IPv4 地址
    struct in_addr ipv4_addr {};
    if (inet_pton(AF_INET, "192.168.1.1", &ipv4_addr) == 1) {
        node.ips[0].af = AF_INET;
        node.ips[0].ipv4 = ipv4_addr;
    }

    // 初始化 IPv6 地址
    struct in6_addr ipv6_addr {};
    if (inet_pton(AF_INET6, "::1", &ipv6_addr) == 1) {
        node.ips[1].af = AF_INET6;
        node.ips[1].ipv6 = ipv6_addr;
    }
    node.hostName = "node1";
    return node;
}

std::vector<UbseNode> BuildNodeList()
{
    std::vector<UbseNode> nodeList{};

    auto node1 = BuildNode();
    nodeList.push_back(node1);

    // 创建第二个节点
    UbseNode node2 = node1;
    node2.slotId = 2; // 节点2是slotId是2
    node2.hostName = "node2";
    nodeList.push_back(node2);
    return nodeList;
}
// 4. 正常流程
TEST_F(TestUbsEngineTopo, UbsTopoNodeListWhenSuccess)
{
    ubs_topo_node_t* list = nullptr;
    uint32_t cnt = 0;

    // 构造正常数据
    auto nodeList = BuildNodeList();
    ipc::UbseIpcMessage respMessage{};
    UbseNodeListPack(nodeList, respMessage);
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t*>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    delete respMessage.buffer;
    respMessage.buffer = nullptr;
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    int32_t ret = ubs_topo_node_list(&list, &cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 测试正常情况：成功获取本地节点信息
TEST_F(TestUbsEngineTopo, UbsTopoNodeLocalGet_NormalCase)
{
    ubs_topo_node_t node;
    // 构造正常数据
    auto nodeInfo = BuildNode();
    ipc::UbseIpcMessage respMessage{};
    auto ret = UbseNodePack(nodeInfo, respMessage);
    EXPECT_EQ(ret, UBSE_OK);
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t*>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    ret = ubs_topo_node_local_get(&node);
    EXPECT_EQ(ret, UBS_SUCCESS);
}

// 测试参数为空的情况
TEST_F(TestUbsEngineTopo, UbsTopoNodeLocalGet_NullPointerCase)
{
    int32_t ret = ubs_topo_node_local_get(nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 测试接口调用失败的情况
TEST_F(TestUbsEngineTopo, UbsTopoNodeLocalGet_IpcFailureCase)
{
    ubs_topo_node_t node;

    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    int32_t ret = ubs_topo_node_local_get(&node);
    EXPECT_NE(ret, UBS_SUCCESS);
}

// 测试解包失败的情况
TEST_F(TestUbsEngineTopo, UbsTopoNodeLocalGet_UnpackFailureCase)
{
    ubs_topo_node_t node;

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t*>(malloc(1));
    respBuffer.length = 1;
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));

    int32_t ret = ubs_topo_node_local_get(&node);
    EXPECT_NE(ret, UBS_SUCCESS);
}

std::vector<UbseCpuLink> BuildCpuLinkList()
{
    std::vector<UbseCpuLink> cpuLinkList{};
    UbseCpuLink link1;
    link1.slotId = 1;
    link1.socketId = 1;
    link1.portId = 1;
    link1.peerSlotId = 2;   // 对端slotId 2
    link1.peerSocketId = 2; // 对端socketId 2
    link1.peerPortId = 2;   // 对端portId 2
    cpuLinkList.push_back(link1);

    UbseCpuLink link2;
    link2.slotId = 2; // slotId 2
    link2.socketId = 1;
    link2.portId = 2; // portId 2
    link2.peerSlotId = 1;
    link2.peerSocketId = 2; // socketId 2
    link2.peerPortId = 1;
    cpuLinkList.push_back(link2);
    return cpuLinkList;
}
// 测试正常情况：成功获取CPU拓扑链接信息
TEST_F(TestUbsEngineTopo, UbsTopoLinkList_NormalCase)
{
    ubs_topo_link_t* cpu_links = nullptr;
    uint32_t cpu_link_cnt = 0;
    // 构造正常数据
    auto cpuLinkList = BuildCpuLinkList();
    ipc::UbseIpcMessage respMessage{};
    auto ret = UbseCpuLinkListPack(cpuLinkList, respMessage);
    EXPECT_EQ(ret, UBSE_OK);
    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t*>(malloc(respMessage.length));
    respBuffer.length = respMessage.length;
    if (memcpy_s(respBuffer.buffer, respBuffer.length, respMessage.buffer, respMessage.length) != EOK) {
        free(respBuffer.buffer);
        respBuffer.buffer = nullptr;
        respBuffer.length = 0;
    }
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    ret = ubs_topo_link_list(&cpu_links, &cpu_link_cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
    // 检查返回的链接数量是否大于零
    EXPECT_GT(cpu_link_cnt, 0);
}

// 测试参数为空的情况
TEST_F(TestUbsEngineTopo, UbsTopoLinkList_NullPointerCase)
{
    int32_t ret = ubs_topo_link_list(nullptr, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
}

// 测试接口调用失败的情况
TEST_F(TestUbsEngineTopo, UbsTopoLinkList_IpcFailureCase)
{
    ubs_topo_link_t* cpu_links = nullptr;
    uint32_t cpu_link_cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    int32_t ret = ubs_topo_link_list(&cpu_links, &cpu_link_cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}

// 测试解包失败的情况
TEST_F(TestUbsEngineTopo, UbsTopoLinkList_UnpackFailureCase)
{
    ubs_topo_link_t* cpu_links = nullptr;
    uint32_t cpu_link_cnt = 0;

    ubse_api_buffer_t respBuffer{};
    respBuffer.buffer = static_cast<uint8_t*>(malloc(1));
    respBuffer.length = 1;
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    int32_t ret = ubs_topo_link_list(&cpu_links, &cpu_link_cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}
} // namespace ubse::sdk::ut