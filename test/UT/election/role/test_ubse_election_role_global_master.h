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

#ifndef UBSE_MANAGER_TEST_UBSE_ELECTION_ROLE_GLOBAL_MASTER_H
#define UBSE_MANAGER_TEST_UBSE_ELECTION_ROLE_GLOBAL_MASTER_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "role/ubse_election_role_global_master.h"
#include "role/ubse_election_role_mgr.h"
#include "ubse_election_def.h"
#include "ubse_context.h"

namespace ubse::event::election {
using namespace ubse::election;

class TestUbseElectionRoleGlobalMaster : public ::testing::Test {
public:
    void SetUp() override
    {
        ubse::context::g_globalStop.store(false);
        RoleType roleType = RoleType::MASTER;
        RoleContext ctx;
        RoleMgr::GetInstance().SwitchRole(roleType, ctx);
        RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
    }
    void TearDown() override
    {
        Test::TearDown();
        GlobalMockObject::verify();
    }
};

} // namespace ubse::event::election

#endif // UBSE_MANAGER_TEST_UBSE_ELECTION_ROLE_GLOBAL_MASTER_H