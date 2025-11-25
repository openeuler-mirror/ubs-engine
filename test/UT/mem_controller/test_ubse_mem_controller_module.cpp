/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_controller_module.h"

#include <ubse_mem_controller_api_agent.h>

#include <mockcpp/mockcpp.hpp>

#include "UbseMemDebInfoQueryHandler.h"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_rpc.h"
#include "ubse_mem_utils.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace nodeController;
using namespace ubse::mem::utils;

void TestUbseMemControllerModule::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerModule, Initialize)
{
    auto memModulePtr = std::make_shared<UbseMemControllerModule>();
    MOCKER_CPP(&RegisterNodeCtlNotify).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(memModulePtr->Initialize(), UBSE_ERROR);
    EXPECT_EQ(memModulePtr->Initialize(), UBSE_OK);
}

TEST_F(TestUbseMemControllerModule, Start)
{
    auto memModulePtr = std::make_shared<UbseMemControllerModule>();
    MOCKER_CPP(&MemScheduleHandler::RegHandler).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(memModulePtr->Start(), UBSE_ERROR);

    MOCKER_CPP(&rpc::UbseMemDebInfoQueryHandler::RegUbseMemDebInfoQueryHandler)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(memModulePtr->Start(), UBSE_ERROR);

    MOCKER_CPP(&ubse::mem::controller::agent::Init).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(memModulePtr->Start(), UBSE_OK);
}
}