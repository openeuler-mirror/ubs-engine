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

#ifndef UBSE_MANAGER_TEST_UBSE_ELECTION_ROLE_GLOBAL_INITIALIZER_H
#define UBSE_MANAGER_TEST_UBSE_ELECTION_ROLE_GLOBAL_INITIALIZER_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "role/ubse_election_role_global_initializer.h"
#include "role/ubse_election_role_mgr.h"
#include "ubse_election_def.h"
#include "ubse_context.h"

namespace ubse::event::election {
using namespace ubse::election;

class TestUbseElectionRoleGlobalInitializer : public ::testing::Test {
public:
    void SetUp() override
    {
        ubse::context::g_globalStop.store(false);
        RoleType roleType = RoleType::INITIALIZER;
        RoleContext ctx;
        RoleMgr::GetInstance().SwitchRole(roleType, ctx);
        RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_INITIALIZER, ctx);
    }
    void TearDown() override;
};

void TestUbseElectionRoleGlobalInitializer::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

}

#endif // UBSE_MANAGER_TEST_UBSE_ELECTION_ROLE_GLOBAL_INITIALIZER_H