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
#include "set_smap_remote_numa_info_send.h"

#include "ubse_def.h"
#include "ubse_com.h"
#include "mempooling_interface.h"
#include "mp_error.h"
#include "response_info_simpo.h"
#include "set_smap_remote_numa_info_trans_msg.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace mempooling;
using namespace std;
using namespace ubse::com;
namespace mempooling::over_commit {
class TestSetSmapRemoteNumaInfoSend : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestSetSmapRemoteNumaInfoSend SetUp Begin]" << endl;
        cout << "[TestSetSmapRemoteNumaInfoSend SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestSetSmapRemoteNumaInfoSend TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestSetSmapRemoteNumaInfoSend TearDown End]" << endl;
    }
};

TEST_F(TestSetSmapRemoteNumaInfoSend, SendMsgCreateRequestDataFailed)
{
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoSend setSmapRemoteNumaInfoSend =
        SetSmapRemoteNumaInfoSend(srcMemoryBorrowParam, memBorrowInfos);
    MOCKER_CPP(&SetSmapRemoteNumaInfoSend::CreateRequestData, MpResult (*)(UbseByteBuffer&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    ASSERT_EQ(setSmapRemoteNumaInfoSend.SendMsg(), MEM_POOLING_OK);
}

TEST_F(TestSetSmapRemoteNumaInfoSend, SendMsgRackRpcSendFail)
{
    // RackRpcSend失败
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoSend setSmapRemoteNumaInfoSend =
        SetSmapRemoteNumaInfoSend(srcMemoryBorrowParam, memBorrowInfos);
    MOCKER(UbseRpcSend).stubs().will(returnValue(1));
    ASSERT_EQ(setSmapRemoteNumaInfoSend.SendMsg(), 1);
}

TEST_F(TestSetSmapRemoteNumaInfoSend, SendMsgOk)
{
    // 成功
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoSend setSmapRemoteNumaInfoSend =
        SetSmapRemoteNumaInfoSend(srcMemoryBorrowParam, memBorrowInfos);
    MOCKER(UbseRpcSend).stubs().will(returnValue(0));
    ASSERT_EQ(setSmapRemoteNumaInfoSend.SendMsg(), 0);
}

TEST_F(TestSetSmapRemoteNumaInfoSend, CreateRequestDataMemcpy_sSuccess)
{
    UbseByteBuffer reqData;
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoSend setSmapRemoteNumaInfoSend =
        SetSmapRemoteNumaInfoSend(srcMemoryBorrowParam, memBorrowInfos);
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    ASSERT_EQ(setSmapRemoteNumaInfoSend.CreateRequestData(reqData), MEM_POOLING_OK);
}

TEST_F(TestSetSmapRemoteNumaInfoSend, CreateRequestDataOk)
{
    UbseByteBuffer reqData;
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoSend setSmapRemoteNumaInfoSend =
        SetSmapRemoteNumaInfoSend(srcMemoryBorrowParam, memBorrowInfos);
    ASSERT_EQ(setSmapRemoteNumaInfoSend.CreateRequestData(reqData), MEM_POOLING_OK);
}

TEST_F(TestSetSmapRemoteNumaInfoSend, RespHandlerResCodeNotOk)
{
    // resCode不ok
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoSend setSmapRemoteNumaInfoSend =
        SetSmapRemoteNumaInfoSend(srcMemoryBorrowParam, memBorrowInfos);
    UbseByteBuffer respData = {.data = reinterpret_cast<uint8_t*>(std::string("").data()), .len = 0};
    setSmapRemoteNumaInfoSend.RespHandler(static_cast<void*>(&setSmapRemoteNumaInfoSend), respData, MEM_POOLING_ERROR);
}

TEST_F(TestSetSmapRemoteNumaInfoSend, RespHandlerResCodeOk1)
{
    // resCode不ok
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoSend setSmapRemoteNumaInfoSend =
        SetSmapRemoteNumaInfoSend(srcMemoryBorrowParam, memBorrowInfos);
    UbseByteBuffer respData = {.data = reinterpret_cast<uint8_t*>(std::string("").data()), .len = 0};
    setSmapRemoteNumaInfoSend.RespHandler(static_cast<void*>(&setSmapRemoteNumaInfoSend), respData, MEM_POOLING_OK);
}

TEST_F(TestSetSmapRemoteNumaInfoSend, RespHandlerResCodeOk2)
{
    // resCode不ok
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoSend setSmapRemoteNumaInfoSend =
        SetSmapRemoteNumaInfoSend(srcMemoryBorrowParam, memBorrowInfos);

    ResponseInfo responseInfo = {.code = MEM_POOLING_ERROR, .message = "error"};
    ResponseInfoSimpo responseInfoSimpo = ResponseInfoSimpo(responseInfo);
    RmrsOutStream builder;
    builder << responseInfoSimpo;
    UbseByteBuffer respData = {.data = builder.GetBufferPointer(), .len = builder.GetSize()};
    setSmapRemoteNumaInfoSend.RespHandler(static_cast<void*>(&setSmapRemoteNumaInfoSend), respData, MEM_POOLING_OK);
}

TEST_F(TestSetSmapRemoteNumaInfoSend, RespHandlerResCodeOk3)
{
    // resCode不ok
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam = {.srcNid = "test", .srcSocketId = 0, .srcNumaId = 0};
    MemBorrowInfo memBorrowInfo = {.presentNumaId = 0, .borrowSize = 1};
    std::vector<MemBorrowInfo> memBorrowInfos;
    memBorrowInfos.emplace_back(memBorrowInfo);
    SetSmapRemoteNumaInfoSend setSmapRemoteNumaInfoSend =
        SetSmapRemoteNumaInfoSend(srcMemoryBorrowParam, memBorrowInfos);
    ResponseInfo responseInfo = {.code = MEM_POOLING_OK, .message = "error"};
    ResponseInfoSimpo responseInfoSimpo = ResponseInfoSimpo(responseInfo);
    RmrsOutStream builder;
    builder << responseInfoSimpo;

    UbseByteBuffer respData = {.data = builder.GetBufferPointer(), .len = builder.GetSize()};
    setSmapRemoteNumaInfoSend.RespHandler(static_cast<void*>(&setSmapRemoteNumaInfoSend), respData, MEM_POOLING_OK);
}
} // namespace mempooling::over_commit