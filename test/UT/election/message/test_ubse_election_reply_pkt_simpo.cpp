/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_election_reply_pkt_simpo.h"
#include <mockcpp/mockcpp.hpp>

#include "ubse_election_reply_pkt_simpo.h"
#include "ubse_error.h"

namespace ubse::ut::election::message {
using namespace ubse::election;

void TestUbseElectionReplyPktSimpo::SetUp()
{
    electionReplyPkt = { 0, "Node2", 24434, "Node0", 5, 0, 3, 0 };
    ubseElectionReplyPktSimpoPtr = new (std::nothrow) UbseElectionReplyPktSimpo(electionReplyPkt);
    Test::SetUp();
}

void TestUbseElectionReplyPktSimpo::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}
/**
 * 用例描述：测试UbseElectionPktSimpo序列化
 * 测试点：当SerializeTrans()返回UBSE_OK时，测试Serialize()的返回值
 * 测试步骤：
 * 1.设置SerializePreTrans()UBSE_OK
 * 2.序列化
 * 预期结果：
 * 返回UBSE_OK
 */
TEST_F(TestUbseElectionReplyPktSimpo, SerializeSuccess)
{
    EXPECT_EQ(ubseElectionReplyPktSimpoPtr->Serialize(), UBSE_OK);
}

/**
 * 用例描述：测试UbseElectionPktSimpo反序列化
 * 测试点：当mInputRawData为空时，测试Deserialize()的返回值
 * 测试步骤：
 * 1.反序列化
 * 预期结果：
 * UBSE_ERROR
 */
TEST_F(TestUbseElectionReplyPktSimpo, DeSerializeFaild)
{
    EXPECT_EQ(ubseElectionReplyPktSimpoPtr->Deserialize(), UBSE_ERROR);
}

/**
 * 用例描述：测试UbseElectionPktSimpo反序列化
 * 测试点：当DeserializeTrans()返回UBSE_OK时，测试Deserialize()的返回值
 * 测试步骤：
 * 1.序列化
 * 2.反序列化
 * 预期结果：
 * 返回UBSE_OK
 */
TEST_F(TestUbseElectionReplyPktSimpo, DeserializeSuccess)
{
    ASSERT_EQ(ubseElectionReplyPktSimpoPtr->Serialize(), UBSE_OK);
    UbseElectionReplyPktSimpoPtr pUbseElectionReplyPktSimpo = new (std::nothrow) UbseElectionReplyPktSimpo(
        ubseElectionReplyPktSimpoPtr->SerializedData(), ubseElectionReplyPktSimpoPtr->SerializedDataSize());
    EXPECT_EQ(pUbseElectionReplyPktSimpo->Deserialize(), UBSE_OK);
}
}