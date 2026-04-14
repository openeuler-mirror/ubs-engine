/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "test_case_conf_msg.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include "case_conf_msg.h"
#include "vm_serial_util.h"

using namespace vm;
namespace ubse::vm::ut {

TestCaseConfMsg::TestCaseConfMsg() = default;

void TestCaseConfMsg::SetUp()
{
    Test::SetUp();
}

void TestCaseConfMsg::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestCaseConfMsg, CaseConfGetMsg_GetCaseConf)
{
    CaseConfInfo caseConfInfo{"case_type", "1.2", "80", 0, "0"};
    CaseConfGetMsg caseConfGetMsg{caseConfInfo};
    auto ret = caseConfGetMsg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    auto case_conf_info = caseConfGetMsg.GetCaseConf();
    EXPECT_EQ(case_conf_info.index, 0);
    EXPECT_STREQ(case_conf_info.cur_case, "case_type");
    EXPECT_STREQ(case_conf_info.over_commitment_ratio, "1.2");
    EXPECT_STREQ(case_conf_info.migrate_waterLine, "80");
    EXPECT_EQ(case_conf_info.index, 0);
    EXPECT_STREQ(case_conf_info.host_id, "0");
    GlobalMockObject::verify();
}

TEST_F(TestCaseConfMsg, CaseConfGetMsg_DeSerialize)
{
    CaseConfInfo caseConfInfo{"case_type", "1.2", "80", 0, "0"};
    CaseConfGetMsg caseConfGetMsg{caseConfInfo};
    auto ret = caseConfGetMsg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    CaseConfGetMsg msg{caseConfGetMsg.SerializedData(), caseConfGetMsg.SerializedDataSize()};
    ret = msg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestCaseConfMsg, CaseConfSetMsg_GetCaseConf)
{
    CaseConfSetInfo caseConfInfo{"case_type", 1.2, "msg", 0};
    CaseConfSetMsg caseConfSetMsg{caseConfInfo};
    auto ret = caseConfSetMsg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    auto case_conf_info = caseConfSetMsg.GetCaseConf();
    EXPECT_EQ(case_conf_info.ret, 0);
}

TEST_F(TestCaseConfMsg, CaseConfSetMsg_DeSerialize)
{
    CaseConfSetInfo caseConfInfo{"case_type", 1.2, "msg", 0};
    CaseConfSetMsg caseConfSetMsg{caseConfInfo};
    auto ret = caseConfSetMsg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    CaseConfSetMsg msg{caseConfSetMsg.SerializedData(), caseConfSetMsg.SerializedDataSize()};
    ret = msg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

} // namespace ubse::vm::ut