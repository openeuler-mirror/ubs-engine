/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
  
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mockcpp/mokc.h"

#define private public
#include "ubse_def.h"
#include "over_commit_msg.h"
#include "over_commit_def.h"
#include "over_commit_storage.h"
#include "exporter.h"
#include "export_type.h"
#include "ubse_election.h"
#include "ubse_storage.h"
#include "mp_sync_data_helper.h"
#include "rmrs_serialize.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::over_commit {
using std::cout;
using std::endl;
using namespace ubse::com;
using namespace mempooling::exportV2;
using namespace mempooling::sync;
using namespace ubse::election;
using namespace ubse::storage;

class TestOverCommitMsg : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

MpResult GetVmInfoImmediatelyMsgMockSuccess(std::vector<VmDomainInfo> &vmDomainInfos)
{
    VmDomainInfo info;
    info.metaData.pid = 1234;
    VmDomainNumaInfo numaLocal;
    numaLocal.numaId = 0;
    numaLocal.isLocal = true;
    numaLocal.usedMem = 1024;
    numaLocal.socketId = 1;
    VmDomainNumaInfo numaRemote;
    numaRemote.numaId = 1;
    numaRemote.isLocal = false;
    numaRemote.usedMem = 2048;
    info.numaInfo[0] = numaLocal;
    info.numaInfo[1] = numaRemote;
    vmDomainInfos.push_back(info);
    return MEM_POOLING_OK;
}

MpResult GetVmInfoImmediatelyMsgMockFailed(std::vector<VmDomainInfo> &vmDomainInfos)
{
    return MEM_POOLING_ERROR;
}

MpResult GetVmInfoImmediatelyMsgMockEmpty(std::vector<VmDomainInfo> &vmDomainInfos)
{
    return MEM_POOLING_OK;
}

MpResult GetNumaInfoImmediatelyMsgMockSuccess(std::vector<mempooling::exportV2::NumaInfo> &numaInfos)
{
    mempooling::exportV2::NumaInfo info;
    info.metaData.numaId = 0;
    info.metaData.socketId = 1;
    numaInfos.push_back(info);
    return MEM_POOLING_OK;
}

MpResult GetNumaInfoImmediatelyMsgMockFailed(std::vector<mempooling::exportV2::NumaInfo> &numaInfos)
{
    return MEM_POOLING_ERROR;
}

MpResult GetNumaInfoImmediatelyMsgMockEmpty(std::vector<mempooling::exportV2::NumaInfo> &numaInfos)
{
    return MEM_POOLING_OK;
}

uint32_t UbseRpcSendForMsgMockSuccess(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData,
                                      void *ctx, const UbseComRespHandler &handler)
{
    auto *resultList = static_cast<std::vector<VmNumaInfoWithSocket> *>(ctx);
    VmNumaInfoWithSocket info;
    info.pid = 1234;
    info.localNumaId = 0;
    info.remoteNumaId = 1;
    info.localUsedMem = 1024;
    info.remoteUsedMem = 2048;
    info.socketId = 1;
    resultList->push_back(info);
    return MEM_POOLING_OK;
}

uint32_t UbseRpcSendForMsgMockFailed(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData,
                                     void *ctx, const UbseComRespHandler &handler)
{
    return MEM_POOLING_ERROR;
}

uint32_t UbseRpcSendForMsgMockEmpty(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData,
                                    void *ctx, const UbseComRespHandler &handler)
{
    return MEM_POOLING_OK;
}

uint32_t UbseGetMasterNodeIdMsgMockSuccess(std::string &masterNodeId)
{
    masterNodeId = "masterNode";
    return 0;
}

uint32_t UbseGetMasterNodeIdMsgMockFailed(std::string &masterNodeId)
{
    return 1;
}

uint32_t UbseGetMasterNodeIdMsgMockNotMaster(std::string &masterNodeId)
{
    masterNodeId = "otherMaster";
    return 0;
}

uint32_t UbseGetStandbyInfoMsgMockSuccess(UbseRoleInfo &roleInfo)
{
    roleInfo.nodeId = "standbyNode";
    return 0;
}

uint32_t UbseGetStandbyInfoMsgMockFailed(UbseRoleInfo &roleInfo)
{
    return 1;
}

MpResult SyncDataToNodeMsgMockSuccess(MpSyncDataHelper *, const std::vector<std::string> &nodeIdList,
                                      const uint32_t &opCode, const UbseByteBuffer &data)
{
    return MEM_POOLING_OK;
}

MpResult SyncDataToNodeMsgMockFailed(MpSyncDataHelper *, const std::vector<std::string> &nodeIdList,
                                     const uint32_t &opCode, const UbseByteBuffer &data)
{
    return MEM_POOLING_ERROR;
}

uint32_t UbseGetCurrentNodeIdMsgMockSuccess(std::string &currentNodeId)
{
    currentNodeId = "currentNode";
    return 0;
}

uint32_t UbseGetCurrentNodeIdMsgMockFailed(std::string &currentNodeId)
{
    return 1;
}

uint32_t UbseStoragePutDataMsgMockSuccess(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data)
{
    return 0;
}

uint32_t UbseStoragePutDataMsgMockFailed(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data)
{
    return 1;
}

TEST_F(TestOverCommitMsg, GetVmNumaInfoMapRpc_Success)
{
    MOCKER_CPP(UbseRpcSend, uint32_t(*)(const UbseComEndpoint &, const UbseByteBuffer &, void *, const UbseComRespHandler &))
        .stubs()
        .will(invoke(UbseRpcSendForMsgMockSuccess));
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    std::string importNodeId = "remoteNode";
    uint16_t localNumaId = 0;
    auto ret = OverCommitMsg::GetVmNumaInfoMapRpc(importNodeId, vmNumaInfoWithSocketList, localNumaId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(vmNumaInfoWithSocketList.size(), 1);
}

TEST_F(TestOverCommitMsg, GetVmNumaInfoMapRpc_RpcSendFailed)
{
    MOCKER_CPP(UbseRpcSend, uint32_t(*)(const UbseComEndpoint &, const UbseByteBuffer &, void *, const UbseComRespHandler &))
        .stubs()
        .will(invoke(UbseRpcSendForMsgMockFailed));
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    std::string importNodeId = "remoteNode";
    uint16_t localNumaId = 0;
    auto ret = OverCommitMsg::GetVmNumaInfoMapRpc(importNodeId, vmNumaInfoWithSocketList, localNumaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, GetVmNumaInfoMapRpc_EmptyResult)
{
    MOCKER_CPP(UbseRpcSend, uint32_t(*)(const UbseComEndpoint &, const UbseByteBuffer &, void *, const UbseComRespHandler &))
        .stubs()
        .will(invoke(UbseRpcSendForMsgMockEmpty));
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    std::string importNodeId = "remoteNode";
    uint16_t localNumaId = 0;
    auto ret = OverCommitMsg::GetVmNumaInfoMapRpc(importNodeId, vmNumaInfoWithSocketList, localNumaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, GetVmNumaInfoMapLocal_Success)
{
    MOCKER_CPP(&Exporter::GetVmInfoImmediately, MpResult(*)(std::vector<VmDomainInfo> &))
        .stubs()
        .will(invoke(GetVmInfoImmediatelyMsgMockSuccess));
    MOCKER_CPP(&Exporter::GetNumaInfoImmediately, MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> &))
        .stubs()
        .will(invoke(GetNumaInfoImmediatelyMsgMockSuccess));
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t localNumaId = 0;
    auto ret = OverCommitMsg::GetVmNumaInfoMapLocal(vmNumaInfoWithSocketList, localNumaId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsg, GetVmNumaInfoMapLocal_GetVmInfoFailed)
{
    MOCKER_CPP(&Exporter::GetVmInfoImmediately, MpResult(*)(std::vector<VmDomainInfo> &))
        .stubs()
        .will(invoke(GetVmInfoImmediatelyMsgMockFailed));
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t localNumaId = 0;
    auto ret = OverCommitMsg::GetVmNumaInfoMapLocal(vmNumaInfoWithSocketList, localNumaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, GetVmNumaInfoMapLocal_EmptyVmList)
{
    MOCKER_CPP(&Exporter::GetVmInfoImmediately, MpResult(*)(std::vector<VmDomainInfo> &))
        .stubs()
        .will(invoke(GetVmInfoImmediatelyMsgMockEmpty));
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    uint16_t localNumaId = 0;
    auto ret = OverCommitMsg::GetVmNumaInfoMapLocal(vmNumaInfoWithSocketList, localNumaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, SetResponse_Success)
{
    ResponseInfoSimpo response;
    UbseByteBuffer resBuffer;
    OverCommitMsg::SetResponse(response, MEM_POOLING_OK, "test message", resBuffer);
    EXPECT_NE(resBuffer.data, nullptr);
    EXPECT_GT(resBuffer.len, 0);
    if (resBuffer.data != nullptr && resBuffer.freeFunc != nullptr) {
        resBuffer.freeFunc(resBuffer.data);
    }
}

TEST_F(TestOverCommitMsg, SyncDataToStandByNode_GetMasterFailed)
{
    MOCKER_CPP(UbseGetMasterNodeId, uint32_t(*)(std::string &))
        .stubs()
        .will(invoke(UbseGetMasterNodeIdMsgMockFailed));
    MOCKER_CPP(&OverCommitMsg::SetResponse, void(*)(ResponseInfoSimpo &, const MpResult &, const std::string &, UbseByteBuffer &))
        .stubs()
        .will(ignoreReturnValue());
    ResponseInfoSimpo response;
    UbseByteBuffer req;
    UbseByteBuffer resp;
    std::string currentNodeId = "currentNode";
    auto ret = OverCommitMsg::SyncDataToStandByNode(response, req, resp, currentNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, SyncDataToStandByNode_NotMaster)
{
    MOCKER_CPP(UbseGetMasterNodeId, uint32_t(*)(std::string &))
        .stubs()
        .will(invoke(UbseGetMasterNodeIdMsgMockNotMaster));
    ResponseInfoSimpo response;
    UbseByteBuffer req;
    UbseByteBuffer resp;
    std::string currentNodeId = "currentNode";
    auto ret = OverCommitMsg::SyncDataToStandByNode(response, req, resp, currentNodeId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsg, SyncDataToStandByNode_GetStandbyFailed)
{
    MOCKER_CPP(UbseGetMasterNodeId, uint32_t(*)(std::string &))
        .stubs()
        .will(invoke(UbseGetMasterNodeIdMsgMockSuccess));
    MOCKER_CPP(UbseGetStandbyInfo, uint32_t(*)(UbseRoleInfo &))
        .stubs()
        .will(invoke(UbseGetStandbyInfoMsgMockFailed));
    MOCKER_CPP(&OverCommitMsg::SetResponse, void(*)(ResponseInfoSimpo &, const MpResult &, const std::string &, UbseByteBuffer &))
        .stubs()
        .will(ignoreReturnValue());
    ResponseInfoSimpo response;
    UbseByteBuffer req;
    UbseByteBuffer resp;
    std::string currentNodeId = "masterNode";
    auto ret = OverCommitMsg::SyncDataToStandByNode(response, req, resp, currentNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, SyncDataToStandByNode_SyncDataFailed)
{
    MOCKER_CPP(UbseGetMasterNodeId, uint32_t(*)(std::string &))
        .stubs()
        .will(invoke(UbseGetMasterNodeIdMsgMockSuccess));
    MOCKER_CPP(UbseGetStandbyInfo, uint32_t(*)(UbseRoleInfo &))
        .stubs()
        .will(invoke(UbseGetStandbyInfoMsgMockSuccess));
    MOCKER_CPP(&MpSyncDataHelper::SyncDataToNode,
               MpResult(*)(MpSyncDataHelper *, const std::vector<std::string> &, const uint32_t &, const UbseByteBuffer &))
        .stubs()
        .will(invoke(SyncDataToNodeMsgMockFailed));
    MOCKER_CPP(&OverCommitMsg::SetResponse, void(*)(ResponseInfoSimpo &, const MpResult &, const std::string &, UbseByteBuffer &))
        .stubs()
        .will(ignoreReturnValue());
    ResponseInfoSimpo response;
    UbseByteBuffer req;
    UbseByteBuffer resp;
    std::string currentNodeId = "masterNode";
    auto ret = OverCommitMsg::SyncDataToStandByNode(response, req, resp, currentNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, SyncDataToStandByNode_Success)
{
    MOCKER_CPP(UbseGetMasterNodeId, uint32_t(*)(std::string &))
        .stubs()
        .will(invoke(UbseGetMasterNodeIdMsgMockSuccess));
    MOCKER_CPP(UbseGetStandbyInfo, uint32_t(*)(UbseRoleInfo &))
        .stubs()
        .will(invoke(UbseGetStandbyInfoMsgMockSuccess));
    MOCKER_CPP(&MpSyncDataHelper::SyncDataToNode,
               MpResult(*)(MpSyncDataHelper *, const std::vector<std::string> &, const uint32_t &, const UbseByteBuffer &))
        .stubs()
        .will(invoke(SyncDataToNodeMsgMockSuccess));
    ResponseInfoSimpo response;
    UbseByteBuffer req;
    UbseByteBuffer resp;
    std::string currentNodeId = "masterNode";
    auto ret = OverCommitMsg::SyncDataToStandByNode(response, req, resp, currentNodeId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsg, SyncBindTypeDataRecvHandler_NullData)
{
    MOCKER_CPP(&OverCommitMsg::SetResponse, void(*)(ResponseInfoSimpo &, const MpResult &, const std::string &, UbseByteBuffer &))
        .stubs()
        .will(ignoreReturnValue());
    UbseByteBuffer req;
    req.data = nullptr;
    req.len = 0;
    UbseByteBuffer resp;
    auto ret = OverCommitMsg::SyncBindTypeDataRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, SyncBindTypeDataRecvHandler_ZeroLen)
{
    MOCKER_CPP(&OverCommitMsg::SetResponse, void(*)(ResponseInfoSimpo &, const MpResult &, const std::string &, UbseByteBuffer &))
        .stubs()
        .will(ignoreReturnValue());
    uint8_t dummyData[1] = {0};
    UbseByteBuffer req;
    req.data = dummyData;
    req.len = 0;
    UbseByteBuffer resp;
    auto ret = OverCommitMsg::SyncBindTypeDataRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, SyncBindTypeDataRecvHandler_GetCurrentNodeIdFailed)
{
    MOCKER_CPP(&OverCommitMsg::SetResponse, void(*)(ResponseInfoSimpo &, const MpResult &, const std::string &, UbseByteBuffer &))
        .stubs()
        .will(ignoreReturnValue());
    MOCKER_CPP(UbseGetCurrentNodeId, uint32_t(*)(std::string &))
        .stubs()
        .will(invoke(UbseGetCurrentNodeIdMsgMockFailed));
    JSON_MAP jsonMap;
    jsonMap["testKey"] = "BIND_SINGLE";
    RmrsOutStream builder;
    builder << jsonMap;
    UbseByteBuffer req;
    req.data = builder.GetBufferPointer();
    req.len = builder.GetSize();
    req.freeFunc = nullptr;
    UbseByteBuffer resp;
    auto ret = OverCommitMsg::SyncBindTypeDataRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    delete[] req.data;
}

TEST_F(TestOverCommitMsg, SyncBindTypeDataRecvHandler_StoragePutDataFailed)
{
    MOCKER_CPP(&OverCommitMsg::SetResponse, void(*)(ResponseInfoSimpo &, const MpResult &, const std::string &, UbseByteBuffer &))
        .stubs()
        .will(ignoreReturnValue());
    MOCKER_CPP(UbseGetCurrentNodeId, uint32_t(*)(std::string &))
        .stubs()
        .will(invoke(UbseGetCurrentNodeIdMsgMockSuccess));
    MOCKER_CPP(UbseStoragePutData, uint32_t(*)(const std::string &, const std::string &, UbseByteBuffer *))
        .stubs()
        .will(invoke(UbseStoragePutDataMsgMockFailed));
    JSON_MAP jsonMap;
    jsonMap["testKey"] = "BIND_SINGLE";
    RmrsOutStream builder;
    builder << jsonMap;
    UbseByteBuffer req;
    req.data = builder.GetBufferPointer();
    req.len = builder.GetSize();
    req.freeFunc = nullptr;
    UbseByteBuffer resp;
    auto ret = OverCommitMsg::SyncBindTypeDataRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    delete[] req.data;
}

TEST_F(TestOverCommitMsg, SyncBindTypeDataRecvHandler_Success)
{
    MOCKER_CPP(&OverCommitMsg::SetResponse, void(*)(ResponseInfoSimpo &, const MpResult &, const std::string &, UbseByteBuffer &))
        .stubs()
        .will(ignoreReturnValue());
    MOCKER_CPP(UbseGetCurrentNodeId, uint32_t(*)(std::string &))
        .stubs()
        .will(invoke(UbseGetCurrentNodeIdMsgMockSuccess));
    MOCKER_CPP(UbseStoragePutData, uint32_t(*)(const std::string &, const std::string &, UbseByteBuffer *))
        .stubs()
        .will(invoke(UbseStoragePutDataMsgMockSuccess));
    MOCKER_CPP(UbseGetMasterNodeId, uint32_t(*)(std::string &))
        .stubs()
        .will(invoke(UbseGetMasterNodeIdMsgMockNotMaster));
    JSON_MAP jsonMap;
    jsonMap["testKey"] = "BIND_SINGLE";
    RmrsOutStream builder;
    builder << jsonMap;
    UbseByteBuffer req;
    req.data = builder.GetBufferPointer();
    req.len = builder.GetSize();
    req.freeFunc = nullptr;
    UbseByteBuffer resp;
    auto ret = OverCommitMsg::SyncBindTypeDataRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    delete[] req.data;
}

} // namespace mempooling::over_commit