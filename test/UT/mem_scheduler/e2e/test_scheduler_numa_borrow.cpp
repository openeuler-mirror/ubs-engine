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

namespace ubse::mem::scheduler::ut {
namespace {

using namespace ubse::common::def;

UbseMemNumaBorrowImportObj MakeNumaObj(const std::string& name, uint64_t size, int srcSocket = 36,
                                       uint32_t highWatermark = 100)
{
    adapter_plugins::mmi::UbseMemNumaBorrowImportObj obj{};
    obj.req.name = name;
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.size = size;
    obj.req.srcSocket = srcSocket;
    obj.req.highWatermark = highWatermark;
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
    return obj;
}

void SetupTwoNodeEnv(std::unordered_map<std::string, UbseNodeInfo>& nodeMap)
{
    nodeMap = CreateNodeMap(2);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);
}

void SetupTwoNodeNuma1DFullmesh(std::unordered_map<std::string, UbseNodeInfo>& nodeMap, bool socket36Up,
                                bool socket276Up)
{
    nodeMap = CreateNodeMap(2);
    if (socket36Up) {
        AddPeerRelation(nodeMap["2"], "1", 0, 0);
        AddPeerRelation(nodeMap["1"], "2", 0, 0);
    }
    if (socket276Up) {
        AddPeerRelation(nodeMap["2"], "1", 1, 1);
        AddPeerRelation(nodeMap["1"], "2", 1, 1);
    }
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);
}

} // namespace

/**
 * @brief 用例 1: NumaBorrowWithHighWatermark
 *
 * MemoryObjChangeHandler SCHEDULING 调度决策。
 * srcSocket=36, highWatermark=80, NumaPlaneFilter + LinkPortDownFilter 均通过。
 * highWatermark=80（保留 20% 水位线），2GB free × 80% = 1.6GB > 128MB，调度成功。
 *
 * 前置状态: 两节点已注册, 全互联链路, 节点2空闲内存充足
 *
 * 测试输入: UbseMemNumaBorrowImportObj{srcSocket:36, highWatermark:80, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. 注册两节点, full-mesh peer, Mock GetAllNodes
 *   2. 构造 NUMA import obj(srcSocket=36, highWatermark=80, state:SCHEDULING)
 *   3. 调用 MemoryObjChangeHandler(obj)
 *      - ScheduleBorrow → Filter 链通过 → Score 决策 → FillAlgoResult
 *      - UpdateSchedulerAccount → UpdateByBothNotExist → AddNumaLend + AddNumaBorrow
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos[0].nodeId == "2"（仅从节点2借出）
 *   3. algoResult.exportNumaInfos[0].socketId == 36（出借 socket 为 srcSocket 匹配的 36）
 *   4. algoResult.importNumaInfos 非空
 *   5. 节点2对应出借 NUMA 的 GetMemLentSize() == 128MB
 *   6. 节点1对应 NUMA 的 GetMemBorrowSize() == 128MB
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowWithHighWatermark)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeNumaObj("numa-borrow-1", BYTE_128MB, 36, 80);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    ASSERT_FALSE(obj.algoResult.importNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].socketId, 36);
    EXPECT_EQ(obj.algoResult.importNumaInfos[0].nodeId, "1");

    auto exportSize = obj.algoResult.exportNumaInfos[0].size;
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemLentSize(), exportSize);

    auto importSize = obj.algoResult.importNumaInfos[0].size;
    auto importSocket =
        impl.nodeInfo_->GetSocketInfo("1", static_cast<SocketId>(obj.algoResult.importNumaInfos[0].socketId));
    ASSERT_NE(importSocket, nullptr);
    auto* importNuma = importSocket->GetAllNumaInfos().begin()->second.get();
    EXPECT_EQ(importNuma->GetMemBorrowSize(), importSize);
}

/**
 * @brief 用例 2: NumaBorrowMaxTimes
 *
 * NUMA 借用次数超限：requestSize=1024MB, blockSize=128, srcSocket=36。
 * 每次消耗 8 blocks，单个 socket 最多 1024/8=128 次。
 * 129 次时 LendTimesFilter 拒绝。
 *
 * 前置状态: 两节点已注册（1, 2），blockSize=128。全互联链路，两节点同组。
 *           节点2 socket36 下各 NUMA freeSize 设置 128GB。
 *
 * 测试输入:
 *   numaImportObj[i]: {req: {name:"numa-max-i", requestNodeId:"1", importNodeId:"1", size:1024MB, srcSocket:36, highWatermark:100}, status: SCHEDULING}
 *   // i = 1..129
 *
 * 操作步骤:
 *   1. 注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 将节点2各 NUMA 的 freeSize 设为 128GB（KB 单位）
 *   3. 循环 128 次：构造不同 name 的 NUMA import obj，调用 MemoryObjChangeHandler
 *   4. 第 129 次借用
 *
 * 预期输出:
 *   1. 前 128 次借用返回 == UBSE_OK
 *   2. 第 129 次借用返回 != UBSE_OK（LendTimesFilter 过滤）
 *   3. 第 128 次后 socket36 的 GetLentCount() == 1024
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowMaxTimes)
{
    auto nodeMap = CreateNodeMap(2);
    AddFullMeshPeers(nodeMap);

    constexpr uint64_t HUGE_KB = 128ULL * 1024 * 1024;
    for (auto& [loc, numa] : nodeMap["2"].numaInfos) {
        (void)loc;
        numa.size = HUGE_KB;
        numa.freeSize = HUGE_KB;
    }

    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto& impl = SchedulerImpl::GetInstance();
    auto socket = impl.nodeInfo_->GetSocketInfo("2", 36);
    ASSERT_NE(socket, nullptr);

    for (int i = 1; i <= 128; i++) {
        auto obj = MakeNumaObj("numa-max-" + std::to_string(i), BYTE_1GB);
        ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK) << "Failed at iteration " << i;
    }
    EXPECT_EQ(socket->GetLentCount(), 1024u);

    auto obj129 = MakeNumaObj("numa-max-129", BYTE_1GB);
    ASSERT_NE(impl.MemoryObjChangeHandler(obj129), UBSE_OK);
}

// ==================== NUMA HugePage Variants ====================

/**
 * @brief 用例 3: NumaBorrowFullLifecycle
 *
 * NUMA 借用完整生命周期：SCHEDULING → AddNumaBorrow + AddNumaLend,
 * IMPORT_DESTROYED → SubNumaBorrow (memBorrow 归零),
 * EXPORT_DESTROYED → SubNumaLend (memLent 归零).
 *
 * 前置状态: 两节点已注册，全互联链路
 *
 * 测试输入:
 *   步骤1: UbseMemNumaBorrowImportObj{state: SCHEDULING}
 *   步骤2: 同一 obj{state: IMPORT_DESTROYED}
 *   步骤3: 同一 obj{state: EXPORT_DESTROYED}
 *
 * 操作步骤:
 *   1. 调用 MemoryObjChangeHandler(obj) with SCHEDULING → AddNumaBorrow + AddNumaLend
 *   2. 验证 import 节点 NUMA memBorrow == importSize, export 节点 memLent == exportSize
 *   3. obj.status.state = IMPORT_DESTROYED → MemoryObjChangeHandler → SubNumaBorrow
 *   4. 验证 import 节点 NUMA memBorrow == 0
 *   5. obj.status.state = EXPORT_DESTROYED → MemoryObjChangeHandler → SubNumaLend
 *   6. 验证 export 节点 NUMA memLent == 0
 *
 * 预期输出:
 *   1. 步骤1/3/5 返回值 == UBSE_OK
 *   2. 步骤4: memBorrow == 0
 *   3. 步骤6: memLent == 0
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowFullLifecycle)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeNumaObj("numa-full-lifecycle", BYTE_128MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto importSize = obj.algoResult.importNumaInfos[0].size;
    auto importSocket =
        impl.nodeInfo_->GetSocketInfo("1", static_cast<SocketId>(obj.algoResult.importNumaInfos[0].socketId));
    ASSERT_NE(importSocket, nullptr);
    auto* importNuma = importSocket->GetAllNumaInfos().begin()->second.get();
    EXPECT_EQ(importNuma->GetMemBorrowSize(), importSize);

    auto exportSize = obj.algoResult.exportNumaInfos[0].size;
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemLentSize(), exportSize);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), 0u);
    EXPECT_GT(exportNuma->GetMemLentSize(), 0u);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemLentSize(), 0u);
}

/**
 * @brief 用例 4: NumaBorrowInHugetlbPmd
 *
 * NUMA + HUGETLB_PMD, 验证 2M 大页计算路径。
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowInHugetlbPmd)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PMD, 64);
    SetHugeNumaMemPoolZero(nodeMap);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeNumaObj("numa-hugepmd", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");

    auto& impl = SchedulerImpl::GetInstance();
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemTotalSize(), 0u);
}

/**
 * @brief 用例 5: NumaBorrowInHugetlbPud
 *
 * NUMA + HUGETLB_PUD, 验证 1G 大页计算路径。
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowInHugetlbPud)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PUD, 1024);
    SetHugeNumaMemPoolZero(nodeMap);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeNumaObj("numa-hugepud", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");

    auto& impl = SchedulerImpl::GetInstance();
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemTotalSize(), 0u);
}

/**
 * @brief 用例 6: NumaBorrowInHugetlbPmd64K
 *
 * NUMA + HUGETLB_PMD + 64K 页, 验证 512M 大页计算路径。
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowInHugetlbPmd64K)
{
    SetPageSize("65536");

    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PMD, 64);
    SetHugeNumaMemPoolZero(nodeMap);
    for (auto& [_, node] : nodeMap) {
        for (auto& [loc, numa] : node.numaInfos) {
            (void)loc;
            numa.nr_hugepages_512M = 32;
            numa.free_hugepages_512M = 16;
        }
    }
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeNumaObj("numa-hugepmd-64k", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");

    auto& impl = SchedulerImpl::GetInstance();
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemTotalSize(), 0u);
}

// ==================== NUMA + 1Dfullmesh (LinkPortDownFilter) ====================

/**
 * @brief 用例 7: NumaBorrow1DFullmesh_PeerSocketDown
 *
 * 1Dfullmesh 下 node2 socket276 可达，srcSocket=276，NumaPlaneFilter + LinkPortDownFilter 均通过，从 socket276 出借。
 *
 * 前置状态: 两节点已注册, 1Dfullmesh 链路, socket36 down, socket276 up
 *
 * 测试输入: UbseMemNumaBorrowImportObj{srcSocket:276, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeNuma1DFullmesh(socket36=false, socket276=true)
 *   2. 构造 NUMA import obj(srcSocket=276), 调用 MemoryObjChangeHandler
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].socketId == 276
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrow1DFullmesh_PeerSocketDown)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeNuma1DFullmesh(nodeMap, false, true);

    auto obj = MakeNumaObj("numa-1dfull-sock276up", BYTE_128MB, 276);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].socketId, 276);
}

/**
 * @brief 用例 8: NumaBorrow1DFullmesh_SrcSocketUnreachable
 *
 * srcSocket=36 但 socket36 不可达。NumaPlaneFilter 先限为 socket36 平面，LinkPortDownFilter 过滤不可达 socket，候选为空。
 *
 * 前置状态: 两节点已注册, 1Dfullmesh 链路, socket36 down, socket276 up
 *
 * 测试输入: UbseMemNumaBorrowImportObj{srcSocket:36, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeNuma1DFullmesh(socket36=false, socket276=true)
 *   2. 构造 NUMA import obj(srcSocket=36), 调用 MemoryObjChangeHandler
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrow1DFullmesh_SrcSocketUnreachable)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeNuma1DFullmesh(nodeMap, false, true);

    auto obj = MakeNumaObj("numa-1dfull-src-unreachable", BYTE_128MB, 36);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 9: NumaBorrow1DFullmesh_MultiNodePartition
 *
 * 3 节点，node1 可达 node2 不可达 node3。LinkPortDownFilter 过滤 node3。
 *
 * 前置状态: 3 节点已注册（1~3）, 节点间以 1Dfullmesh 方式互联, node2↔node3 可达
 *
 * 测试输入: UbseMemNumaBorrowImportObj{srcSocket:36, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. CreateNodeMap(3), 添加 peer 关系使 node1↔node2 互联, node2↔node3 互联
 *   2. 注册全部 3 节点, Mock GetAllNodes
 *   3. 构造 NUMA import obj, 调用 MemoryObjChangeHandler
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrow1DFullmesh_MultiNodePartition)
{
    auto nodeMap = CreateNodeMap(3);
    AddPeerRelation(nodeMap["1"], "2", 0, 0);
    AddPeerRelation(nodeMap["1"], "2", 1, 1);
    AddPeerRelation(nodeMap["2"], "1", 0, 0);
    AddPeerRelation(nodeMap["2"], "1", 1, 1);
    AddPeerRelation(nodeMap["2"], "3", 0, 0);
    AddPeerRelation(nodeMap["2"], "3", 1, 1);
    AddPeerRelation(nodeMap["3"], "2", 0, 0);
    AddPeerRelation(nodeMap["3"], "2", 1, 1);

    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeNumaObj("numa-1dfull-partition", BYTE_128MB, 36);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

// ==================== NUMA without srcSocket ====================

/**
 * @brief 用例 10: NumaBorrowWithoutSrcSocket
 *
 * srcSocket=-1 时 NumaPlaneFilter 不加入 filter 链，所有 socket 均候选。
 *
 * 前置状态: 两节点已注册, 全互联链路
 *
 * 测试输入: UbseMemNumaBorrowImportObj{srcSocket:-1, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeEnv(nodeMap)
 *   2. 构造 NUMA import obj(srcSocket=-1), 调用 MemoryObjChangeHandler
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos 非空
 *   3. importNumaInfos 非空
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowWithoutSrcSocket)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    auto obj = MakeNumaObj("numa-no-src", BYTE_128MB, -1);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    ASSERT_FALSE(obj.algoResult.importNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.importNumaInfos[0].nodeId, "1");
}

// ==================== NUMA STATE_FAILED ====================

/**
 * @brief 用例 11: NumaBorrow_StateFailed
 *
 * SCHEDULING → STATE_FAILED，memLent + memBorrow 全部归还。
 *
 * 前置状态: 两节点已注册, 全互联链路
 *
 * 测试输入:
 *   步骤1: UbseMemNumaBorrowImportObj{state: SCHEDULING}
 *   步骤2: 同一 obj{state: STATE_FAILED}
 *
 * 操作步骤:
 *   1. 调用 MemoryObjChangeHandler(obj) with SCHEDULING → AddNumaBorrow + AddNumaLend
 *   2. 验证 memLent > 0, memBorrow > 0
 *   3. obj.status.state = STATE_FAILED → MemoryObjChangeHandler → 全部归还
 *   4. 验证 memLent == 0, memBorrow == 0
 *
 * 预期输出:
 *   1. 步骤1 返回 UBSE_OK
 *   2. 步骤3 返回 UBSE_OK
 *   3. memLent == 0 && memBorrow == 0
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrow_StateFailed)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeNumaObj("numa-failed", BYTE_128MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemLentSize(), 0u);

    auto importSocket =
        impl.nodeInfo_->GetSocketInfo("1", static_cast<SocketId>(obj.algoResult.importNumaInfos[0].socketId));
    ASSERT_NE(importSocket, nullptr);
    auto* importNuma = importSocket->GetAllNumaInfos().begin()->second.get();
    EXPECT_GT(importNuma->GetMemBorrowSize(), 0u);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_STATE_FAILED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemLentSize(), 0u);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), 0u);
}

// ==================== NUMA + LinkInfoFilter ====================

/**
 * @brief 用例 12: NumaBorrowWithLinkInfo
 *
 * NUMA 借用指定 linkInfo.lenderNode="2" + lenderSocketId=36，LinkInfoFilter 过滤到 node2/socket36。
 *
 * 前置状态: 两节点已注册, 全互联链路
 *
 * 测试输入: UbseMemNumaBorrowImportObj{srcSocket:36, linkInfo:{lenderNode:"2", lenderSocketId:36}, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeEnv(nodeMap)
 *   2. 构造 NUMA import obj(linkInfo.lenderNode="2", lenderSocketId=36), 调用 MemoryObjChangeHandler
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].socketId == 36
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowWithLinkInfo)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeNumaObj("numa-linkinfo", BYTE_128MB, 36);
    obj.req.linkInfo.lenderNode = "2";
    obj.req.linkInfo.lenderSocketId = 36;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].socketId, 36);
}

/**
 * @brief 用例 13: NumaBorrowWithLinkInfo_WrongNode
 *
 * linkInfo.lenderNode="3" 不存在，LinkInfoFilter 过滤后无候选。
 *
 * 前置状态: 两节点已注册, 全互联链路
 *
 * 测试输入: UbseMemNumaBorrowImportObj{srcSocket:36, linkInfo:{lenderNode:"3", lenderSocketId:36}, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeEnv(nodeMap)
 *   2. 构造 NUMA import obj(linkInfo.lenderNode="3"), 调用 MemoryObjChangeHandler
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowWithLinkInfo_WrongNode)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeNumaObj("numa-linkinfo-wrong", BYTE_128MB, 36);
    obj.req.linkInfo.lenderNode = "3";
    obj.req.linkInfo.lenderSocketId = 36;
    ASSERT_NE(impl.MemoryObjChangeHandler(obj), UBSE_OK);
}

// ==================== NUMA + CandidateList ====================

/**
 * @brief 用例 14: NumaBorrowWithCandidateList
 *
 * 4 节点，candidateNodeList=["3"]，CandidateFilter 仅保留 node3。
 *
 * 前置状态: 4 节点已注册（1~4）, 全互联链路
 *
 * 测试输入: UbseMemNumaBorrowImportObj{candidateNodeList:["3"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. CreateNodeMap(4), AddFullMeshPeers
 *   2. 注册全部 4 节点, Mock GetAllNodes
 *   3. 构造 NUMA import obj(candidateNodeList=["3"]), 调用 MemoryObjChangeHandler
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "3"
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrowWithCandidateList)
{
    auto nodeMap = CreateNodeMap(4);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["4"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    adapter_plugins::mmi::UbseMemNumaBorrowImportObj obj{};
    obj.req.name = "numa-candidate-3";
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.size = BYTE_128MB;
    obj.req.srcSocket = 36;
    obj.req.candidateNodeList = {"3"};
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "3");
}

// ==================== P2: NUMA HighWatermark Zero ====================

/**
 * @brief 用例 15: NumaHighWatermarkZero
 *
 * highWatermark=0，全部内存保留作为水位，SocketFreeMemoryFilter 可借出为 0。
 *
 * 前置状态: 两节点已注册, 全互联链路
 *
 * 测试输入: UbseMemNumaBorrowImportObj{highWatermark:0, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupTwoNodeEnv(nodeMap)
 *   2. 构造 NUMA import obj(highWatermark=0), 调用 MemoryObjChangeHandler
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, NumaHighWatermarkZero)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    auto obj = MakeNumaObj("numa-hwm-0", BYTE_128MB, 36, 0);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 16: NumaBorrow_FaultNodeInCandidate
 *
 * 4 节点，candidateNodeList=["1","2","3"]，node2 为 FAULT。
 * NodeStateFilter 过滤 node2，BorrowedLenderFilter 过滤 importNode(node1)，
 * 仅剩 node3，从 node3 出借。
 *
 * 前置状态: 4 节点已注册（1~4）, 全互联链路, node2 clusterState = FAULT
 *
 * 测试输入: UbseMemNumaBorrowImportObj{candidateNodeList:["1","2","3"], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. CreateNodeMap(4), 设 node2 FAULT, AddFullMeshPeers
 *   2. 注册全部 4 节点, Mock GetAllNodes
 *   3. 构造 NUMA import obj(candidateNodeList=["1","2","3"]), 调用 MemoryObjChangeHandler
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "3"
 */
TEST_F(TestSchedulerEndToEnd, NumaBorrow_FaultNodeInCandidate)
{
    auto nodeMap = CreateNodeMap(4);
    nodeMap["2"].clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["4"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    adapter_plugins::mmi::UbseMemNumaBorrowImportObj obj{};
    obj.req.name = "numa-fault-candidate";
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.size = BYTE_128MB;
    obj.req.srcSocket = 36;
    obj.req.candidateNodeList = {"1", "2", "3"};
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;

    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "3");
}

} // namespace ubse::mem::scheduler::ut
