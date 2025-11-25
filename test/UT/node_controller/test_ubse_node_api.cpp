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

namespace ubse::node::api::ut {
void TestUbseNodeApi::SetUp()
{
    Test::SetUp();
}
void TestUbseNodeApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeApi, UbseSplitStringByHyphenWhenNodeSocketIdIsInvalid)
{
    std::string nodeSocketId = "UbseSplitStringByHyphenWhenSocketIdIsInvalid";
    UbseSerialization ubseSerial;
    auto ret = UbseSplitStringByHyphen(nodeSocketId, ubseSerial);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeApi, UbseSplitStringByHyphenWhenSocketIdIsEmpty)
{
    std::string nodeSocketId = "UbseSplitStringByHyphenWhenSocketIdIsInvalid-";
    UbseSerialization ubseSerial;
    auto ret = UbseSplitStringByHyphen(nodeSocketId, ubseSerial);
    ASSERT_EQ(ret, UBSE_ERROR_NULL_INFO);
}

TEST_F(TestUbseNodeApi, UbseSplitStringByHyphenSuccess)
{
    std::string nodeSocketId = "UbseSplitStringByHyphenWhenSocketIdIsInvalid-1";
    UbseSerialization ubseSerial;
    auto ret = UbseSplitStringByHyphen(nodeSocketId, ubseSerial);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeApi, UbseQueryTopologyInfoHandleWhenGetTopologyInfoFail)
{
    UbseIpcMessage req{};
    UbseRequestContext ctx{};
    req.buffer = nullptr;
    auto ret = UbseNodeApi::UbseQueryTopologyInfoHandle(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseQueryTopologyInfoHandleWhenReqBufferIsNull)
{
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopoInfo;
    MOCKER_CPP(ubse::nodeController::UbseMemGetTopologyInfo)
        .stubs()
        .with(outBound(nodeTopoInfo))
        .will(returnValue(UBSE_ERROR));
    auto ret = UbseNodeApi::UbseQueryTopologyInfoHandle(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeApi, UbseQueryTopologyInfoHandleWhenSplitStringFail)
{
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    std::unordered_map<std::string, std::vector<ubse::nodeController::MemNodeData>> nodeTopology;
    auto data1 = MemNodeData();
    auto data2 = MemNodeData();
    auto data3 = MemNodeData();
    nodeTopology["1-2-3"].emplace_back(data1);
    nodeTopology["1-2"].emplace_back(data2);
    nodeTopology["1-"].emplace_back(data3);
    MOCKER_CPP(ubse::nodeController::UbseMemGetTopologyInfo)
        .stubs()
        .with(outBound(nodeTopology))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSplitStringByHyphen).stubs().will(returnObjectList(UBSE_OK, UBSE_ERROR_NULL_INFO));
    auto ret = UbseNodeApi::UbseQueryTopologyInfoHandle(req, ctx);
    ASSERT_EQ(ret, UBSE_ERROR_NULL_INFO);
}

TEST_F(TestUbseNodeApi, UbseQueryTopologyInfoHandleWhenApiServerModuleIsNull)
{
    auto &ubseCtx = UbseContext::GetInstance();
    auto backupModuleMap = ubseCtx.moduleMap;
    ubseCtx.moduleMap.clear();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    std::unordered_map<std::string, std::vector<ubse::nodeController::MemNodeData>> nodeTopology;
    auto data = MemNodeData();
    nodeTopology["1-2"].emplace_back(data);
    MOCKER_CPP(ubse::nodeController::UbseMemGetTopologyInfo)
        .stubs()
        .with(outBound(nodeTopology))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSplitStringByHyphen).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseNodeApi::UbseQueryTopologyInfoHandle(req, ctx);
    ubseCtx.moduleMap.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseQueryTopologyInfoHandleWhenSendResponseFail)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    std::unordered_map<std::string, std::vector<ubse::nodeController::MemNodeData>> nodeTopology;
    auto data = MemNodeData();
    nodeTopology["1-2"].emplace_back(data);
    MOCKER_CPP(ubse::nodeController::UbseMemGetTopologyInfo)
        .stubs()
        .with(outBound(nodeTopology))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSplitStringByHyphen).stubs().will(returnValue(UBSE_OK));
    auto backupModuleMap = ubseCtx.moduleMap;
    ubseCtx.moduleMap[typeid(::api::server::UbseApiServerModule)] =
        std::make_shared<::api::server::UbseApiServerModule>();
    auto apiServerModule = UbseContext::GetInstance().GetModule<::api::server::UbseApiServerModule>();
    std::unique_ptr<ubse::ipc::UbseIpcServer> nullPtr = nullptr;
    apiServerModule->ipcServer.swap(nullPtr);
    auto ret = UbseNodeApi::UbseQueryTopologyInfoHandle(req, ctx);
    apiServerModule->ipcServer.swap(nullPtr);
    ubseCtx.moduleMap.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeApi, UbseServerNodeGetWhenNodePackFail)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    MOCKER_CPP(UbseNodePack).stubs().will(returnValue(static_cast<uint32_t>(IPC_ERROR_SERIALIZATION_FAILED)));
    auto ret = UbseNodeApi::UbseServerNodeGet(req, ctx);
    ASSERT_EQ(ret, IPC_ERROR_SERIALIZATION_FAILED);
}

TEST_F(TestUbseNodeApi, UbseServerNodeGetWhenApiServerModuleIsNull)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap;
    std::shared_ptr<::api::server::UbseApiServerModule> nullModule = nullptr;
    ubseCtx.moduleMap[typeid(::api::server::UbseApiServerModule)] = nullModule;
    auto ret = UbseNodeApi::UbseServerNodeGet(req, ctx);
    ubseCtx.moduleMap.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerNodeGetWhenSendResponseFail)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap;
    ubseCtx.moduleMap[typeid(::api::server::UbseApiServerModule)] =
        std::make_shared<::api::server::UbseApiServerModule>();
    auto apiServerModule = UbseContext::GetInstance().GetModule<::api::server::UbseApiServerModule>();
    apiServerModule->ipcServer = nullptr;
    auto ret = UbseNodeApi::UbseServerNodeGet(req, ctx);
    ubseCtx.moduleMap.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerNodeListWhenNodeListPackFail)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    MOCKER_CPP(UbseNodeListPack).stubs().will(returnValue(static_cast<uint32_t>(IPC_ERROR_SERIALIZATION_FAILED)));
    auto ret = UbseNodeApi::UbseServerNodeList(req, ctx);
    ASSERT_EQ(ret, IPC_ERROR_SERIALIZATION_FAILED);
}

TEST_F(TestUbseNodeApi, UbseServerNodeListWhenApiServerModuleIsNull)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap;
    std::shared_ptr<::api::server::UbseApiServerModule> nullModule = nullptr;
    ubseCtx.moduleMap[typeid(::api::server::UbseApiServerModule)] = nullModule;
    auto ret = UbseNodeApi::UbseServerNodeList(req, ctx);
    ubseCtx.moduleMap.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerNodeListWhenSendResponseFail)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap;
    ubseCtx.moduleMap[typeid(::api::server::UbseApiServerModule)] =
        std::make_shared<::api::server::UbseApiServerModule>();
    auto apiServerModule = UbseContext::GetInstance().GetModule<::api::server::UbseApiServerModule>();
    apiServerModule->ipcServer = nullptr;
    auto ret = UbseNodeApi::UbseServerNodeList(req, ctx);
    ubseCtx.moduleMap.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerCpuTopoListWhenCpuLinkListPack)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    MOCKER_CPP(UbseCpuLinkListPack).stubs().will(returnValue(static_cast<uint32_t>(IPC_ERROR_SERIALIZATION_FAILED)));
    auto ret = UbseNodeApi::UbseServerCpuTopoList(req, ctx);
    ASSERT_EQ(ret, IPC_ERROR_SERIALIZATION_FAILED);
}

TEST_F(TestUbseNodeApi, UbseServerCpuTopoListWhenApiServerModuleIsNull)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap;
    std::shared_ptr<::api::server::UbseApiServerModule> nullModule = nullptr;
    ubseCtx.moduleMap[typeid(::api::server::UbseApiServerModule)] = nullModule;
    auto ret = UbseNodeApi::UbseServerCpuTopoList(req, ctx);
    ubseCtx.moduleMap.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseServerCpuTopoListWhenSendResponseFail)
{
    auto &ubseCtx = UbseContext::GetInstance();
    uint8_t dummyVal = 0;
    UbseIpcMessage req{.buffer = &dummyVal, .length = sizeof(uint8_t)};
    UbseRequestContext ctx{};
    auto backupModuleMap = ubseCtx.moduleMap;
    ubseCtx.moduleMap[typeid(::api::server::UbseApiServerModule)] =
        std::make_shared<::api::server::UbseApiServerModule>();
    auto apiServerModule = UbseContext::GetInstance().GetModule<::api::server::UbseApiServerModule>();
    apiServerModule->ipcServer = nullptr;
    auto ret = UbseNodeApi::UbseServerCpuTopoList(req, ctx);
    ubseCtx.moduleMap.swap(backupModuleMap);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNodeApi, UbseClusterListWhenNodeInfoIsEmpty)
{
    auto &ubseCtx = UbseContext::GetInstance();
    std::vector<UbseNodeInfo> nodeList;
    auto backupModuleMap = ubseCtx.moduleMap;
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    ubseCtx.moduleMap[typeid(UbseElectionModule)] = nullModule;
    UbseClusterList(nodeList);
    ubseCtx.moduleMap.swap(backupModuleMap);
    ASSERT_EQ(nodeList.empty(), true);
}
}  // namespace ubse::mem_controller::ut