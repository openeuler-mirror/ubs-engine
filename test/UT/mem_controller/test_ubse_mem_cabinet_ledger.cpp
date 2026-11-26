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

#include "test_ubse_mem_cabinet_ledger.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_mem_cabinet_ledger.h"
#include "ubse_node_mgr.h"
#include "api/ubse_mem_controller_helper.h"

// 白盒：引入被测实现，直接覆盖闸门全局状态与编排分支
#include "ubse_mem_cabinet_ledger.cpp"

namespace ubse::mem_controller::ut {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;
using namespace ubse::mem::controller;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj;

namespace {
// 构造仅含一条 fd 导出记录的账本，对端为 peer
NodeMemDebtInfo MakeMasterDebt(const std::string& peer)
{
    NodeMemDebtInfo info{};
    UbseMemFdBorrowExportObj obj{};
    UbseMemDebtNumaInfo numa{};
    numa.nodeId = peer;
    obj.algoResult.exportNumaInfos.push_back(numa);
    info.fdExportObjMap.emplace("res1", obj);
    return info;
}

// 静态拓扑：peer -> groupId
void MockNodeGroup(const std::string& peer, uint16_t groupId)
{
    ubse::nodeMgr::UbseNodeStaticInfo info{};
    info.nodeId = peer;
    info.groupId = groupId;
    MOCKER(ubse::nodeMgr::GetUbseNodeById).stubs().will(returnValue(info));
}
} // namespace

void TestUbseMemCabinetLedger::SetUp()
{
    Test::SetUp();
}
void TestUbseMemCabinetLedger::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemCabinetLedger, PermitAcquireAndRelease)
{
    CrossCabinetRoundPermit permit;
    EXPECT_FALSE(permit.Held());
    EXPECT_TRUE(permit.Acquire(0));
    EXPECT_TRUE(permit.Held());
    // 重复申请直接成功
    EXPECT_TRUE(permit.Acquire(0));
    permit.Release();
    EXPECT_FALSE(permit.Held());
    // 重复释放幂等
    permit.Release();
}

TEST_F(TestUbseMemCabinetLedger, PermitConcurrencyLimit)
{
    CrossCabinetRoundPermit first;
    CrossCabinetRoundPermit second;
    EXPECT_TRUE(first.Acquire(0));
    EXPECT_TRUE(second.Acquire(0));
    // 并发上限 2：第三个申请超时失败
    {
        CrossCabinetRoundPermit third;
        EXPECT_FALSE(third.Acquire(1));
        EXPECT_FALSE(third.Held());
    }
    // 释放一个后可再申请
    first.Release();
    CrossCabinetRoundPermit third;
    EXPECT_TRUE(third.Acquire(100));
}

TEST_F(TestUbseMemCabinetLedger, PermitDestructorReleases)
{
    {
        CrossCabinetRoundPermit scoped;
        ASSERT_TRUE(scoped.Acquire(0));
    }
    CrossCabinetRoundPermit next;
    EXPECT_TRUE(next.Acquire(0));
}

TEST_F(TestUbseMemCabinetLedger, CollectColdStartNoPeerNoQuery)
{
    // T 两份账本均空（冷启动）：不发起任何跨柜查询，也无遮蔽
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    NodeMemDebtInfo emptyMaster;
    NodeMemDebtInfo emptyAgent;
    CrossCabinetRoundCtx round{};
    round.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    std::set<std::string> deferredPeers;
    CollectCrossCabinetLedgers(allDebtInfoMap, emptyMaster, emptyAgent, 1, "node-1", round, deferredPeers);
    EXPECT_TRUE(deferredPeers.empty());
    EXPECT_TRUE(allDebtInfoMap.empty());
    // 无外柜对端时不申请跨柜闸门
    EXPECT_FALSE(round.permit.Held());
}

TEST_F(TestUbseMemCabinetLedger, CollectGroupUnknownPeerDeferred)
{
    // 拓扑无法定位对端柜号：按 SKIPPED 遮蔽，不得当空账本
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    auto masterDebt = MakeMasterDebt("9");
    NodeMemDebtInfo emptyAgent;
    CrossCabinetRoundCtx round{};
    round.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    std::set<std::string> deferredPeers;
    MOCKER(ubse::nodeMgr::GetUbseNodeById).stubs().will(returnValue(ubse::nodeMgr::UbseNodeStaticInfo{}));
    CollectCrossCabinetLedgers(allDebtInfoMap, masterDebt, emptyAgent, 1, "node-1", round, deferredPeers);
    EXPECT_EQ(std::set<std::string>({"9"}), deferredPeers);
    EXPECT_TRUE(allDebtInfoMap.empty());
}

TEST_F(TestUbseMemCabinetLedger, CollectRoundTimeoutAllDeferred)
{
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    auto masterDebt = MakeMasterDebt("3");
    NodeMemDebtInfo emptyAgent;
    CrossCabinetRoundCtx round{};
    round.deadline = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
    std::set<std::string> deferredPeers;
    MockNodeGroup("3", 2); // 外柜
    CollectCrossCabinetLedgers(allDebtInfoMap, masterDebt, emptyAgent, 1, "node-1", round, deferredPeers);
    EXPECT_EQ(std::set<std::string>({"3"}), deferredPeers);
    EXPECT_TRUE(allDebtInfoMap.empty());
}

TEST_F(TestUbseMemCabinetLedger, CollectMasterNotFoundDeferred)
{
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    auto masterDebt = MakeMasterDebt("3");
    NodeMemDebtInfo emptyAgent;
    CrossCabinetRoundCtx round{};
    round.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    std::set<std::string> deferredPeers;
    MockNodeGroup("3", 2);
    MOCKER(UbseGetCascadeMasterNodeIdByAgentNodeId).stubs().will(returnValue(UBSE_ERROR));
    CollectCrossCabinetLedgers(allDebtInfoMap, masterDebt, emptyAgent, 1, "node-1", round, deferredPeers);
    EXPECT_EQ(std::set<std::string>({"3"}), deferredPeers);
    EXPECT_TRUE(allDebtInfoMap.empty());
}

TEST_F(TestUbseMemCabinetLedger, CollectRpcSendFailedDeferred)
{
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    auto masterDebt = MakeMasterDebt("3");
    NodeMemDebtInfo emptyAgent;
    CrossCabinetRoundCtx round{};
    round.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    std::set<std::string> deferredPeers;
    MockNodeGroup("3", 2);
    std::string master = "m2";
    MOCKER(UbseGetCascadeMasterNodeIdByAgentNodeId).stubs().with(any(), outBound(master)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto sendFunc = &UbseComModule::RpcSend<UbseComBaseBufferMessagePtr, UbseComBaseBufferMessagePtr>;
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_ERROR));
    CollectCrossCabinetLedgers(allDebtInfoMap, masterDebt, emptyAgent, 1, "node-1", round, deferredPeers);
    EXPECT_EQ(std::set<std::string>({"3"}), deferredPeers);
    EXPECT_TRUE(allDebtInfoMap.empty());
}

TEST_F(TestUbseMemCabinetLedger, CollectSelfCabinetPeerNotQueried)
{
    // 同柜对端不发起跨柜查询：无外柜分组即直接返回
    std::unordered_map<std::string, NodeMemDebtInfo> allDebtInfoMap;
    auto masterDebt = MakeMasterDebt("3");
    NodeMemDebtInfo emptyAgent;
    CrossCabinetRoundCtx round{};
    round.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    std::set<std::string> deferredPeers;
    MockNodeGroup("3", 1); // 与 selfGroupId 相同
    CollectCrossCabinetLedgers(allDebtInfoMap, masterDebt, emptyAgent, 1, "node-1", round, deferredPeers);
    EXPECT_TRUE(deferredPeers.empty());
    EXPECT_TRUE(allDebtInfoMap.empty());
}

TEST_F(TestUbseMemCabinetLedger, ShadowFilterEmptyDeferredNoop)
{
    auto masterDebt = MakeMasterDebt("2");
    auto agentDebt = MakeMasterDebt("3");
    const std::set<std::string> deferredPeers;
    ShadowFilterDeferredRelations(masterDebt, agentDebt, deferredPeers);
    EXPECT_EQ(1U, masterDebt.fdExportObjMap.size());
    EXPECT_EQ(1U, agentDebt.fdExportObjMap.size());
}

TEST_F(TestUbseMemCabinetLedger, ShadowFilterRemovesDeferredEntries)
{
    auto masterDebt = MakeMasterDebt("2");
    masterDebt.fdExportObjMap.emplace("res2", MakeMasterDebt("3").fdExportObjMap["res1"]);
    auto agentDebt = MakeMasterDebt("3");
    const std::set<std::string> deferredPeers = {"3"};
    ShadowFilterDeferredRelations(masterDebt, agentDebt, deferredPeers);
    // master 只剩对端 "2" 的记录；agent 的 "3" 记录被剔除
    ASSERT_EQ(1U, masterDebt.fdExportObjMap.size());
    EXPECT_NE(masterDebt.fdExportObjMap.end(), masterDebt.fdExportObjMap.find("res1"));
    EXPECT_TRUE(agentDebt.fdExportObjMap.empty());
}
} // namespace ubse::mem_controller::ut
