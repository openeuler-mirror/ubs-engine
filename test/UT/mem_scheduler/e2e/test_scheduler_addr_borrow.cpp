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

UbseMemAddrBorrowImportObj MakeAddrObj(const std::string& name)
{
    adapter_plugins::mmi::UbseMemAddrBorrowImportObj obj{};
    obj.req.name = name;
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.exportNodeId = "2";
    obj.req.dstSocket = 36;
    obj.req.dstNuma = 0;
    obj.req.exportAddrList = {{.addr = 0, .size = BYTE_128MB}};
    obj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
    return obj;
}

void SetupTwoNodeAddrEnv(std::unordered_map<std::string, UbseNodeInfo>& nodeMap)
{
    nodeMap = CreateNodeMap(2);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);
}

} // namespace

/**
 * @brief 用例 1: AddrBorrow_Success
 *
 * 基础 Addr 借用成功路径。Filter 链: LenderNodeFilter → NodeStateFilter → BorrowedLenderFilter → LenderInfoFilter。
 * dstSocket/dstNuma/exportAddrList 均有效, LenderInfoFilter 被加入链。
 *
 * 前置状态: 两节点已注册, 节点2 正常（clusterState=WORKING, isLender=true）
 *
 * 测试输入: UbseMemAddrBorrowImportObj{name:"addr-borrow-1", requestNodeId:"1", importNodeId:"1", exportNodeId:"2", dstSocket:36, dstNuma:0, exportAddrList:[{addr:0, size:128MB}], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. 注册两节点, Mock GetAllNodes
 *   2. 构造 ADDR import obj（SCHEDULING）
 *   3. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. algoResult.exportNumaInfos 非空
 *   3. algoResult.importNumaInfos 非空
 *   4. algoResult.exportNumaInfos[0].nodeId == "2"
 */
TEST_F(TestSchedulerEndToEnd, AddrBorrow_Success)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeAddrEnv(nodeMap);

    auto obj = MakeAddrObj("addr-borrow-1");
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);

    ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
    ASSERT_FALSE(obj.algoResult.importNumaInfos.empty());
    EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "2");
}

/**
 * @brief 用例 2: AddrBorrow_ImportNodeHasLent
 *
 * importNode（节点1）已出借过内存（GetLentSize()>0）,
 * BorrowedLenderFilter 检测到 importNode 已出借, 直接返回错误终止。
 *
 * 前置状态: 两节点已注册, 节点1 已作为出借方参与过 FD 出借（memLent > 0）
 *
 * 测试输入: UbseMemAddrBorrowImportObj{name:"addr-borrow-3", requestNodeId:"1", importNodeId:"1", exportNodeId:"2", dstSocket:36, dstNuma:0, exportAddrList:[{addr:0, size:128MB}], state:SCHEDULING}（节点1 已出借, memLent>0）
 *
 * 操作步骤:
 *   1. 注册两节点, full-mesh peer, Mock GetAllNodes
 *   2. Step 1: 节点2 向节点1 FD 借入 → 节点1 memLent > 0
 *   3. Step 2: 节点1 发起 ADDR 借用（importNodeId="1"）→ BorrowedLenderFilter 拦截
 *
 * 预期输出:
 *   1. 步骤2: 返回值 == UBSE_OK（FD 借用成功）
 *   2. 步骤3: 返回值 != UBSE_OK（BorrowedLenderFilter 拦截）
 */
TEST_F(TestSchedulerEndToEnd, AddrBorrow_ImportNodeHasLent)
{
    // 使用同一套 nodeMap 跨两个步骤，避免重复 mock
    auto nodeMap = CreateNodeMap(2);
    AddFullMeshPeers(nodeMap);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    // Step 1: FD 借用，让节点1作为出借方（节点2 向节点1 借用）
    adapter_plugins::mmi::UbseMemFdBorrowImportObj fdObj{};
    fdObj.req.name = "fd-lend-by-node1";
    fdObj.req.requestNodeId = "2";
    fdObj.req.importNodeId = "2";
    fdObj.req.size = BYTE_128MB;
    fdObj.status.state = adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
    ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(fdObj), UBSE_OK);

    // 借入成功 → 节点1 已出借 memLent > 0
    auto exportNumaId = static_cast<NumaId>(fdObj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = SchedulerImpl::GetInstance().nodeInfo_->GetNumaInfo("1", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    ASSERT_GT(exportNuma->GetMemLentSize(), 0u);

    // Step 2: Addr 借用 → importNodeId="1" 已出借过 → BorrowedLenderFilter 拦截
    auto obj = MakeAddrObj("addr-borrow-3");
    ASSERT_NE(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), UBSE_OK);
}

/**
 * @brief 用例 3: AddrBorrow_ExportNodeFault
 *
 * exportNode（节点2）clusterState=FAULT, NodeStateFilter 将其从候选移除。
 *
 * 前置状态: 两节点已注册, 节点2 clusterState=FAULT
 *
 * 测试输入: UbseMemAddrBorrowImportObj{name:"addr-borrow-5", requestNodeId:"1", importNodeId:"1", exportNodeId:"2", dstSocket:36, dstNuma:0, exportAddrList:[{addr:0, size:128MB}], state:SCHEDULING}
 *
 * 操作步骤:
 *   1. 注册两节点（节点2 clusterState=FAULT）
 *   2. Mock GetAllNodes
 *   3. 构造 ADDR import obj
 *   4. 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 返回值 != UBSE_OK
 */
TEST_F(TestSchedulerEndToEnd, AddrBorrow_ExportNodeFault)
{
    auto nodeMap = CreateNodeMap(2);
    nodeMap["2"].clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["2"]);
    SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap["1"]);
    SetupMockNodeMap(nodeMap);

    auto obj = MakeAddrObj("addr-borrow-5");
    auto ret = SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj);
    ASSERT_NE(ret, UBSE_OK);
}

// ==================== ADDR Lifecycle ====================

/**
 * @brief 用例 4: AddrBorrow_ImportDestroyed
 *
 * ADDR 借用后, IMPORT_DESTROYED 归还验证。
 * SCHEDULING → AddNumaBorrow + AddNumaLend,
 * IMPORT_DESTROYED → SubNumaBorrow → memBorrow 恢复为 0。
 *
 * 前置状态: 两节点已注册, 节点2 正常（clusterState=WORKING, isLender=true）
 *
 * 测试输入: UbseMemAddrBorrowImportObj{name:"addr-destroy-import", requestNodeId:"1", importNodeId:"1", exportNodeId:"2", dstSocket:36, dstNuma:0, exportAddrList:[{addr:0, size:128MB}], state:SCHEDULING} → state:IMPORT_DESTROYED
 *
 * 操作步骤:
 *   1. 注册两节点, Mock GetAllNodes
 *   2. 构造 ADDR import obj(state:SCHEDULING)
 *   3. 调用 MemoryObjChangeHandler(obj)
 *   4. 校验 importNuma.memBorrowSize == importSize
 *   5. 将 obj.state 改为 IMPORT_DESTROYED, 再次调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 步骤3: 返回值 == UBSE_OK
 *   2. 步骤3: importNuma.memBorrowSize == importSize
 *   3. 步骤5: 返回值 == UBSE_OK
 *   4. 步骤5: importNuma.memBorrowSize == 0
 */
TEST_F(TestSchedulerEndToEnd, AddrBorrow_ImportDestroyed)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeAddrEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeAddrObj("addr-destroy-import");
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto importSize = obj.algoResult.importNumaInfos[0].size;
    auto importSocket =
        impl.nodeInfo_->GetSocketInfo("1", static_cast<SocketId>(obj.algoResult.importNumaInfos[0].socketId));
    ASSERT_NE(importSocket, nullptr);
    auto* importNuma = importSocket->GetAllNumaInfos().begin()->second.get();
    EXPECT_EQ(importNuma->GetMemBorrowSize(), importSize);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(importNuma->GetMemBorrowSize(), 0u);
}

/**
 * @brief 用例 5: AddrBorrow_ExportDestroyed
 *
 * ADDR 借用后 EXPORT_DESTROYED 归还。
 * SCHEDULING → AddNumaLend,
 * IMPORT_DESTROYED → EXPORT_DESTROYED → SubNumaLend → memLent 恢复。
 *
 * 前置状态: 两节点已注册, 节点2 正常（clusterState=WORKING, isLender=true）
 *
 * 测试输入: UbseMemAddrBorrowImportObj{name:"addr-destroy-export", requestNodeId:"1", importNodeId:"1", exportNodeId:"2", dstSocket:36, dstNuma:0, exportAddrList:[{addr:0, size:128MB}], state:SCHEDULING} → state:IMPORT_DESTROYED → state:EXPORT_DESTROYED
 *
 * 操作步骤:
 *   1. 注册两节点, Mock GetAllNodes
 *   2. 构造 ADDR import obj(state:SCHEDULING)
 *   3. 调用 MemoryObjChangeHandler(obj)
 *   4. 校验 exportNuma.memLentSize == exportSize
 *   5. 将 obj.state 改为 IMPORT_DESTROYED, 调用 MemoryObjChangeHandler(obj)
 *   6. 将 obj.state 改为 EXPORT_DESTROYED, 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 步骤3: 返回值 == UBSE_OK
 *   2. 步骤3: exportNuma.memLentSize == exportSize
 *   3. 步骤5: 返回值 == UBSE_OK
 *   4. 步骤6: 返回值 == UBSE_OK
 *   5. 步骤6: exportNuma.memLentSize == 0
 */
TEST_F(TestSchedulerEndToEnd, AddrBorrow_ExportDestroyed)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeAddrEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeAddrObj("addr-destroy-export");
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    auto exportSize = obj.algoResult.exportNumaInfos[0].size;
    auto exportNumaId = static_cast<NumaId>(obj.algoResult.exportNumaInfos[0].numaId);
    auto exportNuma = impl.nodeInfo_->GetNumaInfo("2", exportNumaId);
    ASSERT_NE(exportNuma, nullptr);
    EXPECT_EQ(exportNuma->GetMemLentSize(), exportSize);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);

    obj.status.state = adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    ASSERT_EQ(impl.MemoryObjChangeHandler(obj), UBSE_OK);
    EXPECT_EQ(exportNuma->GetMemLentSize(), 0u);
}

/**
 * @brief 用例 6: AddrBorrow_StateFailed
 *
 * ADDR 借用后 STATE_FAILED 路径。
 * SCHEDULING → AddNumaBorrow + AddNumaLend,
 * STATE_FAILED → SubNumaBorrow + SubNumaLend → 全部恢复。
 *
 * 前置状态: 两节点已注册, 节点2 正常（clusterState=WORKING, isLender=true）
 *
 * 测试输入: UbseMemAddrBorrowImportObj{name:"addr-failed", requestNodeId:"1", importNodeId:"1", exportNodeId:"2", dstSocket:36, dstNuma:0, exportAddrList:[{addr:0, size:128MB}], state:SCHEDULING} → state:STATE_FAILED
 *
 * 操作步骤:
 *   1. 注册两节点, Mock GetAllNodes
 *   2. 构造 ADDR import obj(state:SCHEDULING)
 *   3. 调用 MemoryObjChangeHandler(obj)
 *   4. 校验 memLentSize > 0 且 memBorrowSize > 0
 *   5. 将 obj.state 改为 STATE_FAILED, 调用 MemoryObjChangeHandler(obj)
 *
 * 预期输出:
 *   1. 步骤3: 返回值 == UBSE_OK
 *   2. 步骤3: exportNuma.memLentSize > 0
 *   3. 步骤3: importNuma.memBorrowSize > 0
 *   4. 步骤5: 返回值 == UBSE_OK
 *   5. 步骤5: exportNuma.memLentSize == 0
 *   6. 步骤5: importNuma.memBorrowSize == 0
 */
TEST_F(TestSchedulerEndToEnd, AddrBorrow_StateFailed)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    SetupTwoNodeAddrEnv(nodeMap);
    auto& impl = SchedulerImpl::GetInstance();

    auto obj = MakeAddrObj("addr-failed");
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

} // namespace ubse::mem::scheduler::ut
