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

#include "test_ubse_mti_1825_out_of_band.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_error.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_fe_opt_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_1825_fe_msg.h"
#include "src/adapter_plugins/mti/out_of_band/proxy/ubse_ctrl_q_msg_proxy.h"

namespace ubse::ut::mti::_1825 {

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::bus_instance;
using namespace ubse::mti::_1825;

void TestUbseMti1825OutOfBand::SetUp()
{
    hostBusInstance_.type = UbseMtiBusInstanceType::HOST;
    hostBusInstance_.upi = 0x1234;
    hostBusInstance_.vendor = 0x5678;
    hostBusInstance_.guid.fill(0xAB);
    hostBusInstance_.eid.fill(0xCD);

    vmBusInstance_.type = UbseMtiBusInstanceType::VM;
    vmBusInstance_.upi = 0x2345;
    vmBusInstance_.vendor = 0x6789;
    vmBusInstance_.guid.fill(0xEF);
    vmBusInstance_.eid.fill(0x01);

    UbseMti1825Vf vf1(0x01, 0x02, 0x03, 0x0004, 0x0005);
    vf1.affinityUbController.slotId = 0x01;
    vf1.affinityUbController.chipId = 0x02;
    vf1.affinityUbController.dieId = 0x03;
    vf1.guid.fill(0x11);

    UbseMti1825Vf vf2(0x01, 0x02, 0x03, 0x0004, 0x0006);
    vf2.affinityUbController.slotId = 0x01;
    vf2.affinityUbController.chipId = 0x02;
    vf2.affinityUbController.dieId = 0x03;
    vf2.guid.fill(0x22);

    vfList_.push_back(vf1);
    vfList_.push_back(vf2);
}

void TestUbseMti1825OutOfBand::TearDown()
{
    GlobalMockObject::verify();
}

// ==================== Get1825FeList 测试 ====================

TEST_F(TestUbseMti1825OutOfBand, Get1825FeList_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<UbseMti1825Pf> pfList;
    auto ret = outOfBand_.Get1825FeList(pfList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMti1825OutOfBand, Get1825FeList_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbseMti1825Pf> pfList;
    auto ret = outOfBand_.Get1825FeList(pfList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== Reg1825FeToHostBusInstance 测试 ====================

TEST_F(TestUbseMti1825OutOfBand, Reg1825FeToHostBusInstance_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<bool> resList;
    auto ret = outOfBand_.Reg1825FeToHostBusInstance(hostBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMti1825OutOfBand, Reg1825FeToHostBusInstance_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<bool> resList;
    auto ret = outOfBand_.Reg1825FeToHostBusInstance(hostBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMti1825OutOfBand, Reg1825FeToHostBusInstance_TypeMismatch)
{
    std::vector<bool> resList;
    auto ret = outOfBand_.Reg1825FeToHostBusInstance(vmBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== UnReg1825FeFromHostBusInstance 测试 ====================

TEST_F(TestUbseMti1825OutOfBand, UnReg1825FeFromHostBusInstance_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<bool> resList;
    auto ret = outOfBand_.UnReg1825FeFromHostBusInstance(hostBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMti1825OutOfBand, UnReg1825FeFromHostBusInstance_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<bool> resList;
    auto ret = outOfBand_.UnReg1825FeFromHostBusInstance(hostBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMti1825OutOfBand, UnReg1825FeFromHostBusInstance_TypeMismatch)
{
    std::vector<bool> resList;
    auto ret = outOfBand_.UnReg1825FeFromHostBusInstance(vmBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== Reg1825FeToVmBusInstance 测试 ====================

TEST_F(TestUbseMti1825OutOfBand, Reg1825FeToVmBusInstance_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<bool> resList;
    auto ret = outOfBand_.Reg1825FeToVmBusInstance(vmBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMti1825OutOfBand, Reg1825FeToVmBusInstance_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<bool> resList;
    auto ret = outOfBand_.Reg1825FeToVmBusInstance(vmBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMti1825OutOfBand, Reg1825FeToVmBusInstance_TypeMismatch)
{
    std::vector<bool> resList;
    auto ret = outOfBand_.Reg1825FeToVmBusInstance(hostBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== UnReg1825FeFromVmBusInstance 测试 ====================

TEST_F(TestUbseMti1825OutOfBand, UnReg1825FeFromVmBusInstance_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<bool> resList;
    auto ret = outOfBand_.UnReg1825FeFromVmBusInstance(vmBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMti1825OutOfBand, UnReg1825FeFromVmBusInstance_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<bool> resList;
    auto ret = outOfBand_.UnReg1825FeFromVmBusInstance(vmBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMti1825OutOfBand, UnReg1825FeFromVmBusInstance_TypeMismatch)
{
    std::vector<bool> resList;
    auto ret = outOfBand_.UnReg1825FeFromVmBusInstance(hostBusInstance_, vfList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== 继承关系测试 ====================

TEST(UbseMti1825OutOfBandInheritanceTest, InheritsFromUbseMti1825)
{
    static_assert(std::is_base_of<UbseMti1825, UbseMti1825OutOfBand>::value,
                  "UbseMti1825OutOfBand should inherit from UbseMti1825");
}

} // namespace ubse::ut::mti::_1825
