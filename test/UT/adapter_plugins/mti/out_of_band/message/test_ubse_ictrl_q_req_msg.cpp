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

#include "test_ubse_ictrl_q_req_msg.h"
#include <securec.h>
#include <ubse_error.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"

using namespace ubse::mti::ctrl_q;

namespace ubse::ut::mti::ctrl_q {

class MockCtrlQReqMsg : public ICtrlQReqMsg {
public:
    explicit MockCtrlQReqMsg(uint8_t opCode, uint8_t bbNum = 1) : ICtrlQReqMsg(opCode, bbNum) {}

    UbseResult EncodeReqMsg() override
    {
        return UBSE_OK;
    }

    using ICtrlQReqMsg::reqMsg_;
    using ICtrlQReqMsg::SetBBNum;
    using ICtrlQReqMsg::SetOpCode;
    using ICtrlQReqMsg::SetResv;
    using ICtrlQReqMsg::SetRet;
    using ICtrlQReqMsg::SetSeq;
    using ICtrlQReqMsg::SetServiceType;
    using ICtrlQReqMsg::SetVersion;
};

void TestUbseICtrlQReqMsg::SetUp() {}

void TestUbseICtrlQReqMsg::TearDown()
{
    GlobalMockObject::verify();
}

TEST_F(TestUbseICtrlQReqMsg, Constructor_SetsOpCode)
{
    MockCtrlQReqMsg msg(0x09);
    const auto& reqMsg = msg.GetReqMsg();
    EXPECT_EQ(reqMsg.blocks.front().head.opCode, 0x09);
}

TEST_F(TestUbseICtrlQReqMsg, Constructor_SetsServiceType)
{
    MockCtrlQReqMsg msg(0x09);
    const auto& reqMsg = msg.GetReqMsg();
    EXPECT_EQ(reqMsg.blocks.front().head.serviceType, DEFAULT_SERVICE_TYPE);
}

TEST_F(TestUbseICtrlQReqMsg, Constructor_SetsBbNum)
{
    MockCtrlQReqMsg msg(0x09, 3);
    const auto& reqMsg = msg.GetReqMsg();
    EXPECT_EQ(reqMsg.blocks.front().head.bbNum, 3);
    EXPECT_EQ(reqMsg.blocks.size(), 3);
}

TEST_F(TestUbseICtrlQReqMsg, Constructor_DefaultBbNumIsOne)
{
    MockCtrlQReqMsg msg(0x09);
    const auto& reqMsg = msg.GetReqMsg();
    EXPECT_EQ(reqMsg.blocks.front().head.bbNum, 1);
    EXPECT_EQ(reqMsg.blocks.size(), 1);
}

TEST_F(TestUbseICtrlQReqMsg, GetReqMsg_ReturnsConstRef)
{
    MockCtrlQReqMsg msg(0x09);
    const auto& reqMsg = msg.GetReqMsg();
    EXPECT_EQ(reqMsg.blocks.size(), 1);
}

TEST_F(TestUbseICtrlQReqMsg, SetVersion)
{
    MockCtrlQReqMsg msg(0x09);
    msg.SetVersion(2);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.version, 2);
}

TEST_F(TestUbseICtrlQReqMsg, SetOpCode)
{
    MockCtrlQReqMsg msg(0x09);
    msg.SetOpCode(0x0A);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.opCode, 0x0A);
}

TEST_F(TestUbseICtrlQReqMsg, SetRet)
{
    MockCtrlQReqMsg msg(0x09);
    msg.SetRet(1);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.ret, 1);
}

TEST_F(TestUbseICtrlQReqMsg, SetSeq)
{
    MockCtrlQReqMsg msg(0x09);
    msg.SetSeq(1234);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.seq, 1234);
}

TEST_F(TestUbseICtrlQReqMsg, SetResv)
{
    MockCtrlQReqMsg msg(0x09);
    msg.SetResv(5);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.resv, 5);
}

TEST_F(TestUbseICtrlQReqMsg, SetServiceType)
{
    MockCtrlQReqMsg msg(0x09);
    msg.SetServiceType(2);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.serviceType, 2);
}

TEST_F(TestUbseICtrlQReqMsg, SetBBNum)
{
    MockCtrlQReqMsg msg(0x09, 1);
    msg.SetBBNum(5);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.bbNum, 5);
}

TEST_F(TestUbseICtrlQReqMsg, SetVersion_Overwrite)
{
    MockCtrlQReqMsg msg(0x09);
    msg.SetVersion(1);
    msg.SetVersion(3);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.version, 3);
}

TEST_F(TestUbseICtrlQReqMsg, SetOpCode_Overwrite)
{
    MockCtrlQReqMsg msg(0x09);
    msg.SetOpCode(0x0A);
    msg.SetOpCode(0x0B);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.opCode, 0x0B);
}

TEST_F(TestUbseICtrlQReqMsg, SetSeq_Overwrite)
{
    MockCtrlQReqMsg msg(0x09);
    msg.SetSeq(100);
    msg.SetSeq(200);
    EXPECT_EQ(msg.GetReqMsg().blocks.front().head.seq, 200);
}

} // namespace ubse::ut::mti::ctrl_q
