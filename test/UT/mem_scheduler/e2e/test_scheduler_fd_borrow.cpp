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

#include <random>

#include "test_scheduler_fixture.h"

namespace ubse::mem::scheduler::ut {
namespace {

using namespace ubse::common::def;

UbseMemFdBorrowImportObj MakeFdObj(const std::string& name, uint64_t size, std::vector<std::string> candidates = {})
{
    adapter_plugins::mmi::UbseMemFdBorrowImportObj obj{};
    obj.req.name = name;
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.size = size;
    obj.req.candidateNodeList = std::move(candidates);
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

void SetupTwoNodeFD1DFullmesh(std::unordered_map<std::string, UbseNodeInfo>& nodeMap, bool socket36Up, bool socket276Up)
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

// ==================== lenderLocs ====================

UbseMemFdBorrowImportObj MakeFdObjWithLenderLocs(const std::string& name, uint64_t size,
                                                 std::vector<adapter_plugins::mmi::UbseNumaLocation> lenderLocs,
                                                 std::vector<uint64_t> lenderSizes)
{
    adapter_plugins::mmi::UbseMemFdBorrowImportObj obj{};
    obj.req.name = name;
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.size = size;
    obj.req.lenderLocs = std::move(lenderLocs);
    obj.req.lenderSizes = std::move(lenderSizes);
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
    return obj;
}

UbseMemFdBorrowImportObj MakeFdObjWithLenderLocs(const std::string& name, uint64_t size,
                                                 const std::string& lenderNodeId, uint32_t lenderNumaId,
                                                 uint64_t lenderSize)
{
    adapter_plugins::mmi::UbseNumaLocation loc{};
    loc.nodeId = lenderNodeId;
    loc.numaId = lenderNumaId;
    return MakeFdObjWithLenderLocs(name, size, {loc}, {lenderSize});
}

/**
 * @brief 用例 1: Node1FdBorrowNode2Lend
 *
 * MemoryObjChangeHandler SCHEDULING 调度决策。
 * BorrowedLenderFilter 排除 importNode 后, 节点2 为唯一候选。
 *
 * 前置状态: 两节点已注册, 节点2 isLender=true, 全互联链路
 *
 * 测试输入: UbseMemFdBorrowImportObj{requestNodeId:"1", size:128MB, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. 注册两节点, full-mesh peer, Mock GetAllNodes
 *   2. 构造 FD import obj(state:SCHEDULING)
 *   3. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos[0].nodeId == "2"
 *   3. algoResult.importNumaInfos[0].nodeId == "1"
 */
TEST_F(TestSchedulerEndToEnd, Node1FdBorrowNode2Lend)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    auto obj = MakeFdObj("fd-borrow-1", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    ASSERT_FALSE(obj.algoResult.importNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.importNumaInfos[0].nodeId, "1");
}

/**
 * @brief 用例 2: FourNodeFdBorrowWithCandidateNodeList
 *
 * 测试四节点场景下，FD 借用指定 candidateNodeList={"2"}，CandidateFilter 只允许节点2通过。
 *
 * 前置状态:
 * - 四节点全部注册（"1"~"4"），均为 lender + WORKING
 * - 全互联链路
 *
 * 测试输入: UbseMemFdBorrowImportObj{req:{name:"fd-borrow-candidate", requestNodeId:"1", importNodeId:"1", size:128MB, candidateNodeList:["2"]}, status:{state:UBSE_MEM_SCHEDULING}}
 *
 * 操作步骤:
 *   1. Mock UbseNodeController::GetAllNodes 返回 4 节点
 *   2. 注册四节点，full-mesh peer
 *   3. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos 非空
 *   3. algoResult.exportNumaInfos[0].nodeId == "2"（仅从候选节点2借出）
 */
TEST_F(TestSchedulerEndToEnd, FourNodeFdBorrowWithCandidateNodeList)
{
    auto nodeMap = CreateNodeMap(4);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["4"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-borrow-candidate", BYTE_128MB, {"2"});
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

/**
 * @brief 用例 3: FdBorrow_WithLenderLocs_Success
 *
 * 指定 lenderLocs=[{nodeId:"2",numaId:0}], lenderSizes=[128MB]。LenderInfoFilter 过滤到 node2/numa0，按指定位置填充结果。
 *
 * 前置状态:
 * - 两节点已注册，全互联链路
 *
 * 测试输入: UbseMemFdBorrowImportObj{req:{name:"fd-lenderlocs-ok", requestNodeId:"1", importNodeId:"1", size:128MB, lenderLocs:[{nodeId:"2", socketId:36, numaId:0}], lenderSizes:[128MB]}, status:{state:UBSE_MEM_SCHEDULING}}
 *
 * 操作步骤:
 *   1. 注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. 构造 FD import obj（指定 lenderLocs）
 *   3. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos 非空
 *   3. algoResult.exportNumaInfos[0].nodeId == "2"
 *   4. algoResult.exportNumaInfos[0].numaId == 0
 *   5. algoResult.exportNumaInfos[0].size == 128MB
 */
TEST_F(TestSchedulerEndToEnd, FdBorrow_WithLenderLocs_Success)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    auto obj = MakeFdObjWithLenderLocs("fd-lenderlocs-ok", BYTE_128MB, "2", 0, BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].numaId, 0);
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].size, BYTE_128MB)
        << "exportSize=" << obj.algoResult.exportNumaInfos[0].size;
}

/**
 * @brief 用例 4: FdBorrow_WithLenderLocs_WrongNode
 *
 * 指定 lenderLocs=[{nodeId:"3",numaId:0}]，节点3不存在。LenderInfoFilter 移除所有候选 → 失败。
 *
 * 前置状态:
 * - 两节点已注册，全互联链路
 *
 * 测试输入: UbseMemFdBorrowImportObj{req:{name:"fd-lenderlocs-wrongnode", size:128MB, lenderLocs:[{nodeId:"3", socketId:36, numaId:0}], lenderSizes:[128MB]}, status:{state:UBSE_MEM_SCHEDULING}}
 *
 * 操作步骤:
 *   1. 注册两节点，Mock GetAllNodes
 *   2. 指定不存在的节点3 构造 lenderLocs
 *   3. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, FdBorrow_WithLenderLocs_WrongNode)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    auto obj = MakeFdObjWithLenderLocs("fd-lenderlocs-wrongnode", BYTE_128MB, "3", 0, BYTE_128MB);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 5: FdBorrow_WithLenderLocs_InsufficientMem
 *
 * 指定 lenderSizes 大于该 NUMA 空闲内存。HUGETLB_PMD 下大页空闲数归零，lender_size 超出时报错。
 *
 * 前置状态:
 * - 两节点已注册，allocator=HUGETLB_PMD, blockSize=64
 * - NUMA 空闲 hugepage 数为 0
 *
 * 测试输入: UbseMemFdBorrowImportObj{req:{name:"fd-lenderlocs-nomem", size:1MB, lenderLocs:[{nodeId:"2", socketId:36, numaId:0}], lenderSizes:[1MB]}, status:{state:UBSE_MEM_SCHEDULING}}
 *
 * 操作步骤:
 *   1. 构造两节点（HUGETLB_PMD），空闲 hugepage 设为 0
 *   2. 注册节点，Mock GetAllNodes
 *   3. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, FdBorrow_WithLenderLocs_InsufficientMem)
{
    // 使用 hugepage 已耗尽的场景使 NUMA 空闲为 0
    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PMD, 64);
    AddFullMeshPeers(nodeMap);
    SetHugeNumaMemPoolZero(nodeMap);
    // 将 hugepage 空闲设置为 0
    for (auto& [_, node] : nodeMap) {
        for (auto& [loc, numa] : node.numaInfos) {
            (void)loc;
            numa.free_hugepages_2M = 0;
        }
    }
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    // 请求借出，lender_size 大于 NUMA 空闲（0）
    auto& impl = SchedulerImpl::GetInstance();
    auto obj = MakeFdObjWithLenderLocs("fd-lenderlocs-nomem", BYTE_1MB, "2", 0, BYTE_1MB);
    auto ret = impl.MemoryObjChangeHandler(obj);
    ASSERT_NE(ret, UBSE_OK) << "ret=" << ret << " expected error but got UBSE_OK";
}

// ==================== SocketFreeMemory / GetAvailableLendSize ====================

/**
 * @brief 用例 7: FdBorrowInHugetlbPmd
 *
 * HugeTLB PMD 场景 FD 借用：allocator=HUGETLB_PMD, blockSize=64，验证 HandleHugeTlbPmd 使用 2M 大页计算。
 *
 * 前置状态:
 * - 两节点已注册，allocator=HUGETLB_PMD, blockSize=64
 * - 全互联链路
 *
 * 测试输入: UbseMemFdBorrowImportObj{req:{name:"fd-hugepmd", requestNodeId:"1", importNodeId:"1", size:128MB}, status:{state:UBSE_MEM_SCHEDULING}}
 *
 * 操作步骤:
 *   1. Mock 节点（HUGETLB_PMD），全互联链路
 *   2. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos 非空
 *   3. algoResult.exportNumaInfos[0].nodeId == "2"
 *   4. export NUMA 的 GetMemTotalSize() > 0 && GetMemFreeSize() > 0
 */
TEST_F(TestSchedulerEndToEnd, FdBorrowInHugetlbPmd)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PMD, 64);
    SetHugeNumaMemPoolZero(nodeMap);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-hugepmd", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");

    auto& impl = SchedulerImpl::GetInstance();
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemTotalSize(), 0u);
    EXPECT_GT(exportNuma->GetMemFreeSize(), 0u);
}

/**
 * @brief 用例 8: FdBorrowInHugetlbPud
 *
 * HugeTLB PUD 场景 FD 借用：allocator=HUGETLB_PUD, blockSize=1024，验证 HandleHugeTlbPud 使用 1G 大页计算。
 *
 * 前置状态:
 * - 两节点已注册，allocator=HUGETLB_PUD, blockSize=1024
 * - 全互联链路
 *
 * 测试输入: UbseMemFdBorrowImportObj{req:{name:"fd-hugepud", requestNodeId:"1", importNodeId:"1", size:128MB}, status:{state:UBSE_MEM_SCHEDULING}}
 *
 * 操作步骤:
 *   1. Mock 节点（HUGETLB_PUD），全互联链路
 *   2. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos 非空
 *   3. algoResult.exportNumaInfos[0].nodeId == "2"
 *   4. export NUMA 的 GetMemTotalSize() > 0 && GetMemFreeSize() > 0
 */
TEST_F(TestSchedulerEndToEnd, FdBorrowInHugetlbPud)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PUD, 1024);
    SetHugeNumaMemPoolZero(nodeMap);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-hugepud", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");

    auto& impl = SchedulerImpl::GetInstance();
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemTotalSize(), 0u);
    EXPECT_GT(exportNuma->GetMemFreeSize(), 0u);
}

/**
 * @brief 用例 9: FdBorrowInHugetlbPmd64K
 *
 * HugeTLB PMD + 64K 页场景 FD 借用：验证 HandleHugeTlbPmd 使用 512M 大页计算。
 *
 * 前置状态:
 * - 两节点已注册，allocator=HUGETLB_PMD, blockSize=64
 * - 系统页大小 64K，nr_hugepages_512M=32, free_hugepages_512M=16
 * - 全互联链路
 *
 * 测试输入: UbseMemFdBorrowImportObj{req:{name:"fd-hugepmd-64k", requestNodeId:"1", importNodeId:"1", size:128MB}, status:{state:UBSE_MEM_SCHEDULING}}
 *
 * 操作步骤:
 *   1. 设置页大小 64K
 *   2. Mock 节点（HUGETLB_PMD），设置 512M 大页数据
 *   3. 全互联链路，注册节点
 *   4. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos 非空
 *   3. algoResult.exportNumaInfos[0].nodeId == "2"
 */
TEST_F(TestSchedulerEndToEnd, FdBorrowInHugetlbPmd64K)
{
    SetPageSize("65536");

    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PMD, 64);
    SetHugeNumaMemPoolZero(nodeMap);
    // 设置 512M 大页数据用于 64K 页环境计算
    for (auto& [_, node] : nodeMap) {
        for (auto& [loc, numa] : node.numaInfos) {
            numa.nr_hugepages_512M = 32;
            numa.free_hugepages_512M = 16;
        }
    }
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-hugepmd-64k", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");

    auto& impl = SchedulerImpl::GetInstance();
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemTotalSize(), 0u);
    EXPECT_GT(exportNuma->GetMemFreeSize(), 0u);
}

// ==================== Radius Filter ====================

/**
 * @brief 用例 11: RadiusBorrow_ZeroBlocksAll
 *
 * radius.borrow=0，任一新借用均被 RadiusBorrowFilter 过滤。
 *
 * 前置状态:
 * - 两节点已注册，全互联链路
 * - radius.borrow=0
 *
 * 操作步骤:
 *   1. 设置 Radius 配置 borrow=0
 *   2. 注册两节点
 *   3. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, RadiusBorrow_ZeroBlocksAll)
{
    SetupRadiusConfig("0", "");
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    auto obj = MakeFdObj("radius-borrow-zero", BYTE_128MB);
    // 未借用过 node2，当前半径 0 >= 0 → 被 RadiusBorrowFilter 过滤
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 12: RadiusLender_ZeroBlocksAll
 *
 * radius.lender=0，借入方未超限但出借方当前半径 0 >= 0 被 RadiusLenderFilter 过滤。
 *
 * 前置状态:
 * - 两节点已注册，全互联链路
 * - radius.lender=0
 *
 * 操作步骤:
 *   1. 设置 Radius 配置 lender=0
 *   2. 注册两节点
 *   3. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, RadiusLender_ZeroBlocksAll)
{
    SetupRadiusConfig("", "0");
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    auto obj = MakeFdObj("radius-lender-zero", BYTE_128MB);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 13: RadiusBorrow_WithCandidates_AllFiltered
 *
 * radius.borrow=1，先借入 node2（currentBorrowRadius=1），新请求 candidates=["3"] 全被 RadiusBorrowFilter 过滤后失败；归还后重新借用成功。
 *
 * 前置状态:
 * - 三节点已注册，全互联链路
 * - radius.borrow=1, radius.lender=1
 *
 * 操作步骤:
 *   1. 设置 Radius 配置
 *   2. FD candidates=["2"] 借入 node2 → 成功
 *   3. FD candidates=["3"] → node3 被 RadiusBorrowFilter 过滤 → 失败
 *   4. 将 obj1 状态设为 IMPORT_DESTROYED → 归还，释放 borrow radius
 *   5. FD candidates=["3"] → radius 已释放 → 成功
 *
 * 预期输出:
 *   1. 步骤2: 返回值 == UBSE_OK
 *   2. 步骤3: 返回值 != UBSE_OK
 *   3. 步骤4: 返回值 == UBSE_OK
 *   4. 步骤5: 返回值 == UBSE_OK，exportNumaInfos[0].nodeId == "3"
 */
TEST_F(TestSchedulerEndToEnd, RadiusBorrow_WithCandidates_AllFiltered)
{
    SetupRadiusConfig("1", "1");

    auto nodeMap = CreateNodeMap(3);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    // Step 1: FD candidates=["2"] 借入 node2 → 建立借用关系
    auto obj1 = MakeFdObj("radius-borrow-first", BYTE_128MB, {"2"});
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj1), UBSE_OK);

    // Step 2: FD candidates=["3"] → node3 被 RadiusBorrowFilter 过滤 → 失败
    auto obj2 = MakeFdObj("radius-borrow-second", BYTE_128MB, {"3"});
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj2), UBSE_OK);

    // Step 3: 仅 IMPORT_DESTROYED 即可释放 borrow radius
    obj1.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj1), UBSE_OK);

    // Step 4: FD candidates=["3"] → radius 已释放，node3 可用 → 成功
    auto obj3 = MakeFdObj("radius-borrow-third", BYTE_128MB, {"3"});
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj3), UBSE_OK);
    ASSERT_FALSE(obj3.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj3.algoResult.exportNumaInfos[0].nodeId, "3");
}

/**
 * @brief 用例 14: RadiusBoth_WithCandidates_Mixed
 *
 * radius.borrow=1, radius.lender=1。已借入 node2 后，候选 ["2","3","4"] 中 node2 保留（已有关系），node3/node4 被 RadiusBorrowFilter 过滤，最终仅 node2 可用。
 *
 * 前置状态:
 * - 四节点已注册，全互联链路
 * - radius.borrow=1, radius.lender=1
 *
 * 操作步骤:
 *   1. 设置 Radius 配置
 *   2. FD candidates=["2"] 借入 node2 → 成功
 *   3. FD candidates=["2","3","4"] → node2 保留，node3/node4 被过滤 → 成功
 *
 * 预期输出:
 *   1. 步骤2: 返回值 == UBSE_OK，exportNumaInfos[0].nodeId == "2"
 *   2. 步骤3: 返回值 == UBSE_OK，exportNumaInfos[0].nodeId == "2"
 */
TEST_F(TestSchedulerEndToEnd, RadiusBoth_WithCandidates_Mixed)
{
    SetupRadiusConfig("1", "1");

    auto nodeMap = CreateNodeMap(4);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["4"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    // Step 1: 指定 candidates=["2"] 借入 node2 → 成功
    auto obj1 = MakeFdObj("radius-mixed-first", BYTE_128MB, {"2"});
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj1), UBSE_OK);
    ASSERT_FALSE(obj1.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj1.algoResult.exportNumaInfos[0].nodeId, "2");

    // Step 2: candidates=["2","3","4"]
    //   node2: 已有借用关系 → 保留
    //   node3: 新关系，currentBorrowRadius(1) >= 1 → RadiusBorrowFilter 过滤
    //   node4: 同 node3
    // 仅 node2 保留 → 成功
    auto obj2 = MakeFdObj("radius-mixed-second", BYTE_128MB, {"2", "3", "4"});
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj2), UBSE_OK);
    ASSERT_FALSE(obj2.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj2.algoResult.exportNumaInfos[0].nodeId, "2");
}
/**
 * @brief 用例 15: Node2LendOutNextBorrowFailed
 *
 * 成环检测：节点1借用节点2后（节点2 memLent > 0），节点2再申请借用时 BorrowedLenderFilter 检测 importNode 的 GetLentSize() > 0，直接返回 UBSE_SCHEDULER_ERROR_INVAL 拒绝。
 *
 * 前置状态:
 * - 两节点已注册（1, 2），均 isLender=true
 * - 全互联链路，两节点同组
 *
 * 操作步骤:
 *   1. 注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. Node1 FD 借用（importNodeId="1", SCHEDULING）→ SUCCESS
 *   3. 验证节点2对应出借 NUMA 的 GetMemLentSize() == 128MB
 *   4. Node2 FD 借用（importNodeId="2", SCHEDULING）→ FAIL
 *
 * 预期输出:
 *   1. 步骤2 返回值 == UBSE_OK，exportNumaInfos[0].nodeId == "2"
 *   2. 步骤3 节点2 GetMemLentSize() == 128MB
 *   3. 步骤4 返回值 == UBSE_SCHEDULER_ERROR_INVAL，algoResult.exportNumaInfos 为空
 */
TEST_F(TestSchedulerEndToEnd, Node2LendOutNextBorrowFailed)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj1 = MakeFdObj("fd-1", BYTE_128MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj1), UBSE_OK);
    ASSERT_FALSE(obj1.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj1.algoResult.exportNumaInfos[0].nodeId, "2");

    auto exportSize = obj1.algoResult.exportNumaInfos[0].size;
    auto exportNumaId = static_cast<NumaId>(obj1.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemLentSize(), exportSize);

    adapter_plugins::mmi::UbseMemFdBorrowImportObj obj2{};
    obj2.req.name = "fd-2";
    obj2.req.requestNodeId = "2";
    obj2.req.importNodeId = "2";
    obj2.req.size = BYTE_128MB;
    obj2.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj2), UBSE_SCHEDULER_ERROR_INVAL);
    EXPECT_TRUE(obj2.algoResult.exportNumaInfos.empty());
}

// ==================== ProviderFilter Tests ====================

/**
 * @brief 用例 16: ProviderFilter_InList
 *
 * ProviderFilter：节点均在 providerList 中，应成功。
 *
 * 前置状态:
 * - 两节点已注册，节点均在 providerList
 *
 * 操作步骤:
 *   1. SetupFilterTestConfig("host-1,host-2", true, ...) 设置 provider
 *   2. 注册两节点，Mock GetAllNodes
 *   3. FD 借用
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, ProviderFilter_InList)
{
    SetupFilterTestConfig("host-1,host-2", true, "host-1,host-2", true);

    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto obj = MakeFdObj("provider-in-list", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 17: ProviderFilter_NotInList
 *
 * ProviderFilter：节点1不在 providerList 中，应失败。
 *
 * 前置状态:
 * - 两节点已注册，节点1不在 providerList
 *
 * 操作步骤:
 *   1. SetupFilterTestConfig("host-1", true, "host-1,host-2", true) 设置 provider
 *   2. 注册两节点，Mock GetAllNodes
 *   3. FD 借用
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, ProviderFilter_NotInList)
{
    SetupFilterTestConfig("host-1", true, "host-1,host-2", true);

    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto obj = MakeFdObj("provider-not-in-list", BYTE_128MB);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 18: ProviderFilter_EmptyList
 *
 * ProviderFilter：providerList 为空（白名单模式关闭），不应过滤。
 *
 * 前置状态:
 * - 两节点已注册
 *
 * 操作步骤:
 *   1. SetupFilterTestConfig("", true, ...) providerList 为空
 *   2. 注册两节点，Mock GetAllNodes
 *   3. FD 借用
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, ProviderFilter_EmptyList)
{
    SetupFilterTestConfig("", true, "host-1,host-2", true);

    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto obj = MakeFdObj("provider-empty-list", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

// ==================== GroupFilter Tests ====================

/**
 * @brief 用例 19: GroupFilter_SameGroup
 *
 * GroupFilter：请求方和候选同组，应成功。
 *
 * 前置状态:
 * - 两节点已注册，同组
 *
 * 操作步骤:
 *   1. SetupFilterTestConfig("", false, "host-1,host-2", true) 同组
 *   2. 注册两节点，Mock GetAllNodes
 *   3. FD 借用
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, GroupFilter_SameGroup)
{
    SetupFilterTestConfig("", false, "host-1,host-2", true);

    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto obj = MakeFdObj("group-same", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 20: GroupFilter_DiffGroup
 *
 * GroupFilter：请求方和候选不同组，应失败。
 *
 * 前置状态:
 * - 两节点已注册，不同组
 *
 * 操作步骤:
 *   1. SetupFilterTestConfig("", false, "host-1;host-2", true) 不同组
 *   2. 注册两节点，Mock GetAllNodes
 *   3. FD 借用
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, GroupFilter_DiffGroup)
{
    SetupFilterTestConfig("", false, "host-1;host-2", true);

    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto obj = MakeFdObj("group-diff", BYTE_128MB);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 21: GroupFilter_NoConfig
 *
 * GroupFilter：group 配置为空，不过滤。
 *
 * 前置状态:
 * - 两节点已注册
 *
 * 操作步骤:
 *   1. SetupFilterTestConfig("", false, "", false) 无 group 配置
 *   2. 注册两节点，Mock GetAllNodes
 *   3. FD 借用
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, GroupFilter_NoConfig)
{
    SetupFilterTestConfig("", false, "", false);

    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto obj = MakeFdObj("group-no-config", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 22: Node1BorrowReturnThenNode2Borrow
 *
 * 节点1向节点2借用内存成功后归还，节点2再向节点1借用内存。
 * 验证双向借用生命周期完整：SCHEDULING → IMPORT_DESTROYED → EXPORT_DESTROYED 后,
 * 原借出方节点1可转变为借入方。
 *
 * 前置状态: 两节点已注册，全互联链路
 *
 * 操作步骤:
 *   1. Node1 FD 借用 Node2 (SCHEDULING)
 *   2. Node1 归还借用 (IMPORT_DESTROYED + EXPORT_DESTROYED)
 *   3. Node2 FD 借用 Node1 (SCHEDULING)
 *
 * 预期输出:
 *   1. 步骤1 返回值 == UBSE_OK, exportNodeId="2"
 *   2. 步骤2 返回值 == UBSE_OK
 *   3. 步骤3 返回值 == UBSE_OK, exportNodeId="1"
 */
TEST_F(TestSchedulerEndToEnd, Node1BorrowReturnThenNode2Borrow)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto borrow1 = MakeFdObj("br-1", BYTE_128MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(borrow1), UBSE_OK);
    ASSERT_FALSE(borrow1.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(borrow1.algoResult.exportNumaInfos[0].nodeId, "2");

    borrow1.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(borrow1), UBSE_OK);
    borrow1.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(borrow1), UBSE_OK);

    adapter_plugins::mmi::UbseMemFdBorrowImportObj borrow2{};
    borrow2.req.name = "br-2";
    borrow2.req.requestNodeId = "2";
    borrow2.req.importNodeId = "2";
    borrow2.req.size = BYTE_128MB;
    borrow2.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
    ASSERT_EQ(impl.MemoryObjChangeHandler(borrow2), UBSE_OK);
    ASSERT_FALSE(borrow2.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(borrow2.algoResult.exportNumaInfos[0].nodeId, "1");
}

/**
 * @brief 用例 23: EightNodeInit
 *
 * 八节点 FD 借用，验证多节点 Filter + Score 链对全部候选节点的完整筛选与排序。
 *
 * 前置状态:
 * - 八节点已注册（"1"~"8"），全部 isLender=true, clusterState=WORKING
 * - 全互联链路，所有节点同组
 *
 * 操作步骤:
 *   1. CreateNodeMap(8) + AddFullMeshPeers 构造拓扑
 *   2. 注册全部节点，Mock GetAllNodes
 *   3. FD 借用（importNodeId="1", size=128MB）
 *   4. 验证选中节点非节点1，且 memLent/memBorrow 正确
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId 为 "2"~"8" 之一
 *   3. 选中节点 GetMemLentSize() == 128MB
 *   4. 节点1 GetMemBorrowSize() == 128MB
 */
TEST_F(TestSchedulerEndToEnd, EightNodeInit)
{
    auto nodeMap = CreateNodeMap(8);
    AddFullMeshPeers(nodeMap);
    for (const auto& id : {"2", "3", "4", "5", "6", "7", "8", "1"}) {
        SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap[id]);
    }
    SetupMockNodeMap(nodeMap);

    auto& impl = SchedulerImpl::GetInstance();
    auto obj = MakeFdObj("fd-8node", BYTE_128MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    auto exportNodeId = obj.algoResult.exportNumaInfos[0].nodeId;
    EXPECT_NE(exportNodeId, "1");
    EXPECT_TRUE(exportNodeId >= "2" && exportNodeId <= "8");

    ASSERT_FALSE(obj.algoResult.importNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.importNumaInfos[0].nodeId, "1");

    auto exportSize = obj.algoResult.exportNumaInfos[0].size;
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo(exportNodeId, exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemLentSize(), exportSize);

    auto importSize = obj.algoResult.importNumaInfos[0].size;
    auto importSocket =
        impl.nodeInfo_->GetSocketInfo("1", static_cast<SocketId>(obj.algoResult.importNumaInfos[0].socketId));
    ASSERT_NE(importSocket, nullptr);
    auto* importNuma = importSocket->GetAllNumaInfos().begin()->second.get();
    EXPECT_EQ(importNuma->GetMemBorrowSize(), importSize);
}

// ==================== MemoryConfigFilter Error Paths ====================

/**
 * @brief 用例 24: FdBorrowLenderBalance_NumaPriority
 *
 * lender.balance=true 下，同一节点第二次借用时优先复用已借出过的 NUMA。
 *
 * 前置状态:
 * - lender.balance=true
 * - 两节点已注册，全互联链路
 * - node2 socket36 下两个 NUMA（id=0, id=1）内存大小完全相等（各 4GB total / 2GB free），消除 free size 差异对选取的干扰
 *
 * 测试输入:
 *   步骤1: UbseMemFdBorrowImportObj{name:"fd-prio-1st", requestNodeId:"1", importNodeId:"1", size:128MB, state:SCHEDULING}
 *   步骤2: UbseMemFdBorrowImportObj{name:"fd-prio-2nd", requestNodeId:"1", importNodeId:"1", size:128MB, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. SetupLenderBalanceConfig(true)，设置 lender.balance 配置
 *   2. 构造 node2：清空默认 4 个 NUMA，仅添加 2 个等大 NUMA（id=0, id=1，各 4GB/2GB）
 *   3. 全互联链路，注册两节点，Mock GetAllNodes
 *   4. Step 1：第 1 次 FD 借用 128MB（无历史）→ SelectNumaByReliable 中 neverBorrowedIdx 按序取第一个 NUMA，记录 firstNumaId = exportNumaInfos[0].numaId
 *   5. Step 2：第 2 次 FD 借用 128MB（有历史）→ SelectNumaByReliable 中 borrowedIdx 优先，复用第 1 次的 NUMA
 *
 * 预期输出:
 *   1. 步骤4 返回值 == UBSE_OK
 *   2. 步骤5 返回值 == UBSE_OK
 *   3. exportNumaInfos 非空
 *   4. 步骤5 的 exportNumaInfos[0].numaId 等于步骤4 的 exportNumaInfos[0].numaId（验证已借出 NUMA 被优先复用）
 */
TEST_F(TestSchedulerEndToEnd, FdBorrowLenderBalance_NumaPriority)
{
    SetupLenderBalanceConfig(true);

    auto nodeMap = CreateNodeMap(2);
    nodeMap["2"].numaInfos.clear();
    {
        auto numa0 = MakeNumaInfo("2", 0, 36, KB_4GB, KB_2GB);
        nodeMap["2"].numaInfos[numa0.location] = numa0;
        auto numa1 = MakeNumaInfo("2", 1, 36, KB_4GB, KB_2GB);
        nodeMap["2"].numaInfos[numa1.location] = numa1;
    }

    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj1 = MakeFdObj("fd-prio-1st", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj1), UBSE_OK);
    ASSERT_FALSE(obj1.algoResult.exportNumaInfos.empty());
    auto firstNumaId = obj1.algoResult.exportNumaInfos[0].numaId;

    auto obj2 = MakeFdObj("fd-prio-2nd", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj2), UBSE_OK);
    ASSERT_FALSE(obj2.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj2.algoResult.exportNumaInfos[0].numaId, firstNumaId);
}

/**
 * @brief 用例 25: FdBorrowLenderBalance_NumaWithFewerBorrowers
 *
 * lender.balance=true 下，当目标 socket 上的所有 NUMA 均已被其他节点借出过时，新借用节点优先选取已有 borrower 更少的 NUMA。
 *
 * 前置状态:
 * - lender.balance=true
 * - 四节点已注册（"1"~"4"），全互联链路
 * - node4 socket36 下两个 NUMA（id=0, id=1）内存大小完全相等（各 4GB total / 2GB free）
 * - 前置已建立借用关系：
 *   - node2 从 node4/NUMA0 借出 128MB
 *   - node3 从 node4/NUMA0 借出 128MB → NUMA0 有 2 个 borrower
 *   - node2 从 node4/NUMA1 借出 128MB → NUMA1 有 1 个 borrower
 *
 * 操作步骤:
 *   1. SetupLenderBalanceConfig(true)，设置 lender.balance 配置
 *   2. 构造 node4：清空默认 NUMA，仅添加 2 个等大 NUMA（id=0, id=1，各 4GB/2GB）
 *   3. 全互联链路，注册全部 4 个节点，Mock GetAllNodes
 *   4. 前置建立：通过 lenderLocs 强制指定 NUMA，执行 3 次借用
 *   5. 正式用例：node1 从 node4 借用（candidates=["4"]，不使用 lenderLocs）→ 走 SelectNumaByReliable
 *
 * 预期输出:
 *   1. 步骤4 前置 3 次借用均返回 == UBSE_OK
 *   2. 步骤5 返回值 == UBSE_OK
 *   3. exportNumaInfos 非空
 *   4. exportNumaInfos[0].numaId == 1（borrower 更少的 NUMA1 被优先选取）
 */
TEST_F(TestSchedulerEndToEnd, FdBorrowLenderBalance_NumaWithFewerBorrowers)
{
    SetupLenderBalanceConfig(true);
    auto& impl = SchedulerImpl::GetInstance();

    auto nodeMap = CreateNodeMap(4);
    nodeMap["4"].numaInfos.clear();
    {
        auto numa0 = MakeNumaInfo("4", 0, 36, KB_4GB, KB_2GB);
        nodeMap["4"].numaInfos[numa0.location] = numa0;
        auto numa1 = MakeNumaInfo("4", 1, 36, KB_4GB, KB_2GB);
        nodeMap["4"].numaInfos[numa1.location] = numa1;
    }

    AddFullMeshPeers(nodeMap);
    for (auto& id : {"2", "3", "4", "1"}) {
        impl.NodeObjChangeHandler(nodeMap[id]);
    }
    SetupMockNodeMap(nodeMap);

    auto makeLenderObj = [](const std::string& name, const std::string& reqNode, const std::string& lenderNode,
                            uint32_t lenderNuma) {
        adapter_plugins::mmi::UbseMemFdBorrowImportObj obj{};
        obj.req.name = name;
        obj.req.requestNodeId = reqNode;
        obj.req.importNodeId = reqNode;
        obj.req.size = BYTE_128MB;
        obj.req.lenderLocs = {{lenderNode, lenderNuma}};
        obj.req.lenderSizes = {BYTE_128MB};
        obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
        return obj;
    };

    auto lenderObj1 = makeLenderObj("n2->n4/0", "2", "4", 0);
    ASSERT_EQ(impl.MemoryObjChangeHandler(lenderObj1), UBSE_OK);
    auto lenderObj2 = makeLenderObj("n3->n4/0", "3", "4", 0);
    ASSERT_EQ(impl.MemoryObjChangeHandler(lenderObj2), UBSE_OK);
    auto lenderObj3 = makeLenderObj("n2->n4/1", "2", "4", 1);
    ASSERT_EQ(impl.MemoryObjChangeHandler(lenderObj3), UBSE_OK);

    auto obj = MakeFdObj("fd-fewer-borrowers", BYTE_128MB, {"4"});
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].numaId, 1);
}

/**
 * @brief 用例 26: FdBorrowFullLifecycle
 *
 * 测试 FD 借用 IMPORT_DESTROYED + EXPORT_DESTROYED 归还路径。
 * SCHEDULING → AddNumaBorrow + AddNumaLend,
 * IMPORT_DESTROYED → SubNumaBorrow（memBorrow 归零）,
 * EXPORT_DESTROYED → SubNumaLend（memLent 归零）。
 *
 * 前置状态: 两节点已注册，全互联链路
 *
 * 操作步骤:
 *   1. 注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. FD SCHEDULING → AddNumaLend + AddNumaBorrow
 *   3. 验证借入方 NUMA memBorrow == importSize
 *   4. 验证出借方 NUMA memLent == exportSize
 *   5. IMPORT_DESTROYED → SubNumaBorrow，memBorrow == 0
 *   6. EXPORT_DESTROYED → SubNumaLend，memLent == 0
 *
 * 预期输出:
 *   1. 所有状态返回值 == UBSE_OK
 *   2. 步骤5: memBorrow == 0
 *   3. 步骤6: memLent == 0
 */
TEST_F(TestSchedulerEndToEnd, FdBorrowFullLifecycle)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeFdObj("fd-full-lifecycle", BYTE_128MB);
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
 * @brief 用例 28: CheckMixBorrowReturnIsOk
 *
 * 同一节点多次 FD 借入/归还混合场景（乱序归还），验证 memLent_/memBorrow_ 的加减正确性及账户最终全部清理。
 *
 * 前置状态:
 * - 两节点已注册，全互联链路，节点2空闲内存充足（>512MB）
 *
 * 测试输入:
 *   test1: UbseMemFdBorrowImportObj{name:"test1", requestNodeId:"1", importNodeId:"1", size:128MB, state:SCHEDULING}
 *   test2: UbseMemFdBorrowImportObj{name:"test2", requestNodeId:"1", importNodeId:"1", size:128MB, state:SCHEDULING}
 *   test3: UbseMemFdBorrowImportObj{name:"test3", requestNodeId:"1", importNodeId:"1", size:128MB, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. 注册两节点，full-mesh peer，Mock GetAllNodes
 *   2. test1 SCHEDULING → SUCCESS（节点2 memLent_+128MB，节点1 memBorrow_+128MB）
 *   3. test2 SCHEDULING → SUCCESS（节点2 GetLentSize() == 256MB）
 *   4. test2 IMPORT_DESTROYED → EXPORT_DESTROYED（归还，GetLentSize() == 128MB）
 *   5. test3 SCHEDULING → SUCCESS（GetLentSize() == 256MB）
 *   6. test1 + test3 全归还 → GetLentSize() == 0，GetBorrowSize() == 0
 *
 * 预期输出:
 *   1. 所有 SCHEDULING 返回值 == UBSE_OK
 *   2. 所有 DESTROYED 返回值 == UBSE_OK
 *   3. 最终 GetLentSize() == 0，GetBorrowSize() == 0
 */
TEST_F(TestSchedulerEndToEnd, CheckMixBorrowReturnIsOk)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto test1 = MakeFdObj("test1", BYTE_128MB);
    auto test2 = MakeFdObj("test2", BYTE_128MB);
    auto test3 = MakeFdObj("test3", BYTE_128MB);

    auto node1 = impl.nodeInfo_->GetNodeInfo("1");
    auto node2 = impl.nodeInfo_->GetNodeInfo("2");
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    ASSERT_EQ(impl.MemoryObjChangeHandler(test1), UBSE_OK);
    ASSERT_FALSE(test1.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(node2->GetLentSize(), BYTE_128MB);

    ASSERT_EQ(impl.MemoryObjChangeHandler(test2), UBSE_OK);
    ASSERT_FALSE(test2.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(node2->GetLentSize(), BYTE_128MB * 2);

    test2.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(test2), UBSE_OK);
    test2.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(test2), UBSE_OK);
    EXPECT_EQ(node2->GetLentSize(), BYTE_128MB);

    ASSERT_EQ(impl.MemoryObjChangeHandler(test3), UBSE_OK);
    ASSERT_FALSE(test3.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(node2->GetLentSize(), BYTE_128MB * 2);

    test1.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(test1), UBSE_OK);
    test1.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(test1), UBSE_OK);

    test3.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(test3), UBSE_OK);
    test3.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(test3), UBSE_OK);

    EXPECT_EQ(node2->GetLentSize(), 0u);
    EXPECT_EQ(node1->GetBorrowSize(), 0u);
}

/**
 * @brief 用例 29: SocketFreeMem_Insufficient
 *
 * 请求大小超过 Socket 可借出内存。HUGETLB_PMD（32个2M大页=64MB），waterLine=100，blockSize=128MB。64MB < 128MB → 可借出为 0。
 *
 * 前置状态:
 * - 两节点已注册，allocator=HUGETLB_PMD, blockSize=128
 *
 * 操作步骤:
 *   1. Mock 节点（HUGETLB_PMD），注册
 *   2. 获取 node2 socket36
 *   3. 调用 GetAvailableLendSize(100, 128MB)
 *
 * 预期输出:
 *   1. GetAvailableLendSize 返回值 == 0
 */
TEST_F(TestSchedulerEndToEnd, SocketFreeMem_Insufficient)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PMD, 128);
    SetHugeNumaMemPoolZero(nodeMap);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto& impl = SchedulerImpl::GetInstance();
    auto socket = impl.nodeInfo_->GetSocketInfo("2", 36);
    ASSERT_NE(socket, nullptr);

    // HUGETLB_PMD 32个2M大页 = 64MB 总内存，waterLine=100, blockSize=128MB
    // 可借出 = (64MB / 128MB) * 128MB = 0
    uint64_t available = socket->GetAvailableLendSize(100, 128 * ONE_M);
    EXPECT_EQ(available, 0u);
}

// ==================== Lender Balance ====================

/**
 * @brief 用例 41: MemoryConfig_AllocatorMismatch
 *
 * 两 lender 的 allocator 不同（HUGETLB_PMD vs HUGETLB_PUD），MemoryConfigFilter 检测到不匹配，返回 UBSE_ERROR_CONF_INVALID。
 */
TEST_F(TestSchedulerEndToEnd, MemoryConfig_AllocatorMismatch)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PMD, 64);
    nodeMap["1"].allocator = UbseAllocator::HUGETLB_PUD;
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-allocator-mismatch", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_ERROR_CONF_INVALID);
}

/**
 * @brief 用例 42: MemoryConfig_BlockSizeMismatch
 *
 * 两 lender 的 blockSize 不同（64 vs 128），MemoryConfigFilter 检测到不匹配，返回 UBSE_ERROR_CONF_INVALID。
 */
TEST_F(TestSchedulerEndToEnd, MemoryConfig_BlockSizeMismatch)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::BUDDY_HIGHMEM, 64);
    nodeMap["1"].blockSize = 128;
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-block-mismatch", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_ERROR_CONF_INVALID);
}

/**
 * @brief 用例 43: MemoryConfig_BlockSizeOutOfRange
 *
 * 两 lender 的 blockSize=5000（>4096），MemoryConfigFilter 检测到超出范围，返回 UBSE_ERROR_CONF_INVALID。
 */
TEST_F(TestSchedulerEndToEnd, MemoryConfig_BlockSizeOutOfRange)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::BUDDY_HIGHMEM, 5000);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-blocksize-out", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_ERROR_CONF_INVALID);
}

/**
 * @brief 用例 44: MemoryConfig_BlockSizeOdd
 *
 * 两 lender 的 blockSize=5（奇数），MemoryConfigFilter 检测到不符合偶数要求，返回 UBSE_ERROR_CONF_INVALID。
 */
TEST_F(TestSchedulerEndToEnd, MemoryConfig_BlockSizeOdd)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::BUDDY_HIGHMEM, 5);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-blocksize-odd", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_ERROR_CONF_INVALID);
}

// ==================== LentSizeLimitFilter ====================

/**
 * @brief 用例 45: LentSizeLimit_Exceeded
 *
 * 节点2 socket36 的 maxLentSize 设为 1MB，FD 请求 128MB，
 * LentSizeLimitFilter 检测 maxLentSize < requestSize + lentSize，从候选移除。
 */
TEST_F(TestSchedulerEndToEnd, LentSizeLimit_Exceeded)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    // 限制节点2所有 socket 的 maxLentSize，确保无任何 socket 可借出
    for (const auto& socketId : {36, 276}) {
        auto socket = impl.nodeInfo_->GetSocketInfo("2", socketId);
        ASSERT_NE(socket, nullptr);
        socket->SetMaxLentSize(BYTE_1MB);
    }

    auto obj = MakeFdObj("fd-lentlimit-exceeded", BYTE_128MB);
    ASSERT_NE(impl.MemoryObjChangeHandler(obj), UBSE_OK);
}

// ==================== Account State Machine - Missing Paths ====================

/**
 * @brief 用例 46: FdStateMachine_Failed
 *
 * SCHEDULING（IMPORT_EXPORT_EXIST）→ STATE_FAILED → BOTH_NOT_EXIST。
 * 验证 STATE_FAILED 路径同时归还借入和借出。
 */
TEST_F(TestSchedulerEndToEnd, FdStateMachine_Failed)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeFdObj("fd-failed-test", BYTE_128MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());

    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemLentSize(), 0u);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_STATE_FAILED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemLentSize(), 0u);
}

/**
 * @brief 用例 47: FdStateMachine_ExportFirst
 *
 * SCHEDULING（IMPORT_EXPORT_EXIST）→ EXPORT_DESTROYED（ONLY_IMPORT_EXIST）
 * → IMPORT_DESTROYED（BOTH_NOT_EXIST）。
 * 验证先销毁 export 再销毁 import 的生命周期路径。
 */
TEST_F(TestSchedulerEndToEnd, FdStateMachine_ExportFirst)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeFdObj("fd-export-first", BYTE_128MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());

    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_GT(exportNuma->GetMemLentSize(), 0u);

    auto importSocket =
        impl.nodeInfo_->GetSocketInfo("1", static_cast<SocketId>(obj.algoResult.importNumaInfos[0].socketId));
    ASSERT_NE(importSocket, nullptr);
    auto* importNuma = importSocket->GetAllNumaInfos().begin()->second.get();
    EXPECT_GT(importNuma->GetMemBorrowSize(), 0u);

    // EXPORT_DESTROYED → ONLY_IMPORT_EXIST (SubLend)
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemLentSize(), 0u);
    EXPECT_GT(importNuma->GetMemBorrowSize(), 0u);

    // IMPORT_DESTROYED → BOTH_NOT_EXIST (SubBorrow)
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), 0u);
}

/**
 * @brief 用例 48: FdStateMachine_DirectImportSuccess
 *
 * BOTH_NOT_EXIST + IMPORT_SUCCESS → ONLY_IMPORT_EXIST。
 * 手动填充 algoResult，验证仅增加 memBorrow。
 */
TEST_F(TestSchedulerEndToEnd, FdStateMachine_DirectImportSuccess)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeFdObj("fd-import-only", BYTE_128MB);
    // 手动填充 algoResult（模拟 SCHEDULING 后的结果，跳过调度管线）
    adapter_plugins::mmi::UbseMemDebtNumaInfo importInfo{};
    importInfo.nodeId = "1";
    importInfo.socketId = 36;
    importInfo.numaId = 0;
    importInfo.size = BYTE_128MB;
    adapter_plugins::mmi::UbseMemDebtNumaInfo exportInfo{};
    exportInfo.nodeId = "2";
    exportInfo.socketId = 36;
    exportInfo.numaId = 0;
    exportInfo.size = BYTE_128MB;
    obj.algoResult.importNumaInfos.push_back(importInfo);
    obj.algoResult.exportNumaInfos.push_back(exportInfo);
    obj.algoResult.blockSize = 128;

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto importNuma = impl.nodeInfo_->GetNumaInfo("1", 0);
    ASSERT_NE(importNuma, nullptr);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), BYTE_128MB);

    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", 0);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemLentSize(), 0u);
}

/**
 * @brief 用例 49: FdStateMachine_DirectExportSuccess
 *
 * BOTH_NOT_EXIST + EXPORT_SUCCESS → ONLY_EXPORT_EXIST。
 * 手动填充 algoResult，验证仅增加 memLent。
 */
TEST_F(TestSchedulerEndToEnd, FdStateMachine_DirectExportSuccess)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeFdObj("fd-export-only", BYTE_128MB);
    adapter_plugins::mmi::UbseMemDebtNumaInfo importInfo{};
    importInfo.nodeId = "1";
    importInfo.socketId = 36;
    importInfo.numaId = 0;
    importInfo.size = BYTE_128MB;
    adapter_plugins::mmi::UbseMemDebtNumaInfo exportInfo{};
    exportInfo.nodeId = "2";
    exportInfo.socketId = 36;
    exportInfo.numaId = 0;
    exportInfo.size = BYTE_128MB;
    obj.algoResult.importNumaInfos.push_back(importInfo);
    obj.algoResult.exportNumaInfos.push_back(exportInfo);
    obj.algoResult.blockSize = 128;

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto importNuma = impl.nodeInfo_->GetNumaInfo("1", 0);
    ASSERT_NE(importNuma, nullptr);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), 0u);

    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", 0);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemLentSize(), BYTE_128MB);
}

/**
 * @brief 用例 50: FdStateMachine_CatchUpImport
 *
 * EXPORT_SUCCESS（ONLY_EXPORT_EXIST）→ IMPORT_SUCCESS（IMPORT_EXPORT_EXIST）。
 * 验证 export 先就绪，import 后续到达的补充路径。
 */
TEST_F(TestSchedulerEndToEnd, FdStateMachine_CatchUpImport)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeFdObj("fd-catchup-import", BYTE_128MB);
    adapter_plugins::mmi::UbseMemDebtNumaInfo importInfo{};
    importInfo.nodeId = "1";
    importInfo.socketId = 36;
    importInfo.numaId = 0;
    importInfo.size = BYTE_128MB;
    adapter_plugins::mmi::UbseMemDebtNumaInfo exportInfo{};
    exportInfo.nodeId = "2";
    exportInfo.socketId = 36;
    exportInfo.numaId = 0;
    exportInfo.size = BYTE_128MB;
    obj.algoResult.importNumaInfos.push_back(importInfo);
    obj.algoResult.exportNumaInfos.push_back(exportInfo);
    obj.algoResult.blockSize = 128;

    // Step 1: EXPORT_SUCCESS → ONLY_EXPORT_EXIST
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", 0);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemLentSize(), BYTE_128MB);

    auto importNuma = impl.nodeInfo_->GetNumaInfo("1", 0);
    ASSERT_NE(importNuma, nullptr);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), 0u);

    // Step 2: IMPORT_SUCCESS → IMPORT_EXPORT_EXIST
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), BYTE_128MB);
    EXPECT_EQ(exportNuma->GetMemLentSize(), BYTE_128MB);
}

/**
 * @brief 用例 51: FdStateMachine_CatchUpExport
 *
 * IMPORT_SUCCESS（ONLY_IMPORT_EXIST）→ EXPORT_SUCCESS（IMPORT_EXPORT_EXIST）。
 * 验证 import 先就绪，export 后续到达的补充路径。
 */
TEST_F(TestSchedulerEndToEnd, FdStateMachine_CatchUpExport)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeFdObj("fd-catchup-export", BYTE_128MB);
    adapter_plugins::mmi::UbseMemDebtNumaInfo importInfo{};
    importInfo.nodeId = "1";
    importInfo.socketId = 36;
    importInfo.numaId = 0;
    importInfo.size = BYTE_128MB;
    adapter_plugins::mmi::UbseMemDebtNumaInfo exportInfo{};
    exportInfo.nodeId = "2";
    exportInfo.socketId = 36;
    exportInfo.numaId = 0;
    exportInfo.size = BYTE_128MB;
    obj.algoResult.importNumaInfos.push_back(importInfo);
    obj.algoResult.exportNumaInfos.push_back(exportInfo);
    obj.algoResult.blockSize = 128;

    // Step 1: IMPORT_SUCCESS → ONLY_IMPORT_EXIST
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto importNuma = impl.nodeInfo_->GetNumaInfo("1", 0);
    ASSERT_NE(importNuma, nullptr);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), BYTE_128MB);

    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", 0);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemLentSize(), 0u);

    // Step 2: EXPORT_SUCCESS → IMPORT_EXPORT_EXIST
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), BYTE_128MB);
    EXPECT_EQ(exportNuma->GetMemLentSize(), BYTE_128MB);
}

// ==================== P1: Score Verification ====================

/**
 * @brief 用例 55: BalanceScore_SelectsMostFree
 *
 * 3 候选节点空闲内存不同，验证 BalanceScore 选出 free 最大者。
 * node2: 每 NUMA 4GB total / 2GB free → BalanceScore 最低（最优）
 * node3: 每 NUMA 4GB total / 1GB free → BalanceScore 中等
 * node4: 每 NUMA 4GB total / 512MB free → BalanceScore 最高（最差）
 */
TEST_F(TestSchedulerEndToEnd, BalanceScore_SelectsMostFree)
{
    auto nodeMap = CreateNodeMap(4);
    // 覆盖候选节点的 freeSize（设定不同空闲内存）
    for (auto& [loc, numa] : nodeMap["2"].numaInfos) {
        numa.freeSize = KB_2GB; // 2GB free per NUMA
        numa.mempool_available_cleared = 0;
    }
    for (auto& [loc, numa] : nodeMap["3"].numaInfos) {
        numa.freeSize = KB_1GB; // 1GB free
        numa.mempool_available_cleared = 0;
    }
    for (auto& [loc, numa] : nodeMap["4"].numaInfos) {
        numa.freeSize = KB_1GB / 2; // 512MB free
        numa.mempool_available_cleared = 0;
    }

    AddFullMeshPeers(nodeMap);
    for (const auto& id : {"2", "3", "4", "1"}) {
        SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap[id]);
    }
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-balance-mostfree", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

/**
 * @brief 用例 56: DivideNumaScore_MinimizeNumaCount
 *
 * 两候选节点 BalanceScore 相等，NUMA 分片数不同。
 * node2: 每 NUMA 512MB total / 200MB free → 1 NUMA 满足 128MB 请求
 * node3: 每 NUMA 512MB total / 64MB free → 需 2 NUMA 满足
 * DivideNumaScore 优先选分片更少的 node2。
 */
TEST_F(TestSchedulerEndToEnd, DivideNumaScore_MinimizeNumaCount)
{
    auto nodeMap = CreateNodeMap(3);

    constexpr uint64_t KB_200MB = 200ULL * 1024;
    constexpr uint64_t KB_64MB = 64ULL * 1024;

    for (auto& [loc, numa] : nodeMap["2"].numaInfos) {
        numa.size = KB_1GB / 2;
        numa.freeSize = KB_200MB;
        numa.mempool_available_cleared = 0;
    }
    for (auto& [loc, numa] : nodeMap["3"].numaInfos) {
        numa.size = KB_1GB / 2;
        numa.freeSize = KB_64MB;
        numa.mempool_available_cleared = 0;
    }

    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-divide-min", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

// ==================== P1: SocketFreeMemoryFilter Pipeline ====================

/**
 * @brief 用例 52: SocketFreeMem_PipelineReject
 *
 * HUGETLB_PMD 下内存不足，SocketFreeMemoryFilter 通过完整调度管线拒绝请求。
 * 验证走 MemoryObjChangeHandler 而非直接调用 GetAvailableLendSize。
 */
TEST_F(TestSchedulerEndToEnd, SocketFreeMem_PipelineReject)
{
    auto nodeMap = CreateNodeMap(2, UbseAllocator::HUGETLB_PMD, 128);
    SetHugeNumaMemPoolZero(nodeMap);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-pipeline-nomem", BYTE_128MB);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

// ==================== FD + 1Dfullmesh (LinkPortDownFilter) ====================

/**
 * @brief 用例 32: FDBorrow1DFullmesh_PeerSocketDown
 *
 * 1Dfullmesh 下 node2 socket36 端口 DOWN，socket276 正常。LinkPortDownFilter 过滤掉 socket36，从 socket276 出借。
 *
 * 前置状态: 两节点已注册，1Dfullmesh 组网（仅 socket276 互联）
 *
 * 测试输入: UbseMemFdBorrowImportObj{name:"fd-1dfull-sock36down", requestNodeId:"1", importNodeId:"1", size:128MB, state:SCHEDULING}
 *
 * 操作步骤:
 *   1. 构造 2 节点，仅 socket276 建立 peer relation
 *   2. 注册节点，Mock GetAllNodes
 *   3. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 *   3. exportNumaInfos[0].socketId == 276（socket36 被 LinkPortDownFilter 过滤）
 */
TEST_F(TestSchedulerEndToEnd, FDBorrow1DFullmesh_PeerSocketDown)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeFD1DFullmesh(nodeMap, false, true);

    auto obj = MakeFdObj("fd-1dfull-sock36down", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    ASSERT_FALSE(obj.algoResult.importNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].socketId, 276);
    EXPECT_EQ(obj.algoResult.importNumaInfos[0].nodeId, "1");
}

/**
 * @brief 用例 33: FDBorrow1DFullmesh_AllSocketsDown
 *
 * 1Dfullmesh 下 node2 所有 socket 端口 DOWN，LinkPortDownFilter 清空所有候选。
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, FDBorrow1DFullmesh_AllSocketsDown)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeFD1DFullmesh(nodeMap, false, false);

    auto obj = MakeFdObj("fd-1dfull-alldown", BYTE_128MB);
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 34: FDBorrow1DFullmesh_MultiNodePartition
 *
 * 3 节点，node1↔node2 可达，node1↔node3 不通。LinkPortDownFilter 过滤不可达的 node3。
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 */
TEST_F(TestSchedulerEndToEnd, FDBorrow1DFullmesh_MultiNodePartition)
{
    auto nodeMap = CreateNodeMap(3);
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

    auto obj = MakeFdObj("fd-1dfull-partition", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

// ==================== BorrowedLenderFilter: Provider Node Already Borrowing ====================

/**
 * @brief 用例 35: FdBorrow_ProviderAlreadyBorrowing
 *
 * node2 已从 node3 借入（GetBorrowSize>0），node1 尝试 FD 借用时 node2 被 BorrowedLenderFilter 移除。
 *
 * 操作步骤:
 *   1. node2 向 node3 FD 借入（candidateNodeList=["3"]）→ 成功
 *   2. node1 FD 借用 → node2 被过滤，仅剩 node3 候选
 *
 * 预期输出:
 *   1. 步骤1 返回 UBSE_OK
 *   2. 步骤2 返回 UBSE_OK
 *   3. exportNumaInfos[0].nodeId == "3"
 */
TEST_F(TestSchedulerEndToEnd, FdBorrow_ProviderAlreadyBorrowing)
{
    auto nodeMap = CreateNodeMap(3);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    // Step 1: node2 borrows from node3 specifically → node2.memBorrow > 0
    adapter_plugins::mmi::UbseMemFdBorrowImportObj borrowByNode2{};
    borrowByNode2.req.name = "n2-borrows-from-n3";
    borrowByNode2.req.requestNodeId = "2";
    borrowByNode2.req.importNodeId = "2";
    borrowByNode2.req.size = BYTE_128MB;
    borrowByNode2.req.candidateNodeList = {"3"};
    borrowByNode2.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
    ASSERT_EQ(impl.MemoryObjChangeHandler(borrowByNode2), UBSE_OK);

    auto node2 = impl.nodeInfo_->GetNodeInfo("2");
    ASSERT_NE(node2, nullptr);
    ASSERT_GT(node2->GetBorrowSize(), 0u);

    // Step 2: node1 borrows → node2 is filtered by BorrowedLenderFilter (GetBorrowSize > 0)
    auto obj = MakeFdObj("fd-provider-borrowing", BYTE_128MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "3");
}

// ==================== LentSizeLimit: Single Socket Exceeded ====================

/**
 * @brief 用例 36: LentSizeLimit_SingleSocketExceeded
 *
 * socket36 maxLentSize=1MB，socket276 不设限。FD 请求 128MB 被过滤 socket36，从 socket276 出借。
 *
 * 操作步骤:
 *   1. 设节点2 socket36 的 maxLentSize=1MB
 *   2. FD SCHEDULING 128MB
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].socketId == 276
 */
TEST_F(TestSchedulerEndToEnd, LentSizeLimit_SingleSocketExceeded)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto socket36 = impl.nodeInfo_->GetSocketInfo("2", 36);
    ASSERT_NE(socket36, nullptr);
    socket36->SetMaxLentSize(BYTE_1MB);

    auto obj = MakeFdObj("fd-lentlimit-single", BYTE_128MB);
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].socketId, 276);
}

// ==================== Score Priority: Balance > Divide ====================

/**
 * @brief 用例 37: ScorePriority_BalanceBeforeDivide
 *
 * node2 空闲多（BalanceScore 优），node3 空闲少（BalanceScore 差）。BalanceScore 权重（0.4）> DivideNumaScore（0.15），应选 node2。
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 */
TEST_F(TestSchedulerEndToEnd, ScorePriority_BalanceBeforeDivide)
{
    auto nodeMap = CreateNodeMap(3);

    // node2: 每 NUMA 2GB free → BalanceScore 优
    for (auto& [loc, numa] : nodeMap["2"].numaInfos) {
        numa.freeSize = KB_2GB;
        numa.mempool_available_cleared = 0;
    }

    // node3: 每 NUMA 512MB free → BalanceScore 差
    for (auto& [loc, numa] : nodeMap["3"].numaInfos) {
        numa.freeSize = KB_1GB / 2;
        numa.mempool_available_cleared = 0;
    }

    AddFullMeshPeers(nodeMap);
    for (const auto& id : {"2", "3", "1"}) {
        SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap[id]);
    }
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-score-priority", BYTE_128MB);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

// ==================== P2: FD LenderLocs Node Only ====================

/**
 * @brief 用例 38: FdBorrow_LenderLocsNodeOnly
 *
 * lenderLocs 仅指定 nodeId（socketId/numaId=UINT32_MAX），LenderInfoFilter 仅按 nodeId 过滤，Score 自由选择 socket/numa。
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos[0].nodeId == "2"
 */
TEST_F(TestSchedulerEndToEnd, FdBorrow_LenderLocsNodeOnly)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    adapter_plugins::mmi::UbseNumaLocation loc{};
    loc.nodeId = "2";
    auto obj = MakeFdObjWithLenderLocs("fd-lenderlocs-nodeonly", BYTE_128MB, {loc}, {BYTE_128MB});
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

// ==================== P2: FD LenderLocs Unreachable Socket ====================

/**
 * @brief 用例 39: FdBorrow_LenderLocsUnreachableSocket
 *
 * 1Dfullmesh 下 node2 socket36 不可达，但 lenderLocs 指定 NUMA0（属于 socket36）。LinkPortDownFilter 先过滤 socket36，LenderInfoFilter 后续不符。
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, FdBorrow_LenderLocsUnreachableSocket)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeFD1DFullmesh(nodeMap, false, true); // socket36 DOWN, socket276 UP

    adapter_plugins::mmi::UbseNumaLocation loc{};
    loc.nodeId = "2";
    loc.numaId = 0; // 属于 socket36，但 socket36 不可达
    auto obj = MakeFdObjWithLenderLocs("fd-lenderlocs-unreach-sock", BYTE_128MB, {loc}, {BYTE_128MB});
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

// ==================== P2: FD All Candidates Filtered ====================

/**
 * @brief 用例 40: FdBorrow_AllCandidateNodesFiltered
 *
 * candidateNodeList=["3"] 但 node3 clusterState=FAULT，NodeStateFilter 移除。
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, FdBorrow_AllCandidateNodesFiltered)
{
    auto nodeMap = CreateNodeMap(3);
    nodeMap["3"].clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["3"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj("fd-all-filtered", BYTE_128MB, {"3"});
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 54: FdBorrowRandomSizeVerifyAlignedTotal
 *
 * 循环 5 次随机 requestSize（4MB~4096MB），验证每次借用的 exportNumaInfos 各 entry size 之和等于 requestSize 按 blockSize 向上取整的值。
 *
 * 前置状态:
 * - 两节点已注册，全互联链路
 * - 节点2 空闲内存充足（默认 4GB+2GB，可承载最大 4096MB 请求）
 *
 * 操作步骤:
 *   1. CreateNodeMap(2) + AddFullMeshPeers(nodeMap)
 *   2. 注册两节点，Mock GetAllNodes
 *   3. 循环 5 次：
 *      - 生成随机 requestSize ∈ [4MB, 4096MB]（固定种子 42）
 *      - 构造 FD import obj（state: SCHEDULING）
 *      - 调用 MemoryObjChangeHandler(obj)
 *      - 计算 expectedTotal = ((requestSize + blockBytes - 1) / blockBytes) * blockBytes，其中 blockBytes = algoResult.blockSize * ONE_M
 *      - 计算 actualTotal = sum(exportNumaInfos[i].size)
 *      - 断言 actualTotal == expectedTotal
 *      - 归还借用（IMPORT_DESTROYED → EXPORT_DESTROYED），释放内存供下次循环
 *
 * 预期输出:
 *   1. 所有 5 次调用返回值 == UBSE_OK
 *   2. 每次 actualTotal == expectedTotal（精确块对齐）
 *   3. exportNumaInfos 非空
 */
TEST_F(TestSchedulerEndToEnd, FdBorrowRandomSizeVerifyAlignedTotal)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint64_t> dist(BYTE_4MB, BYTE_4096MB);

    for (int i = 0; i < 5; ++i) {
        uint64_t requestSize = dist(rng);
        std::string name = "fd-align-" + std::to_string(i);
        auto obj = MakeFdObj(name, requestSize);
        ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

        ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
        uint64_t blockBytes = static_cast<uint64_t>(obj.algoResult.blockSize) * ONE_M;
        uint64_t expectedTotal = ((requestSize + blockBytes - 1) / blockBytes) * blockBytes;

        uint64_t actualTotal = 0;
        for (const auto& info : obj.algoResult.exportNumaInfos) {
            actualTotal += static_cast<uint64_t>(info.size);
        }
        EXPECT_EQ(actualTotal, expectedTotal);

        obj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
        ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
        obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
        ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
    }
}

/**
 * @brief 用例 53: FdBorrowMultiNumaSplit
 *
 * NUMA0=12G, NUMA1=16G, 请求 20G。验证 FillFromNumaList 按空闲降序依次填充。
 *
 * 前置状态:
 * - 两节点已注册，BUDDY_HIGHMEM, blockSize=128，全互联链路
 * - node2 socket36 两个 NUMA：NUMA0=12G, NUMA1=16G
 * - node2 socket276 两个 NUMA 可用设为 0
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. exportNumaInfos.size() == 2
 *   3. exportNumaInfos[0]: numaId=1, size=16G（空闲多的 NUMA1 优先）
 *   4. exportNumaInfos[1]: numaId=0, size=4G（NUMA0 补充剩余 4G）
 */
TEST_F(TestSchedulerEndToEnd, FdBorrowMultiNumaSplit)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeEnv(nodeMap);

    for (auto& [loc, numa] : nodeMap["2"].numaInfos) {
        numa.freeSize = 0;
        if (loc.numaId == 0) {
            numa.size = 12ULL * 1024 * 1024;
            numa.mempool_available_cleared = BYTE_1GB * 12;
        } else if (loc.numaId == 1) {
            numa.size = 16ULL * 1024 * 1024;
            numa.mempool_available_cleared = BYTE_1GB * 16;
        } else {
            numa.size = 0;
            numa.mempool_available_cleared = 0;
        }
    }
    SetupMockNodeMap(nodeMap);

    uint64_t blockBytes = 128ULL * ONE_M;
    uint64_t requestSize = BYTE_1GB * 20 - blockBytes / 2;
    auto obj = MakeFdObj("fd-multi-numa", requestSize);
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_EQ(obj.algoResult.exportNumaInfos.size(), 2U);
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].numaId, 1);
    EXPECT_EQ(static_cast<uint64_t>(obj.algoResult.exportNumaInfos[0].size), BYTE_1GB * 16);
    EXPECT_EQ(obj.algoResult.exportNumaInfos[1].numaId, 0);
    EXPECT_EQ(static_cast<uint64_t>(obj.algoResult.exportNumaInfos[1].size), BYTE_1GB * 4);
}

} // namespace ubse::mem::scheduler::ut
