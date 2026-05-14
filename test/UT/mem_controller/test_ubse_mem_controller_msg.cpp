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

#include "test_ubse_mem_controller_msg.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_com.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mem_controller_def_simpo.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_util.h"
#include "ubse_node_controller.h"
#include "ubse_os_util.h"
#include "message/ubse_mem_controller_serial.h"
#include "message/ubse_mem_node_debt_info_conversion.h"
#include "node_mem_debt_info_simpo.h"
#include "ubse_mem_controller_msg.cpp"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::mem::util;
using namespace ubse::election;
using namespace ubse::mem::def;
using namespace ubse::mem::controller::debt;
using namespace ubse::mem::controller::message;
using namespace ubse::context;
using namespace ubse::utils;
using namespace ubse::config;
void TestUbseMemControllerMsg::SetUp()
{
    Test::SetUp();
    NodeMemDebtInfo debt1{{{"name", UbseMemFdBorrowImportObj{}}},    {{"name", UbseMemFdBorrowExportObj{}}},
                          {{"name", UbseMemNumaBorrowImportObj{}}},  {{"name", UbseMemNumaBorrowExportObj{}}},
                          {{"name", UbseMemShareBorrowImportObj{}}}, {{"name", UbseMemShareBorrowExportObj{}}},
                          {{"name", UbseMemAddrBorrowImportObj{}}},  {{"name", UbseMemAddrBorrowExportObj{}}}};
    mockDebtMap["1"] = debt1;
}
void TestUbseMemControllerMsg::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

using namespace ubse::mem::controller::message;
using namespace ubse::mem::serial;
TEST_F(TestUbseMemControllerMsg, CollectLedge)
{
    std::string nodeId = "node1";
    NodeMemDebtInfo info;

    // 场景 1: GetModule 返回 ptr
    EXPECT_EQ(CollectLedge(nodeId, info), UBSE_ERROR_NULLPTR);

    // 场景 3: IsUrma 返回 true，RpcSend 返回错误
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseComBaseBufferMessagePtr, UbseComBaseBufferMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(IsUrma).stubs().will(returnValue(true)); // 模拟IsUrma返回true
    EXPECT_EQ(CollectLedge(nodeId, info), UBSE_ERROR);

    // 场景 4: IsUrma 返回 false，RpcRndvSend 返回错误
    MOCKER(IsUrma).reset();
    MOCKER(IsUrma).stubs().will(returnValue(false)); // 模拟IsUrma返回false

    MOCKER(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(CollectLedge(nodeId, info), UBSE_ERROR);

    // 场景 6: NodeMemDebtInfoSimpo::Deserialize 返回错误
    MOCKER(&UbseComBaseBufferMessage::GetDataLen).reset();
    std::unique_ptr<uint8_t> ptr1 = std::make_unique<uint8_t>(1);
    MOCKER(&UbseComBaseBufferMessage::GetData).stubs().will(returnValue(ptr1.get()));
    MOCKER(&UbseComBaseBufferMessage::GetDataLen).stubs().will(returnValue(1)); // 模拟GetData返回ptr
    MOCKER_CPP(&NodeMemDebtInfoDeserialize).stubs().will(returnValue(false));
    EXPECT_EQ(CollectLedge(nodeId, info), UBSE_ERROR);
}

UbseResult MockErrorSerial(UbseMemLedgerRespSerial* _this)
{
    return UBSE_ERROR;
}

UbseResult MockSuccessSerial(UbseMemLedgerRespSerial* _this)
{
    return UBSE_OK;
}

TEST_F(TestUbseMemControllerMsg, CollectLedgeHandler)
{
    std::string nodeId = "1";
    MOCKER(&GetCurNodeId).stubs().will(returnValue(nodeId));
    UbseByteBuffer req{};
    UbseByteBuffer resp{};

    MOCKER(CreateRespBuffer).stubs().will(returnValue(UBSE_OK));
    UbseMemLedgerRespSerial serial{};
    MOCKER_CPP_VIRTUAL(serial, &UbseMemLedgerRespSerial::Serialize)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(CollectLedgeHandler(req, resp), UBSE_ERROR);
    EXPECT_EQ(CollectLedgeHandler(req, resp), UBSE_OK);

    MOCKER_CPP_VIRTUAL(serial, &UbseMemLedgerRespSerial::Serialize).reset();
    MOCKER_CPP_VIRTUAL(serial, &UbseMemLedgerRespSerial::Serialize)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    nodeController::UbseNodeInfo node{};
    node.nodeId = nodeId;
    MOCKER(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(CollectLedgeHandler(req, resp), UBSE_ERROR);
    EXPECT_EQ(CollectLedgeHandler(req, resp), UBSE_OK);

    node.localState = UbseNodeLocalState::UBSE_NODE_READY;
    MOCKER(&nodeController::UbseNodeController::GetCurNode).reset();
    MOCKER(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(CollectLedgeHandler(req, resp), UBSE_OK);
    SafeDeleteArray(resp.data);
}

TEST_F(TestUbseMemControllerMsg, QueryFdImportObj)
{
    std::string nodeId = "1";
    std::string name = "name";
    UbseMemFdBorrowImportObj info{};
    EXPECT_EQ(QueryFdImportObj(nodeId, name, info, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryFdImportObjHandler)
{
    std::string nodeId = "1";
    MOCKER(&GetCurNodeId).stubs().will(returnValue(nodeId));
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    EXPECT_EQ(QueryFdImportObjHandler(req, resp), UBSE_ERROR);

    nodeController::UbseNodeInfo node{};
    node.nodeId = nodeId;
    MOCKER(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryFdImportObjHandler(req, resp), UBSE_ERROR);

    node.localState = UbseNodeLocalState::UBSE_NODE_READY;
    MOCKER(&nodeController::UbseNodeController::GetCurNode).reset();
    MOCKER(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryFdImportObjHandler(req, resp), UBSE_ERROR);

    req.len = 100;
    req.data = new uint8_t[100];
    MOCKER(&mem::serial::UbseMemFdBorrowImportObjDeserialization).stubs().will(returnValue(true));
    EXPECT_EQ(QueryFdImportObjHandler(req, resp), UBSE_OK);
    SafeDeleteArray(req.data);
    SafeDeleteArray(resp.data);
}

TEST_F(TestUbseMemControllerMsg, QueryNumaImportObj)
{
    std::string nodeId = "1";
    std::string name = "name";
    UbseMemNumaBorrowImportObj info{};
    EXPECT_EQ(QueryNumaImportObj(nodeId, name, info, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryNumaImportObjHandler)
{
    std::string nodeId = "1";
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    MOCKER(&GetCurNodeId).stubs().will(returnValue(nodeId));
    EXPECT_EQ(QueryNumaImportObjHandler(req, resp), UBSE_ERROR);

    nodeController::UbseNodeInfo node{};
    node.nodeId = nodeId;
    MOCKER(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryNumaImportObjHandler(req, resp), UBSE_ERROR);

    node.localState = UbseNodeLocalState::UBSE_NODE_READY;
    MOCKER(&nodeController::UbseNodeController::GetCurNode).reset();
    MOCKER(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryNumaImportObjHandler(req, resp), UBSE_ERROR);

    mem::serial::UbseSerialization out;
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    mem::serial::UbseMemNumaBorrowImportObjSerialization(out, importObj);
    UbseByteBuffer req1{};
    UbseByteBuffer resp1{};
    req1.data = out.GetBuffer();
    req1.len = out.GetLength();
    AddNumaImport(importObj);
    EXPECT_EQ(QueryNumaImportObjHandler(req1, resp1), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryAddrImportObjHandler)
{
    std::string nodeId = "1";
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    MOCKER(&GetCurNodeId).stubs().will(returnValue(nodeId));
    EXPECT_EQ(QueryAddrImportObjHandler(req, resp), UBSE_ERROR);

    nodeController::UbseNodeInfo node{};
    node.nodeId = nodeId;
    MOCKER(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryAddrImportObjHandler(req, resp), UBSE_ERROR);

    node.localState = UbseNodeLocalState::UBSE_NODE_READY;
    MOCKER(&nodeController::UbseNodeController::GetCurNode).reset();
    MOCKER(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    EXPECT_EQ(QueryAddrImportObjHandler(req, resp), UBSE_ERROR);

    mem::serial::UbseSerialization out;
    UbseMemAddrBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    mem::serial::UbseMemAddrBorrowImportObjSerialization(out, importObj);
    req.data = out.GetBuffer();
    req.len = out.GetLength();
    AddAddrImport(importObj);
    EXPECT_EQ(QueryAddrImportObjHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryAddrImportObj)
{
    std::string nodeId = "1";
    std::string name = "name";
    UbseMemAddrBorrowImportObj info;
    EXPECT_EQ(QueryAddrImportObj(nodeId, name, info, mockDebtMap), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, PreOnLineRequest)
{
    std::string nodeId = "1";
    PreOnLineReq req;
    SocketCnaInfo cna;
    cna.importNodeId = "1";
    cna.exportNodeId = "2";
    cna.dcna = 10;
    cna.exportSocketId = 216;
    cna.deid = 1;
    cna.importSocketId = 216;
    cna.marId = 1;
    cna.seid = 1;
    req.cnas.push_back(cna);
    req.taskName = "task";
    EXPECT_NE(PreOnLineRequest(nodeId, req), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, PreOnLineHandler)
{
    PreOnLineReq req;
    SocketCnaInfo cna;
    cna.importNodeId = "1";
    cna.exportNodeId = "2";
    cna.dcna = 10;
    cna.exportSocketId = 216;
    cna.deid = 1;
    cna.importSocketId = 216;
    cna.marId = 1;
    cna.seid = 1;
    req.cnas.push_back(cna);
    req.taskName = "task";
    UbseByteBuffer request;
    UbseByteBuffer resp;
    SerializePreOnLine(req, request.data, request.len);
    MOCKER(DeSerializePreOnLine).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(PreOnLineHandler(request, resp), UBSE_OK);
    MOCKER(DeSerializePreOnLine).reset();
    EXPECT_EQ(PreOnLineHandler(request, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, PreOnLineReply)
{
    PreOnLineResp resp{.taskName = "task", .operateNode = "1", .ret = 0};
    EXPECT_NE(PreOnLineReply(resp), UBSE_OK);

    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(PreOnLineReply(resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, PreOnLineReplyHandler)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    PreOnLineResp res{.taskName = "task", .operateNode = "1", .ret = 0};
    SerializePreOnlineResp(res, req.data, req.len);
    MOCKER(DeSerializePreOnLineResp).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_OK, PreOnLineReplyHandler(req, resp));

    MOCKER(DeSerializePreOnLineResp).reset();
    EXPECT_EQ(UBSE_OK, PreOnLineReplyHandler(req, resp));
}

TEST_F(TestUbseMemControllerMsg, GetLocalNumaInfoByPid)
{
    uint64_t pid = 1234;
    uint32_t numaId = 1;
    uint32_t socketId = 0;

    // MockUbseOsUtil::GetNumaIdByPid
    MOCKER(UbseOsUtil::GetNumaIdByPid).stubs().with(pid, _).will(returnValue(UBSE_OK));

    // MockUbseNodeController::GetInstance().GetCurNode()
    ubse::nodeController::UbseNodeInfo node;
    node.nodeId = "node1";
    ubse::nodeController::UbseNumaLocation location{node.nodeId, numaId};
    UbseNumaInfo numaInfo;
    numaInfo.socketId = socketId;
    node.numaInfos[location] = numaInfo;
    MOCKER(&ubse::nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node));

    EXPECT_EQ(GetLocalNumaInfoByPid(pid, numaId, socketId), UBSE_OK);
    EXPECT_EQ(numaId, 1);
    EXPECT_EQ(socketId, 0);
}

UbseResult MockGetLocalNumaInfoByPid([[maybe_unused]] const uint64_t& pid, uint32_t& numaId, uint32_t& socketId)
{
    numaId = 1;
    socketId = 0;
    return UBSE_OK;
}

TEST_F(TestUbseMemControllerMsg, GetNumaInfoByPidHandler)
{
    uint64_t pid = 1234;
    uint32_t numaId = 1;
    uint32_t socketId = 0;

    // MockGetLocalNumaInfoByPid
    MOCKER(GetLocalNumaInfoByPid).stubs().will(invoke(MockGetLocalNumaInfoByPid));

    // 构造请求
    serial::UbseSerialization output;
    output << pid;
    UbseByteBuffer req{output.GetBuffer(true), output.GetLength(), [](uint8_t* p) {
                           SafeDeleteArray(p);
                       }};

    // 处理请求
    UbseByteBuffer resp;
    EXPECT_EQ(GetNumaInfoByPidHandler(req, resp), UBSE_OK);

    // 验证响应
    serial::UbseDeSerialization input(resp.data, resp.len);
    uint32_t respNumaId;
    uint32_t respSocketId;
    input >> respNumaId >> respSocketId;
    EXPECT_EQ(respNumaId, numaId);
    EXPECT_EQ(respSocketId, socketId);
}

TEST_F(TestUbseMemControllerMsg, GetNumaInfoFromRemoteRespHandler)
{
    uint32_t numaId = 1;
    uint32_t socketId = 0;
    UbseResult getRet = UBSE_OK;

    // 构造响应
    serial::UbseSerialization output;
    output << numaId << socketId;
    UbseByteBuffer resp{output.GetBuffer(true), output.GetLength(), [](uint8_t* p) {
                            SafeDeleteArray(p);
                        }};

    // 处理响应
    GetNumaInfoFromRemoteRespHandler(resp, UBSE_OK, numaId, socketId, getRet);

    // 验证结果
    EXPECT_EQ(getRet, UBSE_OK);
    EXPECT_EQ(numaId, 1);
    EXPECT_EQ(socketId, 0);
}

TEST_F(TestUbseMemControllerMsg, GetNumaInfoFromAgent)
{
    std::string nodeId = "node1";
    uint64_t pid = 1234;
    uint32_t numaId = 1;
    uint32_t socketId = 0;
    // MockGetLocalNumaInfoByPid
    MOCKER(GetLocalNumaInfoByPid).stubs().with(pid, _, _).will(returnValue(UBSE_OK));
    // MockUbseRpcSend
    MOCKER(UbseRpcSend).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(GetNumaInfoFromAgent(nodeId, pid, numaId, socketId), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryFdExport)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    // MockUbseComModule::RpcSend
    UbseMemFdBorrowExportObj obj{};
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(QueryFdExport(request, obj), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryFdExportHandler)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemFdBorrowExportObj obj{};

    // 构造请求
    UbseByteBuffer req{};
    UbseMemDebtQueryRequestSimpo simpo{};
    simpo.SetUbseMemDebtQueryRequest(request);
    simpo.Serialize();
    req.data = simpo.mOutputRawData.get();
    req.len = simpo.mOutputRawDataSize;

    // 处理请求
    UbseByteBuffer resp;
    EXPECT_EQ(QueryFdExportHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryNumaExport)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    // MockUbseComModule::RpcSend
    UbseMemNumaBorrowExportObj obj{};
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(QueryNumaExport(request, obj), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryNumaExportHandler)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemNumaBorrowExportObj obj{};

    // 构造请求
    UbseByteBuffer req{};
    UbseMemDebtQueryRequestSimpo simpo{};
    simpo.SetUbseMemDebtQueryRequest(request);
    simpo.Serialize();
    req.data = simpo.mOutputRawData.get();
    req.len = simpo.mOutputRawDataSize;

    // 处理请求
    UbseByteBuffer resp;
    EXPECT_EQ(QueryNumaExportHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryAddrExport)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";

    // MockUbseComModule::RpcSend
    UbseMemAddrBorrowExportObj obj{};
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(QueryAddrExport(request, obj), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryAddrExportHandler)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";

    // 构造请求
    UbseByteBuffer req{};
    UbseMemDebtQueryRequestSimpo simpo{};
    simpo.SetUbseMemDebtQueryRequest(request);
    simpo.Serialize();
    req.data = simpo.mOutputRawData.get();
    req.len = simpo.mOutputRawDataSize;

    // 处理请求
    UbseByteBuffer resp;
    EXPECT_EQ(QueryAddrExportHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryShareExport)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemShareBorrowExportObj obj{};
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryShareExport(request, obj), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryShareExportHandler)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemDebtQueryRequestSimpo simpo{};
    simpo.SetUbseMemDebtQueryRequest(request);
    simpo.Serialize();
    req.data = simpo.mOutputRawData.get();
    req.len = simpo.mOutputRawDataSize;
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryShareExportHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryFdImport)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemFdBorrowImportObj obj{};
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryFdImport(request, obj), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryFdImportHandler)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemDebtQueryRequestSimpo simpo{};
    simpo.SetUbseMemDebtQueryRequest(request);
    simpo.Serialize();
    req.data = simpo.mOutputRawData.get();
    req.len = simpo.mOutputRawDataSize;
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryFdImportHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryNumaImport)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemNumaBorrowImportObj obj{};
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryNumaImport(request, obj), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryNumaImportHandler)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemDebtQueryRequestSimpo simpo{};
    simpo.SetUbseMemDebtQueryRequest(request);
    simpo.Serialize();
    req.data = simpo.mOutputRawData.get();
    req.len = simpo.mOutputRawDataSize;
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryNumaImportHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryAddrImport)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemAddrBorrowImportObj obj{};
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryAddrImport(request, obj), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryAddrImportHandler)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemDebtQueryRequestSimpo simpo{};
    simpo.SetUbseMemDebtQueryRequest(request);
    simpo.Serialize();
    req.data = simpo.mOutputRawData.get();
    req.len = simpo.mOutputRawDataSize;
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryAddrImportHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryShareImport)
{
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemShareBorrowImportObj obj{};
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryShareImport(request, obj), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, QueryShareImportHandler)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    UbseMemDebtQueryRequest request;
    request.exportNodeId = "1";
    request.name = "name";
    UbseMemDebtQueryRequestSimpo simpo{};
    simpo.SetUbseMemDebtQueryRequest(request);
    simpo.Serialize();
    req.data = simpo.mOutputRawData.get();
    req.len = simpo.mOutputRawDataSize;
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryShareImportHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerMsg, IsUrma)
{
    // 场景 1: GetModule 返回 ptr
    EXPECT_TRUE(IsUrma());

    // 场景 2: GetConf 返回错误
    MOCKER_CPP(&UbseContext::GetModule<ubse::config::UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::config::UbseConfModule>()));
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR)); // 模拟GetConf返回错误
    EXPECT_TRUE(IsUrma());

    // 场景 3: GetConf 返回成功
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).reset();
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_OK)); // 模拟GetConf返回错误
    EXPECT_FALSE(IsUrma());
}
} // namespace ubse::mem_controller::ut