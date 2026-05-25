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
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "ubse_com.h"
#include "ubse_common_def.h"
#include "ubse_logger.h"
#include "rmrs_serialize.h"
#define private public
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "over_commit_ucache_strategy.h"

#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
using namespace mempooling;
using namespace ubse::com;
using namespace rmrs;
using namespace ubse::log;
using namespace rmrs::serialize;
using namespace mempooling::message;
using namespace turbo::rmrs;

namespace mempooling::over_commit {
class TestOverCommitUCache : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "[TestOverCommitMsgHandler SetUp Begin]" << std::endl;
        std::cout << "[TestOverCommitMsgHandler SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[TestOverCommitMsgHandler TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        std::cout << "[TestOverCommitMsgHandler TearDown End]" << std::endl;
    }
};

uint32_t MockRackRpcSend(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                         const UbseComRespHandler& handler)
{
    uint32_t* result_ptr = static_cast<uint32_t*>(ctx);
    *result_ptr = MEM_POOLING_OK;
    return MEM_POOLING_OK;
}

uint32_t MockRackRpcSendReturnError(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                    const UbseComRespHandler& handler)
{
    uint32_t* result_ptr = static_cast<uint32_t*>(ctx);
    *result_ptr = MEM_POOLING_ERROR;
    return MEM_POOLING_OK;
}

uint32_t MockRackRpcSendForUsageRatioOK(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                        const UbseComRespHandler& handler)
{
    UCacheRatioRes res{};
    res.resCode = MEM_POOLING_OK;
    res.ucacheUsageRatio = 0.5f;
    UCacheRatioRes* result_ptr = static_cast<UCacheRatioRes*>(ctx);
    *result_ptr = res;
    return MEM_POOLING_OK;
}

uint32_t MockRackRpcSendForUsageRatioERROR(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                           const UbseComRespHandler& handler)
{
    UCacheRatioRes res{};
    res.resCode = MEM_POOLING_ERROR;
    res.ucacheUsageRatio = 0.5f;
    UCacheRatioRes* result_ptr = static_cast<UCacheRatioRes*>(ctx);
    *result_ptr = res;
    return MEM_POOLING_OK;
}

TEST_F(TestOverCommitUCache, SendUCacheMigrationStrategy_ShouldReturnERROR_WhenRackRpcSendFailed)
{
    MOCKER_CPP(&UbseRpcSend, uint32_t (*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                          const UbseComRespHandler& handler))
        .stubs()
        .will(invoke(MockRackRpcSendReturnError));
    UCacheMigrationStrategyParam ucacheStrategy{};
    std::string srcNid = "Node0";
    ASSERT_EQ(SendUCacheMigrationStrategy(ucacheStrategy, srcNid), MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitUCache, SendUCacheMigrationStrategy_ShouldReturnOK_WhenRackRpcSendSucceed)
{
    MOCKER_CPP(&UbseRpcSend, uint32_t (*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                          const UbseComRespHandler& handler))
        .stubs()
        .will(invoke(MockRackRpcSend));
    UCacheMigrationStrategyParam ucacheStrategy{};
    std::string srcNid = "Node0";
    ASSERT_EQ(SendUCacheMigrationStrategy(ucacheStrategy, srcNid), MEM_POOLING_OK);
}

TEST_F(TestOverCommitUCache, CheckAndStopUCacheMigration_ShouldReturnERROR_WhenRackRpcSendFailed)
{
    MOCKER_CPP(&UbseRpcSend, uint32_t (*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                          const UbseComRespHandler& handler))
        .stubs()
        .will(invoke(MockRackRpcSendReturnError));
    std::string srcNid = "Node0";
    CheckAndStopUCacheMigration(srcNid);
}

TEST_F(TestOverCommitUCache, CheckAndStopUCacheMigration_ShouldReturnOK_WhenRackRpcSendSucceed)
{
    MOCKER_CPP(&UbseRpcSend, uint32_t (*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                          const UbseComRespHandler& handler))
        .stubs()
        .will(invoke(MockRackRpcSend));
    std::string srcNid = "Node0";
    CheckAndStopUCacheMigration(srcNid);
}

static uint32_t MockOsturboFunctionCallerReturn0(const std::string& function, const TurboByteBuffer& params,
                                                 TurboByteBuffer& result)
{
    return 0;
}

static uint32_t MockRmrsUCacheMigrateStrategySucceed(const UCacheMigrationStrategyParam& uCacheMigrationStrategyParam,
                                                     ResCode& resCode)
{
    return 0;
}

static uint32_t MockRmrsUCacheMigrateStrategyFailed(const UCacheMigrationStrategyParam& uCacheMigrationStrategyParam,
                                                    ResCode& resCode)
{
    return 1;
}

static uint32_t MockRmrsUCacheMigrateStopSucceed(ResCode& resCode)
{
    return 0;
}

static uint32_t MockRmrsUCacheMigrateStopFailed(ResCode& resCode)
{
    return 1;
}

static uint32_t MockRmrsUpdateUCacheRatioSucceed(const MigrationInfoParam& migrationInfoParam,
                                                 UCacheRatioRes& uCacheRatioRes)
{
    return 0;
}

TEST_F(TestOverCommitUCache, UCacheSendMigrationStrategyRecvHandlerSuccess)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;
    UCacheMigrationStrategyParam param;
    std::vector<uint16_t> emptyNumaIds;
    std::vector<pid_t> emptyPids;
    param.localNumaId = 0;
    param.remoteNumaIds = emptyNumaIds;
    param.pids = emptyPids;
    param.ucacheUsageRatio = 0.0;
    RmrsOutStream builder;
    builder << param;
    req.data = builder.GetBufferPointer();
    req.len = builder.GetSize();
    req.freeFunc = nullptr;

    MempoolingMessage::rmrsUCacheMigrateStrategy = &MockRmrsUCacheMigrateStrategySucceed;
    UCacheSendMigrationStrategyRecvHandler(req, resp);

    MempoolingMessage::rmrsUCacheMigrateStrategy = &MockRmrsUCacheMigrateStrategyFailed;
    UCacheSendMigrationStrategyRecvHandler(req, resp);
}

TEST_F(TestOverCommitUCache, UCacheStopMigrationRecvHandlerSuccess)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    turbo::rmrs::ResCode blank{};
    RmrsOutStream builder;
    builder << blank;
    req.data = builder.GetBufferPointer();
    req.len = builder.GetSize();
    req.freeFunc = nullptr;

    MempoolingMessage::rmrsUCacheMigrateStop = &MockRmrsUCacheMigrateStopSucceed;
    UCacheStopMigrationRecvHandler(req, resp);

    MempoolingMessage::rmrsUCacheMigrateStop = &MockRmrsUCacheMigrateStopFailed;
    UCacheStopMigrationRecvHandler(req, resp);
}

TEST_F(TestOverCommitUCache, UCacheSendMigrationStrategyResHandlerSuccess)
{
    UbseByteBuffer resp;
    turbo::rmrs::ResCode result;
    uint32_t rescode = MEM_POOLING_OK;
    result.resCode = rescode;
    RmrsOutStream builder;
    builder << result;
    resp = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    UCacheSendMigrationStrategyResHandler(&result, resp, rescode);
}

TEST_F(TestOverCommitUCache, UCacheSendMigrationStrategyResHandlerFail)
{
    UbseByteBuffer resp;
    turbo::rmrs::ResCode result{};
    uint32_t rescode = MEM_POOLING_ERROR;
    UCacheSendMigrationStrategyResHandler(&result, resp, rescode);
}

TEST_F(TestOverCommitUCache, UCacheStopMigrationResHandlerSuccess)
{
    UbseByteBuffer resp;
    turbo::rmrs::ResCode result;
    uint32_t rescode = MEM_POOLING_OK;
    result.resCode = rescode;
    RmrsOutStream builder;
    builder << result;
    resp = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    UCacheStopMigrationResHandler(&result, resp, rescode);
}

TEST_F(TestOverCommitUCache, UCacheStopMigrationResHandlerFail)
{
    UbseByteBuffer resp;
    turbo::rmrs::ResCode result{};
    uint32_t rescode = MEM_POOLING_ERROR;
    UCacheStopMigrationResHandler(&result, resp, rescode);
}

TEST_F(TestOverCommitUCache, UpdateUCacheRatioRecvHandlerSuccess)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;
    MigrationInfoParam param;
    std::vector<pid_t> emptyPids;
    param.borrowMemKB = 0;
    param.pids = emptyPids;

    RmrsOutStream builder;
    builder << param;
    req.data = builder.GetBufferPointer();
    req.len = builder.GetSize();
    req.freeFunc = nullptr;

    MempoolingMessage::rmrsUpdateUCacheRatio = &MockRmrsUpdateUCacheRatioSucceed;
    UpdateUCacheRatioRecvHandler(req, resp);
}

TEST_F(TestOverCommitUCache, UpdateUCacheRatioResHandler_Failed)
{
    UbseByteBuffer resp;
    turbo::rmrs::UCacheRatioRes result{};
    uint32_t rescode = MEM_POOLING_ERROR;

    UpdateUCacheRatioResHandler(&result, resp, rescode);
}

TEST_F(TestOverCommitUCache, UpdateUCacheRatioResHandler_Success)
{
    UbseByteBuffer resp;
    turbo::rmrs::UCacheRatioRes result{};
    uint32_t rescode = MEM_POOLING_OK;

    UpdateUCacheRatioResHandler(&result, resp, rescode);
}

TEST_F(TestOverCommitUCache, GenUCacheMigrationStrategy_Success)
{
    int16_t localNumaId{};
    std::vector<pid_t> pids{0};
    std::vector<uint16_t> remoteNumaIds{0};
    std::string nodeId{};
    UCacheMigrationStrategyParam ucacheStrategy{};

    MOCKER(&GetUcacheUsageRatio).stubs().will(returnValue(0.0f));

    GenUCacheMigrationStrategy(localNumaId, pids, remoteNumaIds, nodeId, ucacheStrategy);
}

TEST_F(TestOverCommitUCache, UpdateUcacheUsageRatio_Success)
{
    std::vector<pid_t> pids{0};
    uint64_t borrowMemKB{};
    std::string nodeId = "0";

    MOCKER_CPP(&UbseRpcSend,
               uint32_t (*)(const UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(invoke(MockRackRpcSendForUsageRatioOK));

    UpdateUcacheUsageRatio(pids, borrowMemKB, nodeId);
    GlobalMockObject::verify();
}

TEST_F(TestOverCommitUCache, UpdateUcacheUsageRatio_Failed)
{
    std::vector<pid_t> pids{0};
    uint64_t borrowMemKB{};
    std::string nodeId = "0";
    const float ratioDefault = 0.0f;

    MOCKER_CPP(&UbseRpcSend,
               uint32_t (*)(const UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(invoke(MockRackRpcSendForUsageRatioERROR));

    EXPECT_EQ(GetUcacheUsageRatio(nodeId), ratioDefault);

    UpdateUcacheUsageRatio(pids, borrowMemKB, nodeId);
}

TEST_F(TestOverCommitUCache, UpdateLocalUcacheUsageRatio_Valid)
{
    float ratioOK = 0.2f;
    UpdateLocalUcacheUsageRatio(ratioOK);
}

TEST_F(TestOverCommitUCache, UpdateLocalUcacheUsageRatio_TooSmall)
{
    const float ratioDefault = 0.0f;
    float ratioTooSmall = -0.1f;

    UpdateLocalUcacheUsageRatio(ratioTooSmall);
    EXPECT_EQ(GetLocalUcacheUsageRatio(), ratioDefault);
}

TEST_F(TestOverCommitUCache, UpdateLocalUcacheUsageRatio_TooLarge)
{
    const float ratioDefault = 0.0f;
    float ratioTooLarge = 0.6f;

    UpdateLocalUcacheUsageRatio(ratioTooLarge);
    EXPECT_EQ(GetLocalUcacheUsageRatio(), ratioDefault);
}
} // namespace mempooling::over_commit
