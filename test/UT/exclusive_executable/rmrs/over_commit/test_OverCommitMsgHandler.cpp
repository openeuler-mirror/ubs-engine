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

#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include "over_commit_msg_handler.h"
#undef private

#include "ubse_def.h"
#include "ubse_com.h"
#include "mem_manager.h"
#include "mempooling_interface.h"
#include "mempooling_message.h"
#include "mp_error.h"
#include "mp_smap_helper.h"
#include "mp_smap_module.h"
#include "numa_info.h"
#include "over_commit_mem_migrate_trans_msg.h"
#include "over_commit_serializer.h"
#include "page_file_helper.h"
#include "set_smap_remote_numa_info_trans_msg.h"
#include "smap_remote_process_query_trans_msg.h"
#include "smap_remove_trans_msg.h"
#include "vm_mem_migrate_strategy.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace mempooling;
using namespace std;
using namespace ubse::com;
using namespace mempooling::smap;
namespace mempooling::over_commit {
class TestOverCommitMsgHandler : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestOverCommitMsgHandler SetUp Begin]" << endl;
        cout << "[TestOverCommitMsgHandler SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestOverCommitMsgHandler TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestOverCommitMsgHandler TearDown End]" << endl;
    }
};

void MakeMigrateHandlerRequest(UbseByteBuffer &req)
{
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfoWithSrc memBorrowInfoWithSrc = {.srcNumaId = 0, .presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    memBorrowInfoWithSrcs.emplace_back(memBorrowInfoWithSrc);
    MemMigrateResult memMigrateResult = {.pid = 0, .remoteNumaId = 0, .size = 1, .maxRatio = 1};
    std::vector<MemMigrateResult> memMigrateResults;
    memMigrateResults.emplace_back(memMigrateResult);
    OverCommitMemMigrateTrans overCommitMemMigrateTrans = {.srcMemoryBorrowParam = srcMemoryBorrowParam,
                                                           .memBorrowInfoWithSrcs = memBorrowInfoWithSrcs,
                                                           .memMigrateResults = memMigrateResults};
    OverCommitMemMigrateTransMsg overCommitMemMigrateTransMsg = OverCommitMemMigrateTransMsg(overCommitMemMigrateTrans);
    RmrsOutStream builder;
    builder << overCommitMemMigrateTransMsg;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize()};
}

MpResult CollectBorrowRecordsMock(MemManager *This, const std::string nodeId, std::vector<BorrowRecord> &borrowRecords)
{
    BorrowRecord record;
    record.size = 1024;
    record.borrowNode = nodeId;
    record.borrowMemId.push_back(123);
    borrowRecords.push_back(record);
    return 0;
}

SetSmapRemoteNumaInfoFunc MockGetSetSmapRemoteNumaInfo()
{
    return nullptr;
}

int MockSetSmapRemoteNumaInfoFuncReturnError(RemoteNumaInfo *remoteNumaInfo)
{
    return MEM_POOLING_ERROR;
}

SetSmapRemoteNumaInfoFunc MockGetSetSmapRemoteNumaInfoFail()
{
    return MockSetSmapRemoteNumaInfoFuncReturnError;
}

int MockSetSmapRemoteNumaInfoFuncReturnOK(RemoteNumaInfo *remoteNumaInfo)
{
    return MEM_POOLING_OK;
}

SetSmapRemoteNumaInfoFunc MockGetSetSmapRemoteNumaInfoOk()
{
    return MockSetSmapRemoteNumaInfoFuncReturnOK;
}

void MakeSetSmapRemoteNumaHandlerRequest(UbseByteBuffer &req)
{
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoTrans setSmapRemoteNumaInfoTrans = {.srcMemoryBorrowParam = srcMemoryBorrowParam,
                                                             .memBorrowInfos = memBorrowInfos};
    SetSmapRemoteNumaInfoTransMsg setSmapRemoteNumaInfoTransMsg =
        SetSmapRemoteNumaInfoTransMsg(setSmapRemoteNumaInfoTrans);
    RmrsOutStream builder;
    builder << setSmapRemoteNumaInfoTransMsg;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize()};
}

TEST_F(TestOverCommitMsgHandler, SetSmapRemoteNumaHandlerFail1)
{
    OverCommitMsgHandler overCommitMsgHandler;
    UbseByteBuffer req;
    MakeSetSmapRemoteNumaHandlerRequest(req);
    UbseByteBuffer resp;
    MOCKER_CPP(SmapModule::GetSetSmapRemoteNumaInfo, SetSmapRemoteNumaInfoFunc(*)())
        .stubs()
        .will(invoke(MockGetSetSmapRemoteNumaInfo));
    ASSERT_EQ(overCommitMsgHandler.SetSmapRemoteNumaHandler(req, resp), MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, SetSmapRemoteNumaHandlerFail2)
{
    OverCommitMsgHandler overCommitMsgHandler;
    UbseByteBuffer req;
    MakeSetSmapRemoteNumaHandlerRequest(req);
    UbseByteBuffer resp;
    MOCKER_CPP(SmapModule::GetSetSmapRemoteNumaInfo, SetSmapRemoteNumaInfoFunc(*)())
        .stubs()
        .will(invoke(MockGetSetSmapRemoteNumaInfoFail));
    ASSERT_EQ(overCommitMsgHandler.SetSmapRemoteNumaHandler(req, resp), MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, SetSmapRemoteNumaHandlerSuccess)
{
    OverCommitMsgHandler overCommitMsgHandler;
    UbseByteBuffer req;
    MakeSetSmapRemoteNumaHandlerRequest(req);
    UbseByteBuffer resp;
    MOCKER_CPP(SmapModule::GetSetSmapRemoteNumaInfo, SetSmapRemoteNumaInfoFunc(*)())
        .stubs()
        .will(invoke(MockGetSetSmapRemoteNumaInfoOk));
    ASSERT_EQ(overCommitMsgHandler.SetSmapRemoteNumaHandler(req, resp), MEM_POOLING_OK);
}

SmapRemoveFunc MockGetSmapRemoveFunc()
{
    return nullptr;
}

int MockSmapRemoveFuncReturnError(RemoveMsg *removeMsg, int param)
{
    return MEM_POOLING_ERROR;
}

SmapRemoveFunc MockGetSmapRemoveFuncFail()
{
    return MockSmapRemoveFuncReturnError;
}

int MockSmapRemoveFuncReturnOk(RemoveMsg *removeMsg, int param)
{
    return MEM_POOLING_OK;
}

SmapRemoveFunc MockGetSmapRemoveFuncOk()
{
    return MockSmapRemoveFuncReturnOk;
}

void MakeRemoveHandlerRequest(UbseByteBuffer &req)
{
    std::vector<pid_t> pids = {0, 1};
    SmapRemoveTrans smapRemoveTrans = {.pids = pids};
    SmapRemoveTransMsg smapRemoveTransMsg = SmapRemoveTransMsg(smapRemoveTrans);
    RmrsOutStream builder;
    builder << smapRemoveTransMsg;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize()};
}

TEST_F(TestOverCommitMsgHandler, RemoveHandlerFail1)
{
    OverCommitMsgHandler overCommitMsgHandler;
    UbseByteBuffer req;
    MakeRemoveHandlerRequest(req);
    UbseByteBuffer resp;
    MOCKER_CPP(SmapModule::GetSmapRemoveFunc, SmapRemoveFunc(*)()).stubs().will(invoke(MockGetSmapRemoveFunc));
    ASSERT_EQ(overCommitMsgHandler.RemoveHandler(req, resp), MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, RemoveHandlerFail2)
{
    OverCommitMsgHandler overCommitMsgHandler;
    UbseByteBuffer req;
    MakeRemoveHandlerRequest(req);
    UbseByteBuffer resp;
    MOCKER_CPP(SmapModule::GetSmapRemoveFunc, SmapRemoveFunc(*)()).stubs().will(invoke(MockGetSmapRemoveFuncFail));
    ASSERT_EQ(overCommitMsgHandler.RemoveHandler(req, resp), MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, RemoveHandlerSuccess)
{
    OverCommitMsgHandler overCommitMsgHandler;
    UbseByteBuffer req;
    MakeRemoveHandlerRequest(req);
    UbseByteBuffer resp;
    MOCKER_CPP(SmapModule::GetSmapRemoveFunc, SmapRemoveFunc(*)()).stubs().will(invoke(MockGetSmapRemoveFuncOk));
    ASSERT_EQ(overCommitMsgHandler.RemoveHandler(req, resp), MEM_POOLING_OK);
}

void ProcessQueryHandlerRequest(UbseByteBuffer &req)
{
    std::vector<uint32_t> numaIds = {0, 1};
    SmapRemoteProcessQueryTrans smapRemoteProcessQueryTrans = {.numaIds = numaIds};
    SmapRemoteProcessQueryTransMsg smapRemoteProcessQueryTransMsg =
        SmapRemoteProcessQueryTransMsg(smapRemoteProcessQueryTrans);
    RmrsOutStream builder;
    builder << smapRemoteProcessQueryTransMsg;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize()};
}

int MockSmapQueryFuncReturnOk(int numaId, struct ProcessPayload *processPayloads, int num, int *len)
{
    processPayloads[0] = {.pid = 123};
    *len = 1;
    return MEM_POOLING_OK;
}

int MockSmapQueryFuncReturnERROR(int numaId, struct ProcessPayload *processPayloads, int num, int *len)
{
    processPayloads[0] = {.pid = 123};
    *len = 1;
    return MEM_POOLING_ERROR;
}

SmapGetRemotePidsFunc MockGetSmapQuryFuncOk()
{
    return MockSmapQueryFuncReturnOk;
}

SmapGetRemotePidsFunc MockGetSmapQuryFuncERR()
{
    return MockSmapQueryFuncReturnERROR;
}

TEST_F(TestOverCommitMsgHandler, ProcessQueryHandlerSuccess)
{
    OverCommitMsgHandler overCommitMsgHandler;
    UbseByteBuffer req;
    ProcessQueryHandlerRequest(req);
    UbseByteBuffer resp;
    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(MockGetSmapQuryFuncOk));
    auto ret = overCommitMsgHandler.ProcessQueryHandler(req, resp);
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, ProcessQueryHandlerFailed)
{
    OverCommitMsgHandler overCommitMsgHandler;
    UbseByteBuffer req;
    ProcessQueryHandlerRequest(req);
    UbseByteBuffer resp;
    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(returnValue((SmapGetRemotePidsFunc)nullptr));
    auto ret = overCommitMsgHandler.ProcessQueryHandler(req, resp);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, ProcessQueryHandlerSuccess1)
{
    OverCommitMsgHandler overCommitMsgHandler;
    UbseByteBuffer req;
    ProcessQueryHandlerRequest(req);
    UbseByteBuffer resp;
    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(MockGetSmapQuryFuncERR));
    auto ret = overCommitMsgHandler.ProcessQueryHandler(req, resp);
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, InitUCacheOverCommitRegTest)
{
    MOCKER_CPP(&ubse::com::UbseRegRpcService,
               uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult res = InitUCacheOverCommitReg();
    ASSERT_EQ(res, MEM_POOLING_ERROR);
    GlobalMockObject::verify();

    MOCKER_CPP(&ubse::com::UbseRegRpcService,
               uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(MEM_POOLING_OK))
        .then(returnValue(MEM_POOLING_ERROR));
    res = InitUCacheOverCommitReg();
    ASSERT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, InitExportRegTest)
{
    MOCKER_CPP(&ubse::com::UbseRegRpcService,
               uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult res = InitExportReg();
    ASSERT_EQ(res, 1);
}

TEST_F(TestOverCommitMsgHandler, InitExportRegTest1)
{
    MOCKER_CPP(&ubse::com::UbseRegRpcService,
               uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(MEM_POOLING_OK))
        .then(returnValue(MEM_POOLING_ERROR));

    MpResult res = InitExportReg();
    ASSERT_EQ(res, 1);
}

TEST_F(TestOverCommitMsgHandler, InitExportRegTest2)
{
    MOCKER_CPP(&ubse::com::UbseRegRpcService,
               uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(MEM_POOLING_OK))
        .then(returnValue(MEM_POOLING_ERROR));

    MpResult res = InitExportReg();
    ASSERT_EQ(res, 1);
}

TEST_F(TestOverCommitMsgHandler, InitExportRegTest3)
{
    MOCKER_CPP(&ubse::com::UbseRegRpcService,
               uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(MEM_POOLING_OK))
        .then(returnValue(MEM_POOLING_OK));

    MpResult res = InitExportReg();
    ASSERT_EQ(res, 0);
}

static uint32_t MockOsturboFunctionCallerReturn0(const std::string &function, const TurboByteBuffer &params,
                                                 TurboByteBuffer &result)
{
    return 0;
}

static uint32_t MockOsturboFunctionCallerReturn1(const std::string &function, const TurboByteBuffer &params,
                                                 TurboByteBuffer &result)
{
    return 1;
}

static uint32_t MockOsturboFunctionCallerReturn3(const std::string &function, const TurboByteBuffer &params,
                                                 TurboByteBuffer &result)
{
    return 3;
}

static uint32_t MockRmrsPidNumaInfoCollectReturn0(const turbo::rmrs::PidNumaInfoCollectParam &pidNumaInfoCollectParam,
                                                  turbo::rmrs::PidNumaInfoCollectResult &pidNumaInfoCollectResult)
{
    return 0;
}

static uint32_t MockRmrsPidNumaInfoCollectReturn1(const turbo::rmrs::PidNumaInfoCollectParam &pidNumaInfoCollectParam,
                                                  turbo::rmrs::PidNumaInfoCollectResult &pidNumaInfoCollectResult)
{
    return 1;
}

static uint32_t MockRmrsPidNumaInfoCollectReturn2(const turbo::rmrs::PidNumaInfoCollectParam &pidNumaInfoCollectParam,
                                                  turbo::rmrs::PidNumaInfoCollectResult &pidNumaInfoCollectResult)
{
    return 3;
}

TEST_F(TestOverCommitMsgHandler, PidNumaInfoCollectRecvHandlerSucceed)
{
    UbseByteBuffer req;
    req.len  = 1;
    req.data = new uint8_t[req.len];
    UbseByteBuffer resp;

    MempoolingMessage::rmrsPidNumaInfoCollect = &MockRmrsPidNumaInfoCollectReturn0;
    MpResult res = PidNumaInfoCollectRecvHandler(req, resp);
    ASSERT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, PidNumaInfoCollectRecvHandlerFailed)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    MempoolingMessage::rmrsPidNumaInfoCollect = &MockRmrsPidNumaInfoCollectReturn1;
    MpResult res = PidNumaInfoCollectRecvHandler(req, resp);
    ASSERT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, PidNumaInfoCollectRecvHandlerConnectFailed)
{
    UbseByteBuffer req;
    req.len  = 1;
    req.data = new uint8_t[req.len];
    UbseByteBuffer resp;

    MempoolingMessage::rmrsPidNumaInfoCollect = &MockRmrsPidNumaInfoCollectReturn2;
    MpResult res = PidNumaInfoCollectRecvHandler(req, resp);
    ASSERT_EQ(res, MEM_POOLING_TURBO_CONNECT_ERROR);
}

TEST_F(TestOverCommitMsgHandler, PidNumaInfoCollectRpcResHandlerTestFailed)
{
    UbseByteBuffer respData;
    respData.len = 1;
    respData.data = new uint8_t[respData.len];
    mempooling::over_commit::PidNumaInfoCollectResult result;
    void *ctx = &result;
    uint32_t resCode = 1;

    PidNumaInfoCollectRpcResHandler(ctx, respData, resCode);
    EXPECT_TRUE(result.pidInfoList.empty());
    // 清理
    delete[] respData.data;
}

TEST_F(TestOverCommitMsgHandler, PidNumaInfoCollectRpcResHandlerTestSucceed)
{
    UbseByteBuffer respData;
    respData.len = 1;
    respData.data = new uint8_t[respData.len];
    mempooling::over_commit::PidNumaInfoCollectResult result;
    void *ctx = &result;
    uint32_t resCode = 0;

    PidNumaInfoCollectRpcResHandler(ctx, respData, resCode);
    EXPECT_TRUE(result.pidInfoList.empty());
    // 清理
    delete[] respData.data;
}

TEST_F(TestOverCommitMsgHandler, SetResponseSuccess)
{
    ResponseInfoSimpo response;
    MpResult retCode;
    std::string msg;
    UbseByteBuffer resBuffer;
    OverCommitMsgHandler::SetResponse(response, retCode, msg, resBuffer);
    ASSERT_NE(resBuffer.data, nullptr);
}

} // namespace mempooling::over_commit