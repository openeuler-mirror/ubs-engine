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

#include "test_ubse_mti_urma_out_of_band.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_error.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_fe_opt_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_idev_fe_david_mapping_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_idev_fe_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_vfe_david_opt_msg.h"
#include "src/adapter_plugins/mti/out_of_band/proxy/ubse_ctrl_q_msg_proxy.h"

namespace ubse::ut::mti::urma {

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::urma;
using namespace ubse::mti::bus_instance;

void TestUbseMtiUrmaOutOfBand::SetUp()
{
    testBusInstance_.type = UbseMtiBusInstanceType::VM;
    testBusInstance_.upi = 0x1234;
    testBusInstance_.vendor = 0x5678;
    testBusInstance_.guid.fill(0xAB);
    testBusInstance_.eid.fill(0xCD);

    UbseMtiUbController ctrl(0x01, 0x02);
    ctrl.slotId = 0x01;
    UbseMtiIdevVfe vfe(ctrl, 0x03, 0x04);
    vfe.guid.fill(0x11);
    UbseMtiDavid david(0x05, 0x06);
    testVfeDavidList_.emplace_back(vfe, david);
}

void TestUbseMtiUrmaOutOfBand::TearDown()
{
    GlobalMockObject::verify();
}

// ==================== GetIdevFeList 测试 ====================

TEST_F(TestUbseMtiUrmaOutOfBand, GetIdevFeList_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<UbseMtiIdevPfe> feList;
    auto ret = outOfBand_.GetIdevFeList(feList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMtiUrmaOutOfBand, GetIdevFeList_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbseMtiIdevPfe> feList;
    auto ret = outOfBand_.GetIdevFeList(feList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== GetIdevFeDavidMapping 测试 ====================

TEST_F(TestUbseMtiUrmaOutOfBand, GetIdevFeDavidMapping_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    UbseMtiIdevFeDavidMapping mapping;
    auto ret = outOfBand_.GetIdevFeDavidMapping(mapping);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMtiUrmaOutOfBand, GetIdevFeDavidMapping_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    UbseMtiIdevFeDavidMapping mapping;
    auto ret = outOfBand_.GetIdevFeDavidMapping(mapping);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== BindDavid 测试 ====================

TEST_F(TestUbseMtiUrmaOutOfBand, BindDavid_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<bool> resList;
    auto ret = outOfBand_.BindDavid(0x1234, testVfeDavidList_, resList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMtiUrmaOutOfBand, BindDavid_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<bool> resList;
    auto ret = outOfBand_.BindDavid(0x1234, testVfeDavidList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== UnBindDavid 测试 ====================

TEST_F(TestUbseMtiUrmaOutOfBand, UnBindDavid_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<bool> resList;
    auto ret = outOfBand_.UnBindDavid(0x1234, testVfeDavidList_, resList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMtiUrmaOutOfBand, UnBindDavid_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<bool> resList;
    auto ret = outOfBand_.UnBindDavid(0x1234, testVfeDavidList_, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== RegDavidFeToVmBusInstance 测试 ====================

TEST_F(TestUbseMtiUrmaOutOfBand, RegDavidFeToVmBusInstance_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<UbseMtiIdevVfe> vfeList;
    UbseMtiUbController ctrl(0x01, 0x02);
    ctrl.slotId = 0x01;
    vfeList.emplace_back(ctrl, 0x03, 0x04);

    std::vector<bool> resList;
    auto ret = outOfBand_.RegDavidFeToVmBusInstance(testBusInstance_, vfeList, resList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMtiUrmaOutOfBand, RegDavidFeToVmBusInstance_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbseMtiIdevVfe> vfeList;
    UbseMtiUbController ctrl(0x01, 0x02);
    ctrl.slotId = 0x01;
    vfeList.emplace_back(ctrl, 0x03, 0x04);

    std::vector<bool> resList;
    auto ret = outOfBand_.RegDavidFeToVmBusInstance(testBusInstance_, vfeList, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== UnRegDavidFeFromVmBusInstance 测试 ====================

TEST_F(TestUbseMtiUrmaOutOfBand, UnRegDavidFeFromVmBusInstance_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    std::vector<UbseMtiIdevVfe> vfeList;
    UbseMtiUbController ctrl(0x01, 0x02);
    ctrl.slotId = 0x01;
    vfeList.emplace_back(ctrl, 0x03, 0x04);

    std::vector<bool> resList;
    auto ret = outOfBand_.UnRegDavidFeFromVmBusInstance(testBusInstance_, vfeList, resList);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMtiUrmaOutOfBand, UnRegDavidFeFromVmBusInstance_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbseMtiIdevVfe> vfeList;
    UbseMtiUbController ctrl(0x01, 0x02);
    ctrl.slotId = 0x01;
    vfeList.emplace_back(ctrl, 0x03, 0x04);

    std::vector<bool> resList;
    auto ret = outOfBand_.UnRegDavidFeFromVmBusInstance(testBusInstance_, vfeList, resList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== 继承关系测试 ====================

TEST(UbseMtiUrmaOutOfBandInheritanceTest, InheritsFromUbseMtiUrma)
{
    static_assert(std::is_base_of<UbseMtiUrma, UbseMtiUrmaOutOfBand>::value,
                  "UbseMtiUrmaOutOfBand should inherit from UbseMtiUrma");
}

} // namespace ubse::ut::mti::urma
