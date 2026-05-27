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

#include "test_ubse_node_api.h"

#include <src/api_server/ubse_api_server_module.h>
#include <src/framework/context/ubse_context.h>
#include <src/framework/ha/ubse_election_module.h>
#include <src/framework/ipc/include/ubse_ipc_common.h>
#include <src/framework/ipc/include/ubse_ipc_server.h>
#include <src/framework/serde/ubse_serial_util.h>
#include <ubse_error.h>
#include <ubse_node.h>

#include "src/include/ubse_api_server_def.h"

namespace ubse::node::api::ut {
using namespace ubse::serial;
using namespace ubse::context;
using namespace ubse::nodeController;
using namespace ubse::common::def;
using ::api::server::UbseApiServerModule;
using ::api::server::UbseResult;

// mock UbseGetDirConnectInfo函数 - 正常情况
std::map<std::string, ubse::nodeController::PhysicalLink> MockUbseGetDirConnectInfo()
{
    return {
        {"link0", {1, 1, 1, "interface0", 2, 1, 1, "interface2", LinkStatus::available}},
        {"link1", {0, 1, 0, "interface1", 2, 1, 0, "interface2", LinkStatus::available}},
        {"link2", {2, 1, 2, "interface2", 1, 1, 2, "interface1", LinkStatus::available}},
        {"link3", {1, 1, 3, "interface3", 2, 1, 3, "", LinkStatus::conflict}},
    };
}

void TestUbseNodeApi::SetUp()
{
    Test::SetUp();
}
void TestUbseNodeApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeApi, UbseServerNodeGetWhenNodePackFail)
{
    auto& ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    MOCKER_CPP(UbseNodePack).stubs().will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));
    auto ret = UbseNodeApi::UbseServerNodeGet(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseNodeApi, UbseServerNodeGetWhenApiServerModuleIsNull)
{
    auto& ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap_;
    std::shared_ptr<::api::server::UbseApiServerModule> nullModule = nullptr;
    ubseCtx.moduleMap_[typeid(::api::server::UbseApiServerModule)] = nullModule;
    auto ret = UbseNodeApi::UbseServerNodeGet(req, ctx);
    ubseCtx.moduleMap_.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerNodeGetWhenSendResponseFail)
{
    auto& ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap_;
    ubseCtx.moduleMap_[typeid(::api::server::UbseApiServerModule)] =
        std::make_shared<::api::server::UbseApiServerModule>();
    auto apiServerModule = UbseContext::GetInstance().GetModule<::api::server::UbseApiServerModule>();
    apiServerModule->ipcServer_ = nullptr;
    auto ret = UbseNodeApi::UbseServerNodeGet(req, ctx);
    ubseCtx.moduleMap_.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerNodeListWhenNodeListPackFail)
{
    auto& ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    MOCKER_CPP(UbseNodeListPack).stubs().will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));
    auto ret = UbseNodeApi::UbseServerNodeList(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseNodeApi, UbseServerNodeListWhenApiServerModuleIsNull)
{
    auto& ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap_;
    std::shared_ptr<::api::server::UbseApiServerModule> nullModule = nullptr;
    ubseCtx.moduleMap_[typeid(::api::server::UbseApiServerModule)] = nullModule;
    auto ret = UbseNodeApi::UbseServerNodeList(req, ctx);
    ubseCtx.moduleMap_.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerNodeListWhenSendResponseFail)
{
    auto& ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap_;
    ubseCtx.moduleMap_[typeid(::api::server::UbseApiServerModule)] =
        std::make_shared<::api::server::UbseApiServerModule>();
    auto apiServerModule = UbseContext::GetInstance().GetModule<::api::server::UbseApiServerModule>();
    apiServerModule->ipcServer_ = nullptr;
    auto ret = UbseNodeApi::UbseServerNodeList(req, ctx);
    ubseCtx.moduleMap_.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerCpuTopoListWhenCpuLinkListPack)
{
    auto& ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    MOCKER_CPP(UbseCpuLinkListPack).stubs().will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));
    auto ret = UbseNodeApi::UbseServerCpuTopoList(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseNodeApi, UbseServerCpuTopoListWhenApiServerModuleIsNull)
{
    auto& ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap_;
    std::shared_ptr<::api::server::UbseApiServerModule> nullModule = nullptr;
    ubseCtx.moduleMap_[typeid(::api::server::UbseApiServerModule)] = nullModule;
    auto ret = UbseNodeApi::UbseServerCpuTopoList(req, ctx);
    ubseCtx.moduleMap_.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerCpuTopoListWhenSendResponseFail)
{
    auto& ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap_;
    ubseCtx.moduleMap_[typeid(::api::server::UbseApiServerModule)] =
        std::make_shared<::api::server::UbseApiServerModule>();
    auto apiServerModule = UbseContext::GetInstance().GetModule<::api::server::UbseApiServerModule>();
    apiServerModule->ipcServer_ = nullptr;
    auto ret = UbseNodeApi::UbseServerCpuTopoList(req, ctx);
    ubseCtx.moduleMap_.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseClusterListWhenNodeInfoIsEmpty)
{
    auto& ubseCtx = UbseContext::GetInstance();
    std::vector<UbseNodeInfo> nodeList;
    auto backupModuleMap = ubseCtx.moduleMap_;
    std::shared_ptr<UbseModule> nullModule = nullptr;
    ubseCtx.moduleMap_[typeid(UbseElectionModule)] = nullModule;
    UbseClusterList(nodeList);
    ubseCtx.moduleMap_.swap(backupModuleMap);
    ASSERT_EQ(nodeList.empty(), true);
}

TEST_F(TestUbseNodeApi, UbseClusterListWhenSuccess)
{
    std::vector<UbseNodeInfo> nodeList;
    UbseNodeInfo info1{.nodeId = "0", .slotId = 0};
    UbseNodeInfo info2{.nodeId = "1", .slotId = 1};
    std::unordered_map<std::string, UbseNodeInfo> allNodeInfoMap;
    std::vector<UbseNodeInfo> staticNodeInfo;
    allNodeInfoMap.emplace("0", info1);
    staticNodeInfo.push_back(info2);
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(allNodeInfoMap));
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(staticNodeInfo));
    UbseClusterList(nodeList);
    ASSERT_EQ(nodeList.size(), 2);
}

TEST_F(TestUbseNodeApi, UbseGetRoleMapWhenElectionModuleIsNull)
{
    std::shared_ptr<UbseElectionModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));
    ::api::server::UbseRequestContext ctx{};
    auto roleMap = UbseGetRoleMap(ctx);
    ASSERT_EQ(roleMap.size(), 0);
}

TEST_F(TestUbseNodeApi, UbseGetRoleMapWhenSuccess)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    Node master{.id = "0"};
    Node standby{.id = "1"};
    MOCKER_CPP(&UbseElectionModule::UbseGetMasterNode).stubs().with(outBound(master)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::UbseGetStandbyNode).stubs().with(outBound(standby)).will(returnValue(UBSE_OK));
    ::api::server::UbseRequestContext ctx{};
    auto roleMap = UbseGetRoleMap(ctx);
    ASSERT_EQ(roleMap[master.id], UBSE_ROLE_MASTER);
    ASSERT_EQ(roleMap[standby.id], UBSE_ROLE_SLAVE);
}

TEST_F(TestUbseNodeApi, UbseQueryClusterInfoWhenReqBufferIsNull)
{
    ::api::server::UbseIpcMessage req{.buffer = nullptr, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseQueryClusterInfo(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseQueryClusterInfoWhenSerialCheckFail)
{
    UbseNodeInfo info1{.nodeId = "0", .slotId = 0};
    UbseNodeInfo info2{.nodeId = "1", .slotId = 1};
    std::vector<UbseNodeInfo> nodeList{info1, info2};
    MOCKER_CPP(UbseClusterList).stubs().with(outBound(nodeList));
    std::unordered_map<std::string, std::string> roleMap;
    roleMap["0"] = UBSE_ROLE_MASTER;
    roleMap["1"] = UBSE_ROLE_SLAVE;
    MOCKER_CPP(UbseGetRoleMap).stubs().will(returnValue(roleMap));
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    ::api::server::UbseIpcMessage req{.buffer = new uint8_t, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseQueryClusterInfo(req, ctx);
    delete req.buffer;
    ASSERT_EQ(ret, UBSE_ERROR_SERIALIZE_FAILED);
}

TEST_F(TestUbseNodeApi, UbseQueryClusterInfoWhenApiServerIsNull)
{
    UbseNodeInfo info1{.nodeId = "0", .slotId = 0};
    UbseNodeInfo info2{.nodeId = "1", .slotId = 1};
    std::vector<UbseNodeInfo> nodeList{info1, info2};
    MOCKER_CPP(UbseClusterList).stubs().with(outBound(nodeList));
    std::unordered_map<std::string, std::string> roleMap;
    roleMap["0"] = UBSE_ROLE_MASTER;
    roleMap["1"] = UBSE_ROLE_SLAVE;
    MOCKER_CPP(UbseGetRoleMap).stubs().will(returnValue(roleMap));
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(true));
    ::api::server::UbseIpcMessage req{.buffer = new uint8_t, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    std::shared_ptr<UbseApiServerModule> nullApiModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullApiModule));
    auto ret = UbseNodeApi::UbseQueryClusterInfo(req, ctx);
    delete req.buffer;
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseQueryClusterInfoWhenSendResponseFail)
{
    UbseNodeInfo info1{.nodeId = "0", .slotId = 0};
    UbseNodeInfo info2{.nodeId = "1", .slotId = 1};
    std::vector<UbseNodeInfo> nodeList{info1, info2};
    MOCKER_CPP(UbseClusterList).stubs().with(outBound(nodeList));
    std::unordered_map<std::string, std::string> roleMap;
    roleMap["0"] = UBSE_ROLE_MASTER;
    roleMap["1"] = UBSE_ROLE_SLAVE;
    MOCKER_CPP(UbseGetRoleMap).stubs().will(returnValue(roleMap));
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(true));
    ::api::server::UbseIpcMessage req{.buffer = new uint8_t, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto apiModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseNodeApi::UbseQueryClusterInfo(req, ctx);
    delete req.buffer;
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeApi, UbseQueryClusterInfoWhenSuccess)
{
    UbseNodeInfo info1{.nodeId = "0", .slotId = 0};
    UbseNodeInfo info2{.nodeId = "1", .slotId = 1};
    std::vector<UbseNodeInfo> nodeList{info1, info2};
    MOCKER_CPP(UbseClusterList).stubs().with(outBound(nodeList));
    std::unordered_map<std::string, std::string> roleMap;
    roleMap["0"] = UBSE_ROLE_MASTER;
    roleMap["1"] = UBSE_ROLE_SLAVE;
    MOCKER_CPP(UbseGetRoleMap).stubs().will(returnValue(roleMap));
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(true));
    ::api::server::UbseIpcMessage req{.buffer = new uint8_t, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto apiModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseNodeApi::UbseQueryClusterInfo(req, ctx);
    delete req.buffer;
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeApi, RegisterWhenApiServerIsNull)
{
    std::shared_ptr<UbseApiServerModule> nullApiModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullApiModule));
    auto ret = UbseNodeApi::Register();
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, RegisterWhenRegisterIpcHandlerFail)
{
    auto apiModule = std::make_shared<::api::server::UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiModule));
    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseNodeApi::Register();
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeApi, RegisterWhenSuccess)
{
    auto apiModule = std::make_shared<::api::server::UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiModule));
    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseNodeApi::Register();
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeApi, UbseServerNodeNumaMemGetWhenListPackFail)
{
    MOCKER_CPP(UbseSlotIdUnpack).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER_CPP(UbseNodeNumaMemGet).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseNumaInfoListPack).stubs().will(returnValue((uint32_t)UBSE_ERROR_INVAL));
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseServerNodeNumaMemGet(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseNodeApi, UbseServerNodeNumaMemGetWhenServerModuleIsNull)
{
    MOCKER_CPP(UbseSlotIdUnpack).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER_CPP(UbseNodeNumaMemGet).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseNumaInfoListPack).stubs().will(returnValue((uint32_t)UBSE_OK));
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullModule));
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseServerNodeNumaMemGet(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerNodeNumaMemGetWhenSuccess)
{
    MOCKER_CPP(UbseSlotIdUnpack).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER_CPP(UbseNodeNumaMemGet).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseNumaInfoListPack).stubs().will(returnValue((uint32_t)UBSE_OK));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseServerNodeNumaMemGet(req, ctx);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeApi, UbseQueryCpuTopoWhenSuccess)
{
    std::vector<CliPhysicalLink> cpuTopoLinks;
    cpuTopoLinks.emplace_back("0", "0", "0", "eth0", "1", "0", "0", "eth0", "0/0/0-1/0/0");
    MOCKER_CPP(GetCpuTopoLink).stubs().with(outBound(cpuTopoLinks)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseQueryCpuTopo(req, ctx);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeApi, UbseQueryCpuTopoWhenEmpty)
{
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseQueryCpuTopo(req, ctx);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeApi, UbseQueryCpuTopoWhenFailed)
{
    MOCKER_CPP(GetCpuTopoLink).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseQueryCpuTopo(req, ctx);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeApi, UbseQueryCpuTopoWhenSerialFailed)
{
    MOCKER_CPP(GetCpuTopoLink).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SerializePhysicalLinks).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseQueryCpuTopo(req, ctx);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeApi, UbseQueryCpuTopoWhenApiServerModuleIsNull)
{
    std::shared_ptr<UbseApiServerModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullModule));
    uint8_t dummyVal = 0;
    ::api::server::UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    ::api::server::UbseRequestContext ctx{};
    auto ret = UbseNodeApi::UbseQueryCpuTopo(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, GetCpuTopoLink)
{
    MOCKER_CPP(&UbseNodeController::UbseGetDirConnectInfo).stubs().will(invoke(MockUbseGetDirConnectInfo));
    std::unordered_map<std::string, UbseNodeInfo> allNodeInfoMap;
    allNodeInfoMap["0"] =
        UbseNodeInfo{.nodeId = "0",
                     .slotId = 0,
                     .hostName = "node0",
                     .cpuInfos = {{UbseCpuLocation{"0", 1}, UbseCpuInfo{.slotId = 0, .socketId = 2, .chipId = "1"}}},
                     .clusterState = UbseNodeClusterState::UBSE_NODE_UNKNOWN};
    allNodeInfoMap["1"] =
        UbseNodeInfo{.nodeId = "1",
                     .slotId = 1,
                     .hostName = "node1",
                     .cpuInfos = {{UbseCpuLocation{"1", 1}, UbseCpuInfo{.slotId = 1, .socketId = 2, .chipId = "1"}}},
                     .clusterState = UbseNodeClusterState::UBSE_NODE_SMOOTHING};
    allNodeInfoMap["2"] =
        UbseNodeInfo{.nodeId = "2",
                     .slotId = 2,
                     .hostName = "node2",
                     .cpuInfos = {{UbseCpuLocation{"2", 1}, UbseCpuInfo{.slotId = 2, .socketId = 2, .chipId = "1"}}},
                     .clusterState = UbseNodeClusterState::UBSE_NODE_WORKING};
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(allNodeInfoMap));
    std::vector<CliPhysicalLink> cpuTopoLinks;
    GetCpuTopoLink(cpuTopoLinks);
    ASSERT_EQ(cpuTopoLinks.size(), 4);
}

TEST_F(TestUbseNodeApi, GetCpuTopoLinkWhenNodesUnknown)
{
    MOCKER_CPP(&UbseNodeController::UbseGetDirConnectInfo).stubs().will(invoke(MockUbseGetDirConnectInfo));
    std::unordered_map<std::string, UbseNodeInfo> allNodeInfoMap;
    allNodeInfoMap["0"] =
        UbseNodeInfo{.nodeId = "0",
                     .slotId = 0,
                     .hostName = "node0",
                     .cpuInfos = {{UbseCpuLocation{"0", 1}, UbseCpuInfo{.slotId = 0, .socketId = 2, .chipId = "1"}}},
                     .clusterState = UbseNodeClusterState::UBSE_NODE_UNKNOWN};
    allNodeInfoMap["1"] =
        UbseNodeInfo{.nodeId = "1",
                     .slotId = 1,
                     .hostName = "node1",
                     .cpuInfos = {{UbseCpuLocation{"1", 1}, UbseCpuInfo{.slotId = 1, .socketId = 2, .chipId = "1"}}},
                     .clusterState = UbseNodeClusterState::UBSE_NODE_WORKING};
    allNodeInfoMap["2"] =
        UbseNodeInfo{.nodeId = "2",
                     .slotId = 2,
                     .hostName = "node2",
                     .cpuInfos = {{UbseCpuLocation{"2", 1}, UbseCpuInfo{.slotId = 2, .socketId = 2, .chipId = "1"}}},
                     .clusterState = UbseNodeClusterState::UBSE_NODE_UNKNOWN};
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(allNodeInfoMap));
    std::vector<CliPhysicalLink> cpuTopoLinks;
    GetCpuTopoLink(cpuTopoLinks);
    ASSERT_EQ(cpuTopoLinks.size(), 3);
}
} // namespace ubse::node::api::ut
