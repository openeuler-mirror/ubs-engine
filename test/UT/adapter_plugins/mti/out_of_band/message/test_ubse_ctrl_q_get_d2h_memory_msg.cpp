/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_ctrl_q_get_d2h_memory_msg.h"
#include <securec.h>
#include <ubse_error.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::bus_instance;

namespace ubse::ut::mti::ctrl_q {
void TestUbseCtrlQGetD2hMemoryMsg::SetUp()
{
    // 初始化测试数据
    testUpi_ = 0x1234;
    testVendor_ = 0x5678;

    // 初始化测试总线实例
    testBusInstance_.type = ubse::mti::bus_instance::UbseMtiBusInstanceType::HOST;
    testBusInstance_.upi = testUpi_;
    testBusInstance_.vendor = testVendor_;

    // 设置EID和GUID
    uint32_t testEidValue = 0x12345678;
    memcpy_s(testBusInstance_.eid.data(), testBusInstance_.eid.size(), &testEidValue, sizeof(testEidValue));

    uint8_t testGuidValue[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    memcpy_s(testBusInstance_.guid.data(), testBusInstance_.guid.size(), testGuidValue, sizeof(testGuidValue));
}

void TestUbseCtrlQGetD2hMemoryMsg::TearDown()
{
    GlobalMockObject::verify();
}

// 测试 UbseCtrlQGetD2hMemoryReqMsg
TEST_F(TestUbseCtrlQGetD2hMemoryMsg, GetD2hMemoryReqMsgTest)
{
    // 测试构造函数
    UbseCtrlQGetD2hMemoryReqMsg reqMsg(testBusInstance_);

    // 测试编码功能
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

// 测试 UbseCtrlQGetD2hMemoryRespMsg
TEST_F(TestUbseCtrlQGetD2hMemoryMsg, GetD2hMemoryRespMsgTest)
{
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 0x1;
    block.head.bbNum = 1;
    block.head.opCode = 0xD; // GET_UBA_TID_SIZE_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    // 设置响应数据
    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    // eidTid (eid: 0x12345, tid: 0x678)
    data[0] = 0x45; // eid 低8位
    data[1] = 0x23; // eid 中8位
    data[2] = 0x81; // tid 高4位 (0x1) + eid 低4位 (0x8)
    data[3] = 0x67; // tid 高8位
    // ubaHigh
    data[4] = 0x12;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    // ubaLow
    data[8] = 0x90;
    data[9] = 0x78;
    data[10] = 0x56;
    data[11] = 0x34;
    // size
    data[12] = 0x00;
    data[13] = 0x10;
    data[14] = 0x00;
    data[15] = 0x00;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能
    UbseCtrlQGetD2hMemoryRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    // 测试获取结果
    EXPECT_EQ(resp.GetTid(), 0x678);
    EXPECT_EQ(resp.GetUba(), 0x1234567890);
    EXPECT_EQ(resp.GetSize(), 0x1000);
}

// 测试无效的 opCode
TEST_F(TestUbseCtrlQGetD2hMemoryMsg, GetD2hMemoryRespMsg_InvalidOpCode)
{
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息，使用无效的 opCode
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 1;
    block.head.opCode = 0xFF; // 无效的 opCode
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能，应该返回错误
    UbseCtrlQGetD2hMemoryRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

// 测试无效的 bbNum
TEST_F(TestUbseCtrlQGetD2hMemoryMsg, GetD2hMemoryRespMsg_InvalidBbNum)
{
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息，使用无效的 bbNum
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 2;    // 无效的 bbNum，应该为 1
    block.head.opCode = 0xD; // GET_UBA_TID_SIZE_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能，应该返回错误
    UbseCtrlQGetD2hMemoryRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

} // namespace ubse::ut::mti::ctrl_q
