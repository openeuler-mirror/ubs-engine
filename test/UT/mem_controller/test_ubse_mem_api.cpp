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

#include "test_ubse_mem_api.h"

#include <ubse_node_api.h>
#include <ubse_node_api_convert.h>

#include <mockcpp/mockcpp.hpp>

#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_mem_api.cpp"
#include "ubse_mem_api.h"
#include "ubse_mem_buffer_convert.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_mem_controller_dispatcher.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_util.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_query_api.h"
#include "ubse_thread_pool_module.h"

namespace ubse::mem_controller::ut {
using namespace usbe::mem::api;
using namespace api::server;
using namespace context;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::controller;
using namespace ubse::nodeController;
using namespace ubse::election;
using namespace ubse::task_executor;
using namespace ubse::mem::util;
void TestUbseMemApi::SetUp()
{
    Test::SetUp();
}
void TestUbseMemApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemApi, Register)
{
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemApi::Register(), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::Register(), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::Register(), UBSE_OK);
}

TEST_F(TestUbseMemApi, UbseSeverNodeNumaGet)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_NE(node::api::UbseNodeApi::UbseServerNodeNumaMemGet(req, context), UBSE_OK);

    MOCKER(UbseNodeNumaMemGet).stubs().will(returnValue(UBSE_OK));

    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    MOCKER_CPP(&node::api::UbseNumaInfoListPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(node::api::UbseNodeApi::UbseServerNodeNumaMemGet(req, context), UBSE_OK);

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(node::api::UbseNodeApi::UbseServerNodeNumaMemGet(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(node::api::UbseNodeApi::UbseServerNodeNumaMemGet(req, context), UBSE_OK);
    EXPECT_EQ(node::api::UbseNodeApi::UbseServerNodeNumaMemGet(req, context), UBSE_OK);
    if (req.buffer != nullptr) {
        delete[] req.buffer;
        req.buffer = nullptr;
    }
}

TEST_F(TestUbseMemApi, UbseServerFdGet)
{
    UbseIpcMessage req{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    UbseRequestContext context{};
    MOCKER_CPP(&UbseMemFdGet).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemFdGetDispatch(req, context), UBSE_OK);

    MOCKER_CPP(&UbseMemFdDescPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemFdGetDispatch(req, context), UBSE_OK);

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemFdGetDispatch(req, context), UBSE_OK);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemFdGetDispatch(req, context), UBSE_OK);
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemFdGetDispatch(req, context), UBSE_OK);
    if (req.buffer != nullptr) {
        delete[] req.buffer;
        req.buffer = nullptr;
    }
}

TEST_F(TestUbseMemApi, UbseServerFdList)
{
    UbseIpcMessage req{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    UbseRequestContext context{};
    MOCKER_CPP(&UbseMemFdList).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdListDispatch(req, context), UBSE_ERROR);

    MOCKER_CPP(&UbseMemFdDescListPack)
        .stubs()
        .will(returnValue(UBSE_ERROR_SERIALIZE_FAILED))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdListDispatch(req, context),
              UBSE_ERROR_SERIALIZE_FAILED);

    MOCKER_CPP(&UbseMemFdDescListPack)
        .stubs()
        .will(returnValue(UBSE_ERROR_SERIALIZE_FAILED))
        .then(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemFdListDispatch(req, context), UBSE_OK);

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdListDispatch(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdListDispatch(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdListDispatch(req, context), UBSE_OK);
    if (req.buffer != nullptr) {
        delete[] req.buffer;
        req.buffer = nullptr;
    }
}

TEST_F(TestUbseMemApi, UbseServerNumaList)
{
    UbseIpcMessage req{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    UbseRequestContext context{};
    MOCKER_CPP(&UbseMemNumaList).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemNumaListDispatch(req, context), UBSE_ERROR);

    MOCKER_CPP(&UbseMemFdDescListPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemNumaListDispatch(req, context), UBSE_OK);

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemNumaListDispatch(req, context), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemNumaListDispatch(req, context), UBSE_OK);
    if (req.buffer != nullptr) {
        delete[] req.buffer;
        req.buffer = nullptr;
    }
}

TEST_F(TestUbseMemApi, UbseServerNumaGet)
{
    UbseIpcMessage req{};
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    UbseRequestContext context{};
    MOCKER_CPP(&UbseMemNumaGet).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemNumaGetDispatch(req, context), UBSE_OK);

    MOCKER_CPP(&UbseMemNumaDescPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemNumaGetDispatch(req, context), UBSE_OK);

    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemNumaGetDispatch(req, context), UBSE_OK);

    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemNumaGetDispatch(req, context), UBSE_OK);
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemNumaGetDispatch(req, context), UBSE_OK);
    if (req.buffer != nullptr) {
        delete[] req.buffer;
        req.buffer = nullptr;
    }
}

void SetupTestObjects(UbseMemFdBorrowReq &fdBorrowReq, UbseMemNumaBorrowReq &numaBorrowReq,
                      UbseMemFdBorrowImportObj &fdImportObj, UbseMemFdBorrowExportObj &fdExportObj,
                      UbseMemNumaBorrowImportObj &numaImportObj, UbseMemNumaBorrowExportObj &numaExportObj,
                      NodeMemDebtInfoMap &nodeDebtInfoMap)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};

    // 初始化 fdBorrowReq
    fdBorrowReq.name = "test";
    fdBorrowReq.requestNodeId = "1";
    fdBorrowReq.importNodeId = "1";
    fdBorrowReq.size = 128;
    fdBorrowReq.udsInfo = udsInfo;

    // 初始化 numaBorrowReq
    numaBorrowReq.name = "test";
    numaBorrowReq.requestNodeId = "1";
    numaBorrowReq.importNodeId = "1";
    numaBorrowReq.size = 128;
    numaBorrowReq.udsInfo = udsInfo;

    // 构造 fdImportObj
    fdImportObj.req = fdBorrowReq;
    UbseMemDebtNumaInfo fdImportNmaInfo{.nodeId = "1", .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo fdExportNmaInfo{.nodeId = "2", .socketId = 0, .numaId = 0, .size = 128};
    fdImportObj.algoResult.exportNumaInfos.emplace_back(fdExportNmaInfo);
    fdImportObj.algoResult.importNumaInfos.emplace_back(fdImportNmaInfo);
    fdImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    // 构造 fdExportObj
    fdExportObj.req = fdBorrowReq;
    fdExportObj.algoResult = fdImportObj.algoResult;
    fdExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 构造 numaImportObj
    numaImportObj.req = numaBorrowReq;
    UbseMemDebtNumaInfo numaImportNmaInfo{.nodeId = "1", .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo numaExportNmaInfo{.nodeId = "2", .socketId = 0, .numaId = 0, .size = 128};
    numaImportObj.algoResult.exportNumaInfos.emplace_back(numaExportNmaInfo);
    numaImportObj.algoResult.importNumaInfos.emplace_back(numaImportNmaInfo);
    numaImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    // 构造 numaExportObj
    numaExportObj.req = numaBorrowReq;
    numaExportObj.algoResult = numaImportObj.algoResult;
    numaExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 填充 nodeDebtInfoMap
    nodeDebtInfoMap[fdExportObj.algoResult.exportNumaInfos[0].nodeId].fdExportObjMap[fdBorrowReq.name] = fdExportObj;
    nodeDebtInfoMap[fdBorrowReq.importNodeId].fdImportObjMap[fdBorrowReq.name] = fdImportObj;

    nodeDebtInfoMap[numaExportObj.algoResult.importNumaInfos[0].nodeId].numaExportObjMap[numaBorrowReq.name] =
        numaExportObj;
    nodeDebtInfoMap[numaBorrowReq.importNodeId].numaImportObjMap[numaBorrowReq.name] = numaImportObj;
}

TEST_F(TestUbseMemApi, UbseClusterList)
{
    // 准备节点信息
    ubse::nodeController::UbseNodeInfo nodeInfo1{.nodeId = "1", .slotId = 1, .hostName = "host1"};
    ubse::nodeController::UbseNodeInfo nodeInfo2{.nodeId = "2", .slotId = 2, .hostName = "host2"};
    ubse::nodeController::UbseNodeInfo staticNodeInfo1{.nodeId = "3", .slotId = 3, .hostName = "staticHost1"};
    ubse::nodeController::UbseNodeInfo staticNodeInfo2{.nodeId = "4", .slotId = 4, .hostName = "staticHost2"};

    // 模拟动态节点信息
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> dynamicNodes;
    dynamicNodes[nodeInfo1.nodeId] = nodeInfo1;
    dynamicNodes[nodeInfo2.nodeId] = nodeInfo2;

    // 模拟静态节点信息
    std::vector<ubse::nodeController::UbseNodeInfo> staticNodes;
    staticNodes.push_back(staticNodeInfo1);
    staticNodes.push_back(staticNodeInfo2);

    // 设置模拟对象
    auto &nodeController = ubse::nodeController::UbseNodeController::GetInstance();
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(dynamicNodes));
    MOCKER_CPP(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(staticNodes));

    // 调用被测试的方法
    std::vector<ubse::nodeController::UbseNodeInfo> nodeList;
    UbseClusterList(nodeList);

    // 验证结果
    ASSERT_EQ(nodeList.size(), 4);
    EXPECT_EQ(nodeList[0].slotId, 1);
    EXPECT_EQ(nodeList[1].slotId, 2);
    EXPECT_EQ(nodeList[2].slotId, 3);
    EXPECT_EQ(nodeList[3].slotId, 4);
}

void MockUbseClusterList(std::vector<ubse::nodeController::UbseNodeInfo> &nodeList)
{
    ubse::nodeController::UbseNodeInfo info;
    nodeList.emplace_back(info);
}

TEST_F(TestUbseMemApi, UbseCheckMemoryStatus)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_ERROR_NULLPTR);
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    MOCKER_CPP(&UbseClusterList).stubs().will(invoke(MockUbseClusterList));
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_ERROR_NULLPTR);

    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_OK);
    if (req.buffer != nullptr) {
        delete[] req.buffer;
        req.buffer = nullptr;
    }
}

TEST_F(TestUbseMemApi, UbseNodeMemConfigHandle)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseNodeMemConfigHandle(req, context), UBSE_ERROR_NULLPTR);
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos;
    ubse::nodeController::UbseNodeInfo node1;
    node1.nodeId = "1";
    node1.slotId = 1;
    node1.hostName = "1";
    ubse::nodeController::UbseNodeInfo node2;
    node2.nodeId = "2";
    node2.slotId = 2;
    node1.hostName = "2";
    nodeInfos["1"] = node1;
    nodeInfos["2"] = node2;
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    req.length = sizeof(uint32_t);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    EXPECT_EQ(UbseMemApi::UbseNodeMemConfigHandle(req, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseNodeMemConfigHandle(req, context), UBSE_OK);
    delete[] req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseNumaStatusHandler)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_NE(UbseMemApi::UbseNumaStatusHandler(req, context), UBSE_OK);
    std::vector<ubse::mem::account::UbseNumaNodeInfo> numaInfoList{};
    ubse::mem::account::UbseNumaNodeInfo numaNodeInfo;
    numaNodeInfo.mMemTotal = 1024;
    numaNodeInfo.mMemFree = 128;
    numaNodeInfo.nodeId = "1";
    numaNodeInfo.hostName = "1";
    numaInfoList.push_back(numaNodeInfo);
    MOCKER(ubse::mem::account::UbseAllNumaInfo).stubs().with(outBound(numaInfoList)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseNumaStatusHandler(req, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseNumaStatusHandler(req, context), UBSE_OK);
}

TEST_F(TestUbseMemApi, UbseBorrowDetailsFetchDebtHandle)
{
    // 场景 1: 请求信息为空
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsFetchDebtHandle(req, context), UBSE_ERROR_NULLPTR);

    // 场景 2: 获取主节点信息失败
    UbseMemDebtInfoPartialFetchReq ubseMemDebtInfoPartialFetchReq;
    DebtFetchInfo debtFetchInfo{};
    ubseMemDebtInfoPartialFetchReq.SetUbseMemDebtFetchInfo(debtFetchInfo);
    ubseMemDebtInfoPartialFetchReq.Serialize();
    req.buffer = ubseMemDebtInfoPartialFetchReq.SerializedData();
    req.length = ubseMemDebtInfoPartialFetchReq.SerializedDataSize();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsFetchDebtHandle(req, context), UBSE_ERROR);

    // 场景 3: 发送 RPC 请求失败
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseBorrowDetailsSendRpcAndFetchResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsFetchDebtHandle(req, context), UBSE_ERROR);

    // 场景 4: 发送响应失败
    MOCKER_CPP(&UbseBorrowDetailsSendRpcAndFetchResponse).reset();
    MOCKER_CPP(&UbseBorrowDetailsSendRpcAndFetchResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseBorrowDetailsSendResponseToClient).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsFetchDebtHandle(req, context), UBSE_ERROR);

    // 场景 5: 成功处理请求
    MOCKER_CPP(&UbseBorrowDetailsSendResponseToClient).reset();
    MOCKER_CPP(&UbseBorrowDetailsSendResponseToClient).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseBorrowDetailsFetchDebtHandle(req, context), UBSE_OK);
}

TEST_F(TestUbseMemApi, TestUbseMemApiUbseCheckMemoryStatus)
{
    // 场景 1: 请求信息为空
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_ERROR_NULLPTR);

    // 场景 2: 获取 API 服务模块失败
    req.buffer = new (std::nothrow) uint8_t[1];
    req.length = 1;
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_ERROR_NULLPTR);

    // 场景 3: 发送响应失败
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_ERROR);

    // 场景 4: 成功处理请求
    MOCKER_CPP(&UbseApiServerModule::SendResponse).reset();
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseCheckMemoryStatus(req, context), UBSE_OK);

    delete[] req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, TestUbseMemApiUbseNodeMemConfigHandle)
{
    // 场景 1: 请求信息为空
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseNodeMemConfigHandle(req, context), UBSE_ERROR_NULLPTR);

    // 场景 2: 获取 API 服务模块失败
    req.buffer = new (std::nothrow) uint8_t[1];
    req.length = 1;
    EXPECT_EQ(UbseMemApi::UbseNodeMemConfigHandle(req, context), UBSE_ERROR_NULLPTR);

    // 场景 3: 发送响应失败
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseNodeMemConfigHandle(req, context), UBSE_ERROR);

    // 场景 4: 成功处理请求
    MOCKER_CPP(&UbseApiServerModule::SendResponse).reset();
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseNodeMemConfigHandle(req, context), UBSE_OK);

    delete[] req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, TestUbseMemApiUbseNumaStatusHandler)
{
    // 场景 1: 请求信息为空
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseNumaStatusHandler(req, context), UBSE_ERROR_NULLPTR);

    // 场景 2: 获取 NUMA 信息失败
    req.buffer = new (std::nothrow) uint8_t[1];
    req.length = 1;
    MOCKER(ubse::mem::account::UbseAllNumaInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseNumaStatusHandler(req, context), UBSE_ERROR);

    // 场景 3: 获取 API 服务模块失败
    MOCKER(ubse::mem::account::UbseAllNumaInfo).reset();
    MOCKER(ubse::mem::account::UbseAllNumaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseNumaStatusHandler(req, context), UBSE_ERROR_NULLPTR);

    // 场景 4: 发送响应失败
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseNumaStatusHandler(req, context), UBSE_ERROR);

    // 场景 5: 成功处理请求
    MOCKER_CPP(&UbseApiServerModule::SendResponse).reset();
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseNumaStatusHandler(req, context), UBSE_OK);

    delete[] req.buffer;
    req.buffer = nullptr;
}

TEST_F(TestUbseMemApi, QueryNumaStateHandler)
{
    // 场景 1: 请求信息为空
    UbseIpcMessage request{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::QueryNumaStateHandler(request, context), UBSE_ERROR);

    // 场景 2: 反序列化失败
    request.buffer = new (std::nothrow) uint8_t[1];
    request.length = 1;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(true));
    MOCKER_CPP(&ubse::mem::util::CheckName).stubs().will(returnValue(false));
    EXPECT_EQ(UbseMemApi::QueryNumaStateHandler(request, context), UBSE_ERROR);

    // 场景 3: 名称无效
    MOCKER_CPP(&ubse::mem::util::CheckName).reset();
    MOCKER_CPP(&ubse::mem::util::CheckName).stubs().will(returnValue(true));
    MOCKER(ubse::mem::controller::UbseMemNumaGet)
        .stubs()
        .will(returnValue(UBSE_ERR_NOT_EXIST));
    EXPECT_EQ(UbseMemApi::QueryNumaStateHandler(request, context), UBSE_IPC_ERROR_QUERY_NUMA_NOT_EXIST);

    // 场景 4: 获取 NUMA 信息失败
    MOCKER(ubse::mem::controller::UbseMemNumaGet).reset();
    MOCKER(ubse::mem::controller::UbseMemNumaGet).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::QueryNumaStateHandler(request, context), UBSE_IPC_ERROR_QUERY_STATE_FAILED);

    // 场景 5: 获取模块失败
    ubse::mem::def::UbseMemNumaDesc memNumaDesc{};
    MOCKER(ubse::mem::controller::UbseMemNumaGet).reset();
    MOCKER(ubse::mem::controller::UbseMemNumaGet).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::QueryNumaStateHandler(request, context), UBSE_ERROR_NULLPTR);

    // 场景 6: 发送响应失败
    MOCKER(ubse::mem::controller::UbseMemNumaGet).reset();
    MOCKER(ubse::mem::controller::UbseMemNumaGet).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::QueryNumaStateHandler(request, context), UBSE_ERROR);

    // 场景 7: 成功处理请求
    MOCKER_CPP(&UbseApiServerModule::SendResponse).reset();
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::QueryNumaStateHandler(request, context), UBSE_OK);

    delete[] request.buffer;
    request.buffer = nullptr;
}

TEST_F(TestUbseMemApi, UbseBorrowDetailsSendRpcAndFetchResponse)
{
    ubse::election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";

    // 场景 1: 通信模块未初始化
    auto ubseComModule = std::make_shared<UbseComModule>();
    EXPECT_EQ(UbseBorrowDetailsSendRpcAndFetchResponse(masterInfo, nullptr, nullptr), UBSE_ERROR_MODULE_LOAD_FAILED);

    // 场景 2: RPC 发送失败
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtInfoPartialFetchReqPtr, UbseMemDebtInfoPartialFetchResPtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseBorrowDetailsSendRpcAndFetchResponse(masterInfo, nullptr, nullptr), UBSE_ERROR);

    // 场景 3: RPC 发送成功
    MOCKER(func).reset();
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseBorrowDetailsSendRpcAndFetchResponse(masterInfo, nullptr, nullptr), UBSE_OK);
}

TEST_F(TestUbseMemApi, UbseBorrowDetailsSendResponseToClient)
{
    // 场景 1: 响应数据为空
    UbseMemDebtInfoPartialFetchResPtr ubseResponsePtr = new (std::nothrow) UbseMemDebtInfoPartialFetchRes();
    UbseRequestContext context{};
    EXPECT_EQ(UbseBorrowDetailsSendResponseToClient(ubseResponsePtr, context), UBSE_ERROR_NULLPTR);

    // 场景 2: 获取 API 服务模块失败

    uint8_t *data = new (std::nothrow) uint8_t[1];
    uint32_t size = 1;
    ubseResponsePtr = new (std::nothrow) UbseMemDebtInfoPartialFetchRes(data, size);
    EXPECT_EQ(UbseBorrowDetailsSendResponseToClient(ubseResponsePtr, context), UBSE_ERROR_NULLPTR);

    // 场景 3: 发送响应失败
    auto ubseApiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(ubseApiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseBorrowDetailsSendResponseToClient(ubseResponsePtr, context), UBSE_ERROR);

    // 场景 4: 发送响应成功
    MOCKER_CPP(&UbseApiServerModule::SendResponse).reset();
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseBorrowDetailsSendResponseToClient(ubseResponsePtr, context), UBSE_OK);

    delete[] data;
}

TEST_F(TestUbseMemApi, UbseCliShmAttachDispatch_WhenInvalidParam)
{
    // 场景 1: 请求信息为空
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_ERR_INTERNAL);

    // 场景 2: 反序列化失败
    req.length = sizeof(std::string);
    req.buffer = new (std::nothrow) uint8_t[req.length];
    EXPECT_NE(req.buffer, nullptr);
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_ERR_INTERNAL);
    delete[] req.buffer;
    req.buffer = nullptr;

    // 场景 3: 名称无效
    std::string testName = "test_shm%";
    UbseSerialization serialization;
    serialization << testName;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_ERR_INTERNAL);
}
TEST_F(TestUbseMemApi, UbseCliShmAttachDispatch_WhenExecutorFailed)
{
    // 场景 1: 获取当前节点信息失败
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::string testName = "test_shm";
    UbseSerialization serialization;
    serialization << testName;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_ERROR);

    // 场景 2: 获取executor失败
    MOCKER(UbseGetCurrentNodeInfo).reset();
    UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    UbseTaskExecutorPtr nullExecutor = nullptr;
    MOCKER(ubse::mem::util::GetExecutor).stubs().will(returnValue(nullExecutor));
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_ERROR);

    // 场景 3: 提交任务失败
    auto executor = UbseTaskExecutor::Create("ubseMemController", 1, 1);
    MOCKER(ubse::mem::util::GetExecutor).reset();
    MOCKER(ubse::mem::util::GetExecutor).stubs().will(returnValue(executor));
    MOCKER_CPP(&UbseTaskExecutor::Execute, bool(UbseTaskExecutor::*)(const std::function<void()> &))
        .stubs()
        .will(returnValue(false));
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_ERROR);
}

mem::def::UbseMemShmDesc BuildShmDesc()
{
    mem::def::UbseMemShmDesc desc;
    desc.name = "shm";
    desc.totalMemSize = 128 * 1024 * 1024; // 128M, 1024字节单位转换
    desc.unitSize = 128 * 1024 * 1024;     // 128M, 1024字节单位转换
    desc.exportNode = UbseNode{.slotId = 2};
    // 初始化importDesc向量
    mem::def::UbseMemShmImportDesc importDesc1;
    importDesc1.memIds = {1, 2, 3};
    importDesc1.importNode = UbseNode{.slotId = 1};
    importDesc1.state = ubse::mem::controller::UbseMemStage::UBSE_EXIST;
    desc.importDesc.push_back(importDesc1);
    desc.state = ubse::mem::controller::UbseMemStage::UBSE_EXIST;
    return desc;
}

TEST_F(TestUbseMemApi, UbseCliShmAttachDispatch_WhenAsyncFailed)
{
    // apiModule获取失败
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::string testName = "test_shm";
    UbseSerialization serialization;
    serialization << testName;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();
    UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    auto executor = UbseTaskExecutor::Create("ubseMemController", 1, 1);
    executor->Start();
    MOCKER(ubse::mem::util::GetExecutor).stubs().will(returnValue(executor));
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_OK);
    sleep(1);

    // attach失败
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    MOCKER_CPP(agent::UbseMemShareAttach).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_OK);
    sleep(1);

    // UbseMemShmGet失败
    auto shmDesc = BuildShmDesc();
    MOCKER_CPP(UbseMemShmGet)
        .stubs()
        .with(_, outBound(shmDesc))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_OK);
    sleep(1);

    // 序列化失败
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_OK);
    sleep(1);

    // 正常调用
    MOCKER_CPP(&UbseSerialization::Check).reset();
    EXPECT_EQ(UbseMemApi::UbseCliShmAttachDispatch(req, context), UBSE_OK);
    sleep(1);
    executor->Stop();
}

// UbseCliShmCreateDispatch测试
TEST_F(TestUbseMemApi, UbseCliShmCreateDispatch_WhenInvalidParam)
{
    // 场景 1: 请求信息为空
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_ERR_INTERNAL);

    // 场景 2: 反序列化失败
    req.buffer = new (std::nothrow) uint8_t[1];
    req.length = 1;
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_ERR_INTERNAL);
    delete[] req.buffer;
    req.buffer = nullptr;

    // 场景 3: 名称无效
    std::string testName = "test_shm%";
    uint64_t size = 1024 * 1024; // 1MB
    uint32_t nodeCnt = 1;
    uint32_t slotId = 1;

    UbseSerialization serialization;
    serialization << size << testName << array_len_insert(nodeCnt) << slotId;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();

    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemApi, UbseCliShmCreateDispatch_WhenExecutorFailed)
{
    // 准备有效请求
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::string testName = "test_shm";
    uint64_t size = 1024 * 1024; // 1MB
    uint32_t nodeCnt = 2;
    uint32_t slotId1 = 1;
    uint32_t slotId2 = 2;

    UbseSerialization serialization;
    serialization << size << testName << array_len_insert(nodeCnt) << slotId1 << slotId2;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();

    // 场景 1: 获取当前节点信息失败
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_ERROR);

    // 场景 2: 获取executor失败
    MOCKER(UbseGetCurrentNodeInfo).reset();
    UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    UbseTaskExecutorPtr nullExecutor = nullptr;
    MOCKER(ubse::mem::util::GetExecutor).stubs().will(returnValue(nullExecutor));
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_ERROR);

    // 场景 3: 提交任务失败
    auto executor = UbseTaskExecutor::Create("ubseMemController", 1, 1);
    executor->Start();
    MOCKER(ubse::mem::util::GetExecutor).reset();
    MOCKER(ubse::mem::util::GetExecutor).stubs().will(returnValue(executor));
    MOCKER_CPP(&UbseTaskExecutor::Execute, bool(UbseTaskExecutor::*)(const std::function<void()> &))
        .stubs()
        .will(returnValue(false));
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_ERROR);
}

TEST_F(TestUbseMemApi, UbseCliShmCreateDispatch_WhenAsyncFailed)
{
    // 准备有效请求
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::string testName = "test_shm";
    uint64_t size = 1024 * 1024; // 1MB
    uint32_t nodeCnt = 2;
    uint32_t slotId1 = 1;
    uint32_t slotId2 = 2;

    UbseSerialization serialization;
    serialization << size << testName << array_len_insert(nodeCnt) << slotId1 << slotId2;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();

    UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    auto executor = UbseTaskExecutor::Create("ubseMemController", 1, 1);
    executor->Start();
    MOCKER(ubse::mem::util::GetExecutor).stubs().will(returnValue(executor));
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_OK);
    sleep(1);

    // apiModule获取失败
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    auto module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_OK);
    sleep(1);

    // create失败
    MOCKER_CPP(agent::UbseMemShareBorrow).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_OK);
    sleep(1);

    // UbseMemShmGet失败
    auto shmDesc = BuildShmDesc();
    MOCKER_CPP(UbseMemShmGet)
        .stubs()
        .with(_, outBound(shmDesc))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_OK);
    sleep(1);

    // 序列化失败
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false));
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_OK);
    sleep(1);

    // 正常调用
    MOCKER_CPP(&UbseSerialization::Check).reset();
    EXPECT_EQ(UbseMemApi::UbseCliShmCreateDispatch(req, context), UBSE_OK);
    sleep(1);
    executor->Stop();
}

// UbseCliShmDetachDispatch测试
TEST_F(TestUbseMemApi, UbseCliShmDetachDispatch_WhenInvalidParam)
{
    // 场景 1: 请求信息为空
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseCliShmDetachDispatch(req, context), UBSE_ERR_INTERNAL);

    // 场景 2: 反序列化失败
    req.buffer = new (std::nothrow) uint8_t[1];
    req.length = 1;
    EXPECT_EQ(UbseMemApi::UbseCliShmDetachDispatch(req, context), UBSE_ERR_INTERNAL);
    delete[] req.buffer;
    req.buffer = nullptr;

    // 场景 3: 名称无效
    std::string testName = "test_shm%";
    UbseSerialization serialization;
    serialization << testName;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();
    EXPECT_EQ(UbseMemApi::UbseCliShmDetachDispatch(req, context), UBSE_ERR_INTERNAL);
}

TEST_F(TestUbseMemApi, UbseCliShmDetachDispatch_WhenExecutorFailed)
{
    // 准备有效请求
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::string testName = "test_shm";
    UbseSerialization serialization;
    serialization << testName;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();

    // 场景 1: 获取当前节点信息失败
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseCliShmDetachDispatch(req, context), UBSE_ERROR);

    // 场景 2: 获取executor失败
    MOCKER(UbseGetCurrentNodeInfo).reset();
    UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    UbseTaskExecutorPtr nullExecutor = nullptr;
    MOCKER(ubse::mem::util::GetExecutor).stubs().will(returnValue(nullExecutor));
    EXPECT_EQ(UbseMemApi::UbseCliShmDetachDispatch(req, context), UBSE_ERROR);

    // 场景 3: 提交任务失败
    auto executor = UbseTaskExecutor::Create("ubseMemController", 1, 1);
    executor->Start();
    MOCKER(ubse::mem::util::GetExecutor).reset();
    MOCKER(ubse::mem::util::GetExecutor).stubs().will(returnValue(executor));
    MOCKER_CPP(&UbseTaskExecutor::Execute, bool(UbseTaskExecutor::*)(const std::function<void()> &))
        .stubs()
        .will(returnValue(false));
    EXPECT_EQ(UbseMemApi::UbseCliShmDetachDispatch(req, context), UBSE_ERROR);
}

TEST_F(TestUbseMemApi, UbseCliShmDetachDispatch_WhenAsyncFailed)
{
    // 准备有效请求
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::string testName = "test_shm";
    UbseSerialization serialization;
    serialization << testName;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();

    UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    auto executor = UbseTaskExecutor::Create("ubseMemController", 1, 1);
    executor->Start();
    MOCKER(ubse::mem::util::GetExecutor).stubs().will(returnValue(executor));
    EXPECT_EQ(UbseMemApi::UbseCliShmDetachDispatch(req, context), UBSE_OK);
    sleep(1);

    // apiModule获取失败
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    auto module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemApi::UbseCliShmDetachDispatch(req, context), UBSE_OK);
    sleep(1);

    // detach操作失败
    MOCKER_CPP(agent::UbseMemShareDetach).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemApi::UbseCliShmDetachDispatch(req, context), UBSE_OK);
    sleep(1);
    executor->Stop();
}

// UbseCliShmGetDispatch测试
TEST_F(TestUbseMemApi, UbseCliShmGetDispatch_WhenInvalidParam)
{
    // 场景 1: 请求信息为空
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(UbseMemApi::UbseCliShmGetDispatch(req, context), UBSE_ERR_INTERNAL);

    // 场景 2: 反序列化失败
    req.buffer = new (std::nothrow) uint8_t[1];
    req.length = 1;
    EXPECT_EQ(UbseMemApi::UbseCliShmGetDispatch(req, context), UBSE_ERR_INTERNAL);
    delete[] req.buffer;
    req.buffer = nullptr;

    // 场景 3: 名称无效
    std::string testName = "test_shm%";
    UbseSerialization serialization;
    serialization << testName;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();
    EXPECT_EQ(UbseMemApi::UbseCliShmGetDispatch(req, context), UBSE_ERR_INTERNAL);
}

TEST_F(TestUbseMemApi, UbseCliShmGetDispatch_WhenUbseMemShmGetFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::string testName = "test_shm";
    UbseSerialization serialization;
    serialization << testName;
    req.length = serialization.GetLength();
    req.buffer = serialization.GetBuffer();
    UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseMemShmGet).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemApi::UbseCliShmGetDispatch(req, context), UBSE_ERROR);
}
} // namespace ubse::mem_controller::ut
