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

#ifndef UBSE_MANAGER_TEST_UBSE_ELECTION_COMM_MGR_H
#define UBSE_MANAGER_TEST_UBSE_ELECTION_COMM_MGR_H

#include <cstdint>
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_election_def.h"
#include "ubse_error.h"

namespace ubse::ut::election {
using namespace ubse::election;

class TestUbseElectionCommMgr : public testing::Test {
public:
    TestUbseElectionCommMgr() = default;

    void SetUp() override;

    void TearDown() override;

    void MockEventModuleWithSubEventResults(uint32_t first, uint32_t second = UBSE_OK, uint32_t third = UBSE_OK);

    void MockAllSubEventsSuccess();

    void MockGetMyselfNode(uint32_t result);

    void MockUbseComModuleWithStartService(uint32_t startResult);

    void MockLocalMasterNode(const Node &masterNode, uint32_t result);

private:
    std::vector<UBSE_ID_TYPE> connectedIntraGroupNodes_;
    ElectionPkt pkt;
    ElectionReplyPkt replyPkt;
};
} // namespace ubse::ut::election

#endif // UBSE_MANAGER_TEST_UBSE_ELECTION_COMM_MGR_H
