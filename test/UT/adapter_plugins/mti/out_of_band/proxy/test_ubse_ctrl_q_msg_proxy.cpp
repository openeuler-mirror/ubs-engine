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

struct BandBridgeMbuf {
    int sendBufSize{0};
    int recvBufSize{0};
    void* sendBuf{nullptr};
    void* recvBuf{nullptr};
};
} // namespace ubse::mti::ctrl_q

namespace ubse::mti::ctrl_q::ut {
namespace {
constexpr int TEST_FD = 10;
constexpr uint16_t RESP_SEQ_VALID_MASK = 0x8000;

enum class MockIoctlRespType
{
    IOCTL_FAILED,
    RESP_RET_FAILED,
    RESP_BLOCK_NUM_ZERO,
    RESP_BLOCK_NUM_TOO_LARGE,
    RESP_SEQ_INVALID,
    RESP_SUCCESS,
};

MockIoctlRespType g_ioctlRespType = MockIoctlRespType::RESP_SUCCESS;

int MockOpen(const char* path, int flags)
{
    (void)path;
    (void)flags;
    return TEST_FD;
}

int MockClose(int fd)
{
    EXPECT_EQ(fd, TEST_FD);
    return 0;
}

int MockIoctl(int fd, unsigned long request, void* arg)
{
    (void)request;
    EXPECT_EQ(fd, TEST_FD);
    auto* msgBuf = static_cast<BandBridgeMbuf*>(arg);
    auto* sendHead = reinterpret_cast<FixedHead*>(msgBuf->sendBuf);
    auto* recvHead = reinterpret_cast<FixedHead*>(msgBuf->recvBuf);

    recvHead->ret = UBSE_OK;
    recvHead->bbNum = 1;
    recvHead->seq = sendHead->seq | RESP_SEQ_VALID_MASK;

    switch (g_ioctlRespType) {
        case MockIoctlRespType::IOCTL_FAILED:
            return UBSE_ERROR;
        case MockIoctlRespType::RESP_RET_FAILED:
            recvHead->ret = UBSE_ERROR;
            break;
        case MockIoctlRespType::RESP_BLOCK_NUM_ZERO:
            recvHead->bbNum = 0;
            break;
        case MockIoctlRespType::RESP_BLOCK_NUM_TOO_LARGE:
            recvHead->bbNum = MAX_BASIC_BLOCK_NUM + 1;
            break;
        case MockIoctlRespType::RESP_SEQ_INVALID:
            recvHead->seq = sendHead->seq;
            break;
        case MockIoctlRespType::RESP_SUCCESS:
            break;
    }
    return UBSE_OK;
}

void MockBandBridgeIoctl(MockIoctlRespType respType)
{
    g_ioctlRespType = respType;
    MOCKER(reinterpret_cast<int (*)(const char*, int)>(open)).stubs().will(invoke(MockOpen));
    MOCKER(reinterpret_cast<int (*)(int, unsigned long, void*)>(ioctl)).stubs().will(invoke(MockIoctl));
    MOCKER(close).stubs().will(invoke(MockClose));
}
} // namespace

void TestUbseCtrlQMsgProxy::SetUp()
{
    Test::SetUp();
    GlobalMockObject::reset();
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

    void ClearBlocks()
    {
        reqMsg_.blocks.clear();
    }
};

// Mock implementation for ICtrlQRespMsg
class MockCtrlQRespMsg : public ICtrlQRespMsg {
public:
    explicit MockCtrlQRespMsg(UbseResult decodeResult = UBSE_OK) : decodeResult_(decodeResult) {}

    UbseResult DecodeRespMsg(const CtrlQRespMessage& msg) override
    {
        isDecoded_ = true;
        blockNums_ = msg.blockNums;
        blocks_ = msg.blocks;
        return decodeResult_;
    }

    bool isDecoded_{false};
    uint32_t blockNums_{0};
    CtrlQBasicBlock* blocks_{nullptr};
    UbseResult decodeResult_{UBSE_OK};
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

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_EmptyBlocks_Failed)
{
    MockCtrlQReqMsg reqMsg(1);
    reqMsg.ClearBlocks();
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

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_IoctlFailed_ReturnsError)
{
    MockCtrlQReqMsg reqMsg(1);
    MockCtrlQRespMsg respMsg;
    MockBandBridgeIoctl(MockIoctlRespType::IOCTL_FAILED);

    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_ResponseRetFailed_ReturnsError)
{
    MockCtrlQReqMsg reqMsg(1);
    MockCtrlQRespMsg respMsg;
    MockBandBridgeIoctl(MockIoctlRespType::RESP_RET_FAILED);

    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_ResponseBlockNumZero_ReturnsError)
{
    MockCtrlQReqMsg reqMsg(1);
    MockCtrlQRespMsg respMsg;
    MockBandBridgeIoctl(MockIoctlRespType::RESP_BLOCK_NUM_ZERO);

    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_ResponseBlockNumTooLarge_ReturnsError)
{
    MockCtrlQReqMsg reqMsg(1);
    MockCtrlQRespMsg respMsg;
    MockBandBridgeIoctl(MockIoctlRespType::RESP_BLOCK_NUM_TOO_LARGE);

    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_ResponseSeqInvalid_ReturnsError)
{
    MockCtrlQReqMsg reqMsg(1);
    MockCtrlQRespMsg respMsg;
    MockBandBridgeIoctl(MockIoctlRespType::RESP_SEQ_INVALID);

    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_ResponseDecodeFailed_ReturnsDecodeResult)
{
    MockCtrlQReqMsg reqMsg(1);
    MockCtrlQRespMsg respMsg(UBSE_ERROR);
    MockBandBridgeIoctl(MockIoctlRespType::RESP_SUCCESS);

    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseCtrlQMsgProxy, SendRequest_ResponseSuccess_ReturnsOk)
{
    MockCtrlQReqMsg reqMsg(1);
    MockCtrlQRespMsg respMsg;
    MockBandBridgeIoctl(MockIoctlRespType::RESP_SUCCESS);

    auto result = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    EXPECT_EQ(result, UBSE_OK);
}

} // namespace ubse::mti::ctrl_q::ut
