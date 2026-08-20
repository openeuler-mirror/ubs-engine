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

#include "test_scheduler_fixture.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_scheduler_node_manager.h"

namespace ubse::mem::scheduler::ut {

namespace {

using namespace ubse::common::def;
using namespace ubse::adapter_plugins::mmi;

UbseMemFdBorrowImportObj MakeFdObj(const std::string& name, uint64_t size)
{
    UbseMemFdBorrowImportObj obj{};
    obj.req.name = name;
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.size = size;
    obj.status.state = UBSE_MEM_SCHEDULING;
    return obj;
}

UbseMemNumaBorrowImportObj MakeNumaObj(const std::string& name, uint64_t size)
{
    UbseMemNumaBorrowImportObj obj{};
    obj.req.name = name;
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.size = size;
    obj.req.srcSocket = 0; // 有效 socket，走 IsSocketPairSubHealthy
    obj.status.state = UBSE_MEM_SCHEDULING;
    return obj;
}

void SetupTwoNodeEnv()
{
    auto nodeMap = CreateNodeMap(2);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);
}

} // namespace

// ==================== DISABLED 模式（默认） ====================

// DISABLED 模式下 fd borrow 正常调度，不注入亚健康插件
TEST_F(TestSchedulerEndToEnd, SubHealthDisabledFdBorrowSchedulesNormally)
{
    SetupTwoNodeEnv();

    auto obj = MakeFdObj("fd-disabled", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

// DISABLED 模式下 numa borrow 正常调度
TEST_F(TestSchedulerEndToEnd, SubHealthDisabledNumaBorrowSchedulesNormally)
{
    SetupTwoNodeEnv();

    auto obj = MakeNumaObj("numa-disabled", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
}

// ==================== EXCLUDE 模式 ====================

// EXCLUDE 模式 + 所有链路健康（桩返回 false）→ 调度成功，与 DISABLED 行为一致
TEST_F(TestSchedulerEndToEnd, SubHealthExcludeAllHealthySchedulesNormally)
{
    SetupSubHealthConfig(true, "exclude");
    SetupTwoNodeEnv();

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(false));
    MOCKER(&SchedulerNodeManager::IsHostExportSubHealthy).stubs().will(returnValue(false));

    auto obj = MakeFdObj("fd-exclude-healthy", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

// EXCLUDE 模式 + 部分链路亚健康（mock 返回 true）→ 亚健康链路被过滤
TEST_F(TestSchedulerEndToEnd, SubHealthExcludeFiltersSubHealthyLink)
{
    SetupSubHealthConfig(true, "exclude");
    SetupTwoNodeEnv();

    // 所有链路都标记为亚健康 → SubHealthFilter 过滤所有 socket → 无节点可借用
    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(true));
    MOCKER(&SchedulerNodeManager::IsHostExportSubHealthy).stubs().will(returnValue(true));

    auto obj = MakeFdObj("fd-exclude-subhealthy", BYTE_128MB);
    auto ret = SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj);
    // 所有候选被过滤，应返回无节点可借用错误
    EXPECT_NE(ret, UBSE_OK);
    EXPECT_TRUE(obj.algoResult.exportNumaInfos.empty());
}

// ==================== WEIGHT 模式 ====================

// WEIGHT 模式 + 所有链路健康（桩返回 false）→ 调度成功，SubHealthScore 贡献 0 分
TEST_F(TestSchedulerEndToEnd, SubHealthWeightAllHealthySchedulesNormally)
{
    SetupSubHealthConfig(true, "weight");
    SetupTwoNodeEnv();

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(false));
    MOCKER(&SchedulerNodeManager::IsHostExportSubHealthy).stubs().will(returnValue(false));

    auto obj = MakeFdObj("fd-weight-healthy", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

// WEIGHT 模式 + 部分链路亚健康 → 亚健康链路被施加惩罚分，但仍可被选中（软规避）
TEST_F(TestSchedulerEndToEnd, SubHealthWeightSubHealthyStillSchedulesButPenalized)
{
    SetupSubHealthConfig(true, "weight");
    SetupTwoNodeEnv();

    // 所有链路亚健康 → 惩罚分施加，但 WEIGHT 模式不排除，仍可借用
    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(true));
    MOCKER(&SchedulerNodeManager::IsHostExportSubHealthy).stubs().will(returnValue(true));

    auto obj = MakeFdObj("fd-weight-subhealthy", BYTE_128MB);
    auto ret = SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj);
    // WEIGHT 模式下即使亚健康也可借用（软规避），应调度成功
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(obj.algoResult.exportNumaInfos.empty());
}

// WEIGHT 模式下 numa borrow 同样正常工作
TEST_F(TestSchedulerEndToEnd, SubHealthWeightNumaBorrowSchedulesNormally)
{
    SetupSubHealthConfig(true, "weight");
    SetupTwoNodeEnv();

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(false));
    MOCKER(&SchedulerNodeManager::IsHostExportSubHealthy).stubs().will(returnValue(false));

    auto obj = MakeNumaObj("numa-weight", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
}

} // namespace ubse::mem::scheduler::ut
