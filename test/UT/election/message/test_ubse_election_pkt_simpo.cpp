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

#include "test_ubse_election_pkt_simpo.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_election_pkt_simpo.h"
#include "ubse_error.h"

namespace ubse::ut::election::message {
using namespace ubse::election;

void TestUbseElectionPktSimpo::SetUp()
{
    electionPkt = {0, "Node0","", "Node1", 2, 5, 3, {"Node2", "Node3", "Node4"}, 0, 0};
    ubseElectionPktSimpoPtr = new (std::nothrow) UbseElectionPktSimpo(electionPkt);
    Test::SetUp();
}

void TestUbseElectionPktSimpo::TearDown()
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
TEST_F(TestUbseElectionPktSimpo, SerializeSuccess)
{
    EXPECT_EQ(ubseElectionPktSimpoPtr->Serialize(), UBSE_OK);
}

/**
 * 用例描述：测试UbseElectionPktSimpo反序列化
 * 测试点：当mInputRawData为空时，测试Deserialize()的返回值
 * 测试步骤：
 * 1.反序列化
 * 预期结果：
 * UBSE_ERROR
 */
TEST_F(TestUbseElectionPktSimpo, DeSerializeFaild)
{
    EXPECT_EQ(ubseElectionPktSimpoPtr->Deserialize(), UBSE_ERROR);
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
TEST_F(TestUbseElectionPktSimpo, DeserializeSuccess)
{
    ASSERT_EQ(ubseElectionPktSimpoPtr->Serialize(), UBSE_OK);
    UbseElectionPktSimpoPtr pUbseElectionPktSimpo = new (std::nothrow)
        UbseElectionPktSimpo(ubseElectionPktSimpoPtr->SerializedData(), ubseElectionPktSimpoPtr->SerializedDataSize());
    EXPECT_EQ(pUbseElectionPktSimpo->Deserialize(), UBSE_OK);
}
}