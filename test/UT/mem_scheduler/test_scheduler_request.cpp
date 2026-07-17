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

#include "test_scheduler_request.h"

#include "ubse_mem_scheduler_request.h"
#include "adapter_plugins/mmi/ubse_mmi_def.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::mem::scheduler;
using namespace ubse::adapter_plugins::mmi;

constexpr uint64_t KB = 1024ULL;
constexpr uint64_t MB = KB * 1024;

// ==================== FromFdBorrowReq ====================

TEST_F(TestSchedulerRequest, FromFdBorrowReqMapsFields)
{
    UbseMemFdBorrowReq req{};
    req.name = "r1";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 128 * MB;
    req.candidateNodeList = {"3"};

    auto result = SchedulerRequest::FromFdBorrowReq(req);

    EXPECT_EQ(result.name_, "r1");
    EXPECT_EQ(result.requestNodeId_, "1");
    EXPECT_EQ(result.importNodeId_, "2");
    EXPECT_EQ(result.requestSize_, 128 * MB);
    EXPECT_EQ(result.provideNodes_, (std::vector<NodeId>{"3"}));
    EXPECT_EQ(result.requestMode_, RequestMode::BORROW);
}

TEST_F(TestSchedulerRequest, FromFdBorrowReqFilterChain)
{
    UbseMemFdBorrowReq req{};
    req.name = "r1";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 128 * MB;

    auto result = SchedulerRequest::FromFdBorrowReq(req);

    EXPECT_TRUE(result.filterNames_.size() >= 12);
    EXPECT_EQ(result.filterNames_[0], "ConfigConsistencyFilter");
    EXPECT_EQ(result.filterNames_[1], "RoleConflictFilter");
    EXPECT_EQ(result.filterNames_[2], "LenderRoleFilter");
}

TEST_F(TestSchedulerRequest, FromFdBorrowReqWithCandidate)
{
    UbseMemFdBorrowReq req{};
    req.name = "r1";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 128 * MB;
    req.candidateNodeList = {"2", "3"};

    auto result = SchedulerRequest::FromFdBorrowReq(req);

    EXPECT_NE(std::find(result.filterNames_.begin(), result.filterNames_.end(), "RequestedProvidersFilter"),
              result.filterNames_.end());
}

TEST_F(TestSchedulerRequest, FromFdBorrowReqEmptyCandidateNoRequestedProvidersFilter)
{
    UbseMemFdBorrowReq req{};
    req.name = "r1";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 128 * MB;

    auto result = SchedulerRequest::FromFdBorrowReq(req);

    EXPECT_EQ(std::find(result.filterNames_.begin(), result.filterNames_.end(), "RequestedProvidersFilter"),
              result.filterNames_.end());
}

TEST_F(TestSchedulerRequest, FromFdBorrowReqWithLenderLocs)
{
    UbseMemFdBorrowReq req{};
    req.name = "r1";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 128 * MB;
    req.lenderLocs = {{"3", 0}};
    req.lenderSizes = {128 * MB};

    auto result = SchedulerRequest::FromFdBorrowReq(req);

    EXPECT_NE(std::find(result.filterNames_.begin(), result.filterNames_.end(), "SpecifiedLenderFilter"),
              result.filterNames_.end());
    auto lenderInfos = result.GetParamOpt<std::vector<UbseMemLenderInfo>>("lenderInfos");
    ASSERT_TRUE(lenderInfos.has_value());
    ASSERT_EQ(lenderInfos->size(), 1);
    EXPECT_EQ((*lenderInfos)[0].nodeId, "3");
}

// ==================== FromNumaBorrowReq ====================

TEST_F(TestSchedulerRequest, FromNumaBorrowReqMapsFields)
{
    UbseMemNumaBorrowReq req{};
    req.name = "n1";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 256 * MB;
    req.srcSocket = 36;
    req.highWatermark = 80;

    auto result = SchedulerRequest::FromNumaBorrowReq(req);

    EXPECT_EQ(result.name_, "n1");
    EXPECT_EQ(result.requestSize_, 256 * MB);
    EXPECT_EQ(result.GetParamOpt<size_t>("highWatermark"), 80u);
    EXPECT_EQ(result.GetParamOpt<int>("affinitySocketId"), 36);
}

TEST_F(TestSchedulerRequest, FromNumaBorrowReqAddsSocketAffinityFilter)
{
    UbseMemNumaBorrowReq req{};
    req.name = "n1";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 256 * MB;
    req.srcSocket = 36;

    auto result = SchedulerRequest::FromNumaBorrowReq(req);

    EXPECT_NE(std::find(result.filterNames_.begin(), result.filterNames_.end(), "SocketAffinityFilter"),
              result.filterNames_.end());
}

TEST_F(TestSchedulerRequest, FromNumaBorrowReqNoSrcSocket)
{
    UbseMemNumaBorrowReq req{};
    req.name = "n1";
    req.requestNodeId = "1";
    req.importNodeId = "2";
    req.size = 256 * MB;
    req.srcSocket = -1;

    auto result = SchedulerRequest::FromNumaBorrowReq(req);

    EXPECT_EQ(std::find(result.filterNames_.begin(), result.filterNames_.end(), "SocketAffinityFilter"),
              result.filterNames_.end());
}

// ==================== FromShareBorrowReq ====================

TEST_F(TestSchedulerRequest, FromShareBorrowReqMapsFields)
{
    UbseMemShareBorrowReq req{};
    req.name = "s1";
    req.requestNodeId = "1";
    req.size = 256 * MB;
    req.providerList = {"2", "3"};

    auto result = SchedulerRequest::FromShareBorrowReq(req);

    EXPECT_EQ(result.name_, "s1");
    EXPECT_EQ(result.requestSize_, 256 * MB);
    EXPECT_EQ(result.provideNodes_, (std::vector<NodeId>{"2", "3"}));
    EXPECT_EQ(result.requestMode_, RequestMode::SHARE);
}

TEST_F(TestSchedulerRequest, FromShareBorrowReqWithAffinity)
{
    UbseMemShareBorrowReq req{};
    req.name = "s1";
    req.requestNodeId = "1";
    req.size = 256 * MB;
    req.withAffinity.enableCreateWithAffinity = true;
    req.withAffinity.affinitySocketId = 36;

    auto result = SchedulerRequest::FromShareBorrowReq(req);

    EXPECT_NE(std::find(result.filterNames_.begin(), result.filterNames_.end(), "SocketAffinityFilter"),
              result.filterNames_.end());
    EXPECT_EQ(result.GetParamOpt<int>("affinitySocketId"), 36);
}

// ==================== GetParamOpt ====================

TEST_F(TestSchedulerRequest, GetParamOptFound)
{
    SchedulerRequest req;
    req.SetParam("key", 42);
    auto val = req.GetParamOpt<int>("key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), 42);
}

TEST_F(TestSchedulerRequest, GetParamOptNotFound)
{
    SchedulerRequest req;
    auto val = req.GetParamOpt<int>("nonexist");
    EXPECT_FALSE(val.has_value());
}

TEST_F(TestSchedulerRequest, GetParamOptTypeMismatch)
{
    SchedulerRequest req;
    req.SetParam("key", std::string("hello"));
    auto val = req.GetParamOpt<int>("key");
    EXPECT_FALSE(val.has_value());
}

} // namespace ubse::mem::scheduler::ut
