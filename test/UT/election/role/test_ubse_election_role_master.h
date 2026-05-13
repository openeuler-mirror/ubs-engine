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

#ifndef UBSE_MANAGER_TEST_UBSE_ELECTION_ROLE_MASTER_H
#define UBSE_MANAGER_TEST_UBSE_ELECTION_ROLE_MASTER_H
#include "ubse_election_def.h"
#include "ubse_election_reply_pkt_simpo.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "role/ubse_election_role_master.h"
#include "role/ubse_election_role_mgr.h"

namespace ubse::election {
extern void UpdateBroadcastStatus(const std::string& nodeId, const ElectionReplyPkt& reply,
                                  std::map<UBSE_ID_TYPE, BroadcastStatus>& broad, uint8_t& status, std::mutex& mtx);
extern void ProcessReply(CallbackCtx* context, int32_t result, void* recv, uint32_t len);
extern void AsyncDealReply(void* ctx, void* recv, uint32_t len, int32_t result);
} // namespace ubse::election

namespace ubse::event::election {
using namespace ubse::election;
class TestUbseElectionRoleMaster : public ::testing::Test {
public:
    void SetUp() override
    {
        context::g_globalStop.store(false);
    }

    void TearDown() override;

private:
    std::vector<UBSE_ID_TYPE> connectSuccessNodes;
};

void TestUbseElectionRoleMaster::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

} // namespace ubse::event::election
#endif // UBSE_MANAGER_TEST_UBSE_ELECTION_ROLE_MASTER_H
