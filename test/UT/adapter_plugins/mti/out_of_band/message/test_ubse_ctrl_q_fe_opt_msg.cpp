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

#include "test_ubse_ctrl_q_fe_opt_msg.h"
#include <mockcpp/mockcpp.hpp>
#include <ubse_error.h>
#include <securec.h>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_fe_opt_msg.h"
#include "src/include/adapter_plugins/mti/ubse_mti_bus_instance.h"
#include "src/include/adapter_plugins/mti/ubse_mti_urma.h"
#include "src/include/adapter_plugins/mti/ubse_mti_1825.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ictrl_q_req_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ictrl_q_resp_msg.h"

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::bus_instance;
using namespace ubse::mti::urma;
using namespace ubse::mti::_1825;

namespace ubse::ut::mti::ctrl_q {

void TestUbseCtrlQFeOptMsg::SetUp() {
    // 初始化测试数据
    testUpi_ = 1234;
    testVendor_ = 5678;
    
    // 初始化测试总线实例
    testBusInstance_.type = ubse::mti::bus_instance::UbseMtiBusInstanceType::VM;
    testBusInstance_.upi = testUpi_;
    testBusInstance_.vendor = testVendor_;
    
    // 初始化eid和guid
    std::fill(testBusInstance_.eid.begin(), testBusInstance_.eid.end(), 0);
    testBusInstance_.eid[0] = 1;
    testBusInstance_.eid[1] = 2;
    testBusInstance_.eid[2] = 3;
    
    std::fill(testBusInstance_.guid.begin(), testBusInstance_.guid.end(), 0);
    testBusInstance_.guid[0] = 0x10;
    testBusInstance_.guid[1] = 0x20;
    testBusInstance_.guid[2] = 0x30;
}

void TestUbseCtrlQFeOptMsg::TearDown() {
    // 清理测试数据
    GlobalMockObject::verify();
}

// 测试 UbseCtrlQRegDavidFeToBusInstanceReqMsg
TEST_F(TestUbseCtrlQFeOptMsg, RegDavidFeToBusInstanceReqMsgTest) {
    // 准备测试数据
    std::vector<UbseMtiIdevVfe> vfeList;
    UbseMtiIdevVfe vfe;
    vfe.ubController.slotId = 1;
    vfe.ubController.chipId = 2;
    vfe.ubController.dieId = 3;
    vfe.pfeId = 4;
    vfe.guid.fill(0);
    vfe.vfeId = 5;
    vfeList.push_back(vfe);

    // 测试构造函数
    UbseCtrlQRegDavidFeToBusInstanceReqMsg reqMsg(testBusInstance_, vfeList);

    // 测试编码功能
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

// 测试 UbseCtrlQRegDavidFeToBusInstanceRespMsg
TEST_F(TestUbseCtrlQFeOptMsg, RegDavidFeToBusInstanceRespMsgTest) {
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x7; // REG_IDEV_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    
    // 设置响应数据
    uint8_t *data = reinterpret_cast<uint8_t *>(&block.cmdData);
    // 设置结果数量为2
    data[0] = 2;
    // 设置第一个结果为成功
    data[1] = UBSE_OK;
    // 设置第二个结果为失败
    data[2] = UBSE_ERROR;
    
    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能
    UbseCtrlQRegDavidFeToBusInstanceRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    // 测试获取结果列表
    const auto &retList = resp.GetRetList();
    // 结果列表应该包含2个元素
    EXPECT_EQ(retList.size(), 2);
    // 第一个结果应该为true（成功）
    EXPECT_TRUE(retList[0]);
    // 第二个结果应该为false（失败）
    EXPECT_FALSE(retList[1]);
}

// 测试 UbseCtrlQReg1825FeToBusInstanceReqMsg
TEST_F(TestUbseCtrlQFeOptMsg, Reg1825FeToBusInstanceReqMsgTest) {
    // 准备测试数据
    std::vector<UbseMti1825Vf> vfList;
    UbseMti1825Vf vf;
    vf.slotId = 1;
    vf.chipId = 2;
    vf.dieId = 3;
    vf.pfId = 4;
    vf.affinityUbController.slotId = 1;
    vf.affinityUbController.chipId = 2;
    vf.affinityUbController.dieId = 3;
    vf.guid.fill(0);
    vf.vfId = 5;
    vfList.push_back(vf);

    // 测试构造函数
    UbseCtrlQReg1825FeToBusInstanceReqMsg reqMsg(testBusInstance_, vfList);

    // 测试编码功能
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

// 测试 UbseCtrlQReg1825FeToBusInstanceRespMsg
TEST_F(TestUbseCtrlQFeOptMsg, Reg1825FeToBusInstanceRespMsgTest) {
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0xF; // REG_DEV_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    
    // 设置响应数据
    uint8_t *data = reinterpret_cast<uint8_t *>(&block.cmdData);
    // 设置结果数量为2
    data[0] = 2;
    // 设置第一个结果为成功
    data[1] = UBSE_OK;
    // 设置第二个结果为失败
    data[2] = UBSE_ERROR;
    
    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能
    UbseCtrlQReg1825FeToBusInstanceRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    // 测试获取结果列表
    const auto &retList = resp.GetRetList();
    // 结果列表应该包含2个元素
    EXPECT_EQ(retList.size(), 2);
    // 第一个结果应该为true（成功）
    EXPECT_TRUE(retList[0]);
    // 第二个结果应该为false（失败）
    EXPECT_FALSE(retList[1]);
}

// 测试 UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg
TEST_F(TestUbseCtrlQFeOptMsg, UnRegDavidFeFromBusInstanceReqMsgTest) {
    // 准备测试数据
    std::vector<UbseMtiIdevVfe> vfeList;
    UbseMtiIdevVfe vfe;
    vfe.ubController.slotId = 1;
    vfe.ubController.chipId = 2;
    vfe.ubController.dieId = 3;
    vfe.pfeId = 4;
    vfe.guid.fill(0);
    vfe.vfeId = 5;
    vfeList.push_back(vfe);

    // 测试构造函数
    UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg reqMsg(testBusInstance_, vfeList);

    // 测试编码功能
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

// 测试 UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg
TEST_F(TestUbseCtrlQFeOptMsg, UnRegDavidFeFromBusInstanceRespMsgTest) {
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0x8; // UNREG_IDEV_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    
    // 设置响应数据
    uint8_t *data = reinterpret_cast<uint8_t *>(&block.cmdData);
    // 设置结果数量为2
    data[0] = 2;
    // 设置第一个结果为成功
    data[1] = UBSE_OK;
    // 设置第二个结果为失败
    data[2] = UBSE_ERROR;
    
    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能
    UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    // 测试获取结果列表
    const auto &retList = resp.GetRetList();
    // 结果列表应该包含2个元素
    EXPECT_EQ(retList.size(), 2);
    // 第一个结果应该为true（成功）
    EXPECT_TRUE(retList[0]);
    // 第二个结果应该为false（失败）
    EXPECT_FALSE(retList[1]);
}

// 测试 UbseCtrlQUnReg1825FeFromBusInstanceReqMsg
TEST_F(TestUbseCtrlQFeOptMsg, UnReg1825FeFromBusInstanceReqMsgTest) {
    // 准备测试数据
    std::vector<UbseMti1825Vf> vfList;
    UbseMti1825Vf vf;
    vf.slotId = 1;
    vf.chipId = 2;
    vf.dieId = 3;
    vf.pfId = 4;
    vf.affinityUbController.slotId = 1;
    vf.affinityUbController.chipId = 2;
    vf.affinityUbController.dieId = 3;
    vf.guid.fill(0);
    vf.vfId = 5;
    vfList.push_back(vf);

    // 测试构造函数
    UbseCtrlQUnReg1825FeFromBusInstanceReqMsg reqMsg(testBusInstance_, vfList);

    // 测试编码功能
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_OK);
}

// 测试 UbseCtrlQUnReg1825FeFromBusInstanceRespMsg
TEST_F(TestUbseCtrlQFeOptMsg, UnReg1825FeFromBusInstanceRespMsgTest) {
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 1;
    block.head.bbNum = 0;
    block.head.opCode = 0xE; // UNREG_DEV_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    
    // 设置响应数据
    uint8_t *data = reinterpret_cast<uint8_t *>(&block.cmdData);
    // 设置结果数量为2
    data[0] = 2;
    // 设置第一个结果为成功
    data[1] = UBSE_OK;
    // 设置第二个结果为失败
    data[2] = UBSE_ERROR;
    
    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能
    UbseCtrlQUnReg1825FeFromBusInstanceRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_OK);
    // 测试获取结果列表
    const auto &retList = resp.GetRetList();
    // 结果列表应该包含2个元素
    EXPECT_EQ(retList.size(), 2);
    // 第一个结果应该为true（成功）
    EXPECT_TRUE(retList[0]);
    // 第二个结果应该为false（失败）
    EXPECT_FALSE(retList[1]);
}

// 测试错误情况 - 空的vfList
TEST_F(TestUbseCtrlQFeOptMsg, Reg1825FeEmptyVfListTest) {
    // 准备测试数据
    // 空的vfList
    std::vector<UbseMti1825Vf> vfList;

    // 测试构造函数
    UbseCtrlQReg1825FeToBusInstanceReqMsg reqMsg(testBusInstance_, vfList);

    // 测试编码功能，应该返回错误
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_ERROR);
}

// 测试错误情况 - 不一致的slotId
TEST_F(TestUbseCtrlQFeOptMsg, Reg1825FeInconsistentSlotIdTest) {
    // 准备测试数据
    std::vector<UbseMti1825Vf> vfList;
    
    // 第一个vf的slotId为1
    UbseMti1825Vf vf1;
    vf1.slotId = 1;
    vf1.chipId = 2;
    vf1.dieId = 3;
    vf1.pfId = 4;
    vf1.affinityUbController.slotId = 1;
    vf1.affinityUbController.chipId = 2;
    vf1.affinityUbController.dieId = 3;
    vf1.guid.fill(0);
    vf1.vfId = 5;
    vfList.push_back(vf1);
    
    // 第二个vf的slotId为2，与第一个不一致
    UbseMti1825Vf vf2;
    vf2.slotId = 2;
    vf2.chipId = 2;
    vf2.dieId = 3;
    vf2.pfId = 4;
    vf2.affinityUbController.slotId = 2;
    vf2.affinityUbController.chipId = 2;
    vf2.affinityUbController.dieId = 3;
    vf2.guid.fill(0);
    vf2.vfId = 6;
    vfList.push_back(vf2);

    // 测试构造函数
    UbseCtrlQReg1825FeToBusInstanceReqMsg reqMsg(testBusInstance_, vfList);

    // 测试编码功能，应该返回错误
    EXPECT_EQ(reqMsg.EncodeReqMsg(), UBSE_ERROR);
}

// 测试错误情况 - 无效的serviceType
TEST_F(TestUbseCtrlQFeOptMsg, RegDavidFeInvalidServiceTypeTest) {
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 0; // 错误的serviceType，应该为1
    block.head.bbNum = 0;
    block.head.opCode = 0x7; // REG_IDEV_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    
    // 设置响应数据
    uint8_t *data = reinterpret_cast<uint8_t *>(&block.cmdData);
    // 设置结果数量为2
    data[0] = 2;
    // 设置第一个结果为成功
    data[1] = UBSE_OK;
    // 设置第二个结果为失败
    data[2] = UBSE_ERROR;
    
    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能，应该失败
    UbseCtrlQRegDavidFeToBusInstanceRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

// 测试错误情况 - 无效的serviceType
TEST_F(TestUbseCtrlQFeOptMsg, Reg1825FeInvalidServiceTypeTest) {
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 0; // 错误的serviceType，应该为1
    block.head.bbNum = 0;
    block.head.opCode = 0xF; // REG_DEV_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    
    // 设置响应数据
    uint8_t *data = reinterpret_cast<uint8_t *>(&block.cmdData);
    // 设置结果数量为2
    data[0] = 2;
    // 设置第一个结果为成功
    data[1] = UBSE_OK;
    // 设置第二个结果为失败
    data[2] = UBSE_ERROR;
    
    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能，应该失败
    UbseCtrlQReg1825FeToBusInstanceRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

// 测试错误情况 - 无效的serviceType
TEST_F(TestUbseCtrlQFeOptMsg, UnRegDavidFeInvalidServiceTypeTest) {
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 0; // 错误的serviceType，应该为1
    block.head.bbNum = 0;
    block.head.opCode = 0x8; // UNREG_IDEV_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    
    // 设置响应数据
    uint8_t *data = reinterpret_cast<uint8_t *>(&block.cmdData);
    // 设置结果数量为2
    data[0] = 2;
    // 设置第一个结果为成功
    data[1] = UBSE_OK;
    // 设置第二个结果为失败
    data[2] = UBSE_ERROR;
    
    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能，应该失败
    UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

// 测试错误情况 - 无效的serviceType
TEST_F(TestUbseCtrlQFeOptMsg, UnReg1825FeInvalidServiceTypeTest) {
    // 准备测试数据
    CtrlQBasicBlock block;
    // 设置头部信息
    block.head.version = 1;
    block.head.serviceType = 0; // 错误的serviceType，应该为1
    block.head.bbNum = 0;
    block.head.opCode = 0xE; // UNREG_DEV_OP_CODE
    block.head.ret = 0;
    block.head.seq = 1;
    block.head.resv = 0;
    
    // 设置响应数据
    uint8_t *data = reinterpret_cast<uint8_t *>(&block.cmdData);
    // 设置结果数量为2
    data[0] = 2;
    // 设置第一个结果为成功
    data[1] = UBSE_OK;
    // 设置第二个结果为失败
    data[2] = UBSE_ERROR;
    
    CtrlQRespMessage respMsg;
    respMsg.blocks = &block;
    respMsg.blockNums = 1;

    // 测试解码功能，应该失败
    UbseCtrlQUnReg1825FeFromBusInstanceRespMsg resp;
    EXPECT_EQ(resp.DecodeRespMsg(respMsg), UBSE_ERROR);
}

} // namespace ubse::ut::mti::ctrl_q