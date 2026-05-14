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

#define private public
#include "smap_query_process_send.h"
#undef private

#include "ubse_com.h"
#include "response_info_simpo.h"

using namespace mempooling::over_commit;
using namespace ubse::com;
using namespace ::testing;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {
class TestSmapQueryProcessSend : public ::testing::Test {
protected:
    outinterface::SrcMemoryBorrowParam srcParam;
    std::vector<uint32_t> numaIds = {0, 1};
    UbseByteBuffer reqData;

    void SetUp() override
    {
        std::cout << "[TestSmapQueryProcessSend SetUp Begin]" << std::endl;
        srcParam.srcNid = "Node1";
        srcParam.srcSocketId = 0;
        srcParam.srcNumaId = 1;
        std::cout << "[TestSmapQueryProcessSend SetUp End]" << std::endl;
    }

    void TearDown() override
    {
        std::cout << "[TestSmapQueryProcessSend TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        std::cout << "[TestSmapQueryProcessSend TearDown End]" << std::endl;
    }
};

TEST_F(TestSmapQueryProcessSend, SendMsg_ShouldReturnError_WhenCreateRequestDataFailed)
{
    SmapQueryProcessSend smapQueryProcessSend(std::move(srcParam), std::move(numaIds));
    MOCKER_CPP(&SmapQueryProcessSend::CreateRequestData, MpResult(*)(UbseByteBuffer & reqData))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    EXPECT_EQ(smapQueryProcessSend.SendMsg(), MEM_POOLING_ERROR);
}

TEST_F(TestSmapQueryProcessSend, SendMsg_ShouldReturnError_WhenRackRpcSendFailed)
{
    SmapQueryProcessSend smapQueryProcessSend(std::move(srcParam), std::move(numaIds));
    MOCKER_CPP(&SmapQueryProcessSend::CreateRequestData, MpResult(*)(UbseByteBuffer & reqData))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&UbseRpcSend, uint32_t(*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                         const UbseComRespHandler& handler))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    EXPECT_EQ(smapQueryProcessSend.SendMsg(), MEM_POOLING_ERROR);
}

TEST_F(TestSmapQueryProcessSend, SendMsg_ShouldReturnError_WhenHandleResponseFailed)
{
    SmapQueryProcessSend smapQueryProcessSend(std::move(srcParam), std::move(numaIds));
    MOCKER_CPP(&SmapQueryProcessSend::CreateRequestData, MpResult(*)(UbseByteBuffer & reqData))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&UbseRpcSend, uint32_t(*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                         const UbseComRespHandler& handler))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    smapQueryProcessSend.sendResult_ = MEM_POOLING_ERROR;
    EXPECT_EQ(smapQueryProcessSend.SendMsg(), MEM_POOLING_ERROR);
}

TEST_F(TestSmapQueryProcessSend, SendMsg_ShouldReturnOk_WhenAllOperationsSucceed)
{
    SmapQueryProcessSend smapQueryProcessSend(std::move(srcParam), std::move(numaIds));
    MOCKER_CPP(&UbseRpcSend, uint32_t(*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                         const UbseComRespHandler& handler))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    smapQueryProcessSend.sendResult_ = MEM_POOLING_OK;
    EXPECT_EQ(smapQueryProcessSend.SendMsg(), MEM_POOLING_OK);
}

TEST_F(TestSmapQueryProcessSend, RespHandlerResCodeNotOk)
{
    SmapQueryProcessSend smapQueryProcessSend(std::move(srcParam), std::move(numaIds));
    UbseByteBuffer respData = {.data = reinterpret_cast<uint8_t*>(std::string("1").data()), .len = 1};
    smapQueryProcessSend.RespHandler(static_cast<void*>(&smapQueryProcessSend), respData, MEM_POOLING_ERROR);
    EXPECT_EQ(smapQueryProcessSend.sendResult_, MEM_POOLING_ERROR);
}

TEST_F(TestSmapQueryProcessSend, CreateRequestDataError_WhenMemcpySSuccess)
{
    SmapQueryProcessSend smapQueryProcessSend(std::move(srcParam), std::move(numaIds));

    MOCKER(memcpy_s).stubs().will(returnValue(MEM_POOLING_ERROR));

    UbseByteBuffer reqData;
    EXPECT_EQ(smapQueryProcessSend.CreateRequestData(reqData), MEM_POOLING_OK);
    EXPECT_EQ(smapQueryProcessSend.CreateRequestData(reqData), MEM_POOLING_OK);
}

TEST_F(TestSmapQueryProcessSend, RespHandlerResCodeError_WhenDeserializeSuccess)
{
    SmapQueryProcessSend smapQueryProcessSend(std::move(srcParam), std::move(numaIds));
    UbseByteBuffer respData = {.data = reinterpret_cast<uint8_t*>(std::string("").data()), .len = 0};
    smapQueryProcessSend.RespHandler(static_cast<void*>(&smapQueryProcessSend), respData, MEM_POOLING_OK);
    EXPECT_EQ(smapQueryProcessSend.sendResult_, MEM_POOLING_OK);
}

TEST_F(TestSmapQueryProcessSend, RespHandlerResCodeError_WhenGetErrorRetCode)
{
    SmapQueryProcessSend smapQueryProcessSend(std::move(srcParam), std::move(numaIds));
    ResponseInfo responseInfo = {.code = MEM_POOLING_ERROR, .message = "error"};
    ResponseInfoSimpo responseInfoSimpo = ResponseInfoSimpo(responseInfo);
    RmrsOutStream builder;
    builder << responseInfoSimpo;
    UbseByteBuffer respData = {.data = builder.GetBufferPointer(), .len = builder.GetSize()};
    smapQueryProcessSend.RespHandler(static_cast<void*>(&smapQueryProcessSend), respData, MEM_POOLING_OK);
    EXPECT_EQ(smapQueryProcessSend.sendResult_, MEM_POOLING_ERROR);
}

TEST_F(TestSmapQueryProcessSend, RespHandlerResCodeOk_WhenGetSuccessRetCode)
{
    SmapQueryProcessSend smapQueryProcessSend(std::move(srcParam), std::move(numaIds));
    ResponseInfo responseInfo = {.code = MEM_POOLING_OK, .message = "success"};
    ResponseInfoSimpo responseInfoSimpo = ResponseInfoSimpo(responseInfo);
    RmrsOutStream builder;
    builder << responseInfoSimpo;
    UbseByteBuffer respData = {.data = builder.GetBufferPointer(), .len = builder.GetSize()};
    smapQueryProcessSend.RespHandler(static_cast<void*>(&smapQueryProcessSend), respData, MEM_POOLING_OK);
    EXPECT_EQ(smapQueryProcessSend.sendResult_, MEM_POOLING_OK);
}
} // namespace mempooling