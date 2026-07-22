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

#include "test_ubse_ctrl_q_get_idev_fe_msg.h"
#include <securec.h>
#include <ubse_error.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_idev_fe_msg.cpp"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_idev_fe_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ictrl_q_req_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ictrl_q_resp_msg.h"
#include "src/adapter_plugins/mti/out_of_band/proxy/ubse_ctrl_q_msg_proxy.h"
#include "src/include/adapter_plugins/mti/ubse_mti_urma.h"

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::urma;

namespace ubse::ut::mti::ctrl_q {

void TestUbseCtrlQGetIdevFeMsg::SetUp() {}

void TestUbseCtrlQGetIdevFeMsg::TearDown()
{
    GlobalMockObject::verify();
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, ReqMsgConstructAndEncodeTest)
{
    UbseCtrlQGetIdevFeReqMsg reqMsg;
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, RespMsgInvalidServiceTypeTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 0;
    block.head.bbNum = 0;
    block.head.opCode = 0x1;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQGetIdevFeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, RespMsgInvalidOpCodeTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0xFF;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQGetIdevFeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, RespMsgNullBlocksTest)
{
    CtrlQRespMessage respMsg;
    respMsg.blocks = nullptr;
    respMsg.blockNums = 0;

    UbseCtrlQGetIdevFeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, RespMsgDavidMappingFailedTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x1;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    UbseCtrlQGetIdevFeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, RespMsgEmptyPfeSetTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x1;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 1;
    data[1] = 1;
    data[2] = 1;
    data[3] = 1;
    data[4] = 2;
    data[5] = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    UbseCtrlQGetIdevFeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    const auto& pfeList = resp.GetPfeList();
    EXPECT_TRUE(pfeList.empty());
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, RespMsgDataExceedsBoundsTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x1;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 1;
    data[1] = 10;
    data[2] = 1;
    data[3] = 1;
    data[4] = 1;
    data[5] = 255;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    UbseCtrlQGetIdevFeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, GetPfeMappingDavidSetFailedDirectTest)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::set<UbseMtiIdevPfe> pfeSet;
    EXPECT_EQ(GetPfeMappingDavidSet(pfeSet), UBSE_ERROR);
    EXPECT_TRUE(pfeSet.empty());
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, GetPfeMappingDavidSetSuccessEmptyMappingTest)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::set<UbseMtiIdevPfe> pfeSet;
    EXPECT_EQ(GetPfeMappingDavidSet(pfeSet), UBSE_OK);
    EXPECT_TRUE(pfeSet.empty());
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, GetGuidSuccessTest)
{
    UbseCtrlQGetIdevPfeGuidReqMsg pfeReq(UbseMtiIdevPfe(UbseMtiUbController(1, 2), 3));
    UbseCtrlQGetIdevPfeGuidRespMsg pfeResp;
    UbseMtiGuid guid{};
    guid[0] = 0xAB;
    pfeResp.guid_ = guid;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    UbseMtiGuid resultGuid;
    EXPECT_EQ(GetGuid(pfeReq, pfeResp, resultGuid), UBSE_OK);
    EXPECT_EQ(resultGuid[0], 0xAB);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, GetGuidFailedTest)
{
    UbseCtrlQGetIdevPfeGuidReqMsg pfeReq(UbseMtiIdevPfe(UbseMtiUbController(1, 2), 3));
    UbseCtrlQGetIdevPfeGuidRespMsg pfeResp;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    UbseMtiGuid resultGuid;
    EXPECT_EQ(GetGuid(pfeReq, pfeResp, resultGuid), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, AddPfePfeNotInSetTest)
{
    UbseMtiUbController ubController(1, 2);
    uint8_t pfeNum = 1;
    std::set<UbseMtiIdevPfe> pfeSet;
    std::vector<UbseMtiIdevPfe> pfeList;

    uint8_t buf[64]{};
    buf[0] = 0;
    UbseCtrlQMsgReadHelper readHelper(buf, buf + sizeof(buf));

    EXPECT_EQ(AddPfe(pfeNum, ubController, pfeSet, pfeList, readHelper), UBSE_OK);
    EXPECT_TRUE(pfeList.empty());
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, AddPfePfeInSetGetGuidFailedTest)
{
    UbseMtiUbController ubController(1, 2);
    uint8_t pfeNum = 1;
    UbseMtiIdevPfe pfeInSet(ubController, 0);
    std::set<UbseMtiIdevPfe> pfeSet;
    pfeSet.emplace(pfeInSet);
    std::vector<UbseMtiIdevPfe> pfeList;

    uint8_t buf[64]{};
    buf[0] = 0;
    UbseCtrlQMsgReadHelper readHelper(buf, buf + sizeof(buf));

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(AddPfe(pfeNum, ubController, pfeSet, pfeList, readHelper), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, AddPfePfeInSetVfeGuidFailedTest)
{
    UbseMtiUbController ubController(1, 2);
    uint8_t pfeNum = 1;
    UbseMtiIdevPfe pfeInSet(ubController, 0);
    UbseMtiGuid pfeGuid{};
    pfeGuid[0] = 0x01;
    pfeInSet.guid = pfeGuid;
    std::set<UbseMtiIdevPfe> pfeSet;
    pfeSet.emplace(pfeInSet);
    std::vector<UbseMtiIdevPfe> pfeList;

    uint8_t buf[64]{};
    buf[0] = 0;
    UbseCtrlQMsgReadHelper readHelper(buf, buf + sizeof(buf));

    UbseCtrlQGetIdevPfeGuidRespMsg pfeGuidResp;
    pfeGuidResp.guid_ = pfeGuid;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_ERROR));

    EXPECT_EQ(AddPfe(pfeNum, ubController, pfeSet, pfeList, readHelper), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, AddPfeSuccessTest)
{
    UbseMtiUbController ubController(1, 2);
    uint8_t pfeNum = 1;
    UbseMtiIdevPfe pfeInSet(ubController, 0);
    UbseMtiGuid pfeGuid{};
    pfeGuid[0] = 0x01;
    pfeInSet.guid = pfeGuid;
    std::set<UbseMtiIdevPfe> pfeSet;
    pfeSet.emplace(pfeInSet);
    std::vector<UbseMtiIdevPfe> pfeList;

    uint8_t buf[64]{};
    buf[0] = 0;
    UbseCtrlQMsgReadHelper readHelper(buf, buf + sizeof(buf));

    UbseCtrlQGetIdevPfeGuidRespMsg pfeGuidResp;
    pfeGuidResp.guid_ = pfeGuid;

    UbseMtiGuid vfeGuid{};
    vfeGuid[0] = 0x02;
    UbseCtrlQGetIdevVfeGuidRespMsg vfeGuidResp;
    vfeGuidResp.guid_ = vfeGuid;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_OK));

    EXPECT_EQ(AddPfe(pfeNum, ubController, pfeSet, pfeList, readHelper), UBSE_OK);
    EXPECT_EQ(pfeList.size(), 1);
    EXPECT_EQ(pfeList[0].vfeList.size(), 1);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, GetRespResultSuccessTest)
{
    uint8_t buf[128]{};
    buf[0] = 1;
    buf[1] = 1;
    buf[2] = 1;
    buf[3] = 1;
    buf[4] = 1;
    buf[5] = 2;
    UbseCtrlQMsgReadHelper readHelper(buf, buf + sizeof(buf));

    UbseMtiIdevPfe pfeInSet(UbseMtiUbController(1, 1), 1);
    std::set<UbseMtiIdevPfe> pfeSet;
    pfeSet.emplace(pfeInSet);

    std::vector<UbseMtiIdevPfe> pfeList;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_OK));

    EXPECT_EQ(GetRespResult(pfeSet, pfeList, readHelper), UBSE_OK);
    EXPECT_EQ(pfeList.size(), 1);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, GetRespResultAddPfeFailedTest)
{
    uint8_t buf[128]{};
    buf[0] = 1;
    buf[1] = 1;
    buf[2] = 1;
    buf[3] = 1;
    buf[4] = 1;
    buf[5] = 2;
    UbseCtrlQMsgReadHelper readHelper(buf, buf + sizeof(buf));

    UbseMtiIdevPfe pfeInSet(UbseMtiUbController(1, 1), 1);
    std::set<UbseMtiIdevPfe> pfeSet;
    pfeSet.emplace(pfeInSet);

    std::vector<UbseMtiIdevPfe> pfeList;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(GetRespResult(pfeSet, pfeList, readHelper), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, GetPfeMappingDavidSetSuccessNonEmptyMappingTest)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::set<UbseMtiIdevPfe> pfeSet;
    EXPECT_EQ(GetPfeMappingDavidSet(pfeSet), UBSE_OK);
    EXPECT_TRUE(pfeSet.empty());
}

TEST_F(TestUbseCtrlQGetIdevFeMsg, RespMsgDecodeSuccessPathTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x1;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    UbseCtrlQGetIdevFeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    EXPECT_TRUE(resp.GetPfeList().empty());
}

} // namespace ubse::ut::mti::ctrl_q
