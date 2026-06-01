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

#include "test_ubse_ctrl_q_get_fe_guid_msg.h"
#include <cstring>

namespace ubse::ut::mti::ctrl_q {
using namespace ubse::mti::ctrl_q;

// ==================== 辅助函数 ====================

static CtrlQRespMessage CreateValidIdevRespMsg(const UbseMtiGuid& guid)
{
    CtrlQRespMessage resp;
    resp.blockNums = 1;
    resp.blocks = new CtrlQBasicBlock[1];
    std::memset(resp.blocks, 0, sizeof(CtrlQBasicBlock));

    auto& head = resp.blocks[0].head;
    head.version = 0;
    head.serviceType = DEFAULT_SERVICE_TYPE;
    head.bbNum = 1;
    head.opCode = 0x2;
    head.ret = 0;
    head.seq = 0;
    head.resv = 0;

    auto* guidDst = resp.blocks[0].cmdData;
    std::memcpy(guidDst, guid.data(), 16);

    return resp;
}

static CtrlQRespMessage CreateValid1825RespMsg(const UbseMtiGuid& guid)
{
    CtrlQRespMessage resp;
    resp.blockNums = 1;
    resp.blocks = new CtrlQBasicBlock[1];
    std::memset(resp.blocks, 0, sizeof(CtrlQBasicBlock));

    auto& head = resp.blocks[0].head;
    head.version = 0;
    head.serviceType = DEFAULT_SERVICE_TYPE;
    head.bbNum = 1;
    head.opCode = 0x10;
    head.ret = 0;
    head.seq = 0;
    head.resv = 0;

    auto* guidDst = resp.blocks[0].cmdData;
    std::memcpy(guidDst, guid.data(), 16);

    return resp;
}

static void FreeRespMsg(CtrlQRespMessage& resp)
{
    if (resp.blocks != nullptr) {
        delete[] resp.blocks;
        resp.blocks = nullptr;
    }
    resp.blockNums = 0;
}

static UbseMtiGuid MakeTestGuid(uint8_t fillByte)
{
    UbseMtiGuid guid;
    guid.fill(fillByte);
    return guid;
}

void UbseCtrlQGetIdevPfeGuidReqMsgTest::SetUp()
{
    UbseMtiUbController ctrl(0x01, 0x02);
    pfe_ = UbseMtiIdevPfe(ctrl, 0x03);
}

TEST_F(UbseCtrlQGetIdevPfeGuidReqMsgTest, ConstructWithValidPfe)
{
    EXPECT_NO_THROW({ UbseCtrlQGetIdevPfeGuidReqMsg req(pfe_); });
}

TEST_F(UbseCtrlQGetIdevPfeGuidReqMsgTest, EncodeReqMsg)
{
    UbseCtrlQGetIdevPfeGuidReqMsg req(pfe_);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);

    const auto& msg = req.GetReqMsg();
    ASSERT_FALSE(msg.blocks.empty());

    auto& block = msg.blocks.front();
    EXPECT_EQ(0x2, block.head.opCode);
    EXPECT_EQ(DEFAULT_SERVICE_TYPE, block.head.serviceType);
}

TEST_F(UbseCtrlQGetIdevPfeGuidReqMsgTest, EncodeReqMsgFeLocFields)
{
    UbseMtiUbController ctrl(1, 2);
    ctrl.slotId = 3;
    UbseMtiIdevPfe testPfe(ctrl, 4);
    UbseCtrlQGetIdevPfeGuidReqMsg req(testPfe);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);

    const auto& msg = req.GetReqMsg();
    auto& block = msg.blocks.front();

    auto* feLoc = reinterpret_cast<const FeLoc*>(block.cmdData);
    EXPECT_EQ(3, feLoc->slotId);
    EXPECT_EQ(1, feLoc->chipId);
    EXPECT_EQ(2, feLoc->dieId);
    EXPECT_EQ(4, feLoc->pfeId);
    EXPECT_EQ(0xFF, feLoc->vfeId);
}

TEST_F(UbseCtrlQGetIdevPfeGuidReqMsgTest, EncodeReqMsgZeroValues)
{
    UbseMtiUbController ctrl(0x00, 0x00);
    UbseMtiIdevPfe zeroPfe(ctrl, 0x00);
    UbseCtrlQGetIdevPfeGuidReqMsg req(zeroPfe);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(UbseCtrlQGetIdevPfeGuidReqMsgTest, EncodeReqMsgMaxValues)
{
    UbseMtiUbController ctrl(0xFF, 0xFF);
    UbseMtiIdevPfe maxPfe(ctrl, 0xFF);
    UbseCtrlQGetIdevPfeGuidReqMsg req(maxPfe);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);
}

void UbseCtrlQGetIdevPfeGuidRespMsgTest::TearDown()
{
    FreeRespMsg(resp_);
}

TEST_F(UbseCtrlQGetIdevPfeGuidRespMsgTest, DecodeValidRespMsg)
{
    auto testGuid = MakeTestGuid(0xAB);
    resp_ = CreateValidIdevRespMsg(testGuid);

    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_OK, ret);

    const auto& guid = respMsg.GetGuid();
    EXPECT_EQ(testGuid, guid);
}

TEST_F(UbseCtrlQGetIdevPfeGuidRespMsgTest, DecodeRespMsgWithZeroGuid)
{
    auto zeroGuid = MakeTestGuid(0x00);
    resp_ = CreateValidIdevRespMsg(zeroGuid);

    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_OK, ret);

    const auto& guid = respMsg.GetGuid();
    EXPECT_EQ(zeroGuid, guid);
}

TEST_F(UbseCtrlQGetIdevPfeGuidRespMsgTest, DecodeRespMsgWithMaxGuid)
{
    auto maxGuid = MakeTestGuid(0xFF);
    resp_ = CreateValidIdevRespMsg(maxGuid);

    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_OK, ret);

    const auto& guid = respMsg.GetGuid();
    EXPECT_EQ(maxGuid, guid);
}

TEST_F(UbseCtrlQGetIdevPfeGuidRespMsgTest, DecodeRespMsgNullBlocks)
{
    CtrlQRespMessage nullResp;
    nullResp.blocks = nullptr;
    nullResp.blockNums = 0;

    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(nullResp);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(UbseCtrlQGetIdevPfeGuidRespMsgTest, DecodeRespMsgWrongOpCode)
{
    resp_ = CreateValidIdevRespMsg(MakeTestGuid(0x01));
    resp_.blocks[0].head.opCode = 0xFF;

    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(UbseCtrlQGetIdevPfeGuidRespMsgTest, DecodeRespMsgWrongServiceType)
{
    resp_ = CreateValidIdevRespMsg(MakeTestGuid(0x01));
    resp_.blocks[0].head.serviceType = 0xFF;

    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(UbseCtrlQGetIdevPfeGuidRespMsgTest, DecodeRespMsgWrongBbNum)
{
    resp_ = CreateValidIdevRespMsg(MakeTestGuid(0x01));
    resp_.blocks[0].head.bbNum = 2;

    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(UbseCtrlQGetIdevPfeGuidRespMsgTest, GetGuidBeforeDecode)
{
    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    const auto& guid = respMsg.GetGuid();
    UbseMtiGuid expectedGuid;
    expectedGuid.fill(0xFF);
    EXPECT_EQ(expectedGuid, guid);
}

void UbseCtrlQGetIdevVfeGuidReqMsgTest::SetUp()
{
    UbseMtiUbController ctrl(0x01, 0x02);
    vfe_ = UbseMtiIdevVfe(ctrl, 0x03, 0x04);
}

TEST_F(UbseCtrlQGetIdevVfeGuidReqMsgTest, ConstructWithValidVfe)
{
    EXPECT_NO_THROW({ UbseCtrlQGetIdevVfeGuidReqMsg req(vfe_); });
}

TEST_F(UbseCtrlQGetIdevVfeGuidReqMsgTest, EncodeReqMsg)
{
    UbseCtrlQGetIdevVfeGuidReqMsg req(vfe_);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);

    const auto& msg = req.GetReqMsg();
    ASSERT_FALSE(msg.blocks.empty());

    auto& block = msg.blocks.front();
    EXPECT_EQ(0x2, block.head.opCode);
}

TEST_F(UbseCtrlQGetIdevVfeGuidReqMsgTest, EncodeReqMsgFeLocFields)
{
    UbseMtiUbController ctrl(1, 2);
    ctrl.slotId = 3;
    UbseMtiIdevVfe testVfe(ctrl, 4, 5);
    UbseCtrlQGetIdevVfeGuidReqMsg req(testVfe);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);

    const auto& msg = req.GetReqMsg();
    auto& block = msg.blocks.front();

    auto* feLoc = reinterpret_cast<const FeLoc*>(block.cmdData);
    EXPECT_EQ(3, feLoc->slotId);
    EXPECT_EQ(1, feLoc->chipId);
    EXPECT_EQ(2, feLoc->dieId);
    EXPECT_EQ(4, feLoc->pfeId);
    EXPECT_EQ(5, feLoc->vfeId);
}

TEST_F(UbseCtrlQGetIdevVfeGuidReqMsgTest, EncodeReqMsgVfeIdNotFF)
{
    UbseMtiUbController ctrl(0x01, 0x02);
    UbseMtiIdevVfe testVfe(ctrl, 0x03, 0x04);
    UbseCtrlQGetIdevVfeGuidReqMsg req(testVfe);
    req.EncodeReqMsg();

    const auto& msg = req.GetReqMsg();
    auto& block = msg.blocks.front();

    auto* feLoc = reinterpret_cast<const FeLoc*>(block.cmdData);
    EXPECT_NE(0xFF, feLoc->vfeId);
    EXPECT_EQ(0x04, feLoc->vfeId);
}

// ==================== UbseCtrlQGetIdevVfeGuidRespMsg 测试 ====================

TEST(UbseCtrlQGetIdevVfeGuidRespMsgTest, InheritsFromPfeGuidRespMsg)
{
    UbseCtrlQGetIdevVfeGuidRespMsg respMsg;
    auto* basePtr = dynamic_cast<UbseCtrlQGetIdevPfeGuidRespMsg*>(&respMsg);
    EXPECT_NE(nullptr, basePtr);
}

TEST(UbseCtrlQGetIdevVfeGuidRespMsgTest, DecodeValidRespMsg)
{
    auto testGuid = MakeTestGuid(0xCD);
    auto resp = CreateValidIdevRespMsg(testGuid);

    UbseCtrlQGetIdevVfeGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp);
    EXPECT_EQ(UBSE_OK, ret);

    const auto& guid = respMsg.GetGuid();
    EXPECT_EQ(testGuid, guid);

    FreeRespMsg(resp);
}

void UbseCtrlQGet1825PfGuidReqMsgTest::SetUp()
{
    pf_ = UbseMti1825Pf(0x01, 0x02, 0x03, 0x0004);
}

TEST_F(UbseCtrlQGet1825PfGuidReqMsgTest, ConstructWithValidPf)
{
    EXPECT_NO_THROW({ UbseCtrlQGet1825PfGuidReqMsg req(pf_); });
}

TEST_F(UbseCtrlQGet1825PfGuidReqMsgTest, EncodeReqMsg)
{
    UbseCtrlQGet1825PfGuidReqMsg req(pf_);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);

    const auto& msg = req.GetReqMsg();
    ASSERT_FALSE(msg.blocks.empty());

    auto& block = msg.blocks.front();
    EXPECT_EQ(0x10, block.head.opCode);
    EXPECT_EQ(DEFAULT_SERVICE_TYPE, block.head.serviceType);
}

TEST_F(UbseCtrlQGet1825PfGuidReqMsgTest, EncodeReqMsgFeLoc1825Fields)
{
    UbseMti1825Pf testPf(0x0A, 0x0B, 0x0C, 0x000D);
    UbseCtrlQGet1825PfGuidReqMsg req(testPf);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);

    const auto& msg = req.GetReqMsg();
    auto& block = msg.blocks.front();

    auto* feLoc = reinterpret_cast<const FeLoc1825*>(block.cmdData);
    EXPECT_EQ(0x0A, feLoc->slotId);
    EXPECT_EQ(0x0B, feLoc->chipId);
    EXPECT_EQ(0x0C, feLoc->dieId);
    EXPECT_EQ(0x000D, feLoc->feId);
}

TEST_F(UbseCtrlQGet1825PfGuidReqMsgTest, EncodeReqMsgZeroValues)
{
    UbseMti1825Pf zeroPf(0x00, 0x00, 0x00, 0x0000);
    UbseCtrlQGet1825PfGuidReqMsg req(zeroPf);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(UbseCtrlQGet1825PfGuidReqMsgTest, EncodeReqMsgMaxValues)
{
    UbseMti1825Pf maxPf(0xFF, 0xFF, 0xFF, 0xFFFF);
    UbseCtrlQGet1825PfGuidReqMsg req(maxPf);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);
}

void UbseCtrlQGet1825PfGuidRespMsgTest::TearDown()
{
    FreeRespMsg(resp_);
}

TEST_F(UbseCtrlQGet1825PfGuidRespMsgTest, DecodeValidRespMsg)
{
    auto testGuid = MakeTestGuid(0xEF);
    resp_ = CreateValid1825RespMsg(testGuid);

    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_OK, ret);

    const auto& guid = respMsg.GetGuid();
    EXPECT_EQ(testGuid, guid);
}

TEST_F(UbseCtrlQGet1825PfGuidRespMsgTest, DecodeRespMsgWithZeroGuid)
{
    auto zeroGuid = MakeTestGuid(0x00);
    resp_ = CreateValid1825RespMsg(zeroGuid);

    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(UbseCtrlQGet1825PfGuidRespMsgTest, DecodeRespMsgWithMaxGuid)
{
    auto maxGuid = MakeTestGuid(0xFF);
    resp_ = CreateValid1825RespMsg(maxGuid);

    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(UbseCtrlQGet1825PfGuidRespMsgTest, DecodeRespMsgNullBlocks)
{
    CtrlQRespMessage nullResp;
    nullResp.blocks = nullptr;
    nullResp.blockNums = 0;

    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(nullResp);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(UbseCtrlQGet1825PfGuidRespMsgTest, DecodeRespMsgWrongOpCode)
{
    resp_ = CreateValid1825RespMsg(MakeTestGuid(0x01));
    resp_.blocks[0].head.opCode = 0xFF;

    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(UbseCtrlQGet1825PfGuidRespMsgTest, DecodeRespMsgWrongServiceType)
{
    resp_ = CreateValid1825RespMsg(MakeTestGuid(0x01));
    resp_.blocks[0].head.serviceType = 0x00;

    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(UbseCtrlQGet1825PfGuidRespMsgTest, DecodeRespMsgWrongBbNum)
{
    resp_ = CreateValid1825RespMsg(MakeTestGuid(0x01));
    resp_.blocks[0].head.bbNum = 0;

    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp_);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(UbseCtrlQGet1825PfGuidRespMsgTest, GetGuidBeforeDecode)
{
    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    const auto& guid = respMsg.GetGuid();
    UbseMtiGuid expectedGuid;
    expectedGuid.fill(0xFF);
    EXPECT_EQ(expectedGuid, guid);
}

void UbseCtrlQGet1825VfGuidReqMsgTest::SetUp()
{
    vf_ = UbseMti1825Vf(0x01, 0x02, 0x03, 0x0004, 0x0005);
}

TEST_F(UbseCtrlQGet1825VfGuidReqMsgTest, ConstructWithValidVf)
{
    EXPECT_NO_THROW({ UbseCtrlQGet1825VfGuidReqMsg req(vf_); });
}

TEST_F(UbseCtrlQGet1825VfGuidReqMsgTest, EncodeReqMsg)
{
    UbseCtrlQGet1825VfGuidReqMsg req(vf_);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);

    const auto& msg = req.GetReqMsg();
    ASSERT_FALSE(msg.blocks.empty());

    auto& block = msg.blocks.front();
    EXPECT_EQ(0x10, block.head.opCode);
}

TEST_F(UbseCtrlQGet1825VfGuidReqMsgTest, EncodeReqMsgFeLoc1825Fields)
{
    UbseMti1825Vf testVf(0x0A, 0x0B, 0x0C, 0x000D, 0x000E);
    UbseCtrlQGet1825VfGuidReqMsg req(testVf);
    auto ret = req.EncodeReqMsg();
    EXPECT_EQ(UBSE_OK, ret);

    const auto& msg = req.GetReqMsg();
    auto& block = msg.blocks.front();

    auto* feLoc = reinterpret_cast<const FeLoc1825*>(block.cmdData);
    EXPECT_EQ(0x0A, feLoc->slotId);
    EXPECT_EQ(0x0B, feLoc->chipId);
    EXPECT_EQ(0x0C, feLoc->dieId);
    EXPECT_EQ(0x000E, feLoc->feId);
}

// ==================== UbseCtrlQGet1825VfGuidRespMsg 测试 ====================

TEST(UbseCtrlQGet1825VfGuidRespMsgTest, InheritsFromPfGuidRespMsg)
{
    UbseCtrlQGet1825VfGuidRespMsg respMsg;
    auto* basePtr = dynamic_cast<UbseCtrlQGet1825PfGuidRespMsg*>(&respMsg);
    EXPECT_NE(nullptr, basePtr);
}

TEST(UbseCtrlQGet1825VfGuidRespMsgTest, DecodeValidRespMsg)
{
    auto testGuid = MakeTestGuid(0x12);
    auto resp = CreateValid1825RespMsg(testGuid);

    UbseCtrlQGet1825VfGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp);
    EXPECT_EQ(UBSE_OK, ret);

    const auto& guid = respMsg.GetGuid();
    EXPECT_EQ(testGuid, guid);

    FreeRespMsg(resp);
}

// ==================== 编解码往返测试 ====================

class RoundTripTest : public ::testing::Test {
};

TEST_F(RoundTripTest, IdevPfeGuidRoundTrip)
{
    UbseMtiUbController ctrl(0x01, 0x02);
    UbseMtiIdevPfe pfe(ctrl, 0x03);

    UbseCtrlQGetIdevPfeGuidReqMsg req(pfe);
    EXPECT_EQ(UBSE_OK, req.EncodeReqMsg());

    auto testGuid = MakeTestGuid(0xAA);
    auto resp = CreateValidIdevRespMsg(testGuid);

    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    EXPECT_EQ(UBSE_OK, respMsg.DecodeRespMsg(resp));
    EXPECT_EQ(testGuid, respMsg.GetGuid());

    FreeRespMsg(resp);
}

TEST_F(RoundTripTest, IdevVfeGuidRoundTrip)
{
    UbseMtiUbController ctrl(0x01, 0x02);
    UbseMtiIdevVfe vfe(ctrl, 0x03, 0x04);

    UbseCtrlQGetIdevVfeGuidReqMsg req(vfe);
    EXPECT_EQ(UBSE_OK, req.EncodeReqMsg());

    auto testGuid = MakeTestGuid(0xBB);
    auto resp = CreateValidIdevRespMsg(testGuid);

    UbseCtrlQGetIdevVfeGuidRespMsg respMsg;
    EXPECT_EQ(UBSE_OK, respMsg.DecodeRespMsg(resp));
    EXPECT_EQ(testGuid, respMsg.GetGuid());

    FreeRespMsg(resp);
}

TEST_F(RoundTripTest, Pf1825GuidRoundTrip)
{
    UbseMti1825Pf pf(0x01, 0x02, 0x03, 0x0004);

    UbseCtrlQGet1825PfGuidReqMsg req(pf);
    EXPECT_EQ(UBSE_OK, req.EncodeReqMsg());

    auto testGuid = MakeTestGuid(0xCC);
    auto resp = CreateValid1825RespMsg(testGuid);

    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    EXPECT_EQ(UBSE_OK, respMsg.DecodeRespMsg(resp));
    EXPECT_EQ(testGuid, respMsg.GetGuid());

    FreeRespMsg(resp);
}

TEST_F(RoundTripTest, Vf1825GuidRoundTrip)
{
    UbseMti1825Vf vf(0x01, 0x02, 0x03, 0x0004, 0x0005);

    UbseCtrlQGet1825VfGuidReqMsg req(vf);
    EXPECT_EQ(UBSE_OK, req.EncodeReqMsg());

    auto testGuid = MakeTestGuid(0xDD);
    auto resp = CreateValid1825RespMsg(testGuid);

    UbseCtrlQGet1825VfGuidRespMsg respMsg;
    EXPECT_EQ(UBSE_OK, respMsg.DecodeRespMsg(resp));
    EXPECT_EQ(testGuid, respMsg.GetGuid());

    FreeRespMsg(resp);
}

// ==================== 操作码验证测试 ====================

TEST(OpCodeTest, IdevPfeUsesCorrectOpCode)
{
    UbseMtiUbController ctrl(0x01, 0x02);
    UbseMtiIdevPfe pfe(ctrl, 0x03);
    UbseCtrlQGetIdevPfeGuidReqMsg req(pfe);
    req.EncodeReqMsg();

    EXPECT_EQ(0x2, req.GetReqMsg().blocks.front().head.opCode);
}

TEST(OpCodeTest, IdevVfeUsesCorrectOpCode)
{
    UbseMtiUbController ctrl(0x01, 0x02);
    UbseMtiIdevVfe vfe(ctrl, 0x03, 0x04);
    UbseCtrlQGetIdevVfeGuidReqMsg req(vfe);
    req.EncodeReqMsg();

    EXPECT_EQ(0x2, req.GetReqMsg().blocks.front().head.opCode);
}

TEST(OpCodeTest, Pf1825UsesCorrectOpCode)
{
    UbseMti1825Pf pf(0x01, 0x02, 0x03, 0x0004);
    UbseCtrlQGet1825PfGuidReqMsg req(pf);
    req.EncodeReqMsg();

    EXPECT_EQ(0x10, req.GetReqMsg().blocks.front().head.opCode);
}

TEST(OpCodeTest, Vf1825UsesCorrectOpCode)
{
    UbseMti1825Vf vf(0x01, 0x02, 0x03, 0x0004, 0x0005);
    UbseCtrlQGet1825VfGuidReqMsg req(vf);
    req.EncodeReqMsg();

    EXPECT_EQ(0x10, req.GetReqMsg().blocks.front().head.opCode);
}

// ==================== Idev与1825操作码交叉验证 ====================

TEST(CrossOpCodeTest, IdevRespRejects1825OpCode)
{
    auto testGuid = MakeTestGuid(0x01);
    auto resp = CreateValid1825RespMsg(testGuid);

    UbseCtrlQGetIdevPfeGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp);
    EXPECT_EQ(UBSE_ERROR, ret);

    FreeRespMsg(resp);
}

TEST(CrossOpCodeTest, Resp1825RejectsIdevOpCode)
{
    auto testGuid = MakeTestGuid(0x01);
    auto resp = CreateValidIdevRespMsg(testGuid);

    UbseCtrlQGet1825PfGuidRespMsg respMsg;
    auto ret = respMsg.DecodeRespMsg(resp);
    EXPECT_EQ(UBSE_ERROR, ret);

    FreeRespMsg(resp);
}

// ==================== 继承关系测试 ====================

TEST(InheritanceTest, VfeGuidRespMsgInheritsFromPfeGuidRespMsg)
{
    static_assert(std::is_base_of<UbseCtrlQGetIdevPfeGuidRespMsg, UbseCtrlQGetIdevVfeGuidRespMsg>::value,
                  "UbseCtrlQGetIdevVfeGuidRespMsg should inherit from UbseCtrlQGetIdevPfeGuidRespMsg");
}

TEST(InheritanceTest, Vf1825GuidRespMsgInheritsFromPf1825GuidRespMsg)
{
    static_assert(std::is_base_of<UbseCtrlQGet1825PfGuidRespMsg, UbseCtrlQGet1825VfGuidRespMsg>::value,
                  "UbseCtrlQGet1825VfGuidRespMsg should inherit from UbseCtrlQGet1825PfGuidRespMsg");
}

TEST(InheritanceTest, PfeGuidReqMsgInheritsFromICtrlQReqMsg)
{
    static_assert(std::is_base_of<ICtrlQReqMsg, UbseCtrlQGetIdevPfeGuidReqMsg>::value,
                  "UbseCtrlQGetIdevPfeGuidReqMsg should inherit from ICtrlQReqMsg");
}

TEST(InheritanceTest, PfeGuidRespMsgInheritsFromICtrlQRespMsg)
{
    static_assert(std::is_base_of<ICtrlQRespMsg, UbseCtrlQGetIdevPfeGuidRespMsg>::value,
                  "UbseCtrlQGetIdevPfeGuidRespMsg should inherit from ICtrlQRespMsg");
}

} // namespace ubse::ut::mti::ctrl_q