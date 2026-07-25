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

#include "test_ubse_mti_bus_instance_out_of_band.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_error.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_businstance_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_d2h_memory_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"
#include "src/adapter_plugins/mti/out_of_band/proxy/ubse_ctrl_q_msg_proxy.h"
#include "src/framework/misc/ubse_os_util.h"

namespace ubse::ut::mti::bus_instance {

using namespace ubse::mti::ctrl_q;
using namespace ubse::mti::bus_instance;
using namespace ubse::utils;

static const uint8_t DESTROY_BUSINSTANCE_OP_CODE = 0x5;

static std::string g_lsubOutput;
static std::string g_subDeviceOutput;
static std::string g_readFileResult;

static UbseResult MockExecWithOutput(const std::string& cmd, std::string& res)
{
    if (cmd == "lsub -b") {
        res = g_lsubOutput;
    } else {
        res = g_subDeviceOutput;
    }
    return UBSE_OK;
}

static UbseResult MockExecFail(const std::string& cmd, std::string& res)
{
    return UBSE_ERROR;
}

static UbseResult MockExecLsubOkSubFail(const std::string& cmd, std::string& res)
{
    if (cmd == "lsub -b") {
        res = g_lsubOutput;
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

static UbseResult MockReadFileContentSuccess(const std::string& filePath, std::string& res)
{
    res = g_readFileResult;
    return UBSE_OK;
}

static UbseResult MockReadFileContentFail(const std::string& filePath, std::string& res)
{
    return UBSE_ERROR;
}

static UbseResult MockSendRequestDestroySuccess(CtrlQMsgProxy* pthis, ICtrlQReqMsg& reqMsg, ICtrlQRespMsg& respMsg)
{
    auto* destroyResp = dynamic_cast<UbseCtrlQDestroyBusInstanceRespMsg*>(&respMsg);
    if (destroyResp != nullptr) {
        CtrlQBasicBlock block{};
        block.head.serviceType = DEFAULT_SERVICE_TYPE;
        block.head.bbNum = 1;
        block.head.opCode = DESTROY_BUSINSTANCE_OP_CODE;
        CtrlQRespMessage respMsgRaw{&block, 1};
        destroyResp->DecodeRespMsg(respMsgRaw);
    }
    return UBSE_OK;
}

void TestUbseMtiBusInstanceOutOfBand::SetUp()
{
    testBusInstance_.type = UbseMtiBusInstanceType::VM;
    testBusInstance_.upi = 0x1234;
    testBusInstance_.vendor = 0x5678;
    testBusInstance_.guid.fill(0xAB);
    testBusInstance_.eid.fill(0xCD);
    g_lsubOutput.clear();
    g_subDeviceOutput.clear();
    g_readFileResult.clear();
}

void TestUbseMtiBusInstanceOutOfBand::TearDown()
{
    GlobalMockObject::verify();
}

// ==================== CreateVmBusInstance 测试 ====================

TEST_F(TestUbseMtiBusInstanceOutOfBand, CreateVmBusInstance_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    UbseMtiBusInst busInstance;
    auto ret = outOfBand_.CreateVmBusInstance(0x1234, busInstance);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, CreateVmBusInstance_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    UbseMtiBusInst busInstance;
    auto ret = outOfBand_.CreateVmBusInstance(0x1234, busInstance);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== DestroyVmBusInstance 测试 ====================

TEST_F(TestUbseMtiBusInstanceOutOfBand, DestroyVmBusInstance_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(invoke(MockSendRequestDestroySuccess));

    auto ret = outOfBand_.DestroyVmBusInstance(testBusInstance_);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, DestroyVmBusInstance_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    auto ret = outOfBand_.DestroyVmBusInstance(testBusInstance_);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, DestroyVmBusInstance_GetRetFalse)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    auto ret = outOfBand_.DestroyVmBusInstance(testBusInstance_);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== GetD2hMemory 测试 ====================

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetD2hMemory_Success)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_OK));

    uint32_t tid = 0;
    uint64_t uba = 0;
    uint64_t size = 0;
    auto ret = outOfBand_.GetD2hMemory(testBusInstance_, tid, uba, size);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetD2hMemory_SendRequestFailed)
{
    const auto sendRequestFunc = &CtrlQMsgProxy::SendRequest;
    MOCKER_CPP(sendRequestFunc).stubs().will(returnValue(UBSE_ERROR));

    uint32_t tid = 0;
    uint64_t uba = 0;
    uint64_t size = 0;
    auto ret = outOfBand_.GetD2hMemory(testBusInstance_, tid, uba, size);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// ==================== GetBusInstanceList 测试 ====================
// 通过 GetBusInstanceList 公开接口覆盖 ParseLsubBusInstanceOutput 和 ParseGetSubDeviceOutput 内部逻辑

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_ExecLsubFailed)
{
    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecFail));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_EmptyOutput)
{
    g_lsubOutput = "";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_TRUE(busInstanceList.empty());
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_NoHeaderLine)
{
    g_lsubOutput = "0123456789abcdef0123456789abcdef VM 00001 0100\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_TRUE(busInstanceList.empty());
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_SkipStaticServerType)
{
    g_lsubOutput = "BusInstance show format: guid type eid upi\n"
                   "0123456789abcdef0123456789abcdef Static_Server 00001 0100\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_TRUE(busInstanceList.empty());
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_ParseClusterTypeAsHost)
{
    g_lsubOutput = "BusInstance show format: guid type eid upi\n"
                   "0123456789abcdef0123456789abcdef Static_Cluster 00001 0100\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_OK, ret);
    ASSERT_EQ(busInstanceList.size(), 1u);
    EXPECT_EQ(UbseMtiBusInstanceType::HOST, busInstanceList[0].type);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_ParseVmType)
{
    g_lsubOutput = "BusInstance show format: guid type eid upi\n"
                   "0123456789abcdef0123456789abcdef VM 00001 0100\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_OK, ret);
    ASSERT_EQ(busInstanceList.size(), 1u);
    EXPECT_EQ(UbseMtiBusInstanceType::VM, busInstanceList[0].type);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_MultipleEntries)
{
    g_lsubOutput = "BusInstance show format: guid type eid upi\n"
                   "0123456789abcdef0123456789abcdef Static_Cluster 00001 0100\n"
                   "0123456789abcdef0123456789abcdef VM 00002 0200\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_OK, ret);
    ASSERT_EQ(busInstanceList.size(), 2u);
    EXPECT_EQ(UbseMtiBusInstanceType::HOST, busInstanceList[0].type);
    EXPECT_EQ(UbseMtiBusInstanceType::VM, busInstanceList[1].type);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_SkipInvalidLines)
{
    g_lsubOutput = "BusInstance show format: guid type eid upi\n"
                   "invalid_line\n"
                   "0123456789abcdef0123456789abcdef VM 00001 0100\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_OK, ret);
    ASSERT_EQ(busInstanceList.size(), 1u);
    EXPECT_EQ(UbseMtiBusInstanceType::VM, busInstanceList[0].type);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_SkipDashLines)
{
    g_lsubOutput = "BusInstance show format: guid type eid upi\n"
                   "----\n"
                   "0123456789abcdef0123456789abcdef VM 00001 0100\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_OK, ret);
    ASSERT_EQ(busInstanceList.size(), 1u);
    EXPECT_EQ(UbseMtiBusInstanceType::VM, busInstanceList[0].type);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_SubDeviceExecFailed)
{
    g_lsubOutput = "BusInstance show format: guid type eid upi\n"
                   "0123456789abcdef0123456789abcdef VM 00001 0100\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecLsubOkSubFail));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_ReadFileContentFailed)
{
    g_lsubOutput = "BusInstance show format: guid type eid upi\n"
                   "0123456789abcdef0123456789abcdef VM 00001 0100\n";
    g_subDeviceOutput = "Uents under this busInstance:\n"
                        "01234567\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    const auto readFileFunc = &UbseOsUtil::ReadFileContent;
    MOCKER_CPP(readFileFunc).stubs().will(invoke(MockReadFileContentFail));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMtiBusInstanceOutOfBand, GetBusInstanceList_FullSuccessWithSubDevices)
{
    g_lsubOutput = "BusInstance show format: guid type eid upi\n"
                   "0123456789abcdef0123456789abcdef VM 00001 0100\n";
    g_subDeviceOutput = "Uents under this busInstance:\n"
                        "01234567\n";
    g_readFileResult = "0123456789abcdef0123456789abcdef\n";

    const auto execFunc = &UbseOsUtil::Exec;
    MOCKER_CPP(execFunc).stubs().will(invoke(MockExecWithOutput));

    const auto readFileFunc = &UbseOsUtil::ReadFileContent;
    MOCKER_CPP(readFileFunc).stubs().will(invoke(MockReadFileContentSuccess));

    std::vector<UbseMtiBusInst> busInstanceList;
    auto ret = outOfBand_.GetBusInstanceList(busInstanceList);
    EXPECT_EQ(UBSE_OK, ret);
    ASSERT_EQ(busInstanceList.size(), 1u);
    EXPECT_EQ(UbseMtiBusInstanceType::VM, busInstanceList[0].type);
    ASSERT_EQ(busInstanceList[0].subDeviceGuids.size(), 1u);
}

// ==================== 继承关系测试 ====================

TEST(UbseMtiBusInstanceOutOfBandInheritanceTest, InheritsFromUbseMtiBusInstance)
{
    static_assert(std::is_base_of<UbseMtiBusInstance, UbseMtiBusInstanceOutOfBand>::value,
                  "UbseMtiBusInstanceOutOfBand should inherit from UbseMtiBusInstance");
}

} // namespace ubse::ut::mti::bus_instance
