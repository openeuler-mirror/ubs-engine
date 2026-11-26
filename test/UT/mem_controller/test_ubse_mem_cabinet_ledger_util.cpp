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

#include "test_ubse_mem_cabinet_ledger_util.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_cabinet_ledger_util.h"
#include "ubse_node_mgr.h"

namespace ubse::mem_controller::ut {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::controller;
using namespace ubse::nodeMgr;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;

namespace {
// fd 导出记录：对外对端挂 exportNumaInfos 首元素
template <typename Obj>
Obj MakeExportObj(const std::string& peer)
{
    Obj obj{};
    UbseMemDebtNumaInfo info{};
    info.nodeId = peer;
    obj.algoResult.exportNumaInfos.push_back(info);
    return obj;
}

UbseMemFdBorrowImportObj MakeFdImport(const std::string& peer)
{
    UbseMemFdBorrowImportObj obj{};
    UbseMemDebtNumaInfo info{};
    info.nodeId = peer;
    obj.algoResult.importNumaInfos.push_back(info);
    return obj;
}

UbseMemShareBorrowImportObj MakeShareImport(const std::string& importPeer, const std::string& exportPeer)
{
    UbseMemShareBorrowImportObj obj{};
    obj.importNodeId = importPeer;
    UbseMemDebtNumaInfo info{};
    info.nodeId = exportPeer;
    obj.algoResult.exportNumaInfos.push_back(info);
    return obj;
}
} // namespace

void TestUbseMemCabinetLedgerUtil::SetUp()
{
    Test::SetUp();
}
void TestUbseMemCabinetLedgerUtil::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemCabinetLedgerUtil, GetCabinetGroupIdByNodeId_EmptyOrUnknown)
{
    uint32_t groupId = 99;
    EXPECT_FALSE(GetCabinetGroupIdByNodeId("", groupId));
    MOCKER(ubse::nodeMgr::GetUbseNodeById).stubs().will(returnValue(UbseNodeStaticInfo{}));
    EXPECT_FALSE(GetCabinetGroupIdByNodeId("8", groupId));
    EXPECT_EQ(99, groupId);
}

TEST_F(TestUbseMemCabinetLedgerUtil, GetCabinetGroupIdByNodeId_Found)
{
    UbseNodeStaticInfo info{};
    info.nodeId = "3";
    info.groupId = 2;
    MOCKER(ubse::nodeMgr::GetUbseNodeById).stubs().will(returnValue(info));
    uint32_t groupId = 0;
    EXPECT_TRUE(GetCabinetGroupIdByNodeId("3", groupId));
    EXPECT_EQ(2, groupId);
}

TEST_F(TestUbseMemCabinetLedgerUtil, GetCabinetMemberNodeIds_InvalidGroup)
{
    EXPECT_TRUE(GetCabinetMemberNodeIds(70000).empty());
}

TEST_F(TestUbseMemCabinetLedgerUtil, GetCabinetMemberNodeIds_SortedAndFiltered)
{
    std::vector<UbseNodeStaticInfo> members(3);
    members[0].nodeId = "5";
    members[1].nodeId = "";
    members[2].nodeId = "2";
    MOCKER(ubse::nodeMgr::GetNodesByGroupId).stubs().will(returnValue(members));
    const auto nodeIds = GetCabinetMemberNodeIds(2);
    ASSERT_EQ(2U, nodeIds.size());
    EXPECT_EQ("2", nodeIds[0]);
    EXPECT_EQ("5", nodeIds[1]);
}

TEST_F(TestUbseMemCabinetLedgerUtil, GetDebtPeerNodeIds_AllBorrowTypes)
{
    NodeMemDebtInfo info{};
    info.fdImportObjMap.emplace("fi1", MakeFdImport("2"));
    info.fdExportObjMap.emplace("fe1", MakeExportObj<UbseMemFdBorrowExportObj>("3"));
    info.numaExportObjMap.emplace("ne1", MakeExportObj<UbseMemNumaBorrowExportObj>("4"));
    info.shareImportObjMap.emplace("si1", MakeShareImport("5", "6"));
    const auto peers = GetDebtPeerNodeIds(info);
    const std::set<std::string> expected = {"2", "3", "4", "5", "6"};
    EXPECT_EQ(expected, peers);
}

TEST_F(TestUbseMemCabinetLedgerUtil, GetDebtPeerNodeIds_EmptyAndDedup)
{
    NodeMemDebtInfo info{};
    EXPECT_TRUE(GetDebtPeerNodeIds(info).empty());
    info.fdExportObjMap.emplace("fe1", MakeExportObj<UbseMemFdBorrowExportObj>("3"));
    info.fdExportObjMap.emplace("fe2", MakeExportObj<UbseMemFdBorrowExportObj>("3"));
    info.numaExportObjMap.emplace("ne1", MakeExportObj<UbseMemNumaBorrowExportObj>(""));
    const std::set<std::string> expected = {"3"};
    EXPECT_EQ(expected, GetDebtPeerNodeIds(info));
}

TEST_F(TestUbseMemCabinetLedgerUtil, FilterDebtInfoByPeerNodeIds_KeepInSet)
{
    NodeMemDebtInfo info{};
    info.fdExportObjMap.emplace("fe1", MakeExportObj<UbseMemFdBorrowExportObj>("2"));
    info.fdExportObjMap.emplace("fe2", MakeExportObj<UbseMemFdBorrowExportObj>("3"));
    const std::set<std::string> peers = {"3"};
    const auto filtered = FilterDebtInfoByPeerNodeIds(info, peers, PeerFilterMode::KEEP_IN_SET);
    EXPECT_EQ(0U, filtered.fdImportObjMap.size());
    EXPECT_EQ(1U, filtered.fdExportObjMap.size());
    EXPECT_NE(filtered.fdExportObjMap.end(), filtered.fdExportObjMap.find("fe2"));
    // 原账本不被修改
    EXPECT_EQ(2U, info.fdExportObjMap.size());
}

TEST_F(TestUbseMemCabinetLedgerUtil, FilterDebtInfoByPeerNodeIds_RemoveInSet)
{
    NodeMemDebtInfo info{};
    info.shareImportObjMap.emplace("si1", MakeShareImport("2", ""));
    info.shareImportObjMap.emplace("si2", MakeShareImport("3", "4"));
    const std::set<std::string> peers = {"2"};
    const auto filtered = FilterDebtInfoByPeerNodeIds(info, peers, PeerFilterMode::REMOVE_IN_SET);
    EXPECT_EQ(1U, filtered.shareImportObjMap.size());
    EXPECT_NE(filtered.shareImportObjMap.end(), filtered.shareImportObjMap.find("si2"));
}

TEST_F(TestUbseMemCabinetLedgerUtil, FilterDebtInfoByPeerNodeIds_NoHitKeepReturnsEmpty)
{
    NodeMemDebtInfo info{};
    info.fdExportObjMap.emplace("fe1", MakeExportObj<UbseMemFdBorrowExportObj>("2"));
    const std::set<std::string> peers = {"9"};
    const auto filtered = FilterDebtInfoByPeerNodeIds(info, peers, PeerFilterMode::KEEP_IN_SET);
    EXPECT_TRUE(filtered.fdExportObjMap.empty());
}

TEST_F(TestUbseMemCabinetLedgerUtil, IsConfirmedFaultClusterState)
{
    using nodeController::UbseNodeClusterState;
    EXPECT_TRUE(IsConfirmedFaultClusterState(UbseNodeClusterState::UBSE_NODE_FAULT));
    EXPECT_TRUE(IsConfirmedFaultClusterState(UbseNodeClusterState::UBSE_NODE_PRE_BMC));
    EXPECT_TRUE(IsConfirmedFaultClusterState(UbseNodeClusterState::UBSE_NODE_UNKNOWN));
    EXPECT_FALSE(IsConfirmedFaultClusterState(UbseNodeClusterState::UBSE_NODE_INIT));
    EXPECT_FALSE(IsConfirmedFaultClusterState(UbseNodeClusterState::UBSE_NODE_SMOOTHING));
    EXPECT_FALSE(IsConfirmedFaultClusterState(UbseNodeClusterState::UBSE_NODE_WORKING));
}
} // namespace ubse::mem_controller::ut
