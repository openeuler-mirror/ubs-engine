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

#include "test_ubse_ctrl_q_get_1825_fe_msg.h"
#include <securec.h>
#include <ubse_error.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_fe_guid_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"
#include "src/adapter_plugins/mti/out_of_band/proxy/ubse_ctrl_q_msg_proxy.h"

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::_1825;

namespace ubse::ut::mti::ctrl_q {
void TestUbseCtrlQGet1825FeMsg::SetUp() {}

void TestUbseCtrlQGet1825FeMsg::TearDown()
{
    GlobalMockObject::verify();
}

// 测试 UbseCtrlQGet1825FeReqMsg
TEST_F(TestUbseCtrlQGet1825FeMsg, Get1825FeReqMsgTest)
{
    // 测试构造函数
    UbseCtrlQGet1825FeReqMsg reqMsg;

    // 测试编码功能
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

// 测试 UbseCtrlQGet1825PfeRespMsg
TEST_F(TestUbseCtrlQGet1825FeMsg, Get1825PfeRespMsgTest)
{
    // 准备测试数据 - 使用两个block来容纳超过32字节的数据
    CtrlQBasicBlock blocks[2];

    // 设置第一个block的头部信息
    blocks[0].head.version = 1;
    blocks[0].head.serviceType = 0x1;
    blocks[0].head.bbNum = 0;
    blocks[0].head.opCode = 0xB; // GET_1825_FE_OP_CODE
    blocks[0].head.ret = 0;
    blocks[0].head.seq = 1;
    blocks[0].head.resv = 0;

    // 设置第二个block的头部信息
    blocks[1].head.version = 1;
    blocks[1].head.serviceType = 0x1;
    blocks[1].head.bbNum = 0;
    blocks[1].head.opCode = 0xB; // GET_1825_FE_OP_CODE
    blocks[1].head.ret = 0;
    blocks[1].head.seq = 1;
    blocks[1].head.resv = 0;

    // 设置响应数据
    // 假设响应数据包含1个slot，2个chip，每个chip下1个die，2个pf，每个pf下2个vf
    uint8_t* data0 = reinterpret_cast<uint8_t*>(&blocks[0].cmdData);
    // slotId
    data0[0] = 13;
    // chipNum
    data0[1] = 2;
    // 第一个chip
    // chipId
    data0[2] = 1;
    // dieNum
    data0[3] = 1;
    // dieId
    data0[4] = 1;
    // pfNum
    data0[5] = 2;
    // 第一个PF
    // pfId
    data0[6] = 0; // 低字节
    data0[7] = 0; // 高字节
    // vfNum
    data0[8] = 2; // 低字节
    data0[9] = 0; // 高字节
    // 第一个VF
    // vfId
    data0[10] = 64; // 低字节
    data0[11] = 0;  // 高字节
    // 第二个VF
    // vfId
    data0[12] = 65; // 低字节
    data0[13] = 0;  // 高字节
    // 第二个PF
    // pfId
    data0[14] = 1; // 低字节
    data0[15] = 0; // 高字节
    // vfNum
    data0[16] = 2; // 低字节
    data0[17] = 0; // 高字节
    // 第一个VF
    // vfId
    data0[18] = 66; // 低字节
    data0[19] = 0;  // 高字节
    // 第二个VF
    // vfId
    data0[20] = 67; // 低字节
    data0[21] = 0;  // 高字节

    // 第二个chip的数据在第二个block中
    uint8_t* data1 = reinterpret_cast<uint8_t*>(&blocks[1].cmdData);
    data1[0] = 2; // chipId
    data1[1] = 1; // dieNum
    data1[2] = 1; // dieId
    data1[3] = 1; // pfNum
    // 第一个PF
    data1[4] = 0; // pfId 低字节
    data1[5] = 0; // pfId 高字节
    data1[6] = 0; // vfNum 低字节
    data1[7] = 0; // vfNum 高字节

    CtrlQRespMessage respMsg;
    respMsg.blocks = blocks;
    respMsg.blockNums = 2;

    // 模拟 CtrlQMsgProxy::SendRequest 方法
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    // 测试解码功能
    UbseCtrlQGet1825PfeRespMsg resp;
    // 注意：由于解码过程中会调用GetGuid，这需要外部依赖，所以实际运行时可能会失败
    // 这里主要测试解码的基本流程
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    // 测试获取结果列表
    const auto& pfList = resp.GetPfList();
    // 结果列表应该不为空，因为我们模拟了SendRequest返回UBSE_OK
    EXPECT_FALSE(pfList.empty());
}

// 测试 SendRequest 失败的情况
TEST_F(TestUbseCtrlQGet1825FeMsg, Get1825PfeRespMsg_SendRequestFailed)
{
    // 准备测试数据 - 使用两个block来容纳超过32字节的数据
    CtrlQBasicBlock blocks[2];

    // 设置第一个block的头部信息
    blocks[0].head.version = 1;
    blocks[0].head.serviceType = 0x1;
    blocks[0].head.bbNum = 0;
    blocks[0].head.opCode = 0xB; // GET_1825_FE_OP_CODE
    blocks[0].head.ret = 0;
    blocks[0].head.seq = 1;
    blocks[0].head.resv = 0;

    // 设置第二个block的头部信息
    blocks[1].head.version = 1;
    blocks[1].head.serviceType = 0x1;
    blocks[1].head.bbNum = 0;
    blocks[1].head.opCode = 0xB; // GET_1825_FE_OP_CODE
    blocks[1].head.ret = 0;
    blocks[1].head.seq = 1;
    blocks[1].head.resv = 0;

    // 设置响应数据
    // 假设响应数据包含1个slot，2个chip，每个chip下1个die，2个pf，每个pf下2个vf
    uint8_t* data0 = reinterpret_cast<uint8_t*>(&blocks[0].cmdData);
    // slotId
    data0[0] = 13;
    // chipNum
    data0[1] = 2;
    // 第一个chip
    // chipId
    data0[2] = 1;
    // dieNum
    data0[3] = 1;
    // dieId
    data0[4] = 1;
    // pfNum
    data0[5] = 2;
    // 第一个PF
    // pfId
    data0[6] = 0; // 低字节
    data0[7] = 0; // 高字节
    // vfNum
    data0[8] = 2; // 低字节
    data0[9] = 0; // 高字节
    // 第一个VF
    // vfId
    data0[10] = 64; // 低字节
    data0[11] = 0;  // 高字节
    // 第二个VF
    // vfId
    data0[12] = 65; // 低字节
    data0[13] = 0;  // 高字节
    // 第二个PF
    // pfId
    data0[14] = 1; // 低字节
    data0[15] = 0; // 高字节
    // vfNum
    data0[16] = 2; // 低字节
    data0[17] = 0; // 高字节
    // 第一个VF
    // vfId
    data0[18] = 66; // 低字节
    data0[19] = 0;  // 高字节
    // 第二个VF
    // vfId
    data0[20] = 67; // 低字节
    data0[21] = 0;  // 高字节

    // 第二个chip的数据在第二个block中
    uint8_t* data1 = reinterpret_cast<uint8_t*>(&blocks[1].cmdData);
    data1[0] = 2; // chipId
    data1[1] = 1; // dieNum
    data1[2] = 1; // dieId
    data1[3] = 1; // pfNum
    // 第一个PF
    data1[4] = 0; // pfId 低字节
    data1[5] = 0; // pfId 高字节
    data1[6] = 0; // vfNum 低字节
    data1[7] = 0; // vfNum 高字节

    CtrlQRespMessage respMsg;
    respMsg.blocks = blocks;
    respMsg.blockNums = 2;

    // 模拟 CtrlQMsgProxy::SendRequest 方法返回错误
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    // 测试解码功能
    UbseCtrlQGet1825PfeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
    // 测试获取结果列表
    const auto& pfList = resp.GetPfList();
    // 结果列表应该为空，因为解码失败
    EXPECT_TRUE(pfList.empty());
}

// 测试无效的 opCode
TEST_F(TestUbseCtrlQGet1825FeMsg, Get1825PfeRespMsg_InvalidOpCode)
{
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息，使用无效的 opCode
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0xFF; // 无效的 opCode
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能，应该返回错误
    UbseCtrlQGet1825PfeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

// 测试空的 pfList
TEST_F(TestUbseCtrlQGet1825FeMsg, Get1825PfeRespMsg_EmptyPfList)
{
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0xB; // GET_1825_FE_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    // 设置响应数据 - 空的 pf 列表
    uint8_t* data = reinterpret_cast<uint8_t*>(&block.cmdData);
    data[0] = 1; // slotId
    data[1] = 1; // chipNum
    data[2] = 2; // chipId
    data[3] = 1; // dieNum
    data[4] = 3; // dieId
    data[5] = 0; // pfNum 为 0

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 模拟 CtrlQMsgProxy::SendRequest 方法
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    // 测试解码功能
    UbseCtrlQGet1825PfeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    // 测试获取结果列表 - 应该为空
    const auto& pfList = resp.GetPfList();
    EXPECT_TRUE(pfList.empty());
}

// 测试错误情况 - bbNum不为0
TEST_F(TestUbseCtrlQGet1825FeMsg, Get1825PfeRespMsg_InvalidBbNum)
{
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息，使用无效的 bbNum
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 1;    // 错误的bbNum，应该为0
    block.head.opCode = 0xB; // GET_1825_FE_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;

    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能，应该返回错误
    UbseCtrlQGet1825PfeRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

} // namespace ubse::ut::mti::ctrl_q
