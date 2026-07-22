/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_ctrl_q_msg_proxy.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"
#include "src/include/ubse_error.h"

using namespace ubse::mti::ctrl_q;
using namespace ubse::common::def;

// Forward declarations for internal functions/structs defined in ubse_ctrl_q_msg_proxy.cpp
namespace ubse::mti::ctrl_q {
bool CheckSeq(uint16_t sendSeq, uint16_t recvSeq);
} // namespace ubse::mti::ctrl_q

namespace ubse::mti::ctrl_q::ut {

void TestUbseCtrlQMsgProxy::SetUp()
{
    Test::SetUp();
}

void TestUbseCtrlQMsgProxy::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// Mock implementation for ICtrlQReqMsg
class MockCtrlQReqMsg : public ICtrlQReqMsg {
public:
    MockCtrlQReqMsg(uint8_t opCode, uint8_t bbNum = 1) : ICtrlQReqMsg(opCode, bbNum) {}

    UbseResult EncodeReqMsg() override
    {
        return UBSE_OK;
    }
};

// Mock implementation for ICtrlQRespMsg
class MockCtrlQRespMsg : public ICtrlQRespMsg {
public:
    UbseResult DecodeRespMsg(const CtrlQRespMessage& msg) override
    {
        return UBSE_OK;
    }
};

TEST_F(TestUbseCtrlQMsgProxy, GetInstance_ReturnsSameInstance)
{
    auto& instance1 = CtrlQMsgProxy::GetInstance();
    auto& instance2 = CtrlQMsgProxy::GetInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_EncodeReqMsgFailed)
{
    MockCtrlQReqMsg reqMsg(1);
    MockCtrlQRespMsg respMsg;

    // Mock EncodeReqMsg to fail (virtual function requires MOCKER_CPP_VIRTUAL)
    MOCKER_CPP_VIRTUAL(&reqMsg, &MockCtrlQReqMsg::EncodeReqMsg).stubs().will(returnValue(UBSE_ERROR));

    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

// ===== Tests for CheckSeq internal function =====

TEST_F(TestUbseCtrlQMsgProxy, CheckSeq_ValidSeq_ReturnsTrue)
{
    // SEQ_MASK is 0x8000, so valid recvSeq has high bit set
    uint16_t sendSeq = 100;
    uint16_t recvSeq = 100 | 0x8000; // set high bit
    EXPECT_TRUE(CheckSeq(sendSeq, recvSeq));
}

TEST_F(TestUbseCtrlQMsgProxy, CheckSeq_InvalidSeqHighBitNotSet_ReturnsFalse)
{
    uint16_t sendSeq = 100;
    uint16_t recvSeq = 100; // high bit not set
    EXPECT_FALSE(CheckSeq(sendSeq, recvSeq));
}

TEST_F(TestUbseCtrlQMsgProxy, CheckSeq_SeqMismatch_ReturnsFalse)
{
    uint16_t sendSeq = 100;
    uint16_t recvSeq = 200 | 0x8000; // high bit set but seq mismatch
    EXPECT_FALSE(CheckSeq(sendSeq, recvSeq));
}

TEST_F(TestUbseCtrlQMsgProxy, CheckSeq_ZeroRecvSeq_ReturnsFalse)
{
    uint16_t sendSeq = 0;
    uint16_t recvSeq = 0; // high bit not set
    EXPECT_FALSE(CheckSeq(sendSeq, recvSeq));
}

TEST_F(TestUbseCtrlQMsgProxy, CheckSeq_BothZeroWithHighBit_ReturnsTrue)
{
    uint16_t sendSeq = 0;
    uint16_t recvSeq = 0x8000; // high bit set, low bits 0
    EXPECT_TRUE(CheckSeq(sendSeq, recvSeq));
}

// ===== Tests for SendRequest additional paths =====

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_TooManyBlocks_Failed)
{
    // Create reqMsg with more than MAX_BASIC_BLOCK_NUM blocks
    MockCtrlQReqMsg reqMsg(1, MAX_BASIC_BLOCK_NUM + 1);
    MockCtrlQRespMsg respMsg;

    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_OpenDevFailed_ReturnsError)
{
    MockCtrlQReqMsg reqMsg(1);
    MockCtrlQRespMsg respMsg;

    // No mock - real open("/dev/bandbridge") will fail since device doesn't exist
    // This covers SendRequest success path up to SendCtrlQMsg open() failure
    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

} // namespace ubse::mti::ctrl_q::ut