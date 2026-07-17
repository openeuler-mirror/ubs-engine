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

#include "test_scheduler_socket_info.h"

#include "scheduler_cache/ubse_mem_scheduler_socket_info.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::mem::scheduler;

TEST_F(TestSchedulerSocketInfo, InitCreatesNewNumaInfo)
{
    SchedulerSocketInfo socket(36);
    nodeController::UbseNumaInfo info{};
    info.location.numaId = 1;
    info.socketId = 36;
    socket.Init(info);

    auto numa = socket.GetNumaInfo(1);
    ASSERT_NE(numa, nullptr);
    EXPECT_EQ(numa->GetNumaId(), 1u);
}

TEST_F(TestSchedulerSocketInfo, InitReusesExistingNumaInfo)
{
    SchedulerSocketInfo socket(36);
    nodeController::UbseNumaInfo info{};
    info.location.numaId = 1;
    info.socketId = 36;
    info.size = 4096;
    socket.Init(info);

    info.size = 8192;
    socket.Init(info);

    auto numa = socket.GetNumaInfo(1);
    ASSERT_NE(numa, nullptr);
    EXPECT_EQ(numa->GetNumaId(), 1u);
    EXPECT_EQ(socket.GetAllNumaInfos().size(), 1u);
}

TEST_F(TestSchedulerSocketInfo, GetSocketInfoReturnsCorrectView)
{
    SchedulerSocketInfo socket(36);
    nodeController::UbseNumaInfo numa1{};
    numa1.location.numaId = 0;
    numa1.socketId = 36;
    socket.Init(numa1);
    nodeController::UbseNumaInfo numa2{};
    numa2.location.numaId = 1;
    numa2.socketId = 36;
    socket.Init(numa2);

    auto view = socket.GetSocketInfo();
    EXPECT_EQ(view.socketId, 36u);
    EXPECT_EQ(view.numaInfos.size(), 2u);
}

TEST_F(TestSchedulerSocketInfo, GetNumaInfoNotFoundReturnsNull)
{
    SchedulerSocketInfo socket(36);
    nodeController::UbseNumaInfo info{};
    info.location.numaId = 1;
    info.socketId = 36;
    socket.Init(info);

    EXPECT_EQ(socket.GetNumaInfo(999), nullptr);
}

TEST_F(TestSchedulerSocketInfo, GetAllNumaInfosEmptyInitially)
{
    SchedulerSocketInfo socket(36);
    EXPECT_TRUE(socket.GetAllNumaInfos().empty());
}

TEST_F(TestSchedulerSocketInfo, GetAllNumaInfosAfterInit)
{
    SchedulerSocketInfo socket(36);
    nodeController::UbseNumaInfo info1{};
    info1.location.numaId = 0;
    info1.socketId = 36;
    socket.Init(info1);
    nodeController::UbseNumaInfo info2{};
    info2.location.numaId = 1;
    info2.socketId = 36;
    socket.Init(info2);

    const auto& all = socket.GetAllNumaInfos();
    EXPECT_EQ(all.size(), 2u);
    EXPECT_NE(all.find(0), all.end());
    EXPECT_NE(all.find(1), all.end());
}

} // namespace ubse::mem::scheduler::ut
