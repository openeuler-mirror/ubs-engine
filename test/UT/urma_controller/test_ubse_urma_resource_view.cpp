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

#include "test_ubse_urma_controller.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ubse_node_controller.h"
#include "ubse_urma_resource_view.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "test_ubse_urma_controller_def.h"

namespace ubse::urmaController::ut {
using namespace ubse::common::def;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::nodeController;
using namespace ubse::urma;

namespace {
constexpr size_t TEST_URMA_EID_SHARE_DEGREE = 192;

UbseUrmaResourceView& View()
{
    return UbseUrmaResourceView::GetInstance();
}

void StubCurrentNode(bool hostBondingRegistered)
{
    UbseNodeInfo node;
    node.nodeId = "1";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(node));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(hostBondingRegistered));
}

std::vector<UbseUrmaUvsNodeInfo> MakeHostPlanning()
{
    UbseUrmaUvsAggrDev dev;
    dev.urmaDevEid = "host-dev-eid";
    dev.feList.push_back({"1", "11", "host-fe-a", {}});
    dev.feList.push_back({"2", "22", "host-fe-b", {}});
    return {{"1", {std::move(dev)}}};
}

std::map<UbseMtiIouInfo, UbseMtiEidGroup> MakeHostHwResSource()
{
    std::map<UbseMtiIouInfo, UbseMtiEidGroup> comEids;
    comEids[{"1", "1", "3"}] = {"11", "host-fe-a", {}};
    comEids[{"1", "2", "4"}] = {"22", "host-fe-b", {}};
    return comEids;
}

void StubHostHwResSource(std::map<UbseMtiIouInfo, UbseMtiEidGroup> comEids = MakeHostHwResSource())
{
    auto& mti = UbseMtiInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(mti, &UbseMtiInterface::GetMtiComEid)
        .expects(once())
        .with(outBound(comEids))
        .will(returnValue(UBSE_OK));
}

void ReplaceBacking(const std::string& name, UbseUrmaInfo info)
{
    auto& manager = UbseUrmaControllerManager::GetInstance();
    ubse::utils::WriteLocker<ubse::utils::ReadWriteLock> guard(&manager.rwLock);
    manager.nodeInfos["1"].urmaList[name] = std::move(info);
}

void RemoveBacking(const std::string& name)
{
    auto& manager = UbseUrmaControllerManager::GetInstance();
    ubse::utils::WriteLocker<ubse::utils::ReadWriteLock> guard(&manager.rwLock);
    manager.nodeInfos["1"].urmaList.erase(name);
}

void ClearProjectionMappingForTest()
{
    std::lock_guard<std::mutex> guard(View().mappingMutex);
    View().mappedBackingNames.clear();
    View().logicalBackingMap.clear();
}
} // namespace

class TestUbseUrmaResourceView : public testing::Test {
protected:
    void SetUp() override
    {
        Test::SetUp();
        ClearNodeInfosForTest();
        ClearProjectionMappingForTest();
    }

    void TearDown() override
    {
        ClearNodeInfosForTest();
        ClearProjectionMappingForTest();
        Test::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestUbseUrmaResourceView, EmptyBackingSetReturnsEmptyProjection)
{
    StubCurrentNode(false);
    UrmaLogicalProjection projection;
    EXPECT_EQ(View().BuildLogicalProjection({}, projection), UBSE_OK);
    EXPECT_EQ(projection.logicalDeviceCount, 0);
    EXPECT_TRUE(projection.groups.empty());
}

TEST_F(TestUbseUrmaResourceView, ProjectionOwnsNamesCountAndNumericOrder)
{
    InsertBacking("bonding_dev_10", MakeBacking("eid-10", "fe-10-a", "fe-10-b"));
    InsertBacking("bonding_dev_2", MakeBacking("eid-2", "fe-2-a", "fe-2-b"));
    StubCurrentNode(false);

    UrmaLogicalProjection projection;
    ASSERT_EQ(View().BuildLogicalProjection({}, projection), UBSE_OK);
    ASSERT_EQ(projection.groups.size(), 2);
    EXPECT_EQ(projection.logicalDeviceCount, 384);
    EXPECT_EQ(projection.groups[0].backingName, "bonding_dev_2");
    EXPECT_EQ(projection.groups[0].logicalNames.front(), "bonding_dev_1");
    EXPECT_EQ(projection.groups[0].logicalNames.back(), "bonding_dev_192");
    EXPECT_EQ(projection.groups[1].backingName, "bonding_dev_10");
    EXPECT_EQ(projection.groups[1].logicalNames.front(), "bonding_dev_193");
}

TEST_F(TestUbseUrmaResourceView, ProjectionFiltersDeduplicatesAndKeepsLogicalOrder)
{
    InsertBacking("bonding_dev_1", MakeBacking("eid-1", "fe-a", "fe-b"));
    InsertBacking("bonding_dev_2", MakeBacking("eid-2", "fe-c", "fe-d"));
    StubCurrentNode(false);

    UrmaLogicalProjection projection;
    ASSERT_EQ(
        View().BuildLogicalProjection({"bonding_dev_194", "bonding_dev_2", "bonding_dev_194", "missing"}, projection),
        UBSE_OK);
    ASSERT_EQ(projection.logicalDeviceCount, 2);
    ASSERT_EQ(projection.groups.size(), 2);
    EXPECT_EQ(projection.groups[0].logicalNames, std::vector<std::string>({"bonding_dev_2"}));
    EXPECT_EQ(projection.groups[1].logicalNames, std::vector<std::string>({"bonding_dev_194"}));
}

TEST_F(TestUbseUrmaResourceView, ProjectionCachesNamesButReadsCurrentMetadata)
{
    auto info = MakeBacking("eid-1", "fe-a", "fe-b", 11);
    InsertBacking("bonding_dev_1", info);
    StubCurrentNode(false);

    UrmaLogicalProjection first;
    ASSERT_EQ(View().BuildLogicalProjection({}, first), UBSE_OK);
    const auto mapping = View().logicalBackingMap;
    EXPECT_EQ(mapping.size(), TEST_URMA_EID_SHARE_DEGREE);
    EXPECT_EQ(mapping.at("bonding_dev_1"), "bonding_dev_1");
    EXPECT_EQ(mapping.at("bonding_dev_192"), "bonding_dev_1");
    info.hwResId = 22;
    ReplaceBacking("bonding_dev_1", info);

    UrmaLogicalProjection second;
    ASSERT_EQ(View().BuildLogicalProjection({}, second), UBSE_OK);
    EXPECT_EQ(View().logicalBackingMap, mapping);
    EXPECT_EQ(first.groups[0].backingInfo.hwResId, 11);
    EXPECT_EQ(second.groups[0].backingInfo.hwResId, 22);
}

TEST_F(TestUbseUrmaResourceView, ListedBackingNamesDoNotRequireCanonicalFormat)
{
    StubCurrentNode(false);
    const std::vector<std::string> listedNames{"bonding_dev_", "bonding_dev_01", "other_1",
                                               "bonding_dev_18446744073709551615"};
    for (const auto& name : listedNames) {
        ClearNodeInfosForTest();
        InsertBacking(name, MakeBacking("eid-1", "fe-a", "fe-b"));
        UrmaLogicalProjection projection;
        ASSERT_EQ(View().BuildLogicalProjection({}, projection), UBSE_OK) << name;
        ASSERT_EQ(projection.groups.size(), 1) << name;
        EXPECT_EQ(projection.groups[0].backingName, name);
    }
}

TEST_F(TestUbseUrmaResourceView, RuntimeBackingAppendsWithoutRenumbering)
{
    InsertBacking("bonding_dev_10", MakeBacking("eid-10", "fe-10-a", "fe-10-b"));
    StubCurrentNode(false);
    UrmaLogicalProjection projection;
    ASSERT_EQ(View().BuildLogicalProjection({}, projection), UBSE_OK);
    EXPECT_EQ(projection.groups[0].logicalNames.front(), "bonding_dev_1");

    InsertBacking("bonding_dev_11", MakeBacking("eid-11", "fe-11-a", "fe-11-b"));
    ASSERT_EQ(View().BuildLogicalProjection({}, projection), UBSE_OK);
    ASSERT_EQ(projection.groups.size(), 2);
    EXPECT_EQ(projection.groups[0].backingName, "bonding_dev_10");
    EXPECT_EQ(projection.groups[0].logicalNames.front(), "bonding_dev_1");
    EXPECT_EQ(projection.groups[1].backingName, "bonding_dev_11");
    EXPECT_EQ(projection.groups[1].logicalNames.front(), "bonding_dev_193");
}

TEST_F(TestUbseUrmaResourceView, SharedProjectionUsesOnlyOccupiedHostBonding)
{
    InsertBacking("bonding_dev_", MakeBacking("eid-before-zero", "fe-before-a", "fe-before-b"));
    InsertBacking("bonding_dev_2", MakeBacking("eid-2", "fe-2-a", "fe-2-b"));
    StubCurrentNode(true);
    auto planning = MakeHostPlanning();
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(eq(std::string("1")), outBound(planning))
        .will(returnValue(UBSE_OK));
    StubHostHwResSource();

    UrmaLogicalProjection projection;
    ASSERT_EQ(View().BuildLogicalProjection({}, projection, true), UBSE_OK);
    ASSERT_EQ(projection.groups.size(), 1);
    EXPECT_EQ(projection.logicalDeviceCount, TEST_URMA_EID_SHARE_DEGREE);
    EXPECT_EQ(projection.groups[0].backingName, UBSE_HOST_URMA_DEV_NAME);
    EXPECT_TRUE(projection.groups[0].isHostBonding);
    EXPECT_EQ(projection.groups[0].backingInfo.urmaDevEid, "host-dev-eid");
    EXPECT_EQ(projection.groups[0].backingInfo.hwResId, (uint64_t{3} << 32) | 11);
    EXPECT_EQ(projection.groups[0].logicalNames.front(), "bonding_dev_1");
    EXPECT_EQ(projection.groups[0].logicalNames.back(), "bonding_dev_192");
}

TEST_F(TestUbseUrmaResourceView, EmptyListedBackingNameIsInvalid)
{
    InsertBacking("", MakeBacking("eid-1", "fe-a", "fe-b"));
    StubCurrentNode(false);

    UrmaLogicalProjection projection;
    EXPECT_EQ(View().BuildLogicalProjection({}, projection), UBSE_ERROR_INVAL);
    EXPECT_TRUE(projection.groups.empty());

    UrmaAllocTarget resolved;
    EXPECT_EQ(View().ResolveGroupedAllocTarget("bonding_dev_1", resolved), UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID);
}

TEST_F(TestUbseUrmaResourceView, UnoccupiedHostBondingProducesEmptySharedProjection)
{
    InsertBacking("bonding_dev_1", MakeBacking("eid-1", "fe-a", "fe-b"));
    StubCurrentNode(false);
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId).expects(never());

    UrmaLogicalProjection projection;
    ASSERT_EQ(View().BuildLogicalProjection({}, projection, true), UBSE_OK);
    EXPECT_EQ(projection.logicalDeviceCount, 0);
    EXPECT_TRUE(projection.groups.empty());
}

TEST_F(TestUbseUrmaResourceView, HostHwResSourceMustMatchPlanning)
{
    StubCurrentNode(true);
    auto planning = MakeHostPlanning();
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(eq(std::string("1")), outBound(planning))
        .will(returnValue(UBSE_OK));
    StubHostHwResSource({});

    UrmaLogicalProjection projection;
    EXPECT_EQ(View().BuildLogicalProjection({}, projection), UBSE_ERROR_INVAL);
    EXPECT_TRUE(projection.groups.empty());
}

TEST_F(TestUbseUrmaResourceView, IncompleteBackingsFailAtomically)
{
    auto missingEid = MakeBacking("", "fe-a", "fe-b");
    auto missingFe = MakeBacking("eid-1", "fe-a", "fe-b");
    missingFe.eidGroups.pop_back();
    auto emptyPrimary = MakeBacking("eid-1", "", "fe-b");
    auto nullMetadata = MakeBacking("eid-1", "fe-a", "fe-b");
    nullMetadata.eidGroups[1].feInfo.reset();
    const std::vector<UbseUrmaInfo> cases{missingEid, missingFe, emptyPrimary, nullMetadata};

    StubCurrentNode(false);
    for (const auto& invalid : cases) {
        ClearNodeInfosForTest();
        InsertBacking("bonding_dev_1", invalid);
        UrmaLogicalProjection projection;
        projection.logicalDeviceCount = 9;
        projection.groups.push_back({});
        EXPECT_EQ(View().BuildLogicalProjection({}, projection), UBSE_ERROR_INVAL);
        EXPECT_EQ(projection.logicalDeviceCount, 0);
        EXPECT_TRUE(projection.groups.empty());
    }
}

TEST_F(TestUbseUrmaResourceView, ProjectionAllowsRepeatedPhysicalEids)
{
    InsertBacking("bonding_dev_1", MakeBacking("same-eid", "same-fe", "fe-b"));
    InsertBacking("bonding_dev_2", MakeBacking("same-eid", "fe-c", "same-fe"));
    StubCurrentNode(false);

    UrmaLogicalProjection projection;
    ASSERT_EQ(View().BuildLogicalProjection({}, projection), UBSE_OK);
    EXPECT_EQ(projection.logicalDeviceCount, 384);
    EXPECT_EQ(projection.groups.size(), 2);
}

TEST_F(TestUbseUrmaResourceView, ProjectionReflectsChangedAndRemovedBacking)
{
    InsertBacking("bonding_dev_1", MakeBacking("eid-a", "fe-a", "fe-b"));
    StubCurrentNode(false);
    UrmaLogicalProjection projection;
    ASSERT_EQ(View().BuildLogicalProjection({}, projection), UBSE_OK);

    ReplaceBacking("bonding_dev_1", MakeBacking("eid-b", "fe-a", "fe-b"));
    ASSERT_EQ(View().BuildLogicalProjection({}, projection), UBSE_OK);
    ASSERT_EQ(projection.groups.size(), 1);
    EXPECT_EQ(projection.groups[0].backingInfo.urmaDevEid, "eid-b");

    RemoveBacking("bonding_dev_1");
    EXPECT_EQ(View().BuildLogicalProjection({}, projection), UBSE_OK);
    EXPECT_TRUE(projection.groups.empty());
}

TEST_F(TestUbseUrmaResourceView, ResolveRejectsNonCanonicalNamesBeforeLookup)
{
    MOCKER_CPP(&UbseNodeController::GetCurNode).expects(never());
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).expects(never());
    const std::vector<std::string> invalid{"",
                                           "bonding_dev_",
                                           "bonding_dev_01",
                                           "bonding_dev_+1",
                                           "bonding_dev_-1",
                                           " bonding_dev_1",
                                           "bonding_dev_1 ",
                                           "bonding_dev_1x",
                                           "bonding_dev_18446744073709551616"};
    for (const auto& name : invalid) {
        UrmaAllocTarget resolved;
        EXPECT_EQ(View().ResolveGroupedAllocTarget(name, resolved), UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID) << name;
    }

    UrmaAllocTarget resolved;
    EXPECT_EQ(View().ResolveGroupedAllocTarget("bonding_dev_0", resolved), UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST);
}

TEST_F(TestUbseUrmaResourceView, ResolveRejectsAliasOutsideCurrentProjection)
{
    InsertBacking("bonding_dev_1", MakeBacking("eid-1", "fe-a", "fe-b"));
    StubCurrentNode(false);
    UrmaAllocTarget resolved;
    EXPECT_EQ(View().ResolveGroupedAllocTarget("bonding_dev_193", resolved), UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID);
}

TEST_F(TestUbseUrmaResourceView, ResolveHostNameDoesNotReadPlanningMetadata)
{
    InsertBacking("bonding_dev_1", MakeBacking("eid-1", "fe-a", "fe-b"));
    StubCurrentNode(true);
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId).expects(never());

    UrmaAllocTarget resolved;
    ASSERT_EQ(View().ResolveGroupedAllocTarget("bonding_dev_1", resolved), UBSE_OK);
    EXPECT_EQ(resolved.GetBackingName(), UBSE_HOST_URMA_DEV_NAME);
    EXPECT_TRUE(resolved.IsHostBonding());
}

TEST_F(TestUbseUrmaResourceView, ResolveSharedAllocIgnoresNonHostBonding)
{
    InsertBacking("bonding_dev_1", MakeBacking("eid-1", "fe-a", "fe-b"));
    StubCurrentNode(true);

    UrmaAllocTarget resolved;
    ASSERT_EQ(View().ResolveGroupedAllocTarget("bonding_dev_1", resolved), UBSE_OK);
    EXPECT_EQ(resolved.GetBackingName(), UBSE_HOST_URMA_DEV_NAME);
    resolved = UrmaAllocTarget{};
    EXPECT_EQ(View().ResolveGroupedAllocTarget("bonding_dev_193", resolved), UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID);
}

} // namespace ubse::urmaController::ut
