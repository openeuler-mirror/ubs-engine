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

#include "test_ubse_ctrl_q_get_idev_fe_david_mapping_msg.h"
#include <securec.h>
#include <ubse_error.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_idev_fe_david_mapping_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ictrl_q_req_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ictrl_q_resp_msg.h"
#include "src/include/adapter_plugins/mti/ubse_mti_urma.h"

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::urma;

namespace ubse::ut::mti::ctrl_q {

void TestUbseCtrlQGetIdevFeDavidMappingMsg::SetUp() {}

void TestUbseCtrlQGetIdevFeDavidMappingMsg::TearDown()
{
    GlobalMockObject::verify();
}

TEST_F(TestUbseCtrlQGetIdevFeDavidMappingMsg, ReqMsgConstructAndEncodeTest)
{
    UbseCtrlQGetIdevFeDavidMappingReqMsg reqMsg;
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

TEST_F(TestUbseCtrlQGetIdevFeDavidMappingMsg, RespMsgDecodeZeroPfeTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x3;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQGetIdevFeDavidMappingRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    const auto& mapping = resp.GetMapping();
    EXPECT_EQ(mapping.size(), 0);
}

TEST_F(TestUbseCtrlQGetIdevFeDavidMappingMsg, RespMsgDecodeSinglePfeTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x3;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 1;
    data[1] = 1;
    data[2] = 2;
    data[3] = 3;
    data[4] = 4;
    data[5] = 5;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQGetIdevFeDavidMappingRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    const auto& mapping = resp.GetMapping();
    EXPECT_EQ(mapping.size(), 1);

    UbseMtiDavid david(4, 5);
    auto it = mapping.find(david);
    EXPECT_TRUE(it != mapping.end());
    EXPECT_EQ(it->second.ubController.chipId, 1);
    EXPECT_EQ(it->second.ubController.dieId, 2);
    EXPECT_EQ(it->second.pfeId, 3);
}

TEST_F(TestUbseCtrlQGetIdevFeDavidMappingMsg, RespMsgDecodeMultiplePfeTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x3;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 2;
    data[1] = 1;
    data[2] = 2;
    data[3] = 3;
    data[4] = 4;
    data[5] = 5;
    data[6] = 6;
    data[7] = 7;
    data[8] = 8;
    data[9] = 9;
    data[10] = 10;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQGetIdevFeDavidMappingRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    const auto& mapping = resp.GetMapping();
    EXPECT_EQ(mapping.size(), 2);

    UbseMtiDavid david1(4, 5);
    auto it1 = mapping.find(david1);
    EXPECT_TRUE(it1 != mapping.end());
    EXPECT_EQ(it1->second.ubController.chipId, 1);
    EXPECT_EQ(it1->second.ubController.dieId, 2);
    EXPECT_EQ(it1->second.pfeId, 3);

    UbseMtiDavid david2(9, 10);
    auto it2 = mapping.find(david2);
    EXPECT_TRUE(it2 != mapping.end());
    EXPECT_EQ(it2->second.ubController.chipId, 6);
    EXPECT_EQ(it2->second.ubController.dieId, 7);
    EXPECT_EQ(it2->second.pfeId, 8);
}

TEST_F(TestUbseCtrlQGetIdevFeDavidMappingMsg, RespMsgInvalidServiceTypeTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 0;
    block.head.bbNum = 0;
    block.head.opCode = 0x3;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQGetIdevFeDavidMappingRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeDavidMappingMsg, RespMsgInvalidOpCodeTest)
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

    UbseCtrlQGetIdevFeDavidMappingRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeDavidMappingMsg, RespMsgNullBlocksTest)
{
    CtrlQRespMessage respMsg;
    respMsg.blocks = nullptr;
    respMsg.blockNums = 0;

    UbseCtrlQGetIdevFeDavidMappingRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQGetIdevFeDavidMappingMsg, RespMsgDataExceedsBoundsTest)
{
    CtrlQBasicBlock block{};
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x3;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 6;
    data[1] = 1;
    data[2] = 2;
    data[3] = 3;
    data[4] = 4;
    data[5] = 5;
    data[6] = 6;
    data[7] = 7;
    data[8] = 8;
    data[9] = 9;
    data[10] = 10;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQGetIdevFeDavidMappingRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

} // namespace ubse::ut::mti::ctrl_q
