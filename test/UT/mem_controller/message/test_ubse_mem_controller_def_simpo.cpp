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

#include "test_ubse_mem_controller_def_simpo.h"

#include <ubse_error.h>
#include <arpa/inet.h>

#include "message/ubse_mem_controller_def_simpo.h"
namespace ubse::mem::controller::message::ut {
void TestUbseMemControllerDefSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseMemControllerDefSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}
UbseTopoNode ConstructUbseTopoNode()
{
    UbseTopoNode node;
    node.slotId = 1;
    node.socketIdList = {1, 2, 3};
    node.numaIdList = {1, 2, 3};
    struct in_addr ipv4_addr {};

    if (inet_pton(AF_INET, "192.168.1.1", &ipv4_addr) == 1) {
        UbseTopoIpAddress ip{};
        ip.af = AF_INET;
        ip.ipv4 = ipv4_addr;
        node.ips.push_back(ip);
    }

    // 初始化 IPv6 地址
    struct in6_addr ipv6_addr {};
    if (inet_pton(AF_INET6, "::1", &ipv6_addr) == 1) {
        UbseTopoIpAddress ip{};

        ip.af = AF_INET6;
        ip.ipv6 = ipv6_addr;
        node.ips.push_back(ip);
    }
    return node;
}
def::UbseNode ConstructUbseNode()
{
    def::UbseNode node;
    node.slotId = 1;
    node.socketId[0] = 1;  // socketId是1
    node.socketId[1] = 2;  // socketId是2
    node.numaIds[0][0] = 1;  // numaId是1
    node.numaIds[0][1] = 2;  // numaId是2
    node.numaIds[1][0] = 3;  // numaId是3
    node.numaIds[1][1] = 4;  // numaId是4
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
    node.hostName = "1";
    return node;
}
def::UbseMemFdDesc ConstructUbseMemFdDesc()
{
    def::UbseMemFdDesc desc{};
    desc.name = "test_fd";
    desc.memIds = {1};
    desc.totalMemSize = 0;
    desc.unitSize = 0;
    desc.importNode = ConstructUbseNode();
    desc.exportNode = ConstructUbseNode();
    desc.state = UbseMemStage::UBSE_EXIST;
    return desc;
}
def::UbseMemNumaDesc ConstructDefUbseMemNumaDesc()
{
    def::UbseMemNumaDesc desc{};
    desc.name = "test_numa";
    desc.numaId = 1;
    desc.importNode = ConstructUbseNode();
    desc.exportNode = ConstructUbseNode();
    desc.size = 0;
    desc.state = UbseMemStage::UBSE_EXIST;
    return desc;
}
UbseMemNumaDesc ConstructUbseMemNumaDesc()
{
    UbseMemNumaDesc desc{};
    desc.name = "test_numa";
    desc.numaId = 1;
    desc.exportNode = ConstructUbseTopoNode();
    desc.importNode = ConstructUbseTopoNode();
    desc.size = 1;
    return desc;
}
UbseMemAddrDesc ConstructUbseMemAddrDesc()
{
    UbseMemAddrDesc desc{};
    desc.name = "test_addr";
    desc.numaId = 1;
    desc.importNode = ConstructUbseTopoNode();
    desc.size = 1;
    UbseMemAddrBorrowLocAndSizeByPid locAndSizeByPid{};
    desc.lender.vaLists.push_back(locAndSizeByPid);
    return desc;
}
def::UbseMemShmDesc ConstructUbseMemShmDesc()
{
    def::UbseMemShmDesc desc{};
    desc.name = "test_shm";
    desc.totalMemSize = 0;
    desc.unitSize = 0;
    desc.exportNode = ConstructUbseNode();
    desc.state = UbseMemStage::UBSE_EXIST;
    def::UbseMemShmImportDesc importDesc;
    importDesc.memIds.push_back(1);
    importDesc.memIds.push_back(2);
    importDesc.importNode = ConstructUbseNode();
    importDesc.state = UbseMemStage::UBSE_EXIST;
    desc.importDesc.push_back(importDesc);
    return desc;
}
def::UbseMemDebtQueryRequest ConstructUbseMemDebtQueryRequest()
{
    ubse::adapter_plugins::mmi::UbseUdsInfo udsInfo{};
    def::UbseMemDebtQueryRequest request{
        .name = "test_request", .importNodeId = "0", .exportNodeId = "1", .udsInfo = udsInfo};
    return request;
}

TEST_F(TestUbseMemControllerDefSimpo, TestFdDescSerAndDes)
{
    UbseMemFdDescSimpoPtr ptr = new UbseMemFdDescSimpo();

    def::UbseMemFdDesc desc = ConstructUbseMemFdDesc();
    ptr->SetUbseMemFdDesc(desc);
    EXPECT_EQ(ptr->Serialize(), UBSE_OK);

    UbseMemFdDescSimpoPtr ptrDes = new UbseMemFdDescSimpo();

    ptrDes->SetInputRawDataFromShared(ptr->GetSharedOutputData(), ptr->SerializedDataSize());
    EXPECT_EQ(ptrDes->Deserialize(), UBSE_OK);
    EXPECT_EQ(ptrDes->GetUbseMemFdDesc().state, desc.state);
}
TEST_F(TestUbseMemControllerDefSimpo, TestFdDescListSerAndDes)
{
    UbseMemFdDescListSimpoPtr ptr = new UbseMemFdDescListSimpo();

    def::UbseMemFdDesc desc = ConstructUbseMemFdDesc();
    std::vector<def::UbseMemFdDesc> descs = {desc};
    ptr->SetUbseMemFdDescList(descs);
    EXPECT_EQ(ptr->Serialize(), UBSE_OK);
    UbseMemFdDescListSimpoPtr ptrDes = new UbseMemFdDescListSimpo();

    ptrDes->SetInputRawDataFromShared(ptr->GetSharedOutputData(), ptr->SerializedDataSize());
    EXPECT_EQ(ptrDes->Deserialize(), UBSE_OK);
    EXPECT_EQ(ptrDes->GetUbseMemFdDescList().at(0).state, desc.state);
}
TEST_F(TestUbseMemControllerDefSimpo, TestDefNumaDescSerAndDes)
{
    DefUbseMemNumaDescSimpoPtr ptr = new DefUbseMemNumaDescSimpo();

    def::UbseMemNumaDesc desc = ConstructDefUbseMemNumaDesc();
    ptr->SetUbseMemNumaDesc(desc);
    EXPECT_EQ(ptr->Serialize(), UBSE_OK);

    DefUbseMemNumaDescSimpoPtr ptrDes = new DefUbseMemNumaDescSimpo();

    ptrDes->SetInputRawDataFromShared(ptr->GetSharedOutputData(), ptr->SerializedDataSize());
    EXPECT_EQ(ptrDes->Deserialize(), UBSE_OK);
    EXPECT_EQ(ptrDes->GetUbseMemNumaDesc().state, desc.state);
}
TEST_F(TestUbseMemControllerDefSimpo, TestDefNumaDescListSerAndDes)
{
    DefUbseMemNumaDescListSimpoPtr ptr = new DefUbseMemNumaDescListSimpo();

    def::UbseMemNumaDesc desc = ConstructDefUbseMemNumaDesc();
    std::vector descs = {desc};
    ptr->SetUbseMemNumaDescList(descs);
    EXPECT_EQ(ptr->Serialize(), UBSE_OK);
    DefUbseMemNumaDescListSimpoPtr ptrDes = new DefUbseMemNumaDescListSimpo();

    ptrDes->SetInputRawDataFromShared(ptr->GetSharedOutputData(), ptr->SerializedDataSize());
    EXPECT_EQ(ptrDes->Deserialize(), UBSE_OK);
    EXPECT_EQ(ptrDes->GetUbseMemNumaDescList().at(0).state, desc.state);
}
TEST_F(TestUbseMemControllerDefSimpo, TestShmDescSerAndDes)
{
    UbseMemShmDescSimpoPtr ptr = new UbseMemShmDescSimpo();

    def::UbseMemShmDesc desc = ConstructUbseMemShmDesc();
    ptr->SetUbseMemShmDesc(desc);
    EXPECT_EQ(ptr->Serialize(), UBSE_OK);

    UbseMemShmDescSimpoPtr ptrDes = new UbseMemShmDescSimpo();

    ptrDes->SetInputRawDataFromShared(ptr->GetSharedOutputData(), ptr->SerializedDataSize());
    EXPECT_EQ(ptrDes->Deserialize(), UBSE_OK);
    EXPECT_EQ(ptrDes->GetUbseMemShmDesc().state, desc.state);
}
TEST_F(TestUbseMemControllerDefSimpo, TestShmDescListSerAndDes)
{
    UbseMemShmDescListSimpoPtr ptr = new UbseMemShmDescListSimpo();

    def::UbseMemShmDesc desc = ConstructUbseMemShmDesc();
    std::vector descs = {desc};
    ptr->SetUbseMemShmDescList(descs);
    EXPECT_EQ(ptr->Serialize(), UBSE_OK);
    UbseMemShmDescListSimpoPtr ptrDes = new UbseMemShmDescListSimpo();

    ptrDes->SetInputRawDataFromShared(ptr->GetSharedOutputData(), ptr->SerializedDataSize());
    EXPECT_EQ(ptrDes->Deserialize(), UBSE_OK);
    EXPECT_EQ(ptrDes->GetUbseMemShmDescList().at(0).state, desc.state);
}
TEST_F(TestUbseMemControllerDefSimpo, TestNumaDescSerAndDes)
{
    UbseMemNumaDescSimpoPtr ptr = new UbseMemNumaDescSimpo();

    UbseMemNumaDesc desc = ConstructUbseMemNumaDesc();
    ptr->SetUbseMemNumaDesc(desc);
    EXPECT_EQ(ptr->Serialize(), UBSE_OK);

    UbseMemNumaDescSimpoPtr ptrDes = new UbseMemNumaDescSimpo();

    ptrDes->SetInputRawDataFromShared(ptr->GetSharedOutputData(), ptr->SerializedDataSize());
    EXPECT_EQ(ptrDes->Deserialize(), UBSE_OK);
    EXPECT_EQ(ptrDes->GetUbseMemNumaDesc().name, desc.name);
}
TEST_F(TestUbseMemControllerDefSimpo, TestAddrDescSerAndDes)
{
    UbseMemAddrDescSimpoPtr ptr = new UbseMemAddrDescSimpo();

    UbseMemAddrDesc desc = ConstructUbseMemAddrDesc();
    ptr->SetUbseMemAddrDesc(desc);
    EXPECT_EQ(ptr->Serialize(), UBSE_OK);

    UbseMemAddrDescSimpoPtr ptrDes = new UbseMemAddrDescSimpo();

    ptrDes->SetInputRawDataFromShared(ptr->GetSharedOutputData(), ptr->SerializedDataSize());
    EXPECT_EQ(ptrDes->Deserialize(), UBSE_OK);
    EXPECT_EQ(ptrDes->GetUbseMemAddrDesc().name, desc.name);
}
}  // namespace ubse::mem::controller::message::ut
