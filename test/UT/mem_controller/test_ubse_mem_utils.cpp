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

#include "test_ubse_mem_utils.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"
#include "ubse_mem_util.h"
#include "ubse_thread_pool_module.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::util;
using namespace ubse::mti;
using namespace ubse::task_executor;

void TestUbseMemUtils::SetUp()
{
    Test::SetUp();
}
void TestUbseMemUtils::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemUtils, GetExecutor)
{
    std::shared_ptr<UbseTaskExecutorModule> nullModule = nullptr;
    std::shared_ptr<UbseTaskExecutorModule> module = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    std::string name = "name";
    EXPECT_EQ(mem::util::GetExecutor(name), nullptr);

    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&UbseTaskExecutorModule::Get).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    EXPECT_EQ(GetExecutor(name), nullptr);
    EXPECT_EQ(GetExecutor(name), ubseTaskExecutorPtr);
}

UbseResult MockUbseGetLocalNodeInfo(UbseLcneModule*, UbseMtiNodeInfo& ubseNodeInfo)
{
    ubseNodeInfo.nodeId = "1";
    return UBSE_OK;
}

TEST_F(TestUbseMemUtils, GetCurNodeId)
{
    std::shared_ptr<UbseLcneModule> nullModule = nullptr;
    std::shared_ptr<UbseLcneModule> module = std::make_shared<UbseLcneModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseLcneModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ("", GetCurNodeId());

    MOCKER_CPP(&UbseLcneModule::UbseGetLocalNodeInfo)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(MockUbseGetLocalNodeInfo));

    EXPECT_EQ("", GetCurNodeId());
    EXPECT_EQ("1", GetCurNodeId());
}
} // namespace ubse::mem_controller::ut
