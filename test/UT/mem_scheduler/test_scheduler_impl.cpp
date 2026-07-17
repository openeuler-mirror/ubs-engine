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

#include "test_scheduler_impl.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_mem_scheduler_impl.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::common::def;
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::mmi;

void TestSchedulerImpl::SetUp()
{
    Test::SetUp();
}

void TestSchedulerImpl::TearDown()
{
    SchedulerImpl::GetInstance().ClearCache();
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestSchedulerImpl, InitReturnsOk)
{
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<config::UbseConfModule>()));

    auto ret = SchedulerImpl::GetInstance().Init();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestSchedulerImpl, NodeObjChangeHandlerEmptyNodeIdReturnsError)
{
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<config::UbseConfModule>()));

    SchedulerImpl::GetInstance().Init();

    UbseNodeInfo info{};
    info.nodeId = "";
    auto ret = SchedulerImpl::GetInstance().NodeObjChangeHandler(info);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestSchedulerImpl, NodeObjChangeHandlerWithValidNode)
{
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<config::UbseConfModule>()));

    SchedulerImpl::GetInstance().Init();

    UbseNodeInfo info{};
    info.nodeId = "1";
    info.hostName = "host-1";
    info.allocator = UbseAllocator::BUDDY_HIGHMEM;
    info.blockSize = 128;
    info.isLender = true;
    info.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;

    auto ret = SchedulerImpl::GetInstance().NodeObjChangeHandler(info);
    EXPECT_EQ(ret, UBSE_OK);

    auto node = SchedulerImpl::GetInstance().nodeInfo_->GetNodeInfo("1");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->GetNodeId(), "1");
}

TEST_F(TestSchedulerImpl, MemoryObjChangeHandlerSchedulingFailsWithoutNodes)
{
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<config::UbseConfModule>()));

    SchedulerImpl::GetInstance().Init();

    UbseMemFdBorrowImportObj obj{};
    obj.req.name = "test-fd";
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.size = 128 * 1024 * 1024;
    obj.status.state = UBSE_MEM_SCHEDULING;

    auto ret = SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestSchedulerImpl, ClearCacheClearsNodeAndAccount)
{
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<config::UbseConfModule>()));

    SchedulerImpl::GetInstance().Init();

    UbseNodeInfo info{};
    info.nodeId = "1";
    info.hostName = "host-1";
    info.allocator = UbseAllocator::BUDDY_HIGHMEM;
    info.blockSize = 128;
    info.isLender = true;
    info.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    SchedulerImpl::GetInstance().NodeObjChangeHandler(info);

    EXPECT_NE(SchedulerImpl::GetInstance().nodeInfo_->GetNodeInfo("1"), nullptr);

    SchedulerImpl::GetInstance().ClearCache();

    EXPECT_EQ(SchedulerImpl::GetInstance().nodeInfo_->GetNodeInfo("1"), nullptr);
}

} // namespace ubse::mem::scheduler::ut
