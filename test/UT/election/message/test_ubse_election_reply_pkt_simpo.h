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

#ifndef TEST_UBSE_ELECTION_REPLY_PKT_SIMPO_H
#define TEST_UBSE_ELECTION_REPLY_PKT_SIMPO_H

#include <gtest/gtest.h>

#include "ubse_election_reply_pkt_simpo.h"

namespace ubse::ut::election::message {
using namespace ubse::election::message;

class TestUbseElectionReplyPktSimpo : public testing::Test {
public:
    TestUbseElectionReplyPktSimpo() = default;

    void SetUp() override;

    void TearDown() override;

private:
    ubse::election::ElectionReplyPkt electionReplyPkt;
    UbseElectionReplyPktSimpoPtr ubseElectionReplyPktSimpoPtr;
};
} // namespace ubse::ut::election::message

#endif // TEST_UBSE_ELECTION_REPLY_PKT_SIMPO_H
