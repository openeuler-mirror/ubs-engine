/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "test_ham_make_decision_msg.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include "ham_make_decision_msg.h"
#include "vm_serial_util.h"

using namespace vm;
namespace ubse::vm::ut {

TestHamMakeDecisionMsg::TestHamMakeDecisionMsg() = default;

void TestHamMakeDecisionMsg::SetUp()
{
    Test::SetUp();
}

void TestHamMakeDecisionMsg::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestHamMakeDecisionMsg, HamMakeDecisionMsg_Serialize)
{
    InputParams inputParams{100, "123", "0", 101};
    HamMakeDecisionMsg hamMakeDecisionMsg{inputParams};
    auto ret = hamMakeDecisionMsg.Serialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestHamMakeDecisionMsg, HamMakeDecisionMsg_Deserialize)
{
    InputParams inputParams{100, "123", "0", 101};
    HamMakeDecisionMsg hamMakeDecisionMsg{inputParams};
    auto ret = hamMakeDecisionMsg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MOCKER(&VmDeSerialization::Check).stubs().will(returnValue(true));
    HamMakeDecisionMsg msg{hamMakeDecisionMsg.SerializedData(), hamMakeDecisionMsg.SerializedDataSize()};
    ret = msg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
    auto param = msg.GetInputParams();
    EXPECT_EQ(param.vmMemoryMB, inputParams.vmMemoryMB);
    EXPECT_EQ(param.uuid, inputParams.uuid);
    EXPECT_EQ(param.destHostName, inputParams.destHostName);
    EXPECT_EQ(param.destNumaId, inputParams.destNumaId);
    GlobalMockObject::verify();
}
} // namespace ubse::vm::ut