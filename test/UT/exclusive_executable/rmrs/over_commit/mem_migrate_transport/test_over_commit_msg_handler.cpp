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
#include "over_commit_msg_handler.h"
#include "over_commit_def.h"
#include "mp_smap_helper.h"
#include "page_file_helper.h"
#include "vm_mem_migrate_strategy.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::over_commit {
using std::cout;
using std::endl;
using namespace smap;
using namespace outinterface;

class TestOverCommitMsgHandler : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

uint32_t MempoolingMessageRmrsPidNumaInfoCollectMockSuccess(
    const turbo::rmrs::PidNumaInfoCollectParam &param,
    turbo::rmrs::PidNumaInfoCollectResult &result)
{
    turbo::rmrs::PidInfo info;
    info.pid = 1234;
    info.totalLocalUsedMem = 1024;
    info.totalRemoteUsedMem = 2048;
    info.localNumaIds.push_back(0);
    result.pidInfoList.push_back(info);
    return IPC_OK;
}

uint32_t MempoolingMessageRmrsPidNumaInfoCollectMockFailed(
    const turbo::rmrs::PidNumaInfoCollectParam &param,
    turbo::rmrs::PidNumaInfoCollectResult &result)
{
    return IPC_BAD_CONNECT;
}

uint32_t MempoolingMessageRmrsPidNumaInfoCollectMockError(
    const turbo::rmrs::PidNumaInfoCollectParam &param,
    turbo::rmrs::PidNumaInfoCollectResult &result)
{
    return IPC_ERROR;
}

TEST_F(TestOverCommitMsgHandler, PidNumaInfoCollectHandler_Success)
{
    MempoolingMessage::rmrsPidNumaInfoCollect = MempoolingMessageRmrsPidNumaInfoCollectMockSuccess;
    turbo::rmrs::PidNumaInfoCollectParam param;
    param.pidList = {1234};
    turbo::rmrs::PidNumaInfoCollectResult result;
    auto ret = PidNumaInfoCollectHandler(param, result);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, PidNumaInfoCollectHandler_ConnectFailed)
{
    MempoolingMessage::rmrsPidNumaInfoCollect = MempoolingMessageRmrsPidNumaInfoCollectMockFailed;
    turbo::rmrs::PidNumaInfoCollectParam param;
    param.pidList = {1234};
    turbo::rmrs::PidNumaInfoCollectResult result;
    auto ret = PidNumaInfoCollectHandler(param, result);
    EXPECT_EQ(ret, MEM_POOLING_TURBO_CONNECT_ERROR);
}

TEST_F(TestOverCommitMsgHandler, PidNumaInfoCollectHandler_Error)
{
    MempoolingMessage::rmrsPidNumaInfoCollect = MempoolingMessageRmrsPidNumaInfoCollectMockError;
    turbo::rmrs::PidNumaInfoCollectParam param;
    param.pidList = {1234};
    turbo::rmrs::PidNumaInfoCollectResult result;
    auto ret = PidNumaInfoCollectHandler(param, result);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, InitOverCommitReg_Success)
{
    MOCKER_CPP(&UbseRegRpcService, uint32_t(*)(const UbseComEndpoint &, UbseRpcServiceHandler))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    auto ret = InitOverCommitReg();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, InitOverCommitReg_RegFailed)
{
    MOCKER_CPP(&UbseRegRpcService, uint32_t(*)(const UbseComEndpoint &, UbseRpcServiceHandler))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto ret = InitOverCommitReg();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

int SmapRemoveMockSuccess(RemoveMsg *msg, int scene)
{
    return 0;
}

int SmapRemoveMockFailed(RemoveMsg *msg, int scene)
{
    return 1;
}

SmapRemoveFunc GetSmapRemoveFuncMockSuccess()
{
    return SmapRemoveMockSuccess;
}

SmapRemoveFunc GetSmapRemoveFuncMockFailed()
{
    return SmapRemoveMockFailed;
}

SmapRemoveFunc GetSmapRemoveFuncMockNull()
{
    return nullptr;
}

TEST_F(TestOverCommitMsgHandler, RemoveLocalHandler_GetFuncNull)
{
    MOCKER_CPP(&SmapModule::GetSmapRemoveFunc, SmapRemoveFunc(*)())
        .stubs()
        .will(invoke(GetSmapRemoveFuncMockNull));
    uint16_t presentNumaId = 1;
    std::vector<pid_t> pids = {1234, 5678};
    auto ret = OverCommitMsgHandler::RemoveLocalHandler(presentNumaId, pids);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, RemoveLocalHandler_PidsTooMany)
{
    MOCKER_CPP(&SmapModule::GetSmapRemoveFunc, SmapRemoveFunc(*)())
        .stubs()
        .will(invoke(GetSmapRemoveFuncMockSuccess));
    uint16_t presentNumaId = 1;
    std::vector<pid_t> pids;
    for (int i = 0; i < MAX_NR_REMOVE_MP + 10; i++) {
        pids.push_back(i);
    }
    auto ret = OverCommitMsgHandler::RemoveLocalHandler(presentNumaId, pids);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, RemoveLocalHandler_Success)
{
    MOCKER_CPP(&SmapModule::GetSmapRemoveFunc, SmapRemoveFunc(*)())
        .stubs()
        .will(invoke(GetSmapRemoveFuncMockSuccess));
    uint16_t presentNumaId = 1;
    std::vector<pid_t> pids = {1234, 5678};
    auto ret = OverCommitMsgHandler::RemoveLocalHandler(presentNumaId, pids);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, RemoveLocalHandler_SmapRemoveFailed)
{
    MOCKER_CPP(&SmapModule::GetSmapRemoveFunc, SmapRemoveFunc(*)())
        .stubs()
        .will(invoke(GetSmapRemoveFuncMockFailed));
    uint16_t presentNumaId = 1;
    std::vector<pid_t> pids = {1234, 5678};
    auto ret = OverCommitMsgHandler::RemoveLocalHandler(presentNumaId, pids);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, NormMigrate_SetSmapRemoteNumaInfoFailed)
{
    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(int16_t, const std::vector<MemBorrowInfoWithSrc> &))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::vector<MemMigrateResult> memMigrateResults;
    MemMigrateResult result;
    result.pid = 1234;
    result.remoteNumaId = 1;
    result.size = 1024;
    result.maxRatio = 50;
    memMigrateResults.push_back(result);
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    MemBorrowInfoWithSrc info;
    info.srcNumaId = 0;
    info.presentNumaId = 1;
    info.borrowSize = 1024;
    memBorrowInfoWithSrcs.push_back(info);
    int16_t srcNumaId = 0;
    auto ret = OverCommitMsgHandler::NormMigrate(memMigrateResults, memBorrowInfoWithSrcs, srcNumaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, NormMigrate_MigrateOutFailed)
{
    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(int16_t, const std::vector<MemBorrowInfoWithSrc> &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MpSmapHelper::MigrateOutInOverCommit,
               MpResult(*)(const std::vector<MemMigrateResult> &, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::vector<MemMigrateResult> memMigrateResults;
    MemMigrateResult result;
    result.pid = 1234;
    result.remoteNumaId = 1;
    result.size = 1024;
    result.maxRatio = 50;
    memMigrateResults.push_back(result);
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    MemBorrowInfoWithSrc info;
    info.srcNumaId = 0;
    info.presentNumaId = 1;
    info.borrowSize = 1024;
    memBorrowInfoWithSrcs.push_back(info);
    int16_t srcNumaId = 0;
    auto ret = OverCommitMsgHandler::NormMigrate(memMigrateResults, memBorrowInfoWithSrcs, srcNumaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, NormMigrate_Success)
{
    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(int16_t, const std::vector<MemBorrowInfoWithSrc> &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MpSmapHelper::MigrateOutInOverCommit,
               MpResult(*)(const std::vector<MemMigrateResult> &, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    std::vector<MemMigrateResult> memMigrateResults;
    MemMigrateResult result;
    result.pid = 1234;
    result.remoteNumaId = 1;
    result.size = 1024;
    result.maxRatio = 50;
    memMigrateResults.push_back(result);
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    MemBorrowInfoWithSrc info;
    info.srcNumaId = 0;
    info.presentNumaId = 1;
    info.borrowSize = 1024;
    memBorrowInfoWithSrcs.push_back(info);
    int16_t srcNumaId = 0;
    auto ret = OverCommitMsgHandler::NormMigrate(memMigrateResults, memBorrowInfoWithSrcs, srcNumaId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, MigrateLocalHandler_EmptyMigrateResults)
{
    outinterface::SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node1";
    srcParam.srcNumaId = 0;
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    std::vector<MemMigrateResult> memMigrateResults;
    auto ret = OverCommitMsgHandler::MigrateLocalHandler(srcParam, memBorrowInfoWithSrcs, memMigrateResults);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, MigrateLocalHandler_RebalanceEnableFailed)
{
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int(*)(pid_t *, int, int, int))
        .stubs()
        .will(returnValue(1));
    outinterface::SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node1";
    srcParam.srcNumaId = 0;
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    std::vector<MemMigrateResult> memMigrateResults;
    MemMigrateResult result;
    result.pid = 1234;
    result.remoteNumaId = 1;
    result.size = 1024;
    result.maxRatio = 50;
    memMigrateResults.push_back(result);
    auto ret = OverCommitMsgHandler::MigrateLocalHandler(srcParam, memBorrowInfoWithSrcs, memMigrateResults);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, MigrateLocalHandler_RebalanceFailed)
{
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int(*)(pid_t *, int, int, int))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&VMMemMigrateStrategy::Rebalance,
               MpResult(*)(VMMemMigrateStrategy *, const std::string &, int16_t,
                          const std::vector<pid_t> &, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    outinterface::SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node1";
    srcParam.srcNumaId = 0;
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    std::vector<MemMigrateResult> memMigrateResults;
    MemMigrateResult result;
    result.pid = 1234;
    result.remoteNumaId = 1;
    result.size = 1024;
    result.maxRatio = 50;
    memMigrateResults.push_back(result);
    auto ret = OverCommitMsgHandler::MigrateLocalHandler(srcParam, memBorrowInfoWithSrcs, memMigrateResults);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, MigrateLocalHandler_Success)
{
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int(*)(pid_t *, int, int, int))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&VMMemMigrateStrategy::Rebalance,
               MpResult(*)(VMMemMigrateStrategy *, const std::string &, int16_t,
                          const std::vector<pid_t> &, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    outinterface::SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node1";
    srcParam.srcNumaId = 0;
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    std::vector<MemMigrateResult> memMigrateResults;
    MemMigrateResult result;
    result.pid = 1234;
    result.remoteNumaId = 1;
    result.size = 1024;
    result.maxRatio = 50;
    memMigrateResults.push_back(result);
    auto ret = OverCommitMsgHandler::MigrateLocalHandler(srcParam, memBorrowInfoWithSrcs, memMigrateResults);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsgHandler, MigrateLocalHandler_NormMigrateFailed)
{
    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(int16_t, const std::vector<MemBorrowInfoWithSrc> &))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    outinterface::SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node1";
    srcParam.srcNumaId = 0;
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    MemBorrowInfoWithSrc info;
    info.srcNumaId = 0;
    info.presentNumaId = 1;
    info.borrowSize = 1024;
    memBorrowInfoWithSrcs.push_back(info);
    std::vector<MemMigrateResult> memMigrateResults;
    MemMigrateResult result;
    result.pid = 1234;
    result.remoteNumaId = 1;
    result.size = 1024;
    result.maxRatio = 50;
    memMigrateResults.push_back(result);
    auto ret = OverCommitMsgHandler::MigrateLocalHandler(srcParam, memBorrowInfoWithSrcs, memMigrateResults);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsgHandler, MigrateLocalHandler_NormMigrateSuccess)
{
    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(int16_t, const std::vector<MemBorrowInfoWithSrc> &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MpSmapHelper::MigrateOutInOverCommit,
               MpResult(*)(const std::vector<MemMigrateResult> &, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    outinterface::SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node1";
    srcParam.srcNumaId = 0;
    std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs;
    MemBorrowInfoWithSrc info;
    info.srcNumaId = 0;
    info.presentNumaId = 1;
    info.borrowSize = 1024;
    memBorrowInfoWithSrcs.push_back(info);
    std::vector<MemMigrateResult> memMigrateResults;
    MemMigrateResult result;
    result.pid = 1234;
    result.remoteNumaId = 1;
    result.size = 1024;
    result.maxRatio = 50;
    memMigrateResults.push_back(result);
    auto ret = OverCommitMsgHandler::MigrateLocalHandler(srcParam, memBorrowInfoWithSrcs, memMigrateResults);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

} // namespace mempooling::over_commit