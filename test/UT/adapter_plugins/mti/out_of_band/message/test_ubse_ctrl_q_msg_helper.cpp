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

#include "test_ubse_ctrl_q_msg_helper.h"
#include <securec.h>
#include <ubse_error.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_msg_helper.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"

using namespace ubse::mti::ctrl_q;

namespace ubse::ut::mti::ctrl_q {

void TestUbseCtrlQMsgHelper::SetUp() {}

void TestUbseCtrlQMsgHelper::TearDown()
{
    GlobalMockObject::verify();
}

static CtrlQBasicBlock MakeValidBlock(uint8_t opCode, uint8_t bbNum = 0)
{
    CtrlQBasicBlock block;
    (void)memset_s(&block, sizeof(block), 0, sizeof(block));
    block.head.version = 1;
    block.head.serviceType = DEFAULT_SERVICE_TYPE;
    block.head.bbNum = bbNum;
    block.head.opCode = opCode;
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    return block;
}

static void FillRespData(CtrlQBasicBlock& block, const std::vector<uint8_t>& results)
{
    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = static_cast<uint8_t>(results.size());
    for (size_t i = 0; i < results.size(); i++) {
        data[i + 1] = results[i];
    }
}

// ==================== UbseCtrlQMsgWriteHelper 测试 ====================

TEST_F(TestUbseCtrlQMsgHelper, WriteHelper_WriteUint8)
{
    uint8_t buffer[BASIC_BLOCK_SIZE] = {0};
    UbseCtrlQMsgWriteHelper writer(buffer, buffer + sizeof(buffer));
    writer.Write<uint8_t>(0x42);
    EXPECT_EQ(buffer[0], 0x42);
}

TEST_F(TestUbseCtrlQMsgHelper, WriteHelper_WriteUint16)
{
    uint8_t buffer[BASIC_BLOCK_SIZE] = {0};
    UbseCtrlQMsgWriteHelper writer(buffer, buffer + sizeof(buffer));
    writer.Write<uint16_t>(0x1234);
    EXPECT_EQ(*reinterpret_cast<uint16_t*>(buffer), 0x1234);
}

TEST_F(TestUbseCtrlQMsgHelper, WriteHelper_WriteUint32)
{
    uint8_t buffer[BASIC_BLOCK_SIZE] = {0};
    UbseCtrlQMsgWriteHelper writer(buffer, buffer + sizeof(buffer));
    writer.Write<uint32_t>(0x12345678);
    EXPECT_EQ(*reinterpret_cast<uint32_t*>(buffer), 0x12345678);
}

TEST_F(TestUbseCtrlQMsgHelper, WriteHelper_MultipleWrites)
{
    uint8_t buffer[BASIC_BLOCK_SIZE] = {0};
    UbseCtrlQMsgWriteHelper writer(buffer, buffer + sizeof(buffer));
    writer.Write<uint8_t>(0x01);
    writer.Write<uint8_t>(0x02);
    writer.Write<uint16_t>(0x0304);
    EXPECT_EQ(buffer[0], 0x01);
    EXPECT_EQ(buffer[1], 0x02);
    EXPECT_EQ(*reinterpret_cast<uint16_t*>(buffer + 2), 0x0304);
}

TEST_F(TestUbseCtrlQMsgHelper, WriteHelper_OutOfRangeThrows)
{
    uint8_t buffer[4] = {0};
    UbseCtrlQMsgWriteHelper writer(buffer, buffer + sizeof(buffer));
    writer.Write<uint32_t>(0x01);
    EXPECT_THROW(writer.Write<uint8_t>(0x02), std::out_of_range);
}

TEST_F(TestUbseCtrlQMsgHelper, WriteHelper_ExactFitNoThrow)
{
    uint8_t buffer[4] = {0};
    UbseCtrlQMsgWriteHelper writer(buffer, buffer + sizeof(buffer));
    EXPECT_NO_THROW(writer.Write<uint32_t>(0xAABBCCDD));
}

TEST_F(TestUbseCtrlQMsgHelper, WriteHelper_WriteOverflowThrows)
{
    uint8_t buffer[2] = {0};
    UbseCtrlQMsgWriteHelper writer(buffer, buffer + sizeof(buffer));
    EXPECT_THROW(writer.Write<uint32_t>(0x01), std::out_of_range);
}

// ==================== UbseCtrlQMsgReadHelper 测试 ====================

TEST_F(TestUbseCtrlQMsgHelper, ReadHelper_ReadUint8)
{
    uint8_t buffer[BASIC_BLOCK_SIZE] = {0};
    buffer[0] = 0x42;
    UbseCtrlQMsgReadHelper reader(buffer, buffer + sizeof(buffer));
    EXPECT_EQ(reader.Read<uint8_t>(), 0x42);
}

TEST_F(TestUbseCtrlQMsgHelper, ReadHelper_ReadUint16)
{
    uint8_t buffer[BASIC_BLOCK_SIZE] = {0};
    *reinterpret_cast<uint16_t*>(buffer) = 0x1234;
    UbseCtrlQMsgReadHelper reader(buffer, buffer + sizeof(buffer));
    EXPECT_EQ(reader.Read<uint16_t>(), 0x1234);
}

TEST_F(TestUbseCtrlQMsgHelper, ReadHelper_ReadUint32)
{
    uint8_t buffer[BASIC_BLOCK_SIZE] = {0};
    *reinterpret_cast<uint32_t*>(buffer) = 0x12345678;
    UbseCtrlQMsgReadHelper reader(buffer, buffer + sizeof(buffer));
    EXPECT_EQ(reader.Read<uint32_t>(), 0x12345678);
}

TEST_F(TestUbseCtrlQMsgHelper, ReadHelper_MultipleReads)
{
    uint8_t buffer[BASIC_BLOCK_SIZE] = {0};
    buffer[0] = 0x01;
    buffer[1] = 0x02;
    *reinterpret_cast<uint16_t*>(buffer + 2) = 0x0304;
    UbseCtrlQMsgReadHelper reader(buffer, buffer + sizeof(buffer));
    EXPECT_EQ(reader.Read<uint8_t>(), 0x01);
    EXPECT_EQ(reader.Read<uint8_t>(), 0x02);
    EXPECT_EQ(reader.Read<uint16_t>(), 0x0304);
}

TEST_F(TestUbseCtrlQMsgHelper, ReadHelper_OutOfRangeThrows)
{
    uint8_t buffer[4] = {0};
    UbseCtrlQMsgReadHelper reader(buffer, buffer + sizeof(buffer));
    reader.Read<uint32_t>();
    EXPECT_THROW(reader.Read<uint8_t>(), std::out_of_range);
}

TEST_F(TestUbseCtrlQMsgHelper, ReadHelper_ExactFitNoThrow)
{
    uint8_t buffer[4] = {0};
    *reinterpret_cast<uint32_t*>(buffer) = 0xAABBCCDD;
    UbseCtrlQMsgReadHelper reader(buffer, buffer + sizeof(buffer));
    EXPECT_NO_THROW(reader.Read<uint32_t>());
}

TEST_F(TestUbseCtrlQMsgHelper, ReadHelper_ReadOverflowThrows)
{
    uint8_t buffer[2] = {0};
    UbseCtrlQMsgReadHelper reader(buffer, buffer + sizeof(buffer));
    EXPECT_THROW(reader.Read<uint32_t>(), std::out_of_range);
}

// ==================== CheckRespValidation 测试 ====================

TEST_F(TestUbseCtrlQMsgHelper, CheckRespValidation_ValidResp)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09);
    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;
    EXPECT_TRUE(CheckRespValidation(msg, 0, 0x09));
}

TEST_F(TestUbseCtrlQMsgHelper, CheckRespValidation_NullBlocks)
{
    CtrlQRespMessage msg;
    msg.blocks = nullptr;
    msg.blockNums = 0;
    EXPECT_FALSE(CheckRespValidation(msg, 0, 0x09));
}

TEST_F(TestUbseCtrlQMsgHelper, CheckRespValidation_InvalidServiceType)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09);
    block.head.serviceType = 0;
    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;
    EXPECT_FALSE(CheckRespValidation(msg, 0, 0x09));
}

TEST_F(TestUbseCtrlQMsgHelper, CheckRespValidation_InvalidOpCode)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09);
    block.head.opCode = 0x0A;
    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;
    EXPECT_FALSE(CheckRespValidation(msg, 0, 0x09));
}

TEST_F(TestUbseCtrlQMsgHelper, CheckRespValidation_BbNumZeroSkipsCheck)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09, 5);
    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;
    EXPECT_TRUE(CheckRespValidation(msg, 0, 0x09));
}

TEST_F(TestUbseCtrlQMsgHelper, CheckRespValidation_BbNumMismatch)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09, 2);
    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;
    EXPECT_FALSE(CheckRespValidation(msg, 1, 0x09));
}

TEST_F(TestUbseCtrlQMsgHelper, CheckRespValidation_BbNumMatch)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09, 3);
    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;
    EXPECT_TRUE(CheckRespValidation(msg, 3, 0x09));
}

// ==================== GetBatchOptRespResult 测试 ====================

TEST_F(TestUbseCtrlQMsgHelper, GetBatchOptRespResult_AllSuccess)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09);
    FillRespData(block, {UBSE_OK, UBSE_OK, UBSE_OK});

    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;

    std::vector<bool> resList;
    EXPECT_EQ(GetBatchOptRespResult(msg, 0x09, resList), UBSE_OK);
    EXPECT_EQ(resList.size(), 3);
    EXPECT_TRUE(resList[0]);
    EXPECT_TRUE(resList[1]);
    EXPECT_TRUE(resList[2]);
}

TEST_F(TestUbseCtrlQMsgHelper, GetBatchOptRespResult_AllFailed)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09);
    FillRespData(block, {1, 2, 3});

    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;

    std::vector<bool> resList;
    EXPECT_EQ(GetBatchOptRespResult(msg, 0x09, resList), UBSE_OK);
    EXPECT_EQ(resList.size(), 3);
    EXPECT_FALSE(resList[0]);
    EXPECT_FALSE(resList[1]);
    EXPECT_FALSE(resList[2]);
}

TEST_F(TestUbseCtrlQMsgHelper, GetBatchOptRespResult_MixedResults)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09);
    FillRespData(block, {UBSE_OK, 1, UBSE_OK, 2});

    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;

    std::vector<bool> resList;
    EXPECT_EQ(GetBatchOptRespResult(msg, 0x09, resList), UBSE_OK);
    EXPECT_EQ(resList.size(), 4);
    EXPECT_TRUE(resList[0]);
    EXPECT_FALSE(resList[1]);
    EXPECT_TRUE(resList[2]);
    EXPECT_FALSE(resList[3]);
}

TEST_F(TestUbseCtrlQMsgHelper, GetBatchOptRespResult_EmptyResults)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09);
    FillRespData(block, {});

    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;

    std::vector<bool> resList;
    EXPECT_EQ(GetBatchOptRespResult(msg, 0x09, resList), UBSE_OK);
    EXPECT_TRUE(resList.empty());
}

TEST_F(TestUbseCtrlQMsgHelper, GetBatchOptRespResult_SingleSuccess)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09);
    FillRespData(block, {UBSE_OK});

    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;

    std::vector<bool> resList;
    EXPECT_EQ(GetBatchOptRespResult(msg, 0x09, resList), UBSE_OK);
    EXPECT_EQ(resList.size(), 1);
    EXPECT_TRUE(resList[0]);
}

TEST_F(TestUbseCtrlQMsgHelper, GetBatchOptRespResult_SingleFailure)
{
    CtrlQBasicBlock block = MakeValidBlock(0x09);
    FillRespData(block, {0xFF});

    CtrlQRespMessage msg;
    msg.blocks = &block;
    msg.blockNums = 1;

    std::vector<bool> resList;
    EXPECT_EQ(GetBatchOptRespResult(msg, 0x09, resList), UBSE_OK);
    EXPECT_EQ(resList.size(), 1);
    EXPECT_FALSE(resList[0]);
}

} // namespace ubse::ut::mti::ctrl_q
