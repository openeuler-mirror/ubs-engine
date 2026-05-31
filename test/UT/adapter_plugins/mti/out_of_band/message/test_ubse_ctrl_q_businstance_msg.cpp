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

#include "test_ubse_ctrl_q_businstance_msg.h"
#include <securec.h>
#include <ubse_error.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_businstance_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"

namespace ubse::ut::mti::ctrl_q {

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::bus_instance;

void TestUbseCtrlQBusInstanceMsg::SetUp()
{
    // 初始化测试数据
    testUpi_ = 0x1234;
    testVendor_ = 0x5678;

    // 初始化测试总线实例
    testBusInstance_.type = UbseMtiBusInstanceType::VM;
    testBusInstance_.upi = testUpi_;
    testBusInstance_.vendor = testVendor_;

    // 设置EID和GUID
    uint32_t testEidValue = 0x12345678;
    memcpy_s(testBusInstance_.eid.data(), testBusInstance_.eid.size(), &testEidValue, sizeof(testEidValue));

    uint8_t testGuidValue[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    memcpy_s(testBusInstance_.guid.data(), testBusInstance_.guid.size(), testGuidValue, sizeof(testGuidValue));
}

void TestUbseCtrlQBusInstanceMsg::TearDown()
{
    GlobalMockObject::verify();
}

// 测试 UbseCtrlQCreateBusInstanceReqMsg
TEST_F(TestUbseCtrlQBusInstanceMsg, CreateBusInstanceReqMsgTest)
{
    // 测试构造函数
    UbseCtrlQCreateBusInstanceReqMsg reqMsg(testUpi_, testVendor_);
    EXPECT_EQ(reqMsg.GetUpi(), testUpi_);
    EXPECT_EQ(reqMsg.GetVendor(), testVendor_);

    // 测试 EncodeReqMsg
    auto result = reqMsg.EncodeReqMsg();
    EXPECT_EQ(result, UBSE_OK);
}

// 测试 UbseCtrlQCreateBusInstanceRespMsg
TEST_F(TestUbseCtrlQBusInstanceMsg, CreateBusInstanceRespMsgTest)
{
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 1;
    block.head.opCode = 0x4; // CREATE_BUSINSTANCE_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    // 设置EID
    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    // EID: 0x12345
    data[0] = 0x45; // 低8位
    data[1] = 0x23; // 中8位
    data[2] = 0x01; // 高4位
    data[3] = 0x00; // 保留位

    // 设置GUID
    uint8_t testGuidValue[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    memcpy_s(data + 4, 16, testGuidValue, sizeof(testGuidValue));

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码
    UbseCtrlQCreateBusInstanceRespMsg respObj;
    auto result = respObj.DecodeRespMsg(respMsg);
    EXPECT_EQ(result, UBSE_OK);

    // 测试获取总线实例
    const auto& busInstance = respObj.GetBusInstance();
    uint32_t expectedEid = 0x12345;
    uint32_t actualEid = 0;
    memcpy_s(&actualEid, sizeof(actualEid), busInstance.eid.data(), sizeof(actualEid));
    EXPECT_EQ(actualEid, expectedEid);

    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(busInstance.guid[i], testGuidValue[i]);
    }
}

// 测试 UbseCtrlQDestroyBusInstanceReqMsg
TEST_F(TestUbseCtrlQBusInstanceMsg, DestroyBusInstanceReqMsgTest)
{
    // 测试构造函数
    UbseCtrlQDestroyBusInstanceReqMsg reqMsg(testBusInstance_);

    // 测试 EncodeReqMsg
    auto result = reqMsg.EncodeReqMsg();
    EXPECT_EQ(result, UBSE_OK);
}

// 测试 UbseCtrlQDestroyBusInstanceRespMsg
TEST_F(TestUbseCtrlQBusInstanceMsg, DestroyBusInstanceRespMsgTest)
{
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 1;
    block.head.opCode = 0x5; // DESTROY_BUSINSTANCE_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码
    UbseCtrlQDestroyBusInstanceRespMsg respObj;
    auto result = respObj.DecodeRespMsg(respMsg);
    EXPECT_EQ(result, UBSE_OK);

    // 测试获取返回值
    EXPECT_TRUE(respObj.GetRet());
}

// 测试错误情况
TEST_F(TestUbseCtrlQBusInstanceMsg, CreateBusInstanceRespMsgInvalidTest)
{
    // 测试无效的响应消息（bbNum不为1）
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 2;    // 无效的bbNum
    block.head.opCode = 0x4; // CREATE_BUSINSTANCE_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQCreateBusInstanceRespMsg respObj;
    auto result = respObj.DecodeRespMsg(respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseCtrlQBusInstanceMsg, DestroyBusInstanceRespMsgInvalidTest)
{
    // 测试无效的响应消息（bbNum不为1）
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 2;    // 无效的bbNum
    block.head.opCode = 0x5; // DESTROY_BUSINSTANCE_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    UbseCtrlQDestroyBusInstanceRespMsg respObj;
    auto result = respObj.DecodeRespMsg(respMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

} // namespace ubse::ut::mti::ctrl_q
