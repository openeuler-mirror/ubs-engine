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

} // namespace ubse::mti::ctrl_q::ut