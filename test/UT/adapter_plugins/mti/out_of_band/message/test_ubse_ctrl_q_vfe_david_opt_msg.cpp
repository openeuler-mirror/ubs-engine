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

#include "test_ubse_ctrl_q_vfe_david_opt_msg.h"
#include <securec.h>
#include <ubse_error.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_vfe_david_opt_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ictrl_q_req_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ictrl_q_resp_msg.h"
#include "src/include/adapter_plugins/mti/ubse_mti_urma.h"

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::urma;

namespace ubse::ut::mti::ctrl_q {

void TestUbseCtrlQVfeDavidOptMsg::SetUp()
{
    testUpi_ = 100;

    UbseMtiIdevVfe vfe1;
    vfe1.ubController.slotId = 1;
    vfe1.ubController.chipId = 2;
    vfe1.ubController.dieId = 3;
    vfe1.pfeId = 4;
    vfe1.vfeId = 5;
    vfe1.guid.fill(0);

    UbseMtiDavid david1;
    david1.slotId = 1;
    david1.chipId = 2;
    david1.channelId = 3;

    testVfeDavidList_.emplace_back(vfe1, david1);
}

void TestUbseCtrlQVfeDavidOptMsg::TearDown()
{
    GlobalMockObject::verify();
}

static UbseMtiIdevVfeDavidPair MakeVfeDavidPair(const UbseMtiIdevVfe& vfe, const UbseMtiDavid& david)
{
    return {vfe, david};
}

static CtrlQBasicBlock MakeRespBlock(uint8_t opCode, uint8_t serviceType = DEFAULT_SERVICE_TYPE)
{
    CtrlQBasicBlock block;
    (void)memset_s(&block, sizeof(block), 0, sizeof(block));
    block.head.version = 1;
    block.head.serviceType = serviceType;
    block.head.bbNum = 0;
    block.head.opCode = opCode;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    return block;
}

static void FillBatchRespData(CtrlQBasicBlock& block, const std::vector<uint8_t>& results)
{
    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = static_cast<uint8_t>(results.size());
    for (size_t i = 0; i < results.size(); i++) {
        data[i + 1] = results[i];
    }
}

// ==================== UbseCtrlQBindVfeDavidReqMsg 测试 ====================

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidReqMsg_ConstructAndEncode)
{
    UbseCtrlQBindVfeDavidReqMsg reqMsg(testUpi_, testVfeDavidList_);
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidReqMsg_MultipleVfeDavid)
{
    UbseMtiIdevVfe vfe1;
    vfe1.ubController.slotId = 1;
    vfe1.ubController.chipId = 2;
    vfe1.ubController.dieId = 3;
    vfe1.pfeId = 4;
    vfe1.vfeId = 5;
    vfe1.guid.fill(0);
    UbseMtiDavid david1;
    david1.slotId = 1;
    david1.chipId = 2;
    david1.channelId = 3;

    UbseMtiIdevVfe vfe2;
    vfe2.ubController.slotId = 2;
    vfe2.ubController.chipId = 3;
    vfe2.ubController.dieId = 4;
    vfe2.pfeId = 5;
    vfe2.vfeId = 6;
    vfe2.guid.fill(0);
    UbseMtiDavid david2;
    david2.slotId = 2;
    david2.chipId = 3;
    david2.channelId = 4;

    std::vector<UbseMtiIdevVfeDavidPair> list = {MakeVfeDavidPair(vfe1, david1), MakeVfeDavidPair(vfe2, david2)};

    UbseCtrlQBindVfeDavidReqMsg reqMsg(testUpi_, list);
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidReqMsg_EmptyList)
{
    std::vector<UbseMtiIdevVfeDavidPair> emptyList;
    UbseCtrlQBindVfeDavidReqMsg reqMsg(testUpi_, emptyList);
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

// ==================== UbseCtrlQUnBindVfeDavidReqMsg 测试 ====================

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidReqMsg_ConstructAndEncode)
{
    UbseCtrlQUnBindVfeDavidReqMsg reqMsg(testUpi_, testVfeDavidList_);
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidReqMsg_MultipleVfeDavid)
{
    UbseMtiIdevVfe vfe1;
    vfe1.ubController.slotId = 1;
    vfe1.ubController.chipId = 2;
    vfe1.ubController.dieId = 3;
    vfe1.pfeId = 4;
    vfe1.vfeId = 5;
    vfe1.guid.fill(0);
    UbseMtiDavid david1;
    david1.slotId = 1;
    david1.chipId = 2;
    david1.channelId = 3;

    UbseMtiIdevVfe vfe2;
    vfe2.ubController.slotId = 2;
    vfe2.ubController.chipId = 3;
    vfe2.ubController.dieId = 4;
    vfe2.pfeId = 5;
    vfe2.vfeId = 6;
    vfe2.guid.fill(0);
    UbseMtiDavid david2;
    david2.slotId = 2;
    david2.chipId = 3;
    david2.channelId = 4;

    std::vector<UbseMtiIdevVfeDavidPair> list = {MakeVfeDavidPair(vfe1, david1), MakeVfeDavidPair(vfe2, david2)};

    UbseCtrlQUnBindVfeDavidReqMsg reqMsg(testUpi_, list);
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidReqMsg_EmptyList)
{
    std::vector<UbseMtiIdevVfeDavidPair> emptyList;
    UbseCtrlQUnBindVfeDavidReqMsg reqMsg(testUpi_, emptyList);
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

// ==================== UbseCtrlQBindVfeDavidRespMsg 测试 ====================

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidRespMsg_AllSuccess)
{
    CtrlQBasicBlock block = MakeRespBlock(0x9);
    FillBatchRespData(block, {UBSE_OK, UBSE_OK});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_EQ(retList.size(), 2);
    EXPECT_TRUE(retList[0]);
    EXPECT_TRUE(retList[1]);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidRespMsg_AllFailed)
{
    CtrlQBasicBlock block = MakeRespBlock(0x9);
    FillBatchRespData(block, {1, 2});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_EQ(retList.size(), 2);
    EXPECT_FALSE(retList[0]);
    EXPECT_FALSE(retList[1]);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidRespMsg_BindRepeatedTreatedAsSuccess)
{
    CtrlQBasicBlock block = MakeRespBlock(0x9);
    FillBatchRespData(block, {23});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_EQ(retList.size(), 1);
    EXPECT_TRUE(retList[0]);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidRespMsg_MixedResults)
{
    CtrlQBasicBlock block = MakeRespBlock(0x9);
    FillBatchRespData(block, {UBSE_OK, 1, 23, 2});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_EQ(retList.size(), 4);
    EXPECT_TRUE(retList[0]);
    EXPECT_FALSE(retList[1]);
    EXPECT_TRUE(retList[2]);
    EXPECT_FALSE(retList[3]);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidRespMsg_InvalidServiceType)
{
    CtrlQBasicBlock block = MakeRespBlock(0x9, 0);
    FillBatchRespData(block, {UBSE_OK});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidRespMsg_InvalidOpCode)
{
    CtrlQBasicBlock block = MakeRespBlock(0xA);
    FillBatchRespData(block, {UBSE_OK});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidRespMsg_NullBlocks)
{
    CtrlQRespMessage respMsg;
    respMsg.blocks = nullptr;
    respMsg.blockNums = 0;

    UbseCtrlQBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, BindVfeDavidRespMsg_EmptyResultList)
{
    CtrlQBasicBlock block = MakeRespBlock(0x9);
    FillBatchRespData(block, {});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_TRUE(retList.empty());
}

// ==================== UbseCtrlQUnBindVfeDavidRespMsg 测试 ====================

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidRespMsg_AllSuccess)
{
    CtrlQBasicBlock block = MakeRespBlock(0xA);
    FillBatchRespData(block, {UBSE_OK, UBSE_OK});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQUnBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_EQ(retList.size(), 2);
    EXPECT_TRUE(retList[0]);
    EXPECT_TRUE(retList[1]);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidRespMsg_AllFailed)
{
    CtrlQBasicBlock block = MakeRespBlock(0xA);
    FillBatchRespData(block, {1, 2});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQUnBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_EQ(retList.size(), 2);
    EXPECT_FALSE(retList[0]);
    EXPECT_FALSE(retList[1]);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidRespMsg_UnbindRepeatedTreatedAsSuccess)
{
    CtrlQBasicBlock block = MakeRespBlock(0xA);
    FillBatchRespData(block, {22});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQUnBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_EQ(retList.size(), 1);
    EXPECT_TRUE(retList[0]);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidRespMsg_MixedResults)
{
    CtrlQBasicBlock block = MakeRespBlock(0xA);
    FillBatchRespData(block, {UBSE_OK, 1, 22, 2});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQUnBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_EQ(retList.size(), 4);
    EXPECT_TRUE(retList[0]);
    EXPECT_FALSE(retList[1]);
    EXPECT_TRUE(retList[2]);
    EXPECT_FALSE(retList[3]);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidRespMsg_InvalidServiceType)
{
    CtrlQBasicBlock block = MakeRespBlock(0xA, 0);
    FillBatchRespData(block, {UBSE_OK});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQUnBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidRespMsg_InvalidOpCode)
{
    CtrlQBasicBlock block = MakeRespBlock(0x9);
    FillBatchRespData(block, {UBSE_OK});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQUnBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidRespMsg_NullBlocks)
{
    CtrlQRespMessage respMsg;
    respMsg.blocks = nullptr;
    respMsg.blockNums = 0;

    UbseCtrlQUnBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

TEST_F(TestUbseCtrlQVfeDavidOptMsg, UnBindVfeDavidRespMsg_EmptyResultList)
{
    CtrlQBasicBlock block = MakeRespBlock(0xA);
    FillBatchRespData(block, {});

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQUnBindVfeDavidRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);

    const auto& retList = resp.GetRetList();
    EXPECT_TRUE(retList.empty());
}

} // namespace ubse::ut::mti::ctrl_q
