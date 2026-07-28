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

#include "adapter_plugins/mti/ubse_smbios.h"
#include "test_scheduler_fixture.h"

namespace ubse::mem::scheduler::ut {
namespace {

using namespace ubse::common::def;

void SetupTwoNodeShareEnv(std::unordered_map<std::string, UbseNodeInfo>& nodeMap)
{
    nodeMap = CreateNodeMap(2);
    nodeMap["1"].isLender = false;
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);
}

adapter_plugins::mmi::UbseMemShareBorrowExportObj MakeShareObj(const std::string& name, uint64_t size,
                                                               bool withAffinity = false)
{
    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = name;
    obj.req.requestNodeId = "1";
    obj.req.size = size;
    obj.req.withAffinity.enableCreateWithAffinity = withAffinity;
    if (withAffinity) {
        obj.req.withAffinity.affinitySocketId = 36;
    }
    for (const auto& nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
    return obj;
}

void SetupTwoNodeShareEnv1DFullmesh(std::unordered_map<std::string, UbseNodeInfo>& nodeMap, bool socket36Up,
                                    bool socket276Up)
{
    nodeMap = CreateNodeMap(2);
    nodeMap["1"].isLender = false;
    // node2's ports (for UpdateLinkInfo → rawPortInfos / ports_)
    if (socket36Up) {
        AddPeerRelation(nodeMap["2"], "1", 0, 0);
    }
    if (socket276Up) {
        AddPeerRelation(nodeMap["2"], "1", 1, 1);
    }
    // node1's ports (determine the reachable peer set for LinkPortDownFilter)
    // Only add peer relations for the sockets that should be reachable
    if (socket36Up) {
        AddPeerRelation(nodeMap["1"], "2", 0, 0);
    }
    if (socket276Up) {
        AddPeerRelation(nodeMap["1"], "2", 1, 1);
    }
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);
}

void SetupThreeNodeShareEnv1DFullmeshWithPartition(std::unordered_map<std::string, UbseNodeInfo>& nodeMap)
{
    nodeMap = CreateNodeMap(3);
    nodeMap["1"].isLender = false;
    // node1 ↔ node2 full mesh
    AddPeerRelation(nodeMap["1"], "2", 0, 0);
    AddPeerRelation(nodeMap["1"], "2", 1, 1);
    AddPeerRelation(nodeMap["2"], "1", 0, 0);
    AddPeerRelation(nodeMap["2"], "1", 1, 1);
    // node2 ↔ node3 full mesh
    AddPeerRelation(nodeMap["2"], "3", 0, 0);
    AddPeerRelation(nodeMap["2"], "3", 1, 1);
    AddPeerRelation(nodeMap["3"], "2", 0, 0);
    AddPeerRelation(nodeMap["3"], "2", 1, 1);
    // node1 ↔ node3 NOT connected (partition)
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);
}

void SetupTwoNodeClosEnv(std::unordered_map<std::string, UbseNodeInfo>& nodeMap, bool node2Sock36Up,
                         bool node2Sock276Up)
{
    MOCKER_CPP(&ubse::adapter_plugins::smbios::UbseSmbios::IsClosType).stubs().will(returnValue(true));
    nodeMap = CreateNodeMap(2);
    nodeMap["1"].isLender = false;
    AddPeerRelation(nodeMap["1"], "0", 0, 0);
    AddPeerRelation(nodeMap["1"], "0", 1, 1);
    if (node2Sock36Up) {
        AddPeerRelation(nodeMap["2"], "0", 0, 0);
    }
    if (node2Sock276Up) {
        AddPeerRelation(nodeMap["2"], "0", 1, 1);
    }
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);
}

} // namespace

/**
 * @brief 用例 1: ShareMemoryBorrowWillSuccess
 *
 * MemoryObjChangeHandler SCHEDULING 共享导出决策。
 * SHARE 链不含 BorrowedLenderFilter，设节点1 isLender=false，
 * LenderNodeFilter 仅保留节点2。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，节点1 isLender=false，full-mesh peer，Mock GetAllNodes
 *   2. MakeShareObj("share-export-1", BYTE_256MB)：构造 SHARE export obj，shmRegion=["1","2"]，state=SCHEDULING
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos[0].nodeId == "2"
 *   3. importNumaInfos 为空（SHARE 不需要）
 */
TEST_F(TestSchedulerEndToEnd, ShareMemoryBorrowWillSuccess)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    auto obj = MakeShareObj("share-export-1", BYTE_256MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_GT(obj.algoResult.blockSize, 0u);
    EXPECT_GT(obj.algoResult.attachSocketId, 0u);
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

/**
 * @brief 用例 2: ShareMemoryBorrowWithAffinity
 *
 * 指定 affinity socketId=36，SocketAffinityFilter 只保留同平面 socket。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{withAffinity:{36}, shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，节点1 isLender=false，full-mesh peer，Mock GetAllNodes
 *   2. MakeShareObj(..., true)：构造 SHARE export obj，affinity=36，shmRegion=["1","2"]，state=SCHEDULING
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos[0].nodeId == "2"
 */
TEST_F(TestSchedulerEndToEnd, ShareMemoryBorrowWithAffinity)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    auto obj = MakeShareObj("share-affinity-1", BYTE_256MB, true);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

/**
 * @brief 用例 3: ShareMemoryBorrowWithProviderList
 *
 * 指定 providerList=["3"]，RequestedProvidersFilter 只保留节点3。
 *
 * 前置状态: 四节点已注册，均为 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{providerList:["3"], shmRegion:["1","2","3","4"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. CreateNodeMap(4)：创建四节点，full-mesh peer，注册并 Mock GetAllNodes
 *   2. 构造 SHARE export obj（providerList=["3"]，shmRegion=["1","2","3","4"]，state=SCHEDULING）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos[0].nodeId == "3"
 */
TEST_F(TestSchedulerEndToEnd, ShareMemoryBorrowWithProviderList)
{
    auto nodeMap = CreateNodeMap(4);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["4"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-provider-1";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    obj.req.providerList = {"3"};
    for (const auto& nodeId : {"1", "2", "3", "4"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "3");
}

/**
 * @brief 用例 4: ShareMemoryBorrowWithAffinityAndProviderList
 *
 * 组合 affinity socket=36 + providerList=["1"]，验证 SocketAffinityFilter
 * 与 RequestedProvidersFilter 联合过滤路径。
 *
 * 前置状态: 四节点已注册，全互联链路
 *
 * 测试输入:
 *   UbseMemShareBorrowExportObj{
 *     providerList:["1"], withAffinity:{36}, shmRegion:["1","2","3","4"], state:SCHEDULING
 *   }
 *
 * 操作步骤:
 *   1. CreateNodeMap(4)：创建四节点，full-mesh peer，注册并 Mock GetAllNodes
 *   2. 构造 SHARE export obj（providerList=["1"]，affinity=36，4-node region，state=SCHEDULING）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "1"（providerList 限制）
 *   3. attachSocketId == 36（affinity 限制）
 */
TEST_F(TestSchedulerEndToEnd, ShareMemoryBorrowWithAffinityAndProviderList)
{
    auto nodeMap = CreateNodeMap(4);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["4"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-affinity-provider-1";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    obj.req.providerList = {"1"};
    obj.req.withAffinity.enableCreateWithAffinity = true;
    obj.req.withAffinity.affinitySocketId = 36;
    for (const auto& nodeId : {"1", "2", "3", "4"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "1");
    EXPECT_EQ(obj.algoResult.attachSocketId, 36u);
}

/**
 * @brief 用例 5: ShareBorrowInHugetlbPmd
 *
 * SHARE + HUGETLB_PMD，验证 2M 大页计算路径。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，allocator=HUGETLB_PMD，blockSize=64，
 *           nr_hugepages_2M=256，free_hugepages_2M=256，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{name:"share-hugepmd", shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 覆盖 allocator/pmdMapping 为 HUGETLB_PMD，设置大页参数
 *   3. ClearCache + 重建注册节点，Mock GetAllNodes
 *   4. MakeShareObj("share-hugepmd", BYTE_256MB)：构造 SHARE export obj
 *   5. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 *   3. export NUMA GetMemTotalSize() > 0
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowInHugetlbPmd)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);
    // 覆盖 allocator/pmdMapping
    for (auto& [_, node] : nodeMap) {
        node.allocator = UbseAllocator::HUGETLB_PMD;
        node.blockSize = 64;
        for (auto& [loc, numa] : node.numaInfos) {
            (void)loc;
            numa.nr_hugepages_2M = 256;
            numa.free_hugepages_2M = 256;
        }
    }
    SetHugeNumaMemPoolZero(nodeMap);
    // 重建注册
    SchedulerImpl::GetInstance().ClearCache();
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeShareObj("share-hugepmd", BYTE_256MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");

    auto& impl = SchedulerImpl::GetInstance();
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemTotalSize(), 0u);
}

// ==================== 1Dfullmesh (LinkPortDownFilter) ====================

/**
 * @brief 用例 6: ShareBorrow1DFullmesh_PeerSocketDown
 *
 * 1Dfullmesh 下 node2 socket36 端口 DOWN（无 peer 可达），socket276 正常。
 * LocalPortDownFilter 过滤掉 socket36，从 socket276 出借。
 *
 * 前置状态: 两节点已注册，1Dfullmesh 链路，socket36 down，socket276 up
 *
 * 测试输入: UbseMemShareBorrowExportObj{shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv1DFullmesh(socket36=false, socket276=true)：注册两节点，仅 socket276 有端口
 *   2. MakeShareObj("share-1dfull-sock36down", BYTE_256MB)：构造 SHARE export obj
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].socketId == 276（socket36 被 LocalPortDownFilter 过滤）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow1DFullmesh_PeerSocketDown)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv1DFullmesh(nodeMap, false, true);

    auto obj = MakeShareObj("share-1dfull-sock36down", BYTE_256MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    // socket36 被过滤，只能从 socket276 出借
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].socketId, 276);
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

/**
 * @brief 用例 7: ShareBorrow1DFullmesh_AllSocketsDown
 *
 * 1Dfullmesh 下 node2 所有 socket 端口 DOWN，无可达 peer。
 * LocalPortDownFilter 过滤所有 socket，无候选节点。
 *
 * 前置状态: 两节点已注册，1Dfullmesh 链路，socket36 down，socket276 down
 *
 * 测试输入: UbseMemShareBorrowExportObj{shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv1DFullmesh(socket36=false, socket276=false)：注册两节点，无端口
 *   2. MakeShareObj("share-1dfull-alldown", BYTE_256MB)：构造 SHARE export obj
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK（LocalPortDownFilter 过滤所有 socket，无候选）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow1DFullmesh_AllSocketsDown)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv1DFullmesh(nodeMap, false, false);

    auto obj = MakeShareObj("share-1dfull-alldown", BYTE_256MB);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 8: ShareBorrow1DFullmesh_MultiNodePartition
 *
 * 3 节点，node1 只能到达 node2，无法到达 node3。
 * LocalPortDownFilter 过滤掉不可达的 node3，从 node2 出借。
 *
 * 前置状态: 3 节点已注册，1Dfullmesh 链路，node1↔node2 互联，node2↔node3 互联，node1↔node3 不通
 *
 * 测试输入: UbseMemShareBorrowExportObj{shmRegion:["1","2","3"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupThreeNodeShareEnv1DFullmeshWithPartition：注册三节点，创建 partition 拓扑
 *   2. 构造 SHARE export obj（shmRegion=["1","2","3"]，state=SCHEDULING）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"（node3 被 LocalPortDownFilter 过滤）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow1DFullmesh_MultiNodePartition)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupThreeNodeShareEnv1DFullmeshWithPartition(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-partition-1";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2", "3"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    // node3 不可达被过滤，只能从 node2 出借
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

// ==================== CLOS (LocalPortDownFilter + RegionFilter) ====================

/**
 * @brief 用例 9: ShareBorrow_Clos_BasicSuccess
 *
 * CLOS 组网，两节点所有 socket 端口 UP。
 * RegionFilter CLOS 路径检查本端端口，两节点均合格。
 * node1 isLender=false 被 LenderRoleFilter 过滤，node2 出借。
 *
 * 前置状态: CLOS 组网，IsClosType()=true，两节点已注册，节点1 非 lender，所有端口 UP
 *
 * 测试输入: UbseMemShareBorrowExportObj{shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true))：Mock CLOS 组网
 *   2. SetupTwoNodeClosEnv(nodeMap, true, true)：注册两节点，两个 socket 均有 UP 端口
 *   3. MakeShareObj("share-clos-basic", BYTE_256MB)：构造 SHARE export obj
 *   4. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"（RegionFilter 保留两节点，LenderRoleFilter 过滤 node1）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow_Clos_BasicSuccess)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeClosEnv(nodeMap, true, true);

    auto obj = MakeShareObj("share-clos-basic", BYTE_256MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

/**
 * @brief 用例 10: ShareBorrow_Clos_RegionNodeNoUpPort
 *
 * CLOS 组网，两节点，node2 端口全 DOWN。
 * RegionFilter CLOS 路径检查 node2 无 UP 端口 → 拒绝。
 * node1 isLender=false → LenderRoleFilter 拒绝。
 * 无候选节点，调度失败。
 *
 * 前置状态: CLOS 组网，IsClosType()=true，两节点已注册，节点1 非 lender，node2 无端口
 *
 * 测试输入: UbseMemShareBorrowExportObj{shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true))：Mock CLOS 组网
 *   2. SetupTwoNodeClosEnv(nodeMap, false, false)：注册两节点，node2 无 UP 端口
 *   3. MakeShareObj("share-clos-noport", BYTE_256MB)：构造 SHARE export obj
 *   4. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK（RegionFilter 拒绝 node2，LenderRoleFilter 拒绝 node1，无候选）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow_Clos_RegionNodeNoUpPort)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeClosEnv(nodeMap, false, false);

    auto obj = MakeShareObj("share-clos-noport", BYTE_256MB);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 11: ShareBorrow_Clos_MultiNode_PartialDown
 *
 * CLOS 组网，三节点，node3 端口全 DOWN。
 * RegionFilter CLOS 路径过滤 node3，保留 node1/node2。
 * node1 isLender=false 被 LenderRoleFilter 过滤，node2 出借。
 *
 * 前置状态: CLOS 组网，IsClosType()=true，三节点已注册，节点1 非 lender，node3 无端口
 *
 * 测试输入: UbseMemShareBorrowExportObj{shmRegion:["1","2","3"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true))：Mock CLOS 组网
 *   2. CreateNodeMap(3)：创建三节点，node1/node2 加 UP 端口，node3 无端口
 *   3. 注册三节点，Mock GetAllNodes
 *   4. 构造 SHARE export obj（shmRegion=["1","2","3"]，state=SCHEDULING）
 *   5. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"（node3 被 RegionFilter 过滤，node1 被 LenderRoleFilter 过滤）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow_Clos_MultiNode_PartialDown)
{
    MOCKER_CPP(&ubse::adapter_plugins::smbios::UbseSmbios::IsClosType).stubs().will(returnValue(true));

    auto nodeMap = CreateNodeMap(3);
    nodeMap["1"].isLender = false;
    AddPeerRelation(nodeMap["1"], "0", 0, 0);
    AddPeerRelation(nodeMap["1"], "0", 1, 1);
    AddPeerRelation(nodeMap["2"], "0", 0, 0);
    AddPeerRelation(nodeMap["2"], "0", 1, 1);
    // node3: no ports (all sockets DOWN)
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-clos-multi-down";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2", "3"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

/**
 * @brief 用例 12: ShareBorrow_Clos_LocalPortDownFilter_SocketDown
 *
 * CLOS 组网，两节点，node2 的 socket36 端口 DOWN（无端口），socket276 正常。
 * RegionFilter：node2 仍有 socket276 UP → 保留。
 * LocalPortDownFilter：socket36 无端口 → 移除；socket276 正常 → 保留。
 * 评分/选择落到 socket276。
 *
 * 前置状态: CLOS 组网，IsClosType()=true，两节点已注册，节点1 非 lender，node2 socket36 无端口
 *
 * 测试输入: UbseMemShareBorrowExportObj{shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true))：Mock CLOS 组网
 *   2. SetupTwoNodeClosEnv(nodeMap, false, true)：注册两节点，node2 仅 socket276 有 UP 端口
 *   3. MakeShareObj("share-clos-sockdown", BYTE_256MB)：构造 SHARE export obj
 *   4. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].socketId == 276（socket36 被 LocalPortDownFilter 过滤）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow_Clos_LocalPortDownFilter_SocketDown)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeClosEnv(nodeMap, false, true);

    auto obj = MakeShareObj("share-clos-sockdown", BYTE_256MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].socketId, 276);
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}
/**
 * @brief 用例 13: ShareBorrow_RegionFilter_NodeNotInRegion
 *
 * 4 节点，shmRegion.nodelist=[node1,node2,node3]（node4 不在域中）。
 * RegionFilter 过滤掉 node4，LenderRoleFilter 过滤掉 node1（isLender=false），
 * 仅 node2/node3 候选，调度成功。
 *
 * 前置状态: 四节点已注册，节点1 isLender=false，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{shmRegion:["1","2","3"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. CreateNodeMap(4)：创建四节点，node1 isLender=false，full-mesh peer，注册并 Mock GetAllNodes
 *   2. 构造 SHARE export obj（shmRegion=["1","2","3"]，state=SCHEDULING）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId 为 "2" 或 "3"（node4 被 RegionFilter 过滤，node1 被 LenderRoleFilter 过滤）
 *   3. importNumaInfos 为空
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow_RegionFilter_NodeNotInRegion)
{
    auto nodeMap = CreateNodeMap(4);
    nodeMap["1"].isLender = false;
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["4"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-region-filter-1";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2", "3"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    auto exportNodeId = obj.algoResult.exportNumaInfos[0].nodeId;
    EXPECT_TRUE(exportNodeId == "2" || exportNodeId == "3")
        << "export nodeId=" << exportNodeId << " should be 2 or 3 (not 4)";
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

// ==================== RegionFilter 过滤 ====================

// ==================== P2: SHM Region Empty Nodelist ====================

/**
 * @brief 用例 14: ShmRegionEmptyNodelist
 *
 * shmRegion.nodelist 为空，RegionFilter 移除全部节点，调度失败。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{name:"share-empty-region", shmRegion:{nodelist:[]}, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 构造 SHARE export obj（nodelist 为空，state=SCHEDULING）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK（RegionFilter 在空 region 下移除全部节点）
 */
TEST_F(TestSchedulerEndToEnd, ShmRegionEmptyNodelist)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-empty-region";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    // nodelist 为空
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

// ==================== LenderInfo (ShareBorrowWithLenderInfo) ====================

/**
 * @brief 用例 15: ShareBorrowWithLenderInfo_NodeOnly
 *
 * lenderInfo 仅指定 nodeId + lender_size（socketId/numaId=UINT32_MAX），
 * SpecifiedLenderFilter 只过滤 node，由 Score + SelectNumaByFreeMemory 自由选择 socket/numa。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{lenderInfo:{nodeId:"2", lender_size:256MB}, shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 构造 SHARE export obj（lenderInfo.nodeId="2"，lender_size=256MB，shmRegion=["1","2"]）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 *   3. attachSocketId > 0
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowWithLenderInfo_NodeOnly)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-lender-nodeonly";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.req.lenderInfo.nodeId = "2";
    obj.req.lenderInfo.lender_size = BYTE_256MB;
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_GT(obj.algoResult.attachSocketId, 0u);
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

/**
 * @brief 用例 16: ShareBorrowWithLenderInfo_SocketOnly
 *
 * lenderInfo 指定 nodeId + socketId（numaId=UINT32_MAX），
 * SpecifiedLenderFilter 过滤到 node2/socket36，由 Score + SelectNumaByFreeMemory 选取 NUMA。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{lenderInfo:{nodeId:"2", socketId:36, lender_size:256MB}, shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 构造 SHARE export obj（lenderInfo.nodeId="2"，socketId=36，lender_size=256MB）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 *   3. attachSocketId == 36
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowWithLenderInfo_SocketOnly)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-lender-socketonly";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.req.lenderInfo.nodeId = "2";
    obj.req.lenderInfo.socketId = 36;
    obj.req.lenderInfo.lender_size = BYTE_256MB;
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.attachSocketId, 36);
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

/**
 * @brief 用例 17: ShareBorrowWithLenderInfo_NumaOnly
 *
 * lenderInfo 指定 nodeId + numaId（socketId=UINT32_MAX），
 * SpecifiedLenderFilter 通过 GetSocketIdByNumaId 解析出 socketId，精确 pin。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{lenderInfo:{nodeId:"2", numaId:0, lender_size:256MB}, shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 构造 SHARE export obj（lenderInfo.nodeId="2"，numaId=0，lender_size=256MB）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 *   3. exportNumaInfos[0].numaId == 0
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowWithLenderInfo_NumaOnly)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-lender-numaonly";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.req.lenderInfo.nodeId = "2";
    obj.req.lenderInfo.numaId = 0;
    obj.req.lenderInfo.lender_size = BYTE_256MB;
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].numaId, 0);
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

/**
 * @brief 用例 18: ShareBorrowWithLenderInfo_Pinned
 *
 * lenderInfo 指定完整 nodeId + socketId + numaId + lender_size，SpecifiedLenderFilter 精确 pin。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{lenderInfo:{nodeId:"2", socketId:36, numaId:0, lender_size:256MB}, shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 构造 SHARE export obj（完整 lenderInfo，shmRegion=["1","2"]）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 *   3. exportNumaInfos[0].numaId == 0
 *   4. exportNumaInfos[0].size == BYTE_256MB
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowWithLenderInfo_Pinned)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-lender-pinned";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.req.lenderInfo.nodeId = "2";
    obj.req.lenderInfo.socketId = 36;
    obj.req.lenderInfo.numaId = 0;
    obj.req.lenderInfo.lender_size = BYTE_256MB;
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].numaId, 0);
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].size, BYTE_256MB);
    EXPECT_TRUE(obj.algoResult.importNumaInfos.empty());
}

/**
 * @brief 用例 19: ShareBorrowWithLenderInfo_NumaOnly_NotFound
 *
 * lenderInfo 指定不存在的 NUMA id（numaId=99），SpecifiedLenderFilter 过滤后无候选。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{lenderInfo:{nodeId:"2", numaId:99, lender_size:256MB}, shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 构造 SHARE export obj（lenderInfo.numaId=99，shmRegion=["1","2"]）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK（SpecifiedLenderFilter 无匹配 NUMA）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowWithLenderInfo_NumaOnly_NotFound)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-lender-numa-notfound";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.req.lenderInfo.nodeId = "2";
    obj.req.lenderInfo.numaId = 99;
    obj.req.lenderInfo.lender_size = BYTE_256MB;
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 20: ShareBorrowWithLenderInfo_NodeNotFound
 *
 * lenderInfo 指定不存在的节点（nodeId="3"），SpecifiedLenderFilter 过滤后无候选。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{lenderInfo:{nodeId:"3", lender_size:256MB}, shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 构造 SHARE export obj（lenderInfo.nodeId="3"，shmRegion=["1","2"]）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK（SpecifiedLenderFilter 找不到 node3）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowWithLenderInfo_NodeNotFound)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-lender-nodenotfound";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.req.lenderInfo.nodeId = "3";
    obj.req.lenderInfo.lender_size = BYTE_256MB;
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 21: ShareBorrowWithLenderInfo_InsufficientMem
 *
 * lenderInfo 指定完整位置，但该 NUMA 空闲 hugepage 为 0，SpecifiedLenderFilter 拒绝。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，allocator=HUGETLB_PMD，blockSize=64，
 *           nr_hugepages_2M=0，free_hugepages_2M=0，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{lenderInfo:{nodeId:"2", socketId:36, numaId:0, lender_size:256MB}, shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 覆盖 allocator 为 HUGETLB_PMD，空闲大页设为 0
 *   3. ClearCache + 重建注册节点，Mock GetAllNodes
 *   4. 构造 SHARE export obj（完整 lenderInfo，shmRegion=["1","2"]）
 *   5. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK（SpecifiedLenderFilter 拒绝空闲大页为 0 的 NUMA）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowWithLenderInfo_InsufficientMem)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);
    // 覆盖为 HUGETLB_PMD, 空闲大页为 0
    for (auto& [_, node] : nodeMap) {
        node.allocator = UbseAllocator::HUGETLB_PMD;
        node.blockSize = 64;
        for (auto& [loc, numa] : node.numaInfos) {
            (void)loc;
            numa.nr_hugepages_2M = 0;
            numa.free_hugepages_2M = 0;
        }
    }
    SetHugeNumaMemPoolZero(nodeMap);
    SchedulerImpl::GetInstance().ClearCache();
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-lender-insufficient";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.req.lenderInfo.nodeId = "2";
    obj.req.lenderInfo.socketId = 36;
    obj.req.lenderInfo.numaId = 0;
    obj.req.lenderInfo.lender_size = BYTE_256MB;
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 22: ShareBorrowWithLenderInfo_NumaNotUnderSocket
 *
 * lenderInfo 指定 socketId=36，numaId=3，但 NUMA3 属于 socket276，
 * SpecifiedLenderFilter 保留 socket36，FilterByNumaIds 移除 NUMA3（不在 socket36 下），全部候选被过滤。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入: UbseMemShareBorrowExportObj{lenderInfo:{nodeId:"2", socketId:36, numaId:3, lender_size:256MB}, shmRegion:["1","2"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeShareEnv：注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 构造 SHARE export obj（lenderInfo.socketId=36，numaId=3，shmRegion=["1","2"]）
 *   3. MemoryObjChangeHandler(obj)：执行调度决策
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK（NUMA3 不在 socket36 下，FilterByNumaIds 移除全部候选）
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowWithLenderInfo_NumaNotUnderSocket)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);

    adapter_plugins::mmi::UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-lender-numa-wrong-socket";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_256MB;
    for (const auto& nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo ni{};
        ni.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(ni);
    }
    obj.req.lenderInfo.nodeId = "2";
    obj.req.lenderInfo.socketId = 36;
    obj.req.lenderInfo.numaId = 3;
    obj.req.lenderInfo.lender_size = BYTE_256MB;
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

// ==================== 生命周期与状态 ====================

/**
 * @brief 用例 23: ShareBorrow_StateFailed
 *
 * SCHEDULING → AddNumaShare（memShared 增加）→ STATE_FAILED → SubNumaShare（memShared 归零）。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入:
 *   步骤1: UbseMemShareBorrowExportObj{state:SCHEDULING}
 *   步骤2: 同一 obj{state:STATE_FAILED}
 *
 * 操作步骤:
 *   1. MemoryObjChangeHandler(obj) with SCHEDULING → AddNumaShare
 *   2. 验证 export 节点 NUMA memShared == exportSize
 *   3. obj.status.state = STATE_FAILED → MemoryObjChangeHandler → SubNumaShare
 *   4. 验证 export 节点 NUMA memShared == 0
 *
 * 预期输出:
 *   1. 步骤1 返回值 == UBSE_OK
 *   2. 步骤3 返回值 == UBSE_OK
 *   3. 步骤4: memShared == 0
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow_StateFailed)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeShareObj("share-failed", BYTE_256MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto exportSize = obj.algoResult.exportNumaInfos[0].size;
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), exportSize);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_STATE_FAILED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), 0u);
}

/**
 * @brief 用例 24: ShareBorrow_CatchUpImportAfterExport
 *
 * SCHEDULING → EXPORT_DESTROYED（SubNumaShare，ONLY_IMPORT_EXIST）
 * → EXPORT_SUCCESS（AddNumaShare，IMPORT_EXPORT_EXIST），
 * 验证 export 销毁后可重新恢复。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入:
 *   步骤1: UbseMemShareBorrowExportObj{state:SCHEDULING}
 *   步骤2: 同一 obj{state:EXPORT_DESTROYED}
 *   步骤3: 同一 obj{state:EXPORT_SUCCESS}
 *
 * 操作步骤:
 *   1. MemoryObjChangeHandler(obj) with SCHEDULING → AddNumaShare（memShared = exportSize）
 *   2. obj.status.state = EXPORT_DESTROYED → MemoryObjChangeHandler → SubNumaShare（memShared = 0）
 *   3. obj.status.state = EXPORT_SUCCESS → MemoryObjChangeHandler → AddNumaShare（memShared = exportSize）
 *   4. 验证 export 节点 NUMA memShared == exportSize
 *
 * 预期输出:
 *   1. 步骤1 返回值 == UBSE_OK
 *   2. 步骤2 返回值 == UBSE_OK
 *   3. 步骤3 返回值 == UBSE_OK
 *   4. 步骤4: memShared == exportSize
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow_CatchUpImportAfterExport)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeShareObj("share-catchup-export", BYTE_256MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto exportSize = obj.algoResult.exportNumaInfos[0].size;
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), exportSize);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), 0u);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), exportSize);
}

/**
 * @brief 用例 25: ShareBorrow_ExportFirstThenImportDestroyed
 *
 * SCHEDULING → EXPORT_DESTROYED → IMPORT_DESTROYED，
 * 验证逆序销毁路径：先 export 销毁，后 import 销毁。
 * SHARE 的 IMPORT_DESTROYED 为 no-op（无 SubNumaBorrow）。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入:
 *   步骤1: UbseMemShareBorrowExportObj{state:SCHEDULING}
 *   步骤2: 同一 obj{state:EXPORT_DESTROYED}
 *   步骤3: 同一 obj{state:IMPORT_DESTROYED}
 *
 * 操作步骤:
 *   1. MemoryObjChangeHandler(obj) with SCHEDULING → AddNumaShare（memShared = exportSize）
 *   2. obj.status.state = EXPORT_DESTROYED → MemoryObjChangeHandler → SubNumaShare（memShared = 0）
 *   3. obj.status.state = IMPORT_DESTROYED → MemoryObjChangeHandler（SHARE 的 IMPORT_DESTROYED 为 no-op）
 *   4. 验证 export 节点 NUMA memShared == 0
 *
 * 预期输出:
 *   1. 步骤1 返回值 == UBSE_OK
 *   2. 步骤2 返回值 == UBSE_OK
 *   3. 步骤3 返回值 == UBSE_OK
 *   4. 步骤4: memShared == 0
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrow_ExportFirstThenImportDestroyed)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeShareObj("share-export-first-destroy", BYTE_256MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto exportSize = obj.algoResult.exportNumaInfos[0].size;
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), exportSize);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), 0u);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), 0u);
}

// ==================== 生命周期与状态 ====================

/**
 * @brief 用例 26: ShareBorrowExportDestroyed
 *
 * SHARE 借用完整销毁路径：SCHEDULING → AddNumaShare（memShared 增加）
 * → EXPORT_DESTROYED → SubNumaShare（memShared 归零）。
 *
 * 前置状态: 两节点已注册，节点1 非 lender，全互联链路
 *
 * 测试输入:
 *   步骤1: UbseMemShareBorrowExportObj{state:SCHEDULING}
 *   步骤2: 同一 obj{state:EXPORT_DESTROYED}
 *
 * 操作步骤:
 *   1. MemoryObjChangeHandler(obj) with SCHEDULING → AddNumaShare
 *   2. 验证 export 节点 NUMA memShared == exportSize
 *   3. obj.status.state = EXPORT_DESTROYED → MemoryObjChangeHandler → SubNumaShare
 *   4. 验证 export 节点 NUMA memShared == 0
 *
 * 预期输出:
 *   1. 步骤1 返回值 == UBSE_OK
 *   2. 步骤3 返回值 == UBSE_OK
 *   3. 步骤4: memShared == 0
 */
TEST_F(TestSchedulerEndToEnd, ShareBorrowExportDestroyed)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeShareEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeShareObj("share-destroy-1", BYTE_256MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto exportSize = obj.algoResult.exportNumaInfos[0].size;
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), exportSize);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemSharedSize(), 0u);
}

} // namespace ubse::mem::scheduler::ut
