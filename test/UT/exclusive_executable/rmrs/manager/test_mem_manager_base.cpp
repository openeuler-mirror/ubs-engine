/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#include <gmock/gmock.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public
#include "exporter.h"
#include "mem_manager_base.h"
#include "mp_sync_data_helper.h"
#include "rmrs_serialize.h"
#include "ubse_com.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "ubse_storage.h"
#undef private
using namespace std;
using namespace mempooling;
using namespace mockcpp;
using namespace ubse::nodeController;
using namespace ubse::com;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {

class TestMemManagerBase : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[TestMemManagerBase SetUp Begin]" << endl;
        GlobalMockObject::reset();
        cout << "[TestMemManagerBase SetUp End]" << endl;
    }

    void TearDown() override    {
        cout << "[TestMemManagerBase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        GlobalMockObject::reset();
        cout << "[TestMemManagerBase TearDown End]" << endl;
    }
};

// =========================================================================
// 1.  GetNodeInfoImmediatelyRecvHandler
// =========================================================================

uint32_t GetNumaInfoImmediatelyMockSuccess(std::vector<mempooling::exportV2::NumaInfo> &numaInfos)
{
    mempooling::exportV2::NumaInfo info;
    info.timestamp = 0;
    numaInfos.push_back(info);
    return MEM_POOLING_OK;
}

TEST_F(TestMemManagerBase, GetNodeInfoImmediatelyRecvHandler_Succeed)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               uint32_t(*)(std::vector<mempooling::exportV2::NumaInfo> &))
        .stubs()
        .will(invoke(GetNumaInfoImmediatelyMockSuccess));

    UbseByteBuffer req = {nullptr, 0, nullptr};
    UbseByteBuffer resp = {nullptr, 0, nullptr};

    auto ret = GetNodeInfoImmediatelyRecvHandler(req, resp);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_NE(resp.data, nullptr);
    EXPECT_GT(resp.len, 0);

    if (resp.freeFunc != nullptr) {
        resp.freeFunc(resp.data);
    }
}

TEST_F(TestMemManagerBase, GetNodeInfoImmediatelyRecvHandler_EmptyDataFail)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               uint32_t(*)(std::vector<mempooling::exportV2::NumaInfo> &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    UbseByteBuffer req = {nullptr, 0, nullptr};
    UbseByteBuffer resp = {nullptr, 0, nullptr};

    auto ret = GetNodeInfoImmediatelyRecvHandler(req, resp);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManagerBase, GetNodeInfoImmediatelyRecvHandler_GetNumaInfoImmediatelyFail)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               uint32_t(*)(std::vector<mempooling::exportV2::NumaInfo> &))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    UbseByteBuffer req = {nullptr, 0, nullptr};
    UbseByteBuffer resp = {nullptr, 0, nullptr};

    auto ret = GetNodeInfoImmediatelyRecvHandler(req, resp);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

// =========================================================================
// 3.  UpdateDataBaseAndCache
// =========================================================================

TEST_F(TestMemManagerBase, UpdateDataBaseAndCache_Succeed)
{
    MOCKER_CPP(&ubse::storage::UbseStoragePutData, uint32_t(*)(const char *, const char *, UbseByteBuffer *))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&ubse::election::UbseGetStandbyInfo, uint32_t(*)(ubse::election::UbseRoleInfo &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    UbseByteBuffer buffer;
    buffer.len = 10;
    buffer.data = new uint8_t[10];

    MpUpdateAntiNodeParam antiParam;
    ubse::election::UbseRoleInfo standbyRole;

    auto ret = UpdateDataBaseAndCache(buffer, antiParam, standbyRole);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(buffer.data, nullptr);
}

// =========================================================================
// 4.  SyncAntiDataStandByRecvHandler
// =========================================================================

TEST_F(TestMemManagerBase, SyncAntiDataStandByRecvHandler_Succeed)
{
    MOCKER_CPP(&ubse::storage::UbseStoragePutData, uint32_t(*)(const char *, const char *, UbseByteBuffer *))
        .stubs()
        .will(returnValue(0));

    SyncUpdateAntiNodeParam paramIn;

    rmrs::serialize::RmrsOutStream reqBuilder;
    reqBuilder << paramIn;

    UbseByteBuffer req;
    req.data = reqBuilder.GetBufferPointer();
    req.len = reqBuilder.GetSize();

    UbseByteBuffer resp = {nullptr, 0, nullptr};

    //
    auto ret = SyncAntiDataStandByRecvHandler(req, resp);

    //
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_NE(resp.data, nullptr);

    if (resp.freeFunc != nullptr) {
        resp.freeFunc(resp.data);
    }
}
// =========================================================================
// 5.  SyncAntiDataRecvHandler
// =========================================================================

//  Mock  1 UbseGetStandbyInfo
uint32_t UbseGetStandbyInfoMockSuccess(ubse::election::UbseRoleInfo &role)
{
    role.nodeId = "standby_node_1";
    return MEM_POOLING_OK;
}

//  Mock  2 ( Mock )
uint32_t SyncDataToNodeMockSuccess(const std::vector<std::string> &nodeList, uint32_t opCode, const UbseByteBuffer &req)
{
    return MEM_POOLING_OK;
}

TEST_F(TestMemManagerBase, SyncAntiDataRecvHandler_Succeed)
{
    // 1.  (UpdateDataBaseAndCache )
    MOCKER_CPP(&ubse::storage::UbseStoragePutData, uint32_t(*)(const char *, const char *, UbseByteBuffer *))
        .stubs()
        .will(returnValue(0));

    // 2.  (UpdateDataBaseAndCache )
    //  lambda
    MOCKER_CPP(&ubse::election::UbseGetStandbyInfo, uint32_t(*)(ubse::election::UbseRoleInfo &))
        .stubs()
        .will(invoke(UbseGetStandbyInfoMockSuccess));

    MOCKER_CPP(&mempooling::sync::MpSyncDataHelper::SyncDataToNode,
               uint32_t(*)(const std::vector<std::string> &, uint32_t, const UbseByteBuffer &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    //  Req
    SyncUpdateAntiNodeParam paramIn;
    //
    std::string mockStr = "anti_node_2";
    std::vector<std::string> mockStrVec;
    mockStrVec.push_back(mockStr);
    paramIn.nodeAntiAffinityMap["node_1"] = mockStrVec;

    rmrs::serialize::RmrsOutStream reqBuilder;
    reqBuilder << paramIn;

    UbseByteBuffer req;
    req.data = reqBuilder.GetBufferPointer();
    req.len = reqBuilder.GetSize();

    UbseByteBuffer resp = {nullptr, 0, nullptr};

    //
    auto ret = SyncAntiDataRecvHandler(req, resp);

    //
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_NE(resp.data, nullptr);

    //  RmrsOutStream    
    if (resp.freeFunc != nullptr) {
        resp.freeFunc(resp.data);
    }
}

// =========================================================================
// 6.  GetAllNodeInfoImmediatelyRecvHandler
// =========================================================================

//  WORKING  30s sleep
std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> GetAllNodesMockWorking()
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodes;
    ubse::nodeController::UbseNodeInfo info;
    info.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    nodes["node_1"] = info;
    return nodes;
}

//  RPC  (ParallelSendGetNodeInfo )
uint32_t UbseRpcSendMockSuccess(const UbseComEndpoint &endpoint, const UbseByteBuffer &req, void *ctx,
                                const UbseComRespHandler &handler)
{
    //
    std::vector<mempooling::exportV2::NumaInfo> dummyInfos;
    mempooling::exportV2::NumaInfo numa;
    numa.timestamp = 0;
    dummyInfos.push_back(numa);

    rmrs::serialize::RmrsOutStream builder;
    builder << dummyInfos;

    UbseByteBuffer respData;
    respData.data = builder.GetBufferPointer();
    respData.len = builder.GetSize();
    return 0;
}

TEST_F(TestMemManagerBase, GetAllNodeInfoImmediatelyRecvHandler_AllWorking_Succeed)
{
    // 1.  GetAllNodes WORKING  sleep
    using str2UbseNodeInfo = std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes,
               str2UbseNodeInfo(*)(ubse::nodeController::UbseNodeController *))
        .stubs()
        .will(invoke(GetAllNodesMockWorking));

    // 2.  ParallelSendGetNodeInfo  UbseRpcSendMockSuccess    
    MOCKER_CPP(&UbseRpcSend, uint32_t(*)(const UbseComEndpoint &, const UbseByteBuffer &, void *,
                                         const UbseComRespHandler &))
        .stubs()
        .will(invoke(UbseRpcSendMockSuccess));

    UbseByteBuffer req = {nullptr, 0, nullptr};
    UbseByteBuffer resp = {nullptr, 0, nullptr};

    //
    auto ret = GetAllNodeInfoImmediatelyRecvHandler(req, resp);

    //
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_NE(resp.data, nullptr);

    if (resp.freeFunc != nullptr) {
        resp.freeFunc(resp.data);
    }
}

std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> GetNoNodes()
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodes;
    return nodes;
}

TEST_F(TestMemManagerBase, GetAllNodeInfoImmediatelyRecvHandler_AllNodesEmpty_Failed)
{
    // 1.  GetAllNodes WORKING  sleep    
    using str2UbseNodeInfo = std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes,
               str2UbseNodeInfo(*)(ubse::nodeController::UbseNodeController *))
        .stubs()
        .will(invoke(GetNoNodes));

    UbseByteBuffer req = {nullptr, 0, nullptr};
    UbseByteBuffer resp = {nullptr, 0, nullptr};

    //
    auto ret = GetAllNodeInfoImmediatelyRecvHandler(req, resp);

    //
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    EXPECT_EQ(resp.data, nullptr);

    if (resp.freeFunc != nullptr) {
        resp.freeFunc(resp.data);
    }
}

std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> GetAllNodesMockSmoothingStatus()
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodes;
    ubse::nodeController::UbseNodeInfo node1;
    node1.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    ubse::nodeController::UbseNodeInfo node2;
    node2.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING;
    nodes["node1"] = node1;
    nodes["node2"] = node2;
    return nodes;
}

TEST_F(TestMemManagerBase, GetAllNodeInfoImmediatelyRecvHandler_SMOOTHING_Failed)
{
    using str2UbseNodeInfo = std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes,
               str2UbseNodeInfo(*)(ubse::nodeController::UbseNodeController *))
        .stubs()
        .will(invoke(GetAllNodesMockSmoothingStatus));
    MOCKER_CPP(&UbseRpcSend, uint32_t(*)(const UbseComEndpoint &, const UbseByteBuffer &, void *,
                                         const UbseComRespHandler &))
        .stubs()
        .will(invoke(UbseRpcSendMockSuccess));
    UbseByteBuffer req = {nullptr, 0, nullptr};
    UbseByteBuffer resp = {nullptr, 0, nullptr};

    auto ret = GetAllNodeInfoImmediatelyRecvHandler(req, resp);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    EXPECT_EQ(resp.data, nullptr);

    if (resp.freeFunc != nullptr) {
        resp.freeFunc(resp.data);
    }
}

std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> GetAllNodesMockInitStatus()
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodes;
    ubse::nodeController::UbseNodeInfo node1;
    node1.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    ubse::nodeController::UbseNodeInfo node2;
    node2.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_INIT;
    nodes["node1"] = node1;
    nodes["node2"] = node2;
    return nodes;
}

TEST_F(TestMemManagerBase, GetAllNodeInfoImmediatelyRecvHandler_INIT_Failed)
{
    using str2UbseNodeInfo = std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes,
               str2UbseNodeInfo(*)(ubse::nodeController::UbseNodeController *))
        .stubs()
        .will(invoke(GetAllNodesMockInitStatus));
    MOCKER_CPP(&UbseRpcSend, uint32_t(*)(const UbseComEndpoint &, const UbseByteBuffer &, void *,
                                         const UbseComRespHandler &))
        .stubs()
        .will(invoke(UbseRpcSendMockSuccess));
    UbseByteBuffer req = {nullptr, 0, nullptr};
    UbseByteBuffer resp = {nullptr, 0, nullptr};

    auto ret = GetAllNodeInfoImmediatelyRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    EXPECT_EQ(resp.data, nullptr);

    if (resp.freeFunc != nullptr) {
        resp.freeFunc(resp.data);
    }
}

TEST_F(TestMemManagerBase, GetNodeInfoImmediatelyResHandler_CtxNUll_Failed)
{
    void *ctx = nullptr;
    UbseByteBuffer resp = {nullptr, 0, nullptr};
    uint32_t resCode;
    (void)GetNodeInfoImmediatelyResHandler(ctx, resp, resCode);

    if (resp.freeFunc != nullptr) {
        resp.freeFunc(resp.data);
    }
}

TEST_F(TestMemManagerBase, GetNodeInfoImmediatelyResHandler_Succeed)
{
    mempooling::exportV2::NumaInfo info;
    info.timestamp = 0;
    std::vector<mempooling::exportV2::NumaInfo> mockNumaInfos;
    mockNumaInfos.push_back(info);
    rmrs::serialize::RmrsOutStream builder;
    builder << mockNumaInfos;
    UbseByteBuffer resp;
    resp.data = builder.GetBufferPointer();
    resp.len = builder.GetSize();
    uint32_t resCode;
    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    (void)GetNodeInfoImmediatelyResHandler(&numaInfos, resp, resCode);
    EXPECT_EQ(numaInfos[0].timestamp, info.timestamp);

    if (resp.freeFunc != nullptr) {
        resp.freeFunc(resp.data);
    }
}
}
