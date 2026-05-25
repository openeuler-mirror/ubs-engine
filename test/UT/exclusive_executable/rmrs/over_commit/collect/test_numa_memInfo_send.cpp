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

#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "ubse_com.h"
#define private public
#include "numa_memInfo_send.h"
#undef private
#include "response_info_simpo.h"
#include "rmrs_serialize.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace mempooling::over_commit {
using namespace mempooling;
using namespace ubse::com;
using namespace rmrs::serialize;
// 测试类
class TestOverCommitNumaMemInfoSend : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestOverCommitNumaMemInfoSend, SendMsgSuccess)
{
    MOCKER_CPP(&UbseRpcSend,
               uint32_t (*)(const UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(returnValue(0));
    auto numaMemInfoSend = over_commit::NumaMemInfoSend("node1", 1);
    auto ret = numaMemInfoSend.SendMsg();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitNumaMemInfoSend, SendMsgFailWhenCreateFail)
{
    MOCKER_CPP(&NumaMemInfoSend::CreateRequestData, MpResult (*)(UbseByteBuffer& reqData))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&UbseRpcSend,
               uint32_t (*)(const UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(returnValue(0));
    auto numaMemInfoSend = over_commit::NumaMemInfoSend("node1", 1);
    auto ret = numaMemInfoSend.SendMsg();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitNumaMemInfoSend, SendMsgFailWhenSendFail)
{
    MOCKER_CPP(&NumaMemInfoSend::CreateRequestData, MpResult (*)(UbseByteBuffer& reqData))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&UbseRpcSend,
               uint32_t (*)(const UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(returnValue(1));
    auto numaMemInfoSend = NumaMemInfoSend("node1", 1);
    auto ret = numaMemInfoSend.SendMsg();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitNumaMemInfoSend, CreateRequestDataSuccess)
{
    auto numaMemInfoSend = NumaMemInfoSend("node1", 1);
    UbseByteBuffer reqData{};
    auto ret = numaMemInfoSend.CreateRequestData(reqData);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitNumaMemInfoSend, CreateRequestDataFail)
{
    auto numaMemInfoSend = NumaMemInfoSend("node1", 1);
    UbseByteBuffer reqData{};
    MOCKER_CPP(&RmrsOutStream::GetBufferPointer, uint8_t* (*)())
        .stubs()
        .will(returnValue(static_cast<uint8_t*>(nullptr)));
    auto ret = numaMemInfoSend.CreateRequestData(reqData);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitNumaMemInfoSend, RespHandlerFail)
{
    auto numaMemInfoSend = NumaMemInfoSend("node1", 1);
    std::string str = "1";
    UbseByteBuffer respData = {.data = reinterpret_cast<uint8_t*>(str.data()), .len = 1};
    numaMemInfoSend.RespHandler(static_cast<void*>(&numaMemInfoSend), respData, MEM_POOLING_ERROR);
    EXPECT_EQ(numaMemInfoSend.sendResult_, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitNumaMemInfoSend, RespHandlerSuccess)
{
    ResponseInfo responseInfo = {.code = MEM_POOLING_OK, .message = "success"};
    ResponseInfoSimpo responseInfoSimpo = ResponseInfoSimpo(responseInfo);
    RmrsOutStream builder;
    builder << responseInfoSimpo;
    UbseByteBuffer respData = {.data = builder.GetBufferPointer(), .len = builder.GetSize()};
    auto numaMemInfoSend = NumaMemInfoSend("node1", 1);
    numaMemInfoSend.RespHandler(static_cast<void*>(&numaMemInfoSend), respData, MEM_POOLING_OK);
    EXPECT_EQ(numaMemInfoSend.sendResult_, MEM_POOLING_OK);
}

} // namespace mempooling::over_commit