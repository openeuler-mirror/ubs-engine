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

#include "test_ubse_mem_controller_master_online_handler.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_mem_controller_master_online_handler.h"

namespace ubse::mem_controller::ut {
using namespace ubse::nodeController;
using namespace ubse::mem::controller;

void TestUbseMemControllerMasterOnlineHandler::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerMasterOnlineHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerMasterOnlineHandler, MasterOnlineHandler)
{
    UbseElectionEventType type;
    UBSE_ID_TYPE nodeId;
    EXPECT_NO_THROW(UbseMemControllerMasterOnlineHandler::MasterOnlineHandler(type, nodeId));
}

TEST_F(TestUbseMemControllerMasterOnlineHandler, Uninitial)
{
    EXPECT_NO_THROW(UbseMemControllerMasterOnlineHandler::Uninitial());
}
} // namespace ubse::mem_controller::ut