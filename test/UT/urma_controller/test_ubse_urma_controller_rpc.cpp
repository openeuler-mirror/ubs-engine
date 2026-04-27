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

#include "test_ubse_urma_controller_rpc.h"
#include <map>
#include "test_ubse_urma_controller_def.h"
#include "ubse_com_module.h"
#include "ubse_com_op_code.h"
#include "ubse_context.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_controller_rpc.h"

namespace ubse::urmaControllerRpc::ut {
using namespace ubse::com;
using namespace ubse::urmaController;
using namespace ubse::urma;
using namespace ubse::message;
using namespace ubse::election;
using namespace ubse::context;

TEST_F(TestUbseUrmaControllerRpc, ConvertUint32ToBondingState_Val1_ReturnsActived)
{
    auto ret = ConvertUint32ToBondingState(1);
    EXPECT_EQ(ret, UrmaDevState::ACTIVED);
}

TEST_F(TestUbseUrmaControllerRpc, ConvertUint32ToBondingState_Val2_ReturnsInactived)
{
    auto ret = ConvertUint32ToBondingState(2);
    EXPECT_EQ(ret, UrmaDevState::INACTIVED);
}

TEST_F(TestUbseUrmaControllerRpc, ConvertUint32ToBondingState_Other_ReturnsUnknown)
{
    auto ret = ConvertUint32ToBondingState(0);
    EXPECT_EQ(ret, UrmaDevState::UNKNOWN);

    ret = ConvertUint32ToBondingState(3);
    EXPECT_EQ(ret, UrmaDevState::UNKNOWN);
}

TEST_F(TestUbseUrmaControllerRpc, UrmaDevQueryReqSimpo_Serialize_Fail)
{
    UrmaDevQueryReqSimpo simpo;
    UrmaDevQueryRpcReq req{};
    req.nodeId = 42;
    simpo.SetUbseUrmaDevReq(req);
    auto ret = simpo.Serialize();
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_GT(simpo.SerializedDataSize(), 0);
    EXPECT_NE(simpo.SerializedData(), nullptr);
}

TEST_F(TestUbseUrmaControllerRpc, UrmaDevQueryReqSimpo_Deserialize_InputNull)
{
    UrmaDevQueryReqSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UrmaDevQueryReqSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UrmaDevQueryReqSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UrmaDevQueryReqSimpo_RoundTrip)
{
    UrmaDevQueryReqSimpo simpo;
    UrmaDevQueryRpcReq req{};
    req.nodeId = 99;
    simpo.SetUbseUrmaDevReq(req);
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UrmaDevQueryReqSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
    EXPECT_EQ(simpo2.GetUbseUrmaDevReq().nodeId, 99u);
}

TEST_F(TestUbseUrmaControllerRpc, UrmaDevQueryRspSimpo_RoundTrip)
{
    UrmaDevQueryRspSimpo simpo;
    UrmaDevQueryRpcRsp rsp;
    rsp.result = 123;
    UbseUrmaInfoForQuery info;
    info.urmaName = "test_urma";
    info.feEids = {"eid1", "eid2"};
    info.feNames = {"fe1", "fe2"};
    info.devEid = "dev_eid_1";
    info.bondingType = UrmaDevType::UNIQUE;
    info.state = UrmaDevState::ACTIVED;
    rsp.urmaInfos.push_back(info);
    simpo.SetUbseUrmaDevQueryRsp(rsp);
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UrmaDevQueryRspSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
    auto rsp2 = simpo2.GetUbseUrmaDevRsp();
    EXPECT_EQ(rsp2.result, 123u);
    ASSERT_EQ(rsp2.urmaInfos.size(), 1);
    EXPECT_EQ(rsp2.urmaInfos[0].urmaName, "test_urma");
    EXPECT_EQ(rsp2.urmaInfos[0].devEid, "dev_eid_1");
    EXPECT_EQ(rsp2.urmaInfos[0].bondingType, UrmaDevType::UNIQUE);
    EXPECT_EQ(rsp2.urmaInfos[0].state, UrmaDevState::ACTIVED);
}

TEST_F(TestUbseUrmaControllerRpc, UrmaDevQueryRspSimpo_Deserialize_InputNull)
{
    UrmaDevQueryRspSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UrmaDevQueryRspSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UrmaDevQueryRspSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaBrocastReqSimpo_RoundTrip)
{
    UbseUrmaBrocastReqSimpo simpo;
    UrmaBrocastReq req;
    req.urmaInfoTimestamps["node0"] = 100;
    req.urmaInfoTimestamps["node1"] = 200;
    simpo.SetUrmaNotifyReq(req);
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UbseUrmaBrocastReqSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
    auto req2 = simpo2.GetUrmaNotifyReq();
    EXPECT_EQ(req2.urmaInfoTimestamps.size(), 2);
    EXPECT_EQ(req2.urmaInfoTimestamps["node0"], 100);
    EXPECT_EQ(req2.urmaInfoTimestamps["node1"], 200);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaBrocastReqSimpo_Deserialize_InputNull)
{
    UbseUrmaBrocastReqSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaBrocastReqSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UbseUrmaBrocastReqSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaBrocastRspSimpo_RoundTrip)
{
    UbseUrmaBrocastRspSimpo simpo;
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UbseUrmaBrocastRspSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaBrocastRspSimpo_Deserialize_InputNull)
{
    UbseUrmaBrocastRspSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaBrocastRspSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UbseUrmaBrocastRspSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaQueryReqSimpo_RoundTrip)
{
    UbseUrmaQueryReqSimpo simpo;
    QueryUrmaInfoReq req;
    req.updateNodeIds = {"node0", "node1"};
    simpo.SetUrmaQueryReq(req);
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UbseUrmaQueryReqSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
    auto req2 = simpo2.GetUrmaQueryReq();
    ASSERT_EQ(req2.updateNodeIds.size(), 2);
    EXPECT_EQ(req2.updateNodeIds[0], "node0");
    EXPECT_EQ(req2.updateNodeIds[1], "node1");
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaQueryReqSimpo_Deserialize_InputNull)
{
    UbseUrmaQueryReqSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaQueryReqSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UbseUrmaQueryReqSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaQueryRspSimpo_RoundTrip)
{
    UbseUrmaQueryRspSimpo simpo;
    QueryUrmaInfoRsp rsp;
    UbseUrmaNodeInfo nodeInfo;
    nodeInfo.nodeId = "node0";
    rsp.queryNodeInfos.push_back(nodeInfo);
    simpo.SetUbseQueryRsp(rsp);
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UbseUrmaQueryRspSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
    auto rsp2 = simpo2.GetUbseUrmaQueryRsp();
    ASSERT_EQ(rsp2.queryNodeInfos.size(), 1);
    EXPECT_EQ(rsp2.queryNodeInfos[0].nodeId, "node0");
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaQueryRspSimpo_Deserialize_InputNull)
{
    UbseUrmaQueryRspSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaQueryRspSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UbseUrmaQueryRspSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaReportUrmaNodeInfoReqSimpo_RoundTrip)
{
    UbseUrmaReportUrmaNodeInfoReqSimpo simpo;
    ReportUrmaNodeInfoReq infoReq;
    infoReq.nodeId = "node0";
    infoReq.urmaNodeInfo.nodeId = "node0";
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "dev_eid_1";
    urmaInfo.urmaDevType = UrmaDevType::UNIQUE;
    urmaInfo.state = UrmaDevState::ACTIVED;
    infoReq.urmaNodeInfo.urmaList["urma_1"] = urmaInfo;
    simpo.SetUbseUrmaNodeInfo(infoReq);
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UbseUrmaReportUrmaNodeInfoReqSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
    auto rspReq = simpo2.GetUbseUrmaNodeInfo();
    EXPECT_EQ(rspReq.nodeId, "node0");
    ASSERT_EQ(rspReq.urmaNodeInfo.urmaList.size(), 1);
    EXPECT_EQ(rspReq.urmaNodeInfo.urmaList["urma_1"].urmaDevEid, "dev_eid_1");
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaReportUrmaNodeInfoReqSimpo_Deserialize_InputNull)
{
    UbseUrmaReportUrmaNodeInfoReqSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaReportUrmaNodeInfoReqSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UbseUrmaReportUrmaNodeInfoReqSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaReportUrmaNodeInfoRspSimpo_RoundTrip)
{
    UbseUrmaReportUrmaNodeInfoRspSimpo simpo;
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UbseUrmaReportUrmaNodeInfoRspSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaReportUrmaNodeInfoRspSimpo_Deserialize_InputNull)
{
    UbseUrmaReportUrmaNodeInfoRspSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaReportUrmaNodeInfoRspSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UbseUrmaReportUrmaNodeInfoRspSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaActivateUrmaInfoReqSimpo_RoundTrip)
{
    UbseUrmaActivateUrmaInfoReqSimpo simpo;
    simpo.SetNodeId("node0");
    simpo.SetUrmaName("urma_1");
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UbseUrmaActivateUrmaInfoReqSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
    EXPECT_EQ(simpo2.GetNodeId(), "node0");
    EXPECT_EQ(simpo2.GetUrmaName(), "urma_1");
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaActivateUrmaInfoReqSimpo_Deserialize_InputNull)
{
    UbseUrmaActivateUrmaInfoReqSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaActivateUrmaInfoReqSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UbseUrmaActivateUrmaInfoReqSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaActivateUrmaInfoRspSimpo_RoundTrip)
{
    UbseUrmaActivateUrmaInfoRspSimpo simpo;
    ASSERT_EQ(simpo.Serialize(), UBSE_OK);

    auto data = simpo.SerializedData();
    auto size = simpo.SerializedDataSize();

    UbseUrmaActivateUrmaInfoRspSimpo simpo2(data, size);
    ASSERT_EQ(simpo2.Deserialize(), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaActivateUrmaInfoRspSimpo_Deserialize_InputNull)
{
    UbseUrmaActivateUrmaInfoRspSimpo simpo;
    auto ret = simpo.Deserialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaActivateUrmaInfoRspSimpo_Deserialize_CorruptData)
{
    uint8_t badData[4] = {0};
    UbseUrmaActivateUrmaInfoRspSimpo simpo(badData, static_cast<uint32_t>(sizeof(badData)));
    EXPECT_EQ(simpo.Deserialize(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaDevQueryMessageHandler_GetModuleCode)
{
    UbseUrmaDevQueryMessageHandler handler;
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_URMA));
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaDevQueryMessageHandler_GetOpCode)
{
    UbseUrmaDevQueryMessageHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_DEV_QUERY));
}

TEST_F(TestUbseUrmaControllerRpc, DevQueryHandle_GlobalStop)
{
    g_globalStop = true;
    UbseUrmaDevQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::gNullPtr, UbseBaseMessage::gNullPtr, nullptr);
    g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, DevQueryHandle_RequestOrResponseNull)
{
    UbseUrmaDevQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::gNullPtr, UbseBaseMessage::gNullPtr, nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, DevQueryHandle_GetMasterInfoFails)
{
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));

    Ref<UrmaDevQueryReqSimpo> req = new UrmaDevQueryReqSimpo();
    req->SetUbseUrmaDevReq({.nodeId = 0});
    Ref<UrmaDevQueryRspSimpo> rsp = new UrmaDevQueryRspSimpo();
    UbseUrmaDevQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, DevQueryHandle_GetCurrentNodeInfoFails)
{
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));

    Ref<UrmaDevQueryReqSimpo> req = new UrmaDevQueryReqSimpo();
    req->SetUbseUrmaDevReq({.nodeId = 0});
    Ref<UrmaDevQueryRspSimpo> rsp = new UrmaDevQueryRspSimpo();
    UbseUrmaDevQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, DevQueryHandle_LocalNodeQuery)
{
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "123";
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaInfoForQuery).stubs();

    Ref<UrmaDevQueryReqSimpo> req = new UrmaDevQueryReqSimpo();
    req->SetUbseUrmaDevReq({.nodeId = 123});
    Ref<UrmaDevQueryRspSimpo> rsp = new UrmaDevQueryRspSimpo();
    UbseUrmaDevQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, DevQueryHandle_OtherNode_ReturnsInval)
{
    UbseRoleInfo masterInfo;
    masterInfo.nodeId = "456";
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "789";
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));

    Ref<UrmaDevQueryReqSimpo> req = new UrmaDevQueryReqSimpo();
    req->SetUbseUrmaDevReq({.nodeId = 123});
    Ref<UrmaDevQueryRspSimpo> rsp = new UrmaDevQueryRspSimpo();
    UbseUrmaDevQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerRpc, DevQueryHandle_MasterForward_ComModuleNull)
{
    UbseRoleInfo masterInfo;
    masterInfo.nodeId = "789";
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "789";
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));

    Ref<UrmaDevQueryReqSimpo> req = new UrmaDevQueryReqSimpo();
    req->SetUbseUrmaDevReq({.nodeId = 123});
    Ref<UrmaDevQueryRspSimpo> rsp = new UrmaDevQueryRspSimpo();
    UbseUrmaDevQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerRpc, DevQueryHandle_MasterForward_RpcSendFails)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseRoleInfo masterInfo;
    masterInfo.nodeId = "789";
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "789";
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));

    Ref<UrmaDevQueryReqSimpo> req = new UrmaDevQueryReqSimpo();
    req->SetUbseUrmaDevReq({.nodeId = 123});
    Ref<UrmaDevQueryRspSimpo> rsp = new UrmaDevQueryRspSimpo();
    UbseUrmaDevQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaNotifyMessageHandler_GetModuleCode)
{
    UbseUrmaNotifyMessageHandler handler;
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_URMA));
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaNotifyMessageHandler_GetOpCode)
{
    UbseUrmaNotifyMessageHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_BROADCAST));
}

TEST_F(TestUbseUrmaControllerRpc, NotifyHandle_GlobalStop)
{
    g_globalStop = true;
    UbseUrmaNotifyMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::gNullPtr, UbseBaseMessage::gNullPtr, nullptr);
    g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, NotifyHandle_RequestOrResponseNull)
{
    UbseUrmaNotifyMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::gNullPtr, UbseBaseMessage::gNullPtr, nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaQueryMessageHandler_GetModuleCode)
{
    UbseUrmaQueryMessageHandler handler;
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_URMA));
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaQueryMessageHandler_GetOpCode)
{
    UbseUrmaQueryMessageHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_QUERY));
}

TEST_F(TestUbseUrmaControllerRpc, QueryHandle_RequestOrResponseNull)
{
    UbseUrmaQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::gNullPtr, UbseBaseMessage::gNullPtr, nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, QueryHandle_Success)
{
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaNodeInfo).stubs().will(returnValue(UbseUrmaNodeInfo{}));

    Ref<UbseUrmaQueryReqSimpo> req = new UbseUrmaQueryReqSimpo();
    QueryUrmaInfoReq queryReq;
    queryReq.updateNodeIds = {"node0"};
    req->SetUrmaQueryReq(queryReq);
    Ref<UbseUrmaQueryRspSimpo> rsp = new UbseUrmaQueryRspSimpo();
    UbseUrmaQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(rsp->GetErrCode(), UBSE_OK);
    auto queryRsp = rsp->GetUbseUrmaQueryRsp();
    ASSERT_EQ(queryRsp.queryNodeInfos.size(), 1);
}

TEST_F(TestUbseUrmaControllerRpc, QueryHandle_GlobalStopDuringIteration)
{
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaNodeInfo).stubs().will(returnValue(UbseUrmaNodeInfo{}));

    Ref<UbseUrmaQueryReqSimpo> req = new UbseUrmaQueryReqSimpo();
    QueryUrmaInfoReq queryReq;
    queryReq.updateNodeIds = {"node0", "node1", "node2"};
    req->SetUrmaQueryReq(queryReq);
    Ref<UbseUrmaQueryRspSimpo> rsp = new UbseUrmaQueryRspSimpo();

    g_globalStop = true;
    UbseUrmaQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, QueryHandle_EmptyUpdateNodeIds)
{
    Ref<UbseUrmaQueryReqSimpo> req = new UbseUrmaQueryReqSimpo();
    QueryUrmaInfoReq queryReq;
    req->SetUrmaQueryReq(queryReq);
    Ref<UbseUrmaQueryRspSimpo> rsp = new UbseUrmaQueryRspSimpo();
    UbseUrmaQueryMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(rsp->GetUbseUrmaQueryRsp().queryNodeInfos.size(), 0);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaReportUrmaNodeInfoMessageHandler_GetModuleCode)
{
    UbseUrmaReportUrmaNodeInfoMessageHandler handler;
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_URMA));
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaReportUrmaNodeInfoMessageHandler_GetOpCode)
{
    UbseUrmaReportUrmaNodeInfoMessageHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_URMA_INFO_REPORT));
}

TEST_F(TestUbseUrmaControllerRpc, ReportHandle_GlobalStop)
{
    g_globalStop = true;
    UbseUrmaReportUrmaNodeInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::gNullPtr, UbseBaseMessage::gNullPtr, nullptr);
    g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, ReportHandle_RequestOrRspNull)
{
    Ref<UbseUrmaReportUrmaNodeInfoReqSimpo> req = new UbseUrmaReportUrmaNodeInfoReqSimpo();
    UbseUrmaReportUrmaNodeInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::gNullPtr, nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, ReportHandle_EmptyChangeNodeId)
{
    Ref<UbseUrmaReportUrmaNodeInfoReqSimpo> req = new UbseUrmaReportUrmaNodeInfoReqSimpo();
    ReportUrmaNodeInfoReq infoReq;
    infoReq.nodeId = "";
    infoReq.urmaNodeInfo.nodeId = "node0";
    req->SetUbseUrmaNodeInfo(infoReq);
    Ref<UbseUrmaReportUrmaNodeInfoRspSimpo> rsp = new UbseUrmaReportUrmaNodeInfoRspSimpo();
    UbseUrmaReportUrmaNodeInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_EQ(rsp->GetErrCode(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, ReportHandle_EmptyNodeInfoNodeId)
{
    Ref<UbseUrmaReportUrmaNodeInfoReqSimpo> req = new UbseUrmaReportUrmaNodeInfoReqSimpo();
    ReportUrmaNodeInfoReq infoReq;
    infoReq.nodeId = "node0";
    infoReq.urmaNodeInfo.nodeId = "";
    req->SetUbseUrmaNodeInfo(infoReq);
    Ref<UbseUrmaReportUrmaNodeInfoRspSimpo> rsp = new UbseUrmaReportUrmaNodeInfoRspSimpo();
    UbseUrmaReportUrmaNodeInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_EQ(rsp->GetErrCode(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, ReportHandle_Success)
{
    MOCKER_CPP(&UbseUrmaControllerManager::InsertNewNodeInfo).stubs();
    MOCKER_CPP(UbseUrmaAsyncBrocastUrmaInfo).stubs().will(returnValue(UBSE_OK));

    Ref<UbseUrmaReportUrmaNodeInfoReqSimpo> req = new UbseUrmaReportUrmaNodeInfoReqSimpo();
    ReportUrmaNodeInfoReq infoReq;
    infoReq.nodeId = "node0";
    infoReq.urmaNodeInfo.nodeId = "node0";
    req->SetUbseUrmaNodeInfo(infoReq);
    Ref<UbseUrmaReportUrmaNodeInfoRspSimpo> rsp = new UbseUrmaReportUrmaNodeInfoRspSimpo();
    UbseUrmaReportUrmaNodeInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(rsp->GetErrCode(), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, ReportHandle_BrocastFails)
{
    MOCKER_CPP(&UbseUrmaControllerManager::InsertNewNodeInfo).stubs();
    MOCKER_CPP(UbseUrmaAsyncBrocastUrmaInfo).stubs().will(returnValue(UBSE_ERROR));

    Ref<UbseUrmaReportUrmaNodeInfoReqSimpo> req = new UbseUrmaReportUrmaNodeInfoReqSimpo();
    ReportUrmaNodeInfoReq infoReq;
    infoReq.nodeId = "node0";
    infoReq.urmaNodeInfo.nodeId = "node0";
    req->SetUbseUrmaNodeInfo(infoReq);
    Ref<UbseUrmaReportUrmaNodeInfoRspSimpo> rsp = new UbseUrmaReportUrmaNodeInfoRspSimpo();
    UbseUrmaReportUrmaNodeInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_EQ(rsp->GetErrCode(), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaActivateUrmaInfoMessageHandler_GetModuleCode)
{
    UbseUrmaActivateUrmaInfoMessageHandler handler;
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_URMA));
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaActivateUrmaInfoMessageHandler_GetOpCode)
{
    UbseUrmaActivateUrmaInfoMessageHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_DEV_ACTIVATE));
}

TEST_F(TestUbseUrmaControllerRpc, ActivateHandle_GlobalStop)
{
    g_globalStop = true;
    UbseUrmaActivateUrmaInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::gNullPtr, UbseBaseMessage::gNullPtr, nullptr);
    g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, ActivateHandle_RequestOrResponseNull)
{
    UbseUrmaActivateUrmaInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::gNullPtr, UbseBaseMessage::gNullPtr, nullptr);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, ActivateHandle_GetCurNodeIdAndMasterNodeIdFails_GetCurrentError)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));

    Ref<UbseUrmaActivateUrmaInfoReqSimpo> req = new UbseUrmaActivateUrmaInfoReqSimpo();
    req->SetNodeId("node0");
    req->SetUrmaName("urma_1");
    Ref<UbseUrmaActivateUrmaInfoRspSimpo> rsp = new UbseUrmaActivateUrmaInfoRspSimpo();
    UbseUrmaActivateUrmaInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR_AGAIN);
}

TEST_F(TestUbseUrmaControllerRpc, ActivateHandle_GetCurNodeIdAndMasterNodeIdFails_GetMasterError)
{
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "node0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    Ref<UbseUrmaActivateUrmaInfoReqSimpo> req = new UbseUrmaActivateUrmaInfoReqSimpo();
    req->SetNodeId("node0");
    req->SetUrmaName("urma_1");
    Ref<UbseUrmaActivateUrmaInfoRspSimpo> rsp = new UbseUrmaActivateUrmaInfoRspSimpo();
    UbseUrmaActivateUrmaInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR_AGAIN);
}

TEST_F(TestUbseUrmaControllerRpc, ActivateHandle_LocalNodeActivate)
{
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "node0";
    UbseRoleInfo masterInfo;
    masterInfo.nodeId = "master";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UrmaController::ActivateSpecifyUrmaBonding).stubs().will(returnValue(UBSE_OK));

    Ref<UbseUrmaActivateUrmaInfoReqSimpo> req = new UbseUrmaActivateUrmaInfoReqSimpo();
    req->SetNodeId("node0");
    req->SetUrmaName("urma_1");
    Ref<UbseUrmaActivateUrmaInfoRspSimpo> rsp = new UbseUrmaActivateUrmaInfoRspSimpo();
    UbseUrmaActivateUrmaInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(rsp->GetErrCode(), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, ActivateHandle_MasterForward_ComModuleNull)
{
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "master";
    UbseRoleInfo masterInfo;
    masterInfo.nodeId = "master";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));

    Ref<UbseUrmaActivateUrmaInfoReqSimpo> req = new UbseUrmaActivateUrmaInfoReqSimpo();
    req->SetNodeId("node0");
    req->SetUrmaName("urma_1");
    Ref<UbseUrmaActivateUrmaInfoRspSimpo> rsp = new UbseUrmaActivateUrmaInfoRspSimpo();
    UbseUrmaActivateUrmaInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    EXPECT_EQ(rsp->GetErrCode(), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerRpc, ActivateHandle_OtherNode)
{
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "other";
    UbseRoleInfo masterInfo;
    masterInfo.nodeId = "master";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));

    Ref<UbseUrmaActivateUrmaInfoReqSimpo> req = new UbseUrmaActivateUrmaInfoReqSimpo();
    req->SetNodeId("node0");
    req->SetUrmaName("urma_1");
    Ref<UbseUrmaActivateUrmaInfoRspSimpo> rsp = new UbseUrmaActivateUrmaInfoRspSimpo();
    UbseUrmaActivateUrmaInfoMessageHandler handler;
    auto ret = handler.Handle(UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp), nullptr);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
    EXPECT_EQ(rsp->GetErrCode(), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerRpc, GetCurNodeIdAndMasterNodeId_GetCurrentNodeInfoFails)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    std::string curNodeId;
    std::string masterNodeId;
    auto ret = GetCurNodeIdAndMasterNodeId(curNodeId, masterNodeId);
    EXPECT_EQ(ret, UBSE_ERROR_AGAIN);
}

TEST_F(TestUbseUrmaControllerRpc, GetCurNodeIdAndMasterNodeId_GetMasterInfoFails)
{
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "node0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    std::string curNodeId;
    std::string masterNodeId;
    auto ret = GetCurNodeIdAndMasterNodeId(curNodeId, masterNodeId);
    EXPECT_EQ(ret, UBSE_ERROR_AGAIN);
}

TEST_F(TestUbseUrmaControllerRpc, GetCurNodeIdAndMasterNodeId_Success)
{
    UbseRoleInfo currentInfo;
    currentInfo.nodeId = "node0";
    UbseRoleInfo masterInfo;
    masterInfo.nodeId = "master";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    std::string curNodeId;
    std::string masterNodeId;
    auto ret = GetCurNodeIdAndMasterNodeId(curNodeId, masterNodeId);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(curNodeId, "node0");
    EXPECT_EQ(masterNodeId, "master");
}

TEST_F(TestUbseUrmaControllerRpc, QueryUrmaInfoFromMaster_ComModuleNull)
{
    UbseRoleInfo roleInfo;
    roleInfo.nodeId = "master";
    std::vector<std::string> updateNodeIds;
    auto ret = QueryUrmaInfoFromMaster(roleInfo, updateNodeIds);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerRpc, ReportUrmaNodeInfoToMaster_GetMasterInfoFails)
{
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = ReportUrmaNodeInfoToMaster("node0");
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, ReportUrmaNodeInfoToMaster_ComModuleNull)
{
    UbseRoleInfo masterInfo;
    masterInfo.nodeId = "master";
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaNodeInfo).stubs().will(returnValue(UbseUrmaNodeInfo{}));
    auto ret = ReportUrmaNodeInfoToMaster("node0");
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerRpc, DoUpdateUrmaInfos_GlobalStop)
{
    g_globalStop = true;
    auto ret = DoUpdateUrmaInfos({"node0"});
    g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, DoUpdateUrmaInfos_GetMasterInfoFails)
{
    MOCKER_CPP(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = DoUpdateUrmaInfos({"node0"});
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, QueryUrmaInfoFromMaster_RpcSendFails)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    ubse::election::UbseRoleInfo roleInfo;
    roleInfo.nodeId = "master";
    std::vector<std::string> updateNodeIds;
    auto ret = QueryUrmaInfoFromMaster(roleInfo, updateNodeIds);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, ReportUrmaNodeInfoToMaster_RpcSendFails)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    ubse::election::UbseRoleInfo masterInfo;
    masterInfo.nodeId = "master";
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerManager::GetUrmaNodeInfo).stubs().will(returnValue(UbseUrmaNodeInfo{}));
    auto ret = ReportUrmaNodeInfoToMaster("node0");
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, ForwardActiveReqToSpecifyNode_RpcSendFails)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    Ref<UbseUrmaActivateUrmaInfoReqSimpo> req = new UbseUrmaActivateUrmaInfoReqSimpo();
    Ref<UbseUrmaActivateUrmaInfoRspSimpo> rsp = new UbseUrmaActivateUrmaInfoRspSimpo();
    auto ret = ForwardActiveReqToSpecifyNode("node0", UbseBaseMessage::Convert(req), UbseBaseMessage::Convert(rsp));
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerRpc, PostUpdateUrmaInfosTask_GlobalStop)
{
    std::map<std::string, uint64_t> timestamps;
    timestamps["node0"] = 100;
    g_globalStop = true;
    auto ret = PostUpdateUrmaInfosTask(timestamps);
    g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, UbseUrmaAsyncNotifyOneNodeUrmaInfoChange_GlobalStop)
{
    g_globalStop = true;
    auto ret = UbseUrmaAsyncNotifyOneNodeUrmaInfoChange("node0");
    g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, BrocastUrmaInfoTask_GlobalStop)
{
    g_globalStop = true;
    auto ret = BrocastUrmaInfoTask("node0");
    g_globalStop = false;
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerRpc, DoUpdateUrmaInfos_QueryUrmaInfoFromMasterFails)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    ubse::election::UbseRoleInfo masterInfo;
    masterInfo.nodeId = "master";
    MOCKER_CPP(UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    auto ret = DoUpdateUrmaInfos({"node0"});
    EXPECT_EQ(ret, UBSE_ERROR);
}
} // namespace ubse::urmaControllerRpc::ut
