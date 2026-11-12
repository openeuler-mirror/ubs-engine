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
#include "test_ubse_mem_controller_migrate_api.h"
#include <src/adapter_plugins/mmi/ubse_mmi_module.h>
#include "ubse_mem_scheduler_impl.h"
#include "migrate/ubse_mem_migrate_helper.h"
#include "migrate/ubse_mem_migrate_internal.h"
#include "test_ubse_mem_controller_share_helper.h"

using ubse::mem::scheduler::SchedulerImpl;
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include "ubse_com_module.h"
#include "ubse_election.h"
#include "ubse_mem_async_processor.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_migrate_session.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mem_util.h"
#include "ubse_node.h"
#include "ubse_timer.h"
#include "ubse_topo_util.h"
#include "api/ubse_mem_controller_share_api.h"
#include "debt/ubse_mem_debt_info_query.h"
#include "debt/ubse_mem_debt_ledger.h"
#include "message/ubse_mem_simpo_types.h"
#include "migrate/ubse_mem_controller_migrate_api.h"

namespace ubse::mem::controller {
uint32_t ShareExportMasterCallback(const std::string& exportNodeId,
                                   ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj& exportObj);
}

// 符号插入覆盖libc的sleep，仅本fixture内生效（SetUp开、TearDown关）：
// 开启时生产重试路径(SendMigrateMessage等)立即返回，不真实休眠；关闭时转发真实休眠，
// 不影响同一UT二进制内其它依赖真实sleep的用例(uds_server/ipc_server/thread_pool等)。
// 普通C函数线程安全；不得改回MOCKER(&::sleep)——libc级mockcpp hook会被泄漏的后台
// 线程并发进入(mockcpp非线程安全)，曾导致malloc(): unaligned tcache chunk detected。
std::atomic<bool> g_fastMigrateSleep{false};
extern "C" unsigned int sleep(unsigned int seconds)
{
    if (!g_fastMigrateSleep.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
    }
    return 0;
}

namespace ubse::mem_controller::share::ut {
using namespace ubse::com;
using namespace mem::controller::message;
using namespace mem::controller;
using namespace mem::controller::debt;
using namespace ubse::utils;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::election;
using namespace ubse::nodeController;

const std::string NODE_ONE = "1";
const std::string NODE_TWO = "2";
const std::string SHM_NAME = "share_test_mem";

namespace {
ubse::task_executor::UbseTaskExecutorPtr g_migrateExecutor;
std::atomic<bool> g_migrateExecutorUnavailable{false};
} // namespace

std::mutex g_migrateTimersMutex;
std::map<std::string, UbseTimerHandler> g_migrateTimers;
uint32_t MigrateTimerRegisterStub(const std::string& name, UbseTimerHandler handler, uint32_t)
{
    std::lock_guard<std::mutex> lock(g_migrateTimersMutex);
    g_migrateTimers[name] = std::move(handler);
    return UBSE_OK;
}
void MigrateTimerUnregisterStub(const std::string& name)
{
    std::lock_guard<std::mutex> lock(g_migrateTimersMutex);
    g_migrateTimers.erase(name);
}
void PumpMigrateTimers()
{
    std::map<std::string, UbseTimerHandler> timers;
    {
        std::lock_guard<std::mutex> lock(g_migrateTimersMutex);
        timers = g_migrateTimers;
    }
    for (const auto& timer : timers) {
        timer.second();
    }
}

void TestUbseMemControllerMigrateApi::SetUp()
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    Test::SetUp();
    g_fastMigrateSleep = true; // 本fixture内sleep立即返回，重试路径不真实休眠
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).stubs().will(invoke(&MigrateTimerRegisterStub));
    MOCKER(&ubse::timer::UbseTimerHandlerUnregister).stubs().will(invoke(&MigrateTimerUnregisterStub));
}
void TestUbseMemControllerMigrateApi::TearDown()
{
    g_migrateExecutorUnavailable = false;
    Test::TearDown();
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    // 终态可早于回调线程退出；清除mock前必须等本用例的异步任务全部结束。
    if (g_migrateExecutor != nullptr) {
        g_migrateExecutor->Wait();
    }
    g_fastMigrateSleep = false; // fixture外恢复真实sleep，避免影响同二进制其它用例
    {
        std::lock_guard<std::mutex> lock(g_migrateTimersMutex);
        g_migrateTimers.clear();
    }
    GlobalMockObject::verify();
}

namespace {
uint32_t MigrateDeleteShareExportStub(const UbseMemShareBorrowExportObj& obj);
}

uint32_t MigrateDeleteExportConfirmStub(const UbseMemShareBorrowExportObj& obj)
{
    return MigrateDeleteShareExportStub(obj);
}

// ==== SHM故障迁移受理====

namespace {
// slotId 与 nodeId 同值口径
const std::string MIGRATE_SRC_NODE_ID = "11";
const std::string MIGRATE_DST_NODE_ID = "12";
constexpr uint32_t SRC_SLOT_ID = 11;
constexpr uint32_t DST_SLOT_ID = 12;
constexpr uint64_t MIGRATE_SIZE_BYTES = 1024ULL * 1024ULL * 1024ULL;     // 1GB
constexpr uint64_t DST_AVAIL_BYTES = 2ULL * 1024ULL * 1024ULL * 1024ULL; // 2GB

ubse::nodeController::UbseNodeInfo MakeClusterNode(const std::string& nodeId, uint32_t slotId,
                                                   ubse::nodeController::UbseNodeClusterState state,
                                                   uint64_t availBytes)
{
    ubse::nodeController::UbseNodeInfo node{};
    node.nodeId = nodeId;
    node.slotId = slotId;
    node.clusterState = state;
    ubse::nodeController::UbseNumaInfo numaInfo{};
    numaInfo.location.nodeId = nodeId;
    numaInfo.freeSize = availBytes / 1024ULL; // mempool未上报时按kb回退
    node.numaInfos[numaInfo.location] = numaInfo;
    return node;
}

void ShmMigrateNodeMockSet(const ubse::nodeController::UbseNodeInfo& srcNode,
                           const ubse::nodeController::UbseNodeInfo& dstNode)
{
    // 按 srcNodeId 查源节点；按 slotId(与nodeId同值)查目标节点
    MOCKER(&ubse::nodeController::UbseNodeController::GetNodeById)
        .stubs()
        .with(eq(srcNode.nodeId))
        .will(returnValue(srcNode));
    MOCKER(&ubse::nodeController::UbseNodeController::GetNodeById)
        .stubs()
        .with(eq(MIGRATE_DST_NODE_ID))
        .will(returnValue(dstNode));
}

UbseMemShareBorrowExportObj BuildMigrateExportObj(const std::string& srcNodeId, uint64_t size)
{
    UbseMemShareBorrowExportObj exportObj{};
    exportObj.req.name = SHM_NAME;
    exportObj.req.baseNodeId = srcNodeId;
    exportObj.req.size = size;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo{};
    numaInfo.nodeId = srcNodeId;
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    // 受理鉴权按name查询，源账本与名称索引必须一并登记。
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(srcNodeId, SHM_NAME,
                                                                                           exportObj);
    return exportObj;
}

UbseMemShmMigrateReq BuildMigrateReq(uint32_t targetNodeId)
{
    UbseMemShmMigrateReq req{};
    req.name = SHM_NAME;
    req.targetNodeId = std::to_string(targetNodeId);
    req.caller.uid = 0;
    req.caller.username = "root";
    return req;
}

// 期望块数: 1GB / 默认blockSize(128MB) = 8
constexpr size_t MIGRATE_EXPECT_BLOCK_COUNT = static_cast<size_t>(MIGRATE_SIZE_BYTES / (128ULL * 1024ULL * 1024ULL));

// 决策副作用桩: SCHEDULING 态回填决策产物(目标节点单numa全量), 其余态(记账/回滚)直通OK
UbseResult MigrateScheduleStub(SchedulerImpl* /*impl*/, UbseMemShareBorrowExportObj& obj)
{
    if (obj.status.state == UBSE_MEM_SCHEDULING) {
        UbseMemDebtNumaInfo numaInfo{};
        numaInfo.nodeId = MIGRATE_DST_NODE_ID;
        numaInfo.size = MIGRATE_SIZE_BYTES;
        obj.algoResult.exportNumaInfos.push_back(numaInfo);
    }
    return UBSE_OK;
}

using MigrateExportReqPtr = ubse::simpo::util::UbseAutoSimpoPtr<UbseMemShareBorrowExportObj>;

std::atomic<bool> g_exportHeld{false};
std::atomic<bool> g_cleanupHeld{false};
std::atomic<uint32_t> g_oldCleanupCount{0};
UbseMemMigrateRpcInfo g_exportResultInfo{};
UbseMemMigrateRpcInfo g_cleanupResultInfo{};
UbseMemShareBorrowExportObj g_exportHeldResponse{};
UbseMemShareBorrowExportObj g_cleanupHeldResponse{};
bool g_holdAllDeletes = false;
bool g_holdOldDelete = false;
bool g_failDeleteSend = false;

UbseResult ReportExportResult(const UbseMemMigrateRpcInfo&, const UbseMemShareBorrowExportObj& response)
{
    const auto executor = ubse::mem::util::GetExecutor("ubseMemController");
    return executor != nullptr && executor->Execute([response]() {
        UbseMemShmMigrateService::GetInstance().NewExportResult(response);
    }) ?
               UBSE_OK :
               UBSE_ERROR;
}

UbseResult ReportDeleteResult(const UbseMemShareBorrowExportObj& response)
{
    const auto executor = ubse::mem::util::GetExecutor("ubseMemController");
    return executor != nullptr && executor->Execute([response]() {
        auto obj = response;
        const auto slot = obj.algoResult.exportNumaInfos.front().nodeId;
        if (GetShareExportObj(slot, obj.req.name) == nullptr) {
            return;
        }
        obj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        obj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        ShareExportMasterCallback(slot, obj); // 普通归还回调回收账户，不再推进迁移。
    }) ?
               UBSE_OK :
               UBSE_ERROR;
}

uint32_t MigrateDeleteShareExportStub(const UbseMemShareBorrowExportObj& obj)
{
    auto deleting = obj;
    deleting.status.state = UBSE_MEM_EXPORT_DESTROYING;
    deleting.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    PutShareExportObj(obj.algoResult.exportNumaInfos.front().nodeId, obj.req.name, deleting);
    const bool oldChain = obj.algoResult.exportNumaInfos.front().nodeId == MIGRATE_SRC_NODE_ID;
    if (oldChain) {
        ++g_oldCleanupCount;
    }
    if (g_failDeleteSend) {
        return UBSE_ERROR;
    }
    if (g_holdAllDeletes || (g_holdOldDelete && oldChain)) {
        g_cleanupHeldResponse = obj;
        g_cleanupHeld.store(true, std::memory_order_release);
        return UBSE_OK;
    }
    return ReportDeleteResult(obj);
}

UbseMemShareBorrowExportObj BuildExportResponse(const UbseMemShareBorrowExportObj& req)
{
    UbseMemShareBorrowExportObj rsp = req;
    rsp.status.state = UBSE_MEM_EXPORT_SUCCESS;
    rsp.status.errCode = UBSE_OK;
    rsp.status.exportObmmInfo.clear();
    for (size_t i = 1; i <= MIGRATE_EXPECT_BLOCK_COUNT; ++i) {
        UbseMemObmmInfo block{};
        block.memId = i;
        block.desc.length = MIGRATE_SIZE_BYTES / MIGRATE_EXPECT_BLOCK_COUNT;
        rsp.status.exportObmmInfo.push_back(block);
    }
    return rsp;
}

UbseResult MigrateExportRpcSuccessStub(UbseComModule*, const SendParam&, MigrateExportReqPtr& req, UbseBaseMessagePtr&,
                                       const bool)
{
    return ReportExportResult({}, BuildExportResponse(req->GetUbseMesgInfo()));
}

UbseResult MigrateExportRpcNoBlockStub(UbseComModule*, const SendParam&, MigrateExportReqPtr& req, UbseBaseMessagePtr&,
                                       const bool)
{
    auto response = BuildExportResponse(req->GetUbseMesgInfo());
    response.status.exportObmmInfo.clear();
    return ReportExportResult({}, response);
}

UbseResult MigrateExportRpcCancelRaceStub(UbseComModule* module, const SendParam& param, MigrateExportReqPtr& req,
                                          UbseBaseMessagePtr& ack, const bool withCopy)
{
    UbseMemMigrateCancelResp response{};
    UbseMemShmMigrateService::GetInstance().Cancel(SHM_NAME, response);
    EXPECT_EQ(UBSE_OK, response.errCode);
    return MigrateExportRpcSuccessStub(module, param, req, ack, withCopy);
}

UbseResult MigrateExportRpcObmmFailStub(UbseComModule*, const SendParam&, MigrateExportReqPtr& req, UbseBaseMessagePtr&,
                                        const bool)
{
    auto response = BuildExportResponse(req->GetUbseMesgInfo());
    response.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
    response.errorCode = UBSE_MMI_OBMM_OP_FAILED;
    response.status.exportObmmInfo.clear();
    return ReportExportResult({}, response);
}

UbseResult MigrateExportHoldCreateStub(UbseComModule*, const SendParam&, MigrateExportReqPtr& req, UbseBaseMessagePtr&,
                                       const bool)
{
    g_exportHeldResponse = BuildExportResponse(req->GetUbseMesgInfo());
    g_exportHeld.store(true, std::memory_order_release);
    return UBSE_OK;
}

UbseResult MigrateExportHoldOldCleanupStub(UbseComModule* module, const SendParam& param, MigrateExportReqPtr& req,
                                           UbseBaseMessagePtr& ack, const bool withCopy)
{
    g_holdOldDelete = true;
    return MigrateExportRpcSuccessStub(module, param, req, ack, withCopy);
}

UbseResult MigrateCancelHoldCleanupStub(UbseComModule* module, const SendParam& param, MigrateExportReqPtr& req,
                                        UbseBaseMessagePtr& ack, const bool withCopy)
{
    g_holdAllDeletes = true;
    return MigrateExportRpcCancelRaceStub(module, param, req, ack, withCopy);
}

// 标准节点mock: 源节点FAULT/目标节点WORKING
void MigrateNodeMockSetForExecute()
{
    ShmMigrateNodeMockSet(
        MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT,
                        0),
        MakeClusterNode(MIGRATE_DST_NODE_ID, DST_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING,
                        DST_AVAIL_BYTES));
}

// 在账本放一个活跃导入节点(importResults/exportObmmInfo 各1块), 逐块迁移 begin 广播目标
// algoResult.exportNumaInfos 指向源节点(GetMaxRefCountExportObj 按 baseNodeId 关联导入 obj, 缺失则不入组)
void PutMigrateImportObj(const std::string& importNodeId);

// 仅受理: 不执行新借出(会话以 name 寻址, 用例内直接用 SHM_NAME); 预置第三方导入节点(目标为未映射节点,
// 导入节点提供 wire 拷贝源 importObj)
// Execute用例直接准备受理后的会话/账本，隔离已确认的Accept和真实调度。
void AcceptMigrateOnly()
{
    static std::atomic<uint64_t> nextRequestId{800000};
    PutMigrateImportObj("13");
    const auto old = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, old);
    auto prepared = *old;
    prepared.req.requestId = ++nextRequestId;
    prepared.req.baseNodeId = MIGRATE_DST_NODE_ID;
    prepared.req.lenderInfo.nodeId = MIGRATE_DST_NODE_ID;
    for (auto& numa : prepared.algoResult.exportNumaInfos) {
        numa.nodeId = MIGRATE_DST_NODE_ID;
    }
    prepared.status.state = UBSE_MEM_EXPORT_RUNNING;
    prepared.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    prepared.status.exportObmmInfo.clear();
    prepared.migrateInfos.clear();
    for (uint32_t b = 0; b < old->status.exportObmmInfo.size(); ++b) {
        prepared.migrateInfos.push_back(MakeShmMigrateInfo(1, b, SRC_SLOT_ID, true));
    }
    PutShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME, prepared);
    const auto session = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, static_cast<uint32_t>(old->status.exportObmmInfo.size()),
        UbseMemMigrateTargetKind::NOT_IMPORT_NODE, MIGRATE_SIZE_BYTES);
    ASSERT_TRUE(session.second);
}

// 决策副作用桩mock: SCHEDULING态回填目标节点决策产物
uint32_t MigratePreparedScheduleStub(const UbseMemShareBorrowReq& req, UbseMemShareBorrowExportObj& obj, bool)
{
    obj.req = req;
    obj.status.state = UBSE_MEM_SCHEDULING;
    UbseMemDebtNumaInfo numa{};
    numa.nodeId = MIGRATE_DST_NODE_ID;
    numa.size = req.size;
    obj.algoResult.exportNumaInfos = {numa};
    return UBSE_OK;
}

void MigrateScheduleMockSet()
{
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowExportObj>)
        .stubs()
        .will(invoke(&MigrateScheduleStub));
}

// comModule + 链路拓扑查询桩(逐块迁移 ComputeMigrateImportLinks 依赖, 不含 RpcSend, 由调用方按需注入)
ubse::task_executor::UbseTaskExecutorPtr MigrateExecutorStub(const std::string& name);

void MigrateComMockSet()
{
    // 不mock全局::sleep：libc级hook会被泄漏的后台线程并发进入，mockcpp非线程安全，曾导致堆破坏。
    // sleep已由本文件符号插入覆盖，且仅本fixture(SetUp开/TearDown关)内立即返回，
    // 重试次数由各桩自身的原子计数断言。
    g_holdAllDeletes = false;
    g_holdOldDelete = false;
    g_failDeleteSend = false;
    MOCKER(&DeleteShareExport).stubs().will(invoke(&MigrateDeleteShareExportStub));
    MOCKER(&ubse::mem::util::GetCurNodeId).stubs().will(returnValue(std::string("1")));
    MOCKER(&ubse::mem::util::GetExecutor).stubs().will(invoke(&MigrateExecutorStub));
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>)
        .stubs()
        .will(returnValue(std::make_shared<com::UbseComModule>()));
    MOCKER(&mem::controller::GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_OK));
    // 新导入布局还会读取导入节点的 CPU 拓扑，覆盖所有参与迁移的第三方节点。
    for (const auto* nodeId : {"13", "14", "15"}) {
        ubse::nodeController::UbseNodeInfo node{};
        node.nodeId = nodeId;
        MOCKER(&UbseNodeController::GetNodeById).stubs().with(eq(std::string(nodeId))).will(returnValue(node));
    }
}

// comModule + 迁移导出RPC成功桩 + 链路拓扑查询桩(逐块迁移 ComputeMigrateImportLinks 依赖)
void MigrateRpcSuccessMockSet()
{
    MigrateComMockSet();
    const auto func = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(invoke(&MigrateExportRpcSuccessStub));
}

// ==== 逐块迁移与账本收敛 用例辅助 ====

// 老 obj 带块(memId 从 1001 递增), 逐块迁移 oldBlockMemIds 提取与块数比对用
void BuildMigrateExportObjWithBlocks(const std::string& srcNodeId, uint64_t size, size_t blockCount)
{
    auto exportObj = BuildMigrateExportObj(srcNodeId, size);
    for (size_t i = 0; i < blockCount; i++) {
        UbseMemObmmInfo obmmInfo{};
        obmmInfo.memId = 1001 + i;
        exportObj.status.exportObmmInfo.push_back(obmmInfo);
        exportObj.migrateInfos.push_back(MakeShmMigrateInfo(0, i, 0, true));
    }
    PutShareExportObj(srcNodeId, SHM_NAME, exportObj);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMarkerPreservesMaxOriginAcrossVersionWrap)
{
    auto source = std::make_shared<UbseMemShareBorrowExportObj>(BuildMigrateExportObj("32767", MIGRATE_SIZE_BYTES));
    source->status.exportObmmInfo.resize(2);
    source->migrateInfos = {MakeShmMigrateInfo(15, 0, 11, true), MakeShmMigrateInfo(15, 1, 11, true)};
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    UbseMemShareBorrowExportObj target{};
    ASSERT_EQ(migrate_detail::PrepareMigrateExport(source, MIGRATE_DST_NODE_ID, nullptr, target), UBSE_OK);
    ASSERT_EQ(target.migrateInfos.size(), 2);
    for (uint32_t i = 0; i < 2; ++i) {
        EXPECT_EQ(target.migrateInfos[i], 0xffff0000U | i);
        EXPECT_TRUE(ShmMigrateIsExport(target.migrateInfos[i]));
        EXPECT_EQ(ShmMigrateOrigin(target.migrateInfos[i]), 32767U);
        EXPECT_EQ(ShmMigrateVersion(target.migrateInfos[i]), 0U);
        EXPECT_EQ(ShmMigrateImportInfo(target.migrateInfos[i]), i);
    }
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateRejectsOriginOverflowBeforeScheduling)
{
    auto source = std::make_shared<UbseMemShareBorrowExportObj>(BuildMigrateExportObj("32768", MIGRATE_SIZE_BYTES));
    source->status.exportObmmInfo.resize(1);
    MOCKER(&ShareBorrowSchedule).expects(never());
    UbseMemShareBorrowExportObj target{};
    EXPECT_EQ(migrate_detail::PrepareMigrateExport(source, MIGRATE_DST_NODE_ID, nullptr, target), UBSE_ERROR_INVAL);
    EXPECT_TRUE(target.migrateInfos.empty());
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedPreparationLeavesExportResultsEmpty)
{
    auto source =
        std::make_shared<UbseMemShareBorrowExportObj>(BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES));
    source->status.exportObmmInfo.resize(2);
    source->algoResult.exportNumaInfos.front().size = MIGRATE_SIZE_BYTES;
    auto targetImport = std::make_shared<UbseMemShareBorrowImportObj>();
    targetImport->req.name = SHM_NAME;
    targetImport->importNodeId = MIGRATE_DST_NODE_ID;
    targetImport->status.state = UBSE_MEM_IMPORT_SUCCESS;
    targetImport->status.importResults.resize(2);
    targetImport->status.decoderResult.resize(2);
    targetImport->exportObmmInfo.resize(2);
    targetImport->status.importResults[0].memId = 2001;
    targetImport->status.importResults[1].memId = 2002;
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).expects(once()).will(invoke(&MigratePreparedScheduleStub));

    UbseMemShareBorrowExportObj target{};
    ASSERT_EQ(migrate_detail::PrepareMigrateExport(source, MIGRATE_DST_NODE_ID, targetImport, target), UBSE_OK);
    EXPECT_TRUE(target.status.exportObmmInfo.empty());
    EXPECT_EQ(target.migrateInfos.size(), 2);
    EXPECT_EQ(targetImport->status.importResults[0].memId, 2001);
    EXPECT_EQ(targetImport->status.importResults[1].memId, 2002);
    ASSERT_EQ(target.algoResult.exportNumaInfos.size(), 1);
    EXPECT_EQ(target.algoResult.exportNumaInfos.front().size, MIGRATE_SIZE_BYTES);
    EXPECT_EQ(targetImport->status.state, UBSE_MEM_IMPORT_SUCCESS);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedPreparationReturnsSchedulingFailure)
{
    auto source =
        std::make_shared<UbseMemShareBorrowExportObj>(BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES));
    source->status.exportObmmInfo.resize(1);
    auto targetImport = std::make_shared<UbseMemShareBorrowImportObj>();
    targetImport->status.importResults.resize(1);
    MOCKER(&ShareBorrowSchedule).expects(once()).will(returnValue(UBSE_ERR_ALLOCATE));
    UbseMemShareBorrowExportObj target{};
    EXPECT_EQ(migrate_detail::PrepareMigrateExport(source, MIGRATE_DST_NODE_ID, targetImport, target),
              UBSE_ERR_ALLOCATE);
    EXPECT_TRUE(target.status.exportObmmInfo.empty());
}

// 在账本放一个活跃导入节点，块数与迁移导出桩保持一致
// algoResult.exportNumaInfos 指向源节点(GetMaxRefCountExportObj 按 baseNodeId 关联导入 obj, 缺失则不入组)
void PutMigrateImportObj(const std::string& importNodeId)
{
    UbseMemShareBorrowImportObj importObj{};
    importObj.req.name = SHM_NAME;
    importObj.importNodeId = importNodeId;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtNumaInfo numaInfo{};
    numaInfo.nodeId = MIGRATE_SRC_NODE_ID;
    importObj.algoResult.exportNumaInfos.push_back(numaInfo);
    for (size_t i = 0; i < MIGRATE_EXPECT_BLOCK_COUNT; ++i) {
        UbseMemImportResult result{};
        result.memId = 2001 + i;
        importObj.status.importResults.push_back(result);
        ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult decoder{};
        decoder.hpa = 0x7000 + i * 0x1000;
        importObj.status.decoderResult.push_back(decoder);
        UbseMemObmmInfo obmmInfo{};
        obmmInfo.memId = 2001 + i;
        obmmInfo.desc.length = MIGRATE_SIZE_BYTES / MIGRATE_EXPECT_BLOCK_COUNT;
        importObj.exportObmmInfo.push_back(obmmInfo);
        importObj.migrateInfos.push_back(MakeShmMigrateInfo(0, i, 0, false));
    }
    PutShareImportObj(importNodeId, SHM_NAME, importObj);
}

// 逐块迁移 三条 RPC 的响应载荷桩(rsp errCode 由桩决定, RpcSend 返回 OK 不触发发送重试 sleep)
std::vector<uint32_t> g_migratePhaseOrder;
enum class TestResultKind
{
    Begin,
    Copy,
    End
};
struct HeldMigrateResult {
    UbseMemMigrateRpcInfo rpcInfo{};
    TestResultKind kind{TestResultKind::Begin};
};

void MigrateFaultQueryMockSet()
{
    const UbseRoleInfo master("1", ELECTION_ROLE_MASTER);
    MOCKER(&UbseGetCurrentNodeInfo).stubs().with(outBound(master)).will(returnValue(UBSE_OK));
}

void ExpectMigrateFaultStatus(std::optional<uint32_t> currentBlock)
{
    const auto target = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, target);
    for (uint32_t index = 0; index < target->status.exportObmmInfo.size(); ++index) {
        EXPECT_EQ(currentBlock == index ? UB_MEM_SWITCHING : UB_MEM_HEALTHY,
                  target->status.exportObmmInfo[index].memIdStatus)
            << "blockIndex=" << index;
    }
    UbseMemDebtQueryRequest request{};
    request.name = SHM_NAME;
    mem::def::UbseMemShmMemStatusDesc faults{};
    ASSERT_EQ(UBSE_OK, debt::UbseMemShmStatusGet(request, faults));
    const auto indexed =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetExportResourceByResId(SHM_NAME);
    const bool indexedTarget = indexed && indexed->algoResult.exportNumaInfos.front().nodeId == MIGRATE_DST_NODE_ID;
    currentBlock = indexedTarget ? currentBlock : std::nullopt;
    ASSERT_EQ(currentBlock ? 1U : 0U, faults.memIds.size());
    ASSERT_EQ(currentBlock ? 1U : 0U, faults.faultTypes.size());
    if (currentBlock) {
        EXPECT_EQ(target->status.exportObmmInfo[*currentBlock].memId, faults.memIds.front());
        EXPECT_EQ(UB_MEM_SWITCHING, faults.faultTypes.front());
    }
}

UbseResult ReportMigrateResult(const HeldMigrateResult& info, const UbseMemMigrateOpResp& response)
{
    auto result = response;
    result.rpcInfo = info.rpcInfo;
    if (info.kind == TestResultKind::End && result.errCode == UBSE_OK && !result.block) {
        const auto local = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
            info.rpcInfo.srcNodeId, info.rpcInfo.name);
        if (local && info.rpcInfo.blockIndex < local->exportObmmInfo.size()) {
            if (!info.rpcInfo.rollback && local->switchContext && local->switchContext->target) {
                result.block = local->switchContext->target;
                result.committed = true;
            } else if (!info.rpcInfo.rollback) {
                const auto target = GetShareExportObj(info.rpcInfo.srcNodeId, info.rpcInfo.name);
                if (target && info.rpcInfo.blockIndex < target->status.exportObmmInfo.size()) {
                    result.block = UbseMemMigrateBlockData{target->status.exportObmmInfo[info.rpcInfo.blockIndex],
                                                           std::nullopt, target->algoResult};
                    result.committed = true;
                }
            } else {
                result.block = migrate_detail::ReadMigrateBlock(*local, info.rpcInfo.blockIndex);
            }
            if (result.block) {
                result.block->exportInfo.memIdStatus = UB_MEM_HEALTHY;
            }
        }
    }
    const auto executor = ubse::mem::util::GetExecutor("ubseMemController");
    return executor != nullptr && executor->Execute([result, kind = info.kind]() {
        auto& service = UbseMemShmMigrateService::GetInstance();
        switch (kind) {
            case TestResultKind::Begin:
                service.BeginImportResult(result);
                break;
            case TestResultKind::Copy:
                service.CopyResult(result);
                break;
            case TestResultKind::End:
                service.EndResult(result);
                break;
        }
    }) ?
               UBSE_OK :
               UBSE_ERROR;
}

UbseResult MigrateSwitchBeginImportRpcRspStub(UbseComModule* /*module*/, const SendParam& /*sendParam*/,
                                              UbseMemMigrateSwitchBeginImportReqSimpoPtr& req,
                                              const HeldMigrateResult& resultInfo, uint32_t rspErr)
{
    UbseMemMigrateOpResp response{rspErr};
    if (rspErr == UBSE_OK) {
        const auto& data = req->GetUbseMesgInfo();
        response.block = UbseMemMigrateBlockData{
            data.targetBlock,
            ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult{0, 0x8000 + data.rpcInfo.blockIndex, 0x9000},
            data.targetLayout};
    }
    return ReportMigrateResult(resultInfo, response);
}

UbseResult MigrateSwitchBeginImportRpcOkStub(UbseComModule* module, const SendParam& sendParam,
                                             UbseMemMigrateSwitchBeginImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                             const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::Begin};
    g_migratePhaseOrder.push_back(req->GetUbseMesgInfo().rpcInfo.blockIndex * 3);
    return MigrateSwitchBeginImportRpcRspStub(module, sendParam, req, resultInfo, UBSE_OK);
}

std::atomic<uint32_t> g_beginBusinessCalls{0};
UbseResult MigrateSwitchBeginImportRpcFailStub(UbseComModule* module, const SendParam& sendParam,
                                               UbseMemMigrateSwitchBeginImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                               const bool withCopy)
{
    ++g_beginBusinessCalls;
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::Begin};
    return MigrateSwitchBeginImportRpcRspStub(module, sendParam, req, resultInfo, UBSE_MMI_OBMM_OP_FAILED);
}

UbseResult MigrateSwitchBeginImportOneNodeFailStub(UbseComModule* module, const SendParam& sendParam,
                                                   UbseMemMigrateSwitchBeginImportReqSimpoPtr& req,
                                                   UbseBaseMessagePtr& ack, const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::Begin};
    return MigrateSwitchBeginImportRpcRspStub(module, sendParam, req, resultInfo,
                                              sendParam.GetRemoteId() == "14" ? UBSE_MMI_OBMM_OP_FAILED : UBSE_OK);
}

std::atomic<uint32_t> g_beginSendAttempts{0};

UbseResult MigrateBeginSendFailStub(UbseComModule*, const SendParam&, UbseMemMigrateSwitchBeginImportReqSimpoPtr& req,
                                    UbseBaseMessagePtr& ack, const bool withCopy)
{
    ++g_beginSendAttempts;
    return UBSE_ERR_TIMEOUT; // 同步RPC超时属于发送失败，不能归为异步业务结果超时。
}

UbseResult MigrateDataCopyRpcOkStub(UbseComModule* /*module*/, const SendParam& /*sendParam*/,
                                    UbseMemMigrateDataCopyReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                    const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::Copy};
    g_migratePhaseOrder.push_back(req->GetUbseMesgInfo().rpcInfo.blockIndex * 3 + 1);
    return ReportMigrateResult(resultInfo, UbseMemMigrateOpResp{UBSE_OK});
}

// copy 桩：仅第一次返回业务失败，之后成功
int g_migrateCopyFailSeq = 0;

UbseResult MigrateDataCopyRpcFailOnceStub(UbseComModule* /*module*/, const SendParam& /*sendParam*/,
                                          UbseMemMigrateDataCopyReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                          const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::Copy};
    if (g_migrateCopyFailSeq++ == 0) {
        return ReportMigrateResult(resultInfo, UbseMemMigrateOpResp{UBSE_MMI_OBMM_OP_FAILED});
    }
    return ReportMigrateResult(resultInfo, UbseMemMigrateOpResp{UBSE_OK});
}

// copy 桩: 捕获 master 组包载荷与目标节点(拷贝执行点=导入节点)后返回OK
UbseMemMigrateDataCopyReq g_capturedCopyReq;
std::string g_copyTargetNodeId;

UbseResult MigrateDataCopyRpcCaptureStub(UbseComModule* module, const SendParam& sendParam,
                                         UbseMemMigrateDataCopyReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                         const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::Copy};
    g_capturedCopyReq = req->GetUbseMesgInfo();
    g_copyTargetNodeId = sendParam.GetRemoteId();
    return MigrateDataCopyRpcOkStub(module, sendParam, req, ack, withCopy);
}

UbseResult MigrateSwitchEndImportRpcOkStub(UbseComModule* /*module*/, const SendParam& /*sendParam*/,
                                           UbseMemMigrateSwitchEndImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                           const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::End};
    g_migratePhaseOrder.push_back(req->GetUbseMesgInfo().rpcInfo.blockIndex * 3 + 2);
    return ReportMigrateResult(resultInfo, UbseMemMigrateOpResp{UBSE_OK});
}

UbseResult MigrateBeginCheckFaultStub(UbseComModule* module, const SendParam& param,
                                      UbseMemMigrateSwitchBeginImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                      const bool withCopy)
{
    ExpectMigrateFaultStatus(req->GetUbseMesgInfo().rpcInfo.blockIndex);
    return MigrateSwitchBeginImportRpcOkStub(module, param, req, ack, withCopy);
}

UbseResult MigrateCopyCheckFaultStub(UbseComModule* module, const SendParam& param,
                                     UbseMemMigrateDataCopyReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                     const bool withCopy)
{
    ExpectMigrateFaultStatus(req->GetUbseMesgInfo().rpcInfo.blockIndex);
    return MigrateDataCopyRpcCaptureStub(module, param, req, ack, withCopy);
}

UbseResult MigrateCopyFailSecondBlockStub(UbseComModule* module, const SendParam& param,
                                          UbseMemMigrateDataCopyReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                          const bool withCopy)
{
    ExpectMigrateFaultStatus(req->GetUbseMesgInfo().rpcInfo.blockIndex);
    if (req->GetUbseMesgInfo().rpcInfo.blockIndex == 1) {
        return ReportMigrateResult({req->GetUbseMesgInfo().rpcInfo, TestResultKind::Copy},
                                   UbseMemMigrateOpResp{UBSE_MMI_OBMM_OP_FAILED});
    }
    return MigrateDataCopyRpcCaptureStub(module, param, req, ack, withCopy);
}

UbseResult MigrateEndCheckFaultStub(UbseComModule* module, const SendParam& param,
                                    UbseMemMigrateSwitchEndImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                    const bool withCopy)
{
    ExpectMigrateFaultStatus(req->GetUbseMesgInfo().rpcInfo.blockIndex);
    return MigrateSwitchEndImportRpcOkStub(module, param, req, ack, withCopy);
}

UbseResult MigrateSwitchEndImportLoseMasterImportStub(UbseComModule* module, const SendParam& sendParam,
                                                      UbseMemMigrateSwitchEndImportReqSimpoPtr& req,
                                                      UbseBaseMessagePtr& ack, const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::End};
    if (req->GetUbseMesgInfo().rpcInfo.blockIndex == MIGRATE_EXPECT_BLOCK_COUNT - 1) {
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().RemoveResource("13", SHM_NAME);
    }
    return MigrateSwitchEndImportRpcOkStub(module, sendParam, req, ack, withCopy);
}

UbseResult MigrateSwitchEndImportRpcFailStub(UbseComModule* /*module*/, const SendParam& /*sendParam*/,
                                             UbseMemMigrateSwitchEndImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                             const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::End};
    return ReportMigrateResult(resultInfo, UbseMemMigrateOpResp{UBSE_ERROR});
}

UbseResult FailCommitThenRollback(UbseComModule* module, const SendParam& param,
                                  UbseMemMigrateSwitchEndImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                  const bool withCopy)
{
    if (req->GetUbseMesgInfo().success == 1) {
        return ReportMigrateResult({req->GetUbseMesgInfo().rpcInfo, TestResultKind::End},
                                   UbseMemMigrateOpResp{UBSE_MMI_OBMM_OP_FAILED});
    }
    return MigrateSwitchEndImportRpcOkStub(module, param, req, ack, withCopy);
}

UbseResult MigrateSwitchEndImportFailSecondBlockStub(UbseComModule* module, const SendParam& sendParam,
                                                     UbseMemMigrateSwitchEndImportReqSimpoPtr& req,
                                                     UbseBaseMessagePtr& ack, const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::End};
    if (req->GetUbseMesgInfo().rpcInfo.blockIndex == 1) {
        return ReportMigrateResult(resultInfo, UbseMemMigrateOpResp{UBSE_ERROR});
    }
    return MigrateSwitchEndImportRpcOkStub(module, sendParam, req, ack, withCopy);
}

int g_migrateEndAgainSeq = 0;
UbseResult MigrateEndAgainOnceStub(UbseComModule* module, const SendParam& sendParam,
                                   UbseMemMigrateSwitchEndImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                   const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::End};
    if (g_migrateEndAgainSeq++ == 0) {
        return ReportMigrateResult(resultInfo, UbseMemMigrateOpResp{UBSE_ERROR_AGAIN});
    }
    return MigrateSwitchEndImportRpcOkStub(module, sendParam, req, ack, withCopy);
}

std::atomic<bool> g_beginHeld{false};
std::atomic<bool> g_copyHeld{false};
std::atomic<bool> g_endHeld{false};
HeldMigrateResult g_beginResultInfo{};
HeldMigrateResult g_copyResultInfo{};
HeldMigrateResult g_endResultInfo{};
UbseMemMigrateOpResp g_beginHeldResponse{};

UbseResult MigrateBeginHoldFirstStub(UbseComModule* module, const SendParam& sendParam,
                                     UbseMemMigrateSwitchBeginImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                     const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::Begin};
    if (req->GetUbseMesgInfo().rpcInfo.blockIndex != 0) {
        return MigrateSwitchBeginImportRpcOkStub(module, sendParam, req, ack, withCopy);
    }
    g_migratePhaseOrder.push_back(0);
    const auto& data = req->GetUbseMesgInfo();
    g_beginHeldResponse = {};
    g_beginHeldResponse.block = UbseMemMigrateBlockData{
        data.targetBlock, ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult{0, 0x8000, 0x9000},
        data.targetLayout};
    g_beginResultInfo = resultInfo;
    g_beginHeld.store(true, std::memory_order_release);
    return UBSE_OK;
}

UbseResult MigrateCopyHoldFirstStub(UbseComModule* module, const SendParam& sendParam,
                                    UbseMemMigrateDataCopyReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                    const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::Copy};
    if (req->GetUbseMesgInfo().rpcInfo.blockIndex != 0) {
        return MigrateDataCopyRpcOkStub(module, sendParam, req, ack, withCopy);
    }
    g_migratePhaseOrder.push_back(1);
    g_copyResultInfo = resultInfo;
    g_copyHeld.store(true, std::memory_order_release);
    return UBSE_OK;
}

UbseResult MigrateEndHoldFirstStub(UbseComModule* module, const SendParam& sendParam,
                                   UbseMemMigrateSwitchEndImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                   const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::End};
    if (req->GetUbseMesgInfo().rpcInfo.blockIndex != 0) {
        return MigrateSwitchEndImportRpcOkStub(module, sendParam, req, ack, withCopy);
    }
    g_migratePhaseOrder.push_back(2);
    g_endResultInfo = resultInfo;
    g_endHeld.store(true, std::memory_order_release);
    return UBSE_OK;
}

bool WaitHeld(const std::atomic<bool>& held)
{
    for (int retry = 0; retry < 500; ++retry) {
        if (held.load(std::memory_order_acquire)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

// 迁移编排线程池桩: 与现有 ubseMemController 同规格(2线程/16队列), 进程级单例跨用例复用
ubse::task_executor::UbseTaskExecutorPtr MigrateExecutorStub(const std::string& /*name*/)
{
    using ubse::task_executor::UbseTaskExecutor;
    using ubse::task_executor::UbseTaskExecutorPtr;
    static UbseTaskExecutorPtr executor = [] {
        auto exec = UbseTaskExecutor::Create("ubseMemControllerUT", 2, 16);
        exec->Start();
        g_migrateExecutor = exec;
        return exec;
    }();
    return g_migrateExecutorUnavailable.load() ? nullptr : executor;
}

template <typename BeginStub, typename CopyStub, typename EndStub>
void MigratePhase2RpcMockSet(BeginStub beginStub, CopyStub copyStub, EndStub endStub)
{
    const auto beginFunc = &UbseComModule::RpcSend<UbseMemMigrateSwitchBeginImportReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(beginFunc).stubs().will(invoke(beginStub));
    const auto copyFunc = &UbseComModule::RpcSend<UbseMemMigrateDataCopyReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(copyFunc).stubs().will(invoke(copyStub));
    const auto endFunc = &UbseComModule::RpcSend<UbseMemMigrateSwitchEndImportReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(endFunc).stubs().will(invoke(endStub));
}

testing::AssertionResult WaitMigrateTerminal(const std::string& name)
{
    // 必须早于单用例30秒watchdog返回，为断言诊断和异步任务收尾留出时间。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < deadline) {
        PumpMigrateTimers();
        UbseMemShmMigrateSession session{};
        if (UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(name, session) &&
            IsMigrateTerminalState(session.state)) {
            return testing::AssertionSuccess();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    UbseMemShmMigrateSession session{};
    if (!UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(name, session)) {
        return testing::AssertionFailure()
               << "Migration session missing while waiting for terminal state, name=" << name;
    }
    return testing::AssertionFailure() << "Migration did not reach terminal state within 20s, name=" << name
                                       << ", state=" << session.state << ", srcNodeId=" << session.srcNodeId
                                       << ", dstNodeId=" << session.dstNodeId
                                       << ", copyStarted=" << session.completedBlocks.has_value()
                                       << ", completedBlocks=" << session.completedBlocks.value_or(0)
                                       << ", failReason=" << session.failReason;
}
} // namespace

/*
 * 用例描述：内部参数校验，name或targetNodeId为空（SDK的slotId校验在SDK入口）
 * 预期结果：UBSE_ERR_OUT_OF_RANGE
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateInvalidParam)
{
    // 本用例只验证参数分支，鉴权先于参数校验执行。
    MOCKER_CPP(&UbseMemShmMigrateService::CheckUserPermission).stubs().will(returnValue(UBSE_OK));
    auto req = BuildMigrateReq(DST_SLOT_ID);
    req.name = "";
    EXPECT_EQ(UBSE_ERR_OUT_OF_RANGE, UbseMemShmMigrateService::GetInstance().Accept(req));

    req = BuildMigrateReq(DST_SLOT_ID);
    req.targetNodeId.clear();
    EXPECT_EQ(UBSE_ERR_OUT_OF_RANGE, UbseMemShmMigrateService::GetInstance().Accept(req));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMissingCallerRejected)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    auto req = BuildMigrateReq(DST_SLOT_ID);
    req.caller.username.clear();
    MOCKER(&AsyncMemShmMigrateProcessor).expects(never());
    EXPECT_EQ(UBSE_ERR_AUTH_FAILED, UbseMemShmMigrateService::GetInstance().Accept(req));
    EXPECT_FALSE(UbseMemShmMigrateSessionManager::GetInstance().HasActiveSession(SHM_NAME));
}

/*
 * 用例描述：借用关系不存在
 * 预期结果：UBSE_ERR_NOT_EXIST
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateBorrowNotExist)
{
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
}

/*
 * 用例描述：ExportObj状态不可迁移(非SUCCESS)
 * 预期结果：UBSE_ERR_MIGRATE_NOT_ALLOWED
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateStateNotAllowed)
{
    auto exportObj = BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    PutShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME, exportObj);

    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateNoActiveImportNode)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MigrateScheduleMockSet();
    ShmMigrateNodeMockSet(
        MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT,
                        0),
        MakeClusterNode(MIGRATE_DST_NODE_ID, DST_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING,
                        DST_AVAIL_BYTES));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
}

void ExpectLegacyMigrateRejectedBeforeAllocation()
{
    MigrateNodeMockSetForExecute();
    MOCKER(&migrate_detail::PrepareMigrateExport).expects(never());
    MOCKER(&AsyncMemShmMigrateProcessor).expects(never());
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    EXPECT_EQ(nullptr, UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME));
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateRejectsV1SourceBeforeAllocation)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    auto source = *GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    source.migrateInfos.clear(); // V1恢复保留旧账本，不生成迁移标记。
    PutShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME, source);

    ExpectLegacyMigrateRejectedBeforeAllocation();
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateRejectsV1TargetImportBeforeAllocation)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    auto targetImport = *GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    targetImport.migrateInfos.clear();
    PutShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME, targetImport);

    ExpectLegacyMigrateRejectedBeforeAllocation();
    EXPECT_NE(nullptr, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateRejectsV1OtherImportBeforeAllocation)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    PutMigrateImportObj("13");
    auto otherImport = *GetShareImportObj("13", SHM_NAME);
    otherImport.migrateInfos.clear();
    PutShareImportObj("13", SHM_NAME, otherImport);

    ExpectLegacyMigrateRejectedBeforeAllocation();
    EXPECT_NE(nullptr, GetShareImportObj("13", SHM_NAME));
}

/*
 * 用例描述：幂等: 同名已有非终态会话, 重复受理返回既有会话
 * 预期结果：UBSE_ERR_MIGRATE_IN_PROGRESS, 既有会话仍活跃
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateActiveSessionIdempotentHit)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    auto [session, isNew] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1);
    ASSERT_TRUE(isNew);

    EXPECT_EQ(UBSE_ERR_MIGRATE_IN_PROGRESS,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    EXPECT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().HasActiveSession(SHM_NAME));
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
}

/*
 * 用例描述：源节点健康(WORKING)时也允许主动迁移
 * 预期结果：UBSE_OK
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateSrcWorkingAllowed)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MigrateScheduleMockSet();
    PutMigrateImportObj("13");
    ShmMigrateNodeMockSet(
        MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING,
                        0),
        MakeClusterNode(MIGRATE_DST_NODE_ID, DST_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING,
                        DST_AVAIL_BYTES));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
}

/*
 * 用例描述：目标节点不存在(slotId无匹配)
 * 预期结果：UBSE_ERR_MIGRATE_NOT_ALLOWED
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateTargetNotExist)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    ShmMigrateNodeMockSet(MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID,
                                          ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT, 0),
                          ubse::nodeController::UbseNodeInfo{});

    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
}

/*
 * 用例描述：目标节点非WORKING
 * 预期结果：UBSE_ERR_MIGRATE_NOT_ALLOWED
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateTargetNotWorking)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    ShmMigrateNodeMockSet(
        MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT,
                        0),
        MakeClusterNode(MIGRATE_DST_NODE_ID, DST_SLOT_ID,
                        ubse::nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING, DST_AVAIL_BYTES));

    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
}

/*
 * 用例描述：目标节点等于源节点
 * 预期结果：UBSE_ERR_MIGRATE_NOT_ALLOWED
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateTargetEqualsSource)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    ShmMigrateNodeMockSet(
        MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT,
                        0),
        MakeClusterNode(MIGRATE_SRC_NODE_ID, DST_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING,
                        DST_AVAIL_BYTES));

    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
}

/*
 * 用例描述：目标已导入，但源导出账本缺少块信息
 * 预期结果：UBSE_ERR_MIGRATE_NOT_ALLOWED
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateTargetAlreadyImportNode)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    ShmMigrateNodeMockSet(
        MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT,
                        0),
        MakeClusterNode(MIGRATE_DST_NODE_ID, DST_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING,
                        DST_AVAIL_BYTES));

    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
}

/*
 * 用例描述：受理成功：会话INIT、槽位正确、幂等命中返回IN_PROGRESS且会话仍活跃
 * 预期结果：首次UBSE_OK；二次UBSE_ERR_MIGRATE_IN_PROGRESS
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateAcceptSuccessAndIdempotent)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MigrateScheduleMockSet();
    PutMigrateImportObj("13"); // 第三方导入节点(目标为未映射节点)
    ShmMigrateNodeMockSet(
        MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT,
                        0),
        MakeClusterNode(MIGRATE_DST_NODE_ID, DST_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING,
                        DST_AVAIL_BYTES));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));

    // 老 obj 仍在源节点账本
    auto exportObjPtr = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(exportObjPtr, nullptr);

    // 会话 INIT、槽位正确
    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_INIT, session->state);
    EXPECT_EQ(MIGRATE_SRC_NODE_ID, session->srcNodeId);
    EXPECT_EQ(MIGRATE_DST_NODE_ID, session->dstNodeId);

    // 幂等：同名二次受理 → IN_PROGRESS 且既有会话仍活跃
    EXPECT_EQ(UBSE_ERR_MIGRATE_IN_PROGRESS,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    EXPECT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().HasActiveSession(SHM_NAME));
}

/*
 * 用例描述：新借出投递失败：返回INTERNAL、会话置FAILED；终态后可重新受理
 * 预期结果：UBSE_ERR_INTERNAL；重新受理UBSE_OK
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateDispatchFailRollback)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MigrateScheduleMockSet();
    PutMigrateImportObj("13"); // 第三方导入节点(目标为未映射节点)
    ShmMigrateNodeMockSet(
        MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT,
                        0),
        MakeClusterNode(MIGRATE_DST_NODE_ID, DST_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING,
                        DST_AVAIL_BYTES));
    // 首次投递失败，重试投递成功（模拟恢复后重新受理）
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    EXPECT_EQ(UBSE_ERROR, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(MIGRATE_FAIL_DISPATCH, session->failReason);
    auto exportObjPtr = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(exportObjPtr, nullptr);

    // 终态后重新受理：新会话覆盖终态留存
    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_INIT, session->state);
}

/*
 * 用例描述：执行体，会话不存在
 * 预期结果：UBSE_ERR_NOT_EXIST
 */
/*
 * 用例描述：逐块迁移前置：会话非INIT态拒绝执行新导出准备
 * 预期结果：UBSE_ERR_MIGRATE_NOT_ALLOWED
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteRequiresInitSession)
{
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, UbseMemShmMigrateService::GetInstance().Execute("no_such_mem"));
    EXPECT_EQ(nullptr, UbseMemShmMigrateSessionManager::GetInstance().GetSession("no_such_mem"));
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();

    const auto prepared = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, prepared);
    UbseMemShmMigrateSessionManager::GetInstance().UpdateSessionState(SHM_NAME, UBS_MEM_MIGRATE_STATE_EXPORTING);
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    const auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(nullptr, session);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_EXPORTING, session->state);
    EXPECT_EQ(prepared, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_EQ(UBSE_MEM_EXPORT_RUNNING, prepared->status.state);
    EXPECT_TRUE(prepared->status.exportObmmInfo.empty());
}

/*
 * 用例描述：完整迁移流程 全链成功（begin/end 广播目标为预置的目标节点导入obj, 响应桩返回OK）
 * 预期结果：UBSE_OK；会话 SUCCESS、migratedMemids=8、endTime 落定；
 */
uint32_t MigrateNewLinkStub(const std::string&, const std::string&, UbseMemBorrowImportBaseObj& obj, bool, uint32_t)
{
    obj.algoResult.attachSocketId = 9;
    for (auto& block : obj.exportObmmInfo) {
        block.desc.scna = 21;
    }
    return UBSE_OK;
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateBeginUsesNewExportLayoutAndCredentials)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    MOCKER(&mem::controller::GetCnaInfoWhenImport).reset();
    MOCKER(&mem::controller::GetCnaInfoWhenImport).stubs().will(invoke(&MigrateNewLinkStub));
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&MigrateExportHoldCreateStub));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcCaptureStub,
                            &MigrateSwitchEndImportRpcOkStub);
    g_exportHeld.store(false);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_exportHeld));
    auto response = g_exportHeldResponse;
    response.algoResult.exportNumaInfos.front().socketId = 7;
    response.algoResult.exportNumaInfos.front().numaId = 8;
    response.algoResult.blockSize = 64;
    response.req.trustRingData.trustRingId = "test-new-export-ring";
    response.req.trustRingData.reqSignedData = "test-new-request-signature";
    response.req.trustRingData.lendSignedDatas.assign(MIGRATE_EXPECT_BLOCK_COUNT, "test-new-export-signature");
    ASSERT_EQ(UBSE_OK, ReportExportResult({}, response));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    EXPECT_EQ(SHM_NAME, g_capturedCopyReq.rpcInfo.name);
    EXPECT_EQ("13", g_capturedCopyReq.rpcInfo.srcNodeId);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteSuccess)
{
    MigrateFaultQueryMockSet();
    g_migratePhaseOrder.clear();
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13"); // 第三方导入节点: begin/end 广播目标 + copy 拷贝源(目标为未映射节点)
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateBeginCheckFaultStub, &MigrateCopyCheckFaultStub, &MigrateEndCheckFaultStub);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_EQ(MIGRATE_EXPECT_BLOCK_COUNT * 3, g_migratePhaseOrder.size());
    for (uint32_t step = 0; step < g_migratePhaseOrder.size(); ++step) {
        EXPECT_EQ(step, g_migratePhaseOrder[step]);
    }

    EXPECT_EQ("13", g_copyTargetNodeId);
    EXPECT_EQ(SHM_NAME, g_capturedCopyReq.rpcInfo.name);
    EXPECT_EQ("13", g_capturedCopyReq.rpcInfo.srcNodeId);

    // 会话 SUCCESS 且 新导出产出/逐块迁移块状态齐备
    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, session->state);
    EXPECT_EQ(MIGRATE_EXPECT_BLOCK_COUNT, session->completedBlocks.value_or(0));
    EXPECT_TRUE(session->completedBlocks.has_value());

    // 迁移终态不等待旧源回收；普通销毁回调处理完后移除旧账本，新obj保持为名称索引。
    g_migrateExecutor->Wait();
    UbseMemShmMigrateSession completed{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, completed));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, completed.state);
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
    auto newObjPtr = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(newObjPtr, nullptr);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, newObjPtr->status.state);
    EXPECT_EQ(MIGRATE_EXPECT_BLOCK_COUNT, newObjPtr->status.exportObmmInfo.size());
    EXPECT_EQ(MIGRATE_DST_NODE_ID, newObjPtr->req.baseNodeId);
    ExpectMigrateFaultStatus(std::nullopt);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateEndResultMissingMasterImportFails)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportLoseMasterImportStub);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    const auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(MIGRATE_FAIL_CONVERGE, session->failReason);
    EXPECT_EQ(MIGRATE_EXPECT_BLOCK_COUNT - 1, session->completedBlocks.value_or(0));
    EXPECT_FALSE(UbseMemShmMigrateService::GetInstance().HasActiveMigration(SHM_NAME));
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateOldExportDeleteDoesNotGateSuccess)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    g_cleanupHeld.store(false);
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&MigrateExportHoldOldCleanupStub));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_cleanupHeld));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
    const auto residual = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    EXPECT_EQ(nullptr, residual);
    ASSERT_EQ(UBSE_OK, ReportDeleteResult(g_cleanupHeldResponse));
    ASSERT_EQ(UBSE_OK, ReportDeleteResult(g_cleanupHeldResponse));
    g_migrateExecutor->Wait();
    g_cleanupResultInfo = {};
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
}

UbseResult FailOldExportAccountCleanup(SchedulerImpl* impl, UbseMemShareBorrowExportObj& obj)
{
    if (obj.status.state == UBSE_MEM_EXPORT_DESTROYED &&
        obj.algoResult.exportNumaInfos.front().nodeId == MIGRATE_SRC_NODE_ID) {
        return UBSE_ERROR;
    }
    return MigrateScheduleStub(impl, obj);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateOldAccountCleanupFailureKeepsTargetUsable)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_SRC_NODE_ID);
    auto selfImport = *GetShareImportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    selfImport.req.baseNodeId = MIGRATE_SRC_NODE_ID;
    selfImport.realExe = false;
    PutShareImportObj(MIGRATE_SRC_NODE_ID, SHM_NAME, selfImport);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowExportObj>)
        .stubs()
        .will(invoke(&FailOldExportAccountCleanup));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowImportObj>)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    MigrateRpcSuccessMockSet();
    g_holdOldDelete = true;
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
    EXPECT_FALSE(snapshot.rebuildRequired);
    g_migrateExecutor->Wait();
    const auto oldExport = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, oldExport);
    EXPECT_EQ(UBSE_MEM_EXPORT_DESTROYING, oldExport->status.state);
    const auto oldImport = GetShareImportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, oldImport);
    EXPECT_EQ(UBSE_MEM_IMPORT_SUCCESS, oldImport->status.state);
    const auto [selected, imports] = GetMaxRefCountExportObj(SHM_NAME);
    ASSERT_NE(nullptr, selected);
    EXPECT_EQ(MIGRATE_DST_NODE_ID, selected->algoResult.exportNumaInfos.front().nodeId);
    const auto indexed =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetExportResourceByResId(SHM_NAME);
    ASSERT_NE(nullptr, indexed);
    EXPECT_EQ(MIGRATE_DST_NODE_ID, indexed->req.baseNodeId);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateRollbackDeleteSendFailureAllowsReleaseButRequiresRebuild)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    g_failDeleteSend = true;
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcFailStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_EQ(MIGRATE_FAIL_BEGIN_IMPORT, snapshot.failReason);
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME, true));
    const auto residual = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    EXPECT_EQ(nullptr, residual);
    const auto [selected, imports] = GetMaxRefCountExportObj(SHM_NAME);
    ASSERT_NE(nullptr, selected);
    EXPECT_EQ(MIGRATE_SRC_NODE_ID, selected->algoResult.exportNumaInfos.front().nodeId);
}

TEST_F(TestUbseMemControllerMigrateApi, InternalExportDeleteFailureRecordsReportedState)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    auto response = *GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    response.returnReq = {};
    response.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    ShareExportUpdateState(response, UBSE_MEM_EXPORT_DESTROYING);
    BuildMigrateExportObjWithBlocks(MIGRATE_DST_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    response.status.state = UBSE_MEM_EXPORT_SUCCESS;
    response.errorCode = UBSE_ERROR;
    EXPECT_EQ(UBSE_OK, ShareExportMasterCallback(MIGRATE_SRC_NODE_ID, response));
    const auto residual = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, residual);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, residual->status.state);
    EXPECT_TRUE(residual->isDestroyedReportReceived);
    const auto [selected, imports] = GetMaxRefCountExportObj(SHM_NAME);
    ASSERT_NE(nullptr, selected);
    EXPECT_EQ(MIGRATE_SRC_NODE_ID, selected->algoResult.exportNumaInfos.front().nodeId);
    const auto indexed =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetExportResourceByResId(SHM_NAME);
    ASSERT_NE(nullptr, indexed);
    EXPECT_EQ(MIGRATE_SRC_NODE_ID, indexed->algoResult.exportNumaInfos.front().nodeId);
}

TEST_F(TestUbseMemControllerMigrateApi, ExportAccountCleanupFailureDeduplicatesRepeatedResult)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    auto response = *GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    response.returnReq = {};
    response.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    ShareExportUpdateState(response, UBSE_MEM_EXPORT_DESTROYING);
    BuildMigrateExportObjWithBlocks(MIGRATE_DST_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowExportObj>)
        .expects(once())
        .will(returnValue(UBSE_ERROR));
    response.status.state = UBSE_MEM_EXPORT_DESTROYED;
    EXPECT_EQ(UBSE_ERROR, ShareExportMasterCallback(MIGRATE_SRC_NODE_ID, response));
    const auto residual = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, residual);
    EXPECT_TRUE(residual->isDestroyedReportReceived);
    EXPECT_EQ(UBSE_MEM_EXPORT_DESTROYING, residual->status.state);
    response.status.state = UBSE_MEM_EXPORT_DESTROYED;
    EXPECT_EQ(UBSE_OK, ShareExportMasterCallback(MIGRATE_SRC_NODE_ID, response));
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
    const auto indexed =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetExportResourceByResId(SHM_NAME);
    ASSERT_NE(nullptr, indexed);
    EXPECT_EQ(MIGRATE_SRC_NODE_ID, indexed->algoResult.exportNumaInfos.front().nodeId);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateAsyncResultsGateNextPhase)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    g_migratePhaseOrder.clear();
    g_beginHeld.store(false);
    g_copyHeld.store(false);
    g_endHeld.store(false);
    MigratePhase2RpcMockSet(&MigrateBeginHoldFirstStub, &MigrateCopyHoldFirstStub, &MigrateEndHoldFirstStub);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_beginHeld));
    ASSERT_EQ(1U, g_migratePhaseOrder.size()); // begin 回调未到，不下发 copy

    auto unrelated = g_beginResultInfo;
    unrelated.rpcInfo.name = "unrelated-migration";
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(unrelated, g_beginHeldResponse));
    unrelated = g_beginResultInfo;
    unrelated.rpcInfo.srcNodeId = 999;
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(unrelated, g_beginHeldResponse));
    unrelated = g_beginResultInfo;
    ++unrelated.rpcInfo.blockIndex;
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(unrelated, g_beginHeldResponse));
    unrelated = g_beginResultInfo;
    ++unrelated.rpcInfo.migrateInfo;
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(unrelated, g_beginHeldResponse));
    unrelated = g_beginResultInfo;
    ++unrelated.rpcInfo.exportMemId;
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(unrelated, g_beginHeldResponse));
    unrelated = g_beginResultInfo;
    unrelated.rpcInfo.rollback = true;
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(unrelated, g_beginHeldResponse));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_FALSE(g_copyHeld.load()); // 归属、来源或轮次错误均不能推进阶段

    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_beginResultInfo, g_beginHeldResponse));
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_beginResultInfo, g_beginHeldResponse)); // 重复回调不得重复推进
    g_beginResultInfo = {};
    ASSERT_TRUE(WaitHeld(g_copyHeld));
    ASSERT_EQ(2U, g_migratePhaseOrder.size()); // copy 回调未到，不下发 end

    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_copyResultInfo, UbseMemMigrateOpResp{UBSE_OK}));
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_copyResultInfo, UbseMemMigrateOpResp{UBSE_OK}));
    g_copyResultInfo = {};
    ASSERT_TRUE(WaitHeld(g_endHeld));
    ASSERT_EQ(3U, g_migratePhaseOrder.size()); // end 回调未到，不开始下一块

    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_endResultInfo, UbseMemMigrateOpResp{UBSE_OK}));
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_endResultInfo, UbseMemMigrateOpResp{UBSE_OK}));
    g_endResultInfo = {};
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_EQ(MIGRATE_EXPECT_BLOCK_COUNT * 3, g_migratePhaseOrder.size());
    for (uint32_t step = 0; step < g_migratePhaseOrder.size(); ++step) {
        EXPECT_EQ(step, g_migratePhaseOrder[step]);
    }
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateBeginTimeoutIgnoresLateResult)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    g_migratePhaseOrder.clear();
    g_beginHeld.store(false);
    MigratePhase2RpcMockSet(&MigrateBeginHoldFirstStub, &MigrateDataCopyRpcOkStub, &MigrateSwitchEndImportRpcOkStub);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_beginHeld));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    const auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(0U, session->completedBlocks.value_or(0));
    EXPECT_EQ(MIGRATE_FAIL_RESULT_TIMEOUT, session->failReason);
    ASSERT_EQ(2U, g_migratePhaseOrder.size());

    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_beginResultInfo, g_beginHeldResponse));
    g_beginResultInfo = {};
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(2U, g_migratePhaseOrder.size()); // 迟到 begin 不能触发 copy
    const auto dstObj = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    EXPECT_EQ(dstObj, nullptr); // abort已确认，安全回收
}

/*
 * 用例描述：准备新导出时老ExportObj丢失
 * 预期结果：UBSE_ERR_NOT_EXIST；会话FAILED且failReason=OLD_OBJ_LOST
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteOldObjLost)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(MIGRATE_SRC_NODE_ID,
                                                                                              SHM_NAME);
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(static_cast<uint32_t>(MIGRATE_FAIL_OLD_OBJ_LOST), session->failReason);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateOtherExportNodeDoesNotAdvanceSession)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    mgr.UpdateSessionState(SHM_NAME, UBS_MEM_MIGRATE_STATE_EXPORTING);
    const auto prepared = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, prepared);
    auto response = BuildExportResponse(*prepared);
    response.algoResult.exportNumaInfos.front().nodeId = "invalid-node";
    UbseMemShmMigrateService::GetInstance().NewExportResult(response);
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_EXPORTING, snapshot.state);
    EXPECT_EQ(UBSE_OK, snapshot.failReason);
    EXPECT_FALSE(snapshot.rebuildRequired);
    EXPECT_TRUE(!snapshot.completedBlocks.has_value());
}

UbseResult RemoveCommittedExportOnAccountingStub(SchedulerImpl*, UbseMemShareBorrowExportObj& obj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(
        obj.algoResult.exportNumaInfos.front().nodeId, obj.req.name);
    return UBSE_OK;
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMissingCommittedExportFailsSession)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    AcceptMigrateOnly();
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    mgr.UpdateSessionState(SHM_NAME, UBS_MEM_MIGRATE_STATE_EXPORTING);
    const auto prepared = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, prepared);
    const auto response = BuildExportResponse(*prepared);
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowExportObj>)
        .stubs()
        .will(invoke(&RemoveCommittedExportOnAccountingStub));
    UbseMemShmMigrateService::GetInstance().NewExportResult(response);
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_EQ(MIGRATE_FAIL_CONVERGE, snapshot.failReason);
    EXPECT_TRUE(snapshot.rebuildRequired);
    EXPECT_TRUE(!snapshot.completedBlocks.has_value());
}

/*
 * 用例描述：新导出落账后通知决策失败
 * 预期结果：通知失败不阻断块迁移，目标登记成功且会话SUCCESS
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExportAccountingFailureDoesNotGateSuccess)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateRpcSuccessMockSet();
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowExportObj>)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
    EXPECT_EQ(0U, snapshot.failReason);
    EXPECT_FALSE(snapshot.rebuildRequired);
    EXPECT_EQ(MIGRATE_EXPECT_BLOCK_COUNT, snapshot.completedBlocks.value_or(0));
    const auto target = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, target);
    EXPECT_TRUE(target->isCreateReportReceived);
    EXPECT_EQ(MIGRATE_EXPECT_BLOCK_COUNT, target->status.exportObmmInfo.size());
    const auto oldExport = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, oldExport);
    EXPECT_EQ(UBSE_MEM_EXPORT_DESTROYING, oldExport->status.state);
}

/*
 * 用例描述：新导出准备同步RPC获取comModule失败(发送失败分支)
 * 预期结果：UBSE_ERROR_NULLPTR；会话FAILED且failReason=RPC
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteRpcFail)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MOCKER(&ubse::mem::util::GetExecutor).stubs().will(invoke(&MigrateExecutorStub));
    // comModule为空 → 发送失败分支(避免重试sleep)
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<com::UbseComModule>()));

    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(static_cast<uint32_t>(MIGRATE_FAIL_RPC), session->failReason);
    auto dstObjPtr = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(dstObjPtr, nullptr);
    EXPECT_EQ(UBSE_MEM_EXPORT_RUNNING, dstObjPtr->status.state);
    EXPECT_TRUE(session->rebuildRequired);
    auto oldObjPtr = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(oldObjPtr, nullptr);
}

/*
 * 用例描述：新导出准备目标节点导出执行失败(rsp errCode非0)
 * 预期结果：UBSE_MMI_OBMM_OP_FAILED；会话FAILED且failReason=OBMM_EXPORT
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteObmmExportFail)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MOCKER(&ubse::mem::util::GetExecutor).stubs().will(invoke(&MigrateExecutorStub));
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>)
        .stubs()
        .will(returnValue(std::make_shared<com::UbseComModule>()));
    const auto func = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(invoke(&MigrateExportRpcObmmFailStub));
    MOCKER(&DeleteShareExport).stubs().will(invoke(&MigrateDeleteShareExportStub));

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(static_cast<uint32_t>(MIGRATE_FAIL_OBMM_EXPORT), session->failReason);
    auto dstObjPtr = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    EXPECT_EQ(dstObjPtr, nullptr);
}

/*
 * 用例描述：新导出准备 rsp块数与老obj不一致(agent回传0块, 老obj 8块) → 老/新块数单点比对失败
 * 预期结果：UBSE_ERR_INTERNAL；会话FAILED且failReason=LAYOUT
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteLayoutMismatch)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13"); // 第三方导入节点: begin/end 广播目标 + copy 拷贝源(目标为未映射节点)
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MOCKER(&ubse::mem::util::GetExecutor).stubs().will(invoke(&MigrateExecutorStub));
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>)
        .stubs()
        .will(returnValue(std::make_shared<com::UbseComModule>()));
    // RpcSend返回OK但rsp不携带块信息 → 老 obj 8块 vs rsp 0块
    const auto func = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(invoke(&MigrateExportRpcNoBlockStub));
    MOCKER(&DeleteShareExport).stubs().will(invoke(&MigrateDeleteShareExportStub));

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(static_cast<uint32_t>(MIGRATE_FAIL_LAYOUT), session->failReason);
    auto dstObjPtr = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(dstObjPtr, nullptr);
    EXPECT_TRUE(session->rebuildRequired);
}

/*
 * 用例描述：逐块迁移前置校验失败(老obj块数与新obj块数不一致)
 * 预期结果：UBSE_ERR_INTERNAL；会话FAILED且failReason=LAYOUT
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteOldBlockCountMismatch)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES); // 老obj无块
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    SendShareExportObjMockSet(); // 新obj强删链路(DeleteShareExport)发送桩

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(static_cast<uint32_t>(MIGRATE_FAIL_LAYOUT), session->failReason);
    auto dstObjPtr = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(dstObjPtr, nullptr);
    EXPECT_TRUE(session->rebuildRequired);
}

/*
 * 用例描述：逐块迁移begin失败，执行一次清理
 * 预期结果：会话FAILED且failReason=BEGIN_IMPORT；单次回退后发起无引用新导出清理
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateBeginFailureCleansOnce)
{
    g_beginBusinessCalls.store(0);
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13"); // 第三方导入节点: begin/end 广播目标 + copy 拷贝源(目标为未映射节点)
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    SendShareExportObjMockSet(); // 新obj强删链路(DeleteShareExport)发送桩
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcFailStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(static_cast<uint32_t>(MIGRATE_FAIL_BEGIN_IMPORT), session->failReason);
    auto dstObjPtr = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    EXPECT_EQ(dstObjPtr, nullptr);
    auto oldObjPtr = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(oldObjPtr, nullptr);
    EXPECT_EQ(1U, g_beginBusinessCalls.load());
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateBeginReasonSurvivesRollbackFailure)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcFailStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcFailStub);
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateGetResp progress{};
    service.Get(SHM_NAME, progress);
    EXPECT_EQ(UBSE_OK, progress.errCode);
    EXPECT_EQ(MIGRATE_FAIL_BEGIN_IMPORT, progress.failReason);
    EXPECT_NE(std::string::npos, progress.errMsg.find("begin_import"));
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateTimerFailureIsNotBusinessTimeout)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).reset();
    MOCKER(&ubse::timer::UbseTimerHandlerRegister)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(invoke(&MigrateTimerRegisterStub));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateGetResp progress{};
    service.Get(SHM_NAME, progress);
    EXPECT_EQ(MIGRATE_FAIL_TIMER, progress.failReason);
    EXPECT_NE(std::string::npos, progress.errMsg.find("timer registration"));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateRequestPreparationFailureIsNotRpcFailure)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MOCKER(&mem::controller::GetCnaInfoWhenImport).reset();
    MOCKER(&mem::controller::GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_ERROR));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateGetResp progress{};
    service.Get(SHM_NAME, progress);
    EXPECT_EQ(MIGRATE_FAIL_REQUEST_PREPARE, progress.failReason);
    EXPECT_NE(std::string::npos, progress.errMsg.find("request preparation"));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigratePartialBeginRollbackRestoresMasterLedger)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    PutMigrateImportObj("14");
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportOneNodeFailStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    MOCKER(&DeleteShareExport).stubs().will(invoke(&MigrateDeleteExportConfirmStub));

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    const auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(0U, session->completedBlocks.value_or(0));
    const auto importObj = GetShareImportObj("13", SHM_NAME);
    ASSERT_NE(nullptr, importObj);
    ASSERT_FALSE(importObj->exportObmmInfo.empty());
    EXPECT_EQ(UB_MEM_HEALTHY, importObj->exportObmmInfo[0].memIdStatus);
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_TRUE(session->rebuildRequired);
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().CheckOperationAllowed(SHM_NAME, true));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateAsyncSendFailureDoesNotWaitForCallback)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateBeginSendFailStub, &MigrateDataCopyRpcOkStub, &MigrateSwitchEndImportRpcOkStub);

    g_beginSendAttempts.store(0);
    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    EXPECT_EQ(SEND_RETRY_TIMES, g_beginSendAttempts.load());
    const auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(MIGRATE_FAIL_RPC, session->failReason);
    const auto dstObj = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    EXPECT_EQ(dstObj, nullptr); // 单次abort确认后可安全清理新导出
}

/*
 * 用例描述：逐块迁移copy首次失败，执行一次清理，不再重试
 * 预期结果：会话FAILED，完成块数为0，copy仅执行一次
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCopyFailureDoesNotRetry)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13"); // 第三方导入节点: begin/end 广播目标 + copy 拷贝源(目标为未映射节点)
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    g_migrateCopyFailSeq = 0;
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcFailOnceStub,
                            &MigrateSwitchEndImportRpcOkStub);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(0U, session->completedBlocks.value_or(0));
    EXPECT_EQ(1, g_migrateCopyFailSeq); // 一次业务调用；end另有一次abort
    EXPECT_EQ(MIGRATE_FAIL_COPY, session->failReason);
}

/*
 * 用例描述：逐块迁移end(1)失败，执行一次end(0)清理，保留可能已提交资源
 * 预期结果：会话FAILED且failReason=END_IMPORT；保留原始提交失败原因
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteEndFailIrreversible)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13"); // 第三方导入节点: begin/end 广播目标 + copy 拷贝源(目标为未映射节点)
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcFailStub);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(static_cast<uint32_t>(MIGRATE_FAIL_END_IMPORT), session->failReason);
    ASSERT_TRUE(session->completedBlocks.has_value());
    EXPECT_EQ(0U, *session->completedBlocks);
    // end(1) 的响应失败不能证明远端未切换，新导出仍可能被使用。
    auto dstObjPtr = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(dstObjPtr, nullptr);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, dstObjPtr->status.state);
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED,
              UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigratePartialCommitVisibleInMasterLedger)
{
    MigrateFaultQueryMockSet();
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportFailSecondBlockStub);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    const auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(1U, session->completedBlocks.value_or(0));
    const auto importObj = GetShareImportObj("13", SHM_NAME);
    ASSERT_NE(nullptr, importObj);
    ASSERT_GE(importObj->exportObmmInfo.size(), 2U);
    EXPECT_EQ(1U, importObj->exportObmmInfo[0].memId);
    EXPECT_EQ(UB_MEM_HEALTHY, importObj->exportObmmInfo[0].memIdStatus);
    EXPECT_EQ(2002U, importObj->exportObmmInfo[1].memId);
    EXPECT_EQ(UB_MEM_SWITCHING, importObj->exportObmmInfo[1].memIdStatus);
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    for (size_t index = 2; index < importObj->exportObmmInfo.size(); ++index) {
        EXPECT_EQ(UB_MEM_HEALTHY, importObj->exportObmmInfo[index].memIdStatus);
    }
    ExpectMigrateFaultStatus(1);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateLaterRollbackClearsCurrentStatusAndKeepsCompletedBlocks)
{
    MigrateFaultQueryMockSet();
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    PutMigrateImportObj("14");
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateBeginCheckFaultStub, &MigrateCopyFailSecondBlockStub, &MigrateEndCheckFaultStub);

    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_EQ(MIGRATE_FAIL_COPY, snapshot.failReason);
    EXPECT_EQ(1U, snapshot.completedBlocks.value_or(0));
    ExpectMigrateFaultStatus(std::nullopt);
    for (const auto* nodeId : {"13", "14"}) {
        const auto local = GetShareImportObj(nodeId, SHM_NAME);
        ASSERT_NE(nullptr, local);
        ASSERT_EQ(MIGRATE_EXPECT_BLOCK_COUNT, local->exportObmmInfo.size());
        EXPECT_EQ(1U, local->exportObmmInfo[0].memId);
        EXPECT_EQ(2002U, local->exportObmmInfo[1].memId);
        for (const auto& block : local->exportObmmInfo) {
            EXPECT_EQ(UB_MEM_HEALTHY, block.memIdStatus);
        }
    }
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateFailedFirstCommitThenConfirmedRollbackAllowsRelease)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    PutMigrateImportObj("14");
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub, &FailCommitThenRollback);
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    g_migrateExecutor->Wait();
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_TRUE(snapshot.rebuildRequired);
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_FALSE(service.HasActiveMigration(SHM_NAME)); // 已回退的匿名内存也允许普通回收。
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME, true));
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateEndFailureDoesNotRetry)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    g_migrateEndAgainSeq = 0;
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub, &MigrateEndAgainOnceStub);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    const auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, session->state);
    EXPECT_EQ(0U, session->completedBlocks.value_or(0));
    EXPECT_EQ(2, g_migrateEndAgainSeq); // 一次业务调用；end另有一次abort
    EXPECT_EQ(MIGRATE_FAIL_END_IMPORT, session->failReason);
}

/*
 * 用例描述：迁移期间 detach 被拒(master 侧活跃会话互斥)
 * 预期结果：UBSE_ERR_MIGRATE_IN_PROGRESS
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateDetachRejectedDuringMigration)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    UbseMemShareBorrowImportObj importObj{};
    importObj.req.name = SHM_NAME;
    importObj.importNodeId = NODE_ONE;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    // 指向源节点(GetMaxRefCountExportObj 按 baseNodeId 关联导入 obj, 缺失则 ExistImportObj 失败)
    UbseMemDebtNumaInfo numaInfo{};
    numaInfo.nodeId = MIGRATE_SRC_NODE_ID;
    importObj.algoResult.exportNumaInfos.push_back(numaInfo);
    PutShareImportObj(NODE_ONE, SHM_NAME, importObj);

    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();

    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    // BuildOperationRespWhenFail 仅在 master 角色时填充 resp.errorCode(与 UbseMemShareDetachTest 同模式)
    UbseRoleInfo currentNode;
    currentNode.nodeId = NODE_ONE;
    currentNode.nodeRole = election::ELECTION_ROLE_MASTER;
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentNode)).will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    UbseMemShareDetachReq req;
    UbseMemOperationResp resp;
    req.name = SHM_NAME;
    req.unImportNodeId = NODE_ONE;
    for (const auto state : {UBS_MEM_MIGRATE_STATE_INIT, UBS_MEM_MIGRATE_STATE_EXPORTING,
                             UBS_MEM_MIGRATE_STATE_DATA_COPYING, UBS_MEM_MIGRATE_STATE_FINISHING}) {
        SCOPED_TRACE(testing::Message() << "state=" << state);
        UbseMemShmMigrateSessionManager::GetInstance().UpdateSessionState(SHM_NAME, state);
        EXPECT_EQ(UBSE_ERR_MIGRATE_IN_PROGRESS, UbseMemShareDetach(req, resp, NODE_ONE));
        EXPECT_EQ(UBSE_ERR_MIGRATE_IN_PROGRESS, resp.errorCode);
    }
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateBlocksOtherShareOperations)
{
    MOCKER(IsMemShareFeatureSupported).stubs().will(returnValue(true));
    auto& sessions = UbseMemShmMigrateSessionManager::GetInstance();
    ASSERT_TRUE(sessions.GetOrCreateSession(SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1).second);
    UbseRoleInfo currentNode;
    currentNode.nodeId = NODE_ONE;
    currentNode.nodeRole = election::ELECTION_ROLE_MASTER;
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentNode)).will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));

    UbseMemOperationResp resp{};
    UbseMemShareBorrowReq borrow{};
    borrow.name = SHM_NAME;
    EXPECT_EQ(UBSE_ERR_MIGRATE_IN_PROGRESS, UbseMemShareBorrow(borrow, resp));

    UbseMemShareAttachReq attach{};
    attach.name = SHM_NAME;
    EXPECT_EQ(UBSE_ERR_MIGRATE_IN_PROGRESS, UbseMemShareAttach(attach, resp));

    UbseMemReturnReq ret{};
    ret.name = SHM_NAME;
    EXPECT_EQ(UBSE_ERR_MIGRATE_IN_PROGRESS, UbseMemShareReturn(ret, resp, NODE_ONE));

    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    sessions.SetRebuildRequired(SHM_NAME, true);
    sessions.FinishSession(SHM_NAME, UBS_MEM_MIGRATE_STATE_FAILED, MIGRATE_FAIL_COPY);
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, UbseMemShareBorrow(borrow, resp));
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, UbseMemShareAttach(attach, resp));
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(MIGRATE_SRC_NODE_ID,
                                                                                              SHM_NAME);
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, UbseMemShareReturn(ret, resp, NODE_ONE)); // 已通过迁移门禁，按普通归还检查资源。
}

namespace {
void SetMigrationSessionForRelease(ubs_mem_migrate_state_t state, bool rebuildRequired)
{
    auto& sessions = UbseMemShmMigrateSessionManager::GetInstance();
    sessions.ClearSessions();
    ASSERT_TRUE(sessions.GetOrCreateSession(SHM_NAME, NODE_TWO, NODE_ONE, 1).second);
    sessions.UpdateSessionState(SHM_NAME, state);
    sessions.SetRebuildRequired(SHM_NAME, rebuildRequired);
}
} // namespace

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateTerminalSessionRetainedUntilAllResourcesReleased)
{
    auto& sessions = UbseMemShmMigrateSessionManager::GetInstance();
    auto& ledger = UbseMemDebtLedger::GetInstance();
    MigrateScheduleMockSet();
    const auto completeSourceDelete = [] {
        auto source = BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
        source.status.state = UBSE_MEM_EXPORT_DESTROYED;
        source.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        return ShareExportMasterCallback(MIGRATE_SRC_NODE_ID, source);
    };
    for (const auto state :
         {UBS_MEM_MIGRATE_STATE_SUCCESS, UBS_MEM_MIGRATE_STATE_FAILED, UBS_MEM_MIGRATE_STATE_CANCELED}) {
        SCOPED_TRACE(state);
        SetMigrationSessionForRelease(state, true);
        // 直接写节点账本，不建立名称索引：清理必须检查所有节点实际资源。
        UbseMemShareBorrowExportObj target{};
        target.req.name = SHM_NAME;
        PutShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME, target);
        ASSERT_EQ(UBSE_OK, completeSourceDelete());
        UbseMemShmMigrateSession snapshot{};
        ASSERT_TRUE(sessions.ReadSessionSnapshot(SHM_NAME, snapshot));
        EXPECT_EQ(state, snapshot.state);
        EXPECT_TRUE(snapshot.rebuildRequired);

        PutMigrateImportObj("13");
        ledger.GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(MIGRATE_DST_NODE_ID, SHM_NAME);
        ASSERT_EQ(UBSE_OK, completeSourceDelete());
        ASSERT_TRUE(sessions.ReadSessionSnapshot(SHM_NAME, snapshot));
        EXPECT_TRUE(snapshot.rebuildRequired);

        ledger.GetDebtMap<UbseMemShareBorrowImportObj>().RemoveResource("13", SHM_NAME);
        ASSERT_EQ(UBSE_OK, completeSourceDelete());
        UbseMemShmMigrateGetResp response{};
        UbseMemShmMigrateService::GetInstance().Get(SHM_NAME, response);
        EXPECT_EQ(UBSE_ERR_NOT_EXIST, response.errCode);
    }
    SetMigrationSessionForRelease(UBS_MEM_MIGRATE_STATE_DATA_COPYING, false);
    ASSERT_EQ(UBSE_OK, completeSourceDelete());
    EXPECT_TRUE(sessions.HasActiveSession(SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateReleaseDependsOnlyOnActiveSession)
{
    auto& sessions = UbseMemShmMigrateSessionManager::GetInstance();
    auto& service = UbseMemShmMigrateService::GetInstance();
    sessions.ClearSessions();
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME, true));
    EXPECT_FALSE(service.HasActiveMigration(SHM_NAME));
    const std::pair<ubs_mem_migrate_state_t, bool> cases[] = {
        {UBS_MEM_MIGRATE_STATE_INIT, true},         {UBS_MEM_MIGRATE_STATE_EXPORTING, true},
        {UBS_MEM_MIGRATE_STATE_DATA_COPYING, true}, {UBS_MEM_MIGRATE_STATE_FINISHING, true},
        {UBS_MEM_MIGRATE_STATE_SUCCESS, false},     {UBS_MEM_MIGRATE_STATE_FAILED, false},
        {UBS_MEM_MIGRATE_STATE_CANCELED, false}};
    for (const auto& [state, active] : cases) {
        for (const bool rebuildRequired : {false, true}) {
            SCOPED_TRACE(testing::Message() << "state=" << state << ", rebuild=" << rebuildRequired);
            SetMigrationSessionForRelease(state, rebuildRequired);
            EXPECT_EQ(active ? UBSE_ERR_MIGRATE_IN_PROGRESS : UBSE_OK, service.CheckOperationAllowed(SHM_NAME, true));
            EXPECT_EQ(active, service.HasActiveMigration(SHM_NAME));
            UbseMemShmMigrateSession snapshot{};
            ASSERT_TRUE(sessions.ReadSessionSnapshot(SHM_NAME, snapshot));
            EXPECT_EQ(rebuildRequired, snapshot.rebuildRequired);
        }
    }
}

// ==== migrate_get 查询(活跃优先/终态留存/同name覆盖) UT ====

/*
 * 用例描述：无任何会话记录时查询
 * 预期结果：errCode=UBSE_ERR_NOT_EXIST
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateGetNotExist)
{
    UbseMemShmMigrateGetResp resp{};
    UbseMemShmMigrateService::GetInstance().Get("no_such_mem", resp);
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, resp.errCode);
}

/*
 * 用例描述：活跃会话查询：进度字段按会话派生(逐块迁移前用受理时老obj块数, totalSize使用受理时记录的值)
 * 预期结果：errCode=0, state/srcNodeId/dstNodeId/totalMemids 正确, FAILED 之外无 errMsg
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateGetActiveSession)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    auto [session, isNew] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 3);
    ASSERT_TRUE(isNew);

    UbseMemShmMigrateGetResp resp{};
    UbseMemShmMigrateService::GetInstance().Get(SHM_NAME, resp);
    EXPECT_EQ(UBSE_OK, resp.errCode);
    EXPECT_EQ(SHM_NAME, resp.name);
    EXPECT_EQ(MIGRATE_SRC_NODE_ID, resp.srcNodeId);
    EXPECT_EQ(MIGRATE_DST_NODE_ID, resp.dstNodeId);
    EXPECT_EQ(static_cast<uint32_t>(UBS_MEM_MIGRATE_STATE_INIT), resp.state);
    EXPECT_EQ(3U, resp.totalMemids); // EXPORTING 前=老 obj 块数
    EXPECT_EQ(0U, resp.migratedMemids);
    EXPECT_EQ(0U, resp.totalSize); // 此测试创建会话时未提供totalSize，默认0
    EXPECT_EQ(0U, resp.failReason);
    EXPECT_TRUE(resp.errMsg.empty());
}

/*
 * 用例描述：终态留存查询：FAILED 会话终态后仍可查到 state/failReason/errMsg
 * 预期结果：errCode=0, state=FAILED, failReason=MIGRATE_FAIL_COPY, errMsg 非空
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateGetTerminalRetained)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    auto [session, isNew] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1);
    ASSERT_TRUE(isNew);
    (void)session; // 会话以 name 寻址, 快照指针不参与断言
    UbseMemShmMigrateSessionManager::GetInstance().FinishSession(SHM_NAME, UBS_MEM_MIGRATE_STATE_FAILED,
                                                                 MIGRATE_FAIL_COPY);

    UbseMemShmMigrateGetResp resp{};
    UbseMemShmMigrateService::GetInstance().Get(SHM_NAME, resp);
    EXPECT_EQ(UBSE_OK, resp.errCode);
    EXPECT_EQ(static_cast<uint32_t>(UBS_MEM_MIGRATE_STATE_FAILED), resp.state);
    EXPECT_EQ(static_cast<uint32_t>(MIGRATE_FAIL_COPY), resp.failReason);
    EXPECT_FALSE(resp.errMsg.empty());
}

/*
 * 用例描述：终态后重新受理覆盖旧终态记录：新会话可见、旧终态记录被覆盖
 * 预期结果：重新受理后查询指向 INIT 新会话
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateGetTerminalOverwrittenByNewAccept)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MigrateScheduleMockSet();
    PutMigrateImportObj("13"); // 第三方导入节点(目标为未映射节点)
    ShmMigrateNodeMockSet(
        MakeClusterNode(MIGRATE_SRC_NODE_ID, SRC_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_FAULT,
                        0),
        MakeClusterNode(MIGRATE_DST_NODE_ID, DST_SLOT_ID, ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING,
                        DST_AVAIL_BYTES));
    // 首次受理后置终态(FAILED)
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    UbseMemShmMigrateGetResp resp{};
    UbseMemShmMigrateService::GetInstance().Get(SHM_NAME, resp);
    EXPECT_EQ(UBSE_OK, resp.errCode);
    EXPECT_EQ(static_cast<uint32_t>(UBS_MEM_MIGRATE_STATE_FAILED), resp.state);

    // 终态后重新受理：查询指向新会话, 旧终态记录被覆盖
    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    UbseMemShmMigrateGetResp resp2{};
    UbseMemShmMigrateService::GetInstance().Get(SHM_NAME, resp2);
    EXPECT_EQ(UBSE_OK, resp2.errCode);
    EXPECT_EQ(static_cast<uint32_t>(UBS_MEM_MIGRATE_STATE_INIT), resp2.state);
    EXPECT_NE(nullptr, UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME));
}

// ==== migrate_cancel 取消链路 UT ====

/*
 * 用例描述：cancel 时无任何会话记录(无活跃/无终态留存)
 * 预期结果：resp.errCode=UBSE_ERR_NOT_EXIST
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCancelNotExist)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    UbseMemMigrateCancelResp resp{};
    UbseMemShmMigrateService::GetInstance().Cancel("no_such_mem", resp);
    EXPECT_EQ(static_cast<uint32_t>(UBSE_ERR_NOT_EXIST), resp.errCode);
}

/*
 * 用例描述：INIT 态取消受理成功，cancelRequested 置位；重复取消幂等返回 0
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCancelAcceptedAndIdempotent)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    auto [session, isNew] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1);
    ASSERT_TRUE(isNew);

    UbseMemMigrateCancelResp resp{};
    UbseMemShmMigrateService::GetInstance().Cancel(SHM_NAME, resp);
    EXPECT_EQ(UBSE_OK, resp.errCode);
    EXPECT_TRUE(session->cancelRequested);
    EXPECT_TRUE(session->cancelRequested);

    UbseMemMigrateCancelResp resp2{};
    UbseMemShmMigrateService::GetInstance().Cancel(SHM_NAME, resp2);
    EXPECT_EQ(UBSE_OK, resp2.errCode);
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
}

/*
 * 用例描述：EXPORTING(新导出执行中, 新借出完成前)仍允许取消
 * 预期结果：受理成功, 会话 取消标志已设置
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCancelAllowedInExporting)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    auto [session, isNew] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1);
    ASSERT_TRUE(isNew);
    UbseMemShmMigrateSessionManager::GetInstance().UpdateSessionState(SHM_NAME, UBS_MEM_MIGRATE_STATE_EXPORTING);

    UbseMemMigrateCancelResp resp{};
    UbseMemShmMigrateService::GetInstance().Cancel(SHM_NAME, resp);
    EXPECT_EQ(UBSE_OK, resp.errCode);
    EXPECT_TRUE(session->cancelRequested);
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
}

/*
 * 用例描述：DATA_COPYING 及之后拒绝取消；终态(FAILED)同样拒绝
 * 预期结果：两次取消均 UBSE_ERR_MIGRATE_NOT_ALLOWED
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCancelRejectedAfterExportedAndTerminal)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    auto [session, isNew] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1);
    ASSERT_TRUE(isNew);
    // 块迁移前取消检查点 消费原语进入逐块迁移(取消检查点已通过)
    auto& copyStartManager = UbseMemShmMigrateSessionManager::GetInstance();
    copyStartManager.UpdateSessionState(SHM_NAME, UBS_MEM_MIGRATE_STATE_EXPORTING);
    EXPECT_EQ(UbseMemShmMigrateSessionManager::CopyStartResult::STARTED,
              copyStartManager.TryStartCopy(SHM_NAME, copyStartManager.GetSession(SHM_NAME)));

    UbseMemMigrateCancelResp resp{};
    UbseMemShmMigrateService::GetInstance().Cancel(SHM_NAME, resp);
    EXPECT_EQ(static_cast<uint32_t>(UBSE_ERR_MIGRATE_NOT_ALLOWED), resp.errCode);

    // 终态(FAILED 留存)后取消仍拒绝
    UbseMemShmMigrateSessionManager::GetInstance().FinishSession(SHM_NAME, UBS_MEM_MIGRATE_STATE_FAILED,
                                                                 MIGRATE_FAIL_COPY);
    UbseMemMigrateCancelResp resp2{};
    UbseMemShmMigrateService::GetInstance().Cancel(SHM_NAME, resp2);
    EXPECT_EQ(static_cast<uint32_t>(UBSE_ERR_MIGRATE_NOT_ALLOWED), resp2.errCode);
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
}

/*
 * 用例描述：导出前取消检查点消费：取消受理后(INIT置取消标志)执行体跳过新导出准备 走清理
 * 预期结果：UBSE_OK；会话 CANCELED(终态留存可查)、endTime 落定
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteConsumeCancelAtCp1)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();

    UbseMemMigrateCancelResp resp{};
    UbseMemShmMigrateService::GetInstance().Cancel(SHM_NAME, resp);
    ASSERT_EQ(UBSE_OK, resp.errCode);

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_CANCELED, session->state);
    auto oldObjPtr = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(oldObjPtr, nullptr);
    // 终态留存: migrate_get 语义可查 CANCELED
    auto found = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(SHM_NAME, found->name);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_CANCELED, found->state);
}

/*
 * 用例描述：块迁移前取消检查点竞态：新导出准备 RPC 成功窗口内 cancel 受理 → 不进入逐块迁移, 回收目标节点半成品
 * 预期结果：UBSE_OK；会话 CANCELED 且无 新导出产出；目标节点新 obj 账本回滚
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExecuteConsumeCancelAtCp2)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet(); // 同一函数指针不可重复mock, 竞态桩内部委托成功桩
    const auto func = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(invoke(&MigrateExportRpcCancelRaceStub));
    MOCKER(&DeleteShareExport).stubs().will(invoke(&MigrateDeleteExportConfirmStub));

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));

    auto session = UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_CANCELED, session->state);
    // 本用例的普通归还桩同步完成销毁，目标账本已移除。
    auto dstObjPtr = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    EXPECT_EQ(dstObjPtr, nullptr);
    auto oldObjPtr = GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME);
    ASSERT_NE(oldObjPtr, nullptr);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCancelCleanupDelegatesToDeleteShareExport)
{
    BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto func = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(invoke(&MigrateCancelHoldCleanupStub));
    MOCKER(&DeleteShareExport).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_CANCELED, snapshot.state);
    EXPECT_FALSE(snapshot.rebuildRequired);
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
}

/*
 * 用例描述：块迁移前取消检查点 消费原语直验：未取消 → 记录产出并进入逐块迁移(正常迁移零开销路径);
 * 已取消 → 返回 false 且保留当前状态与取消标志(锁内互斥)
 */
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCancelConsumeAtCopyStartCheckpoint)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    // 未取消：进入 DATA_COPYING
    auto [sessionA, isNewA] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        "cancel_cp2_a", MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1);
    ASSERT_TRUE(isNewA);
    auto& copyStartManager = UbseMemShmMigrateSessionManager::GetInstance();
    copyStartManager.UpdateSessionState("cancel_cp2_a", UBS_MEM_MIGRATE_STATE_EXPORTING);
    EXPECT_EQ(UbseMemShmMigrateSessionManager::CopyStartResult::STARTED,
              copyStartManager.TryStartCopy("cancel_cp2_a", copyStartManager.GetSession("cancel_cp2_a")));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_DATA_COPYING, sessionA->state);

    // 已取消：保持 EXPORTING，等待清理完成
    auto [sessionB, isNewB] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        "cancel_cp2_b", MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1);
    ASSERT_TRUE(isNewB);
    UbseMemMigrateCancelResp resp{};
    UbseMemShmMigrateService::GetInstance().Cancel("cancel_cp2_b", resp);
    ASSERT_EQ(UBSE_OK, resp.errCode);

    copyStartManager.UpdateSessionState("cancel_cp2_b", UBS_MEM_MIGRATE_STATE_EXPORTING);
    EXPECT_EQ(UbseMemShmMigrateSessionManager::CopyStartResult::CANCELED,
              copyStartManager.TryStartCopy("cancel_cp2_b", copyStartManager.GetSession("cancel_cp2_b")));
    EXPECT_TRUE(sessionB->cancelRequested);
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
}
TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExportResultGatesBlockStart)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&MigrateExportHoldCreateStub));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    g_exportHeld.store(false);
    g_migratePhaseOrder.clear();
    const auto started = std::chrono::steady_clock::now();
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));
    ASSERT_TRUE(WaitHeld(g_exportHeld));
    EXPECT_TRUE(g_migratePhaseOrder.empty());
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_EXPORTING, snapshot.state);
    ASSERT_EQ(UBSE_OK, ReportExportResult(g_exportResultInfo, g_exportHeldResponse));
    ASSERT_EQ(UBSE_OK, ReportExportResult(g_exportResultInfo, g_exportHeldResponse));
    g_exportResultInfo = {};
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
    EXPECT_EQ(MIGRATE_EXPECT_BLOCK_COUNT * 3, g_migratePhaseOrder.size());
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCancelDoesNotWaitForDestroyResult)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&MigrateCancelHoldCleanupStub));
    g_cleanupHeld.store(false);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_cleanupHeld));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_CANCELED, snapshot.state);
    EXPECT_TRUE(snapshot.cancelRequested);
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    ASSERT_EQ(UBSE_OK, ReportDeleteResult(g_cleanupHeldResponse));
    g_cleanupResultInfo = {};
    g_migrateExecutor->Wait(); // 会话在普通销毁回调前已终态，单独等待账本归还完成。
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_CANCELED, snapshot.state);
    EXPECT_FALSE(snapshot.rebuildRequired);
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExportLateResultCannotChangeNewSession)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&MigrateExportHoldCreateStub));
    g_exportHeld.store(false);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_exportHeld));
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    mgr.ClearSessions();
    auto created =
        mgr.GetOrCreateSession(SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, MIGRATE_EXPECT_BLOCK_COUNT);
    ASSERT_TRUE(created.second);
    ASSERT_EQ(UBSE_OK, ReportExportResult(g_exportResultInfo, g_exportHeldResponse));
    g_exportResultInfo = {};
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_INIT, snapshot.state);
    EXPECT_TRUE(!snapshot.completedBlocks.has_value());
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->status.state);
}

uint32_t ExportAndSaveLedgerStub(UbseMemShareBorrowExportObj& obj, const std::string& slot)
{
    obj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    PutShareExportObj(slot, obj.req.name, obj);
    return UBSE_OK;
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateNewExportRetryDoesNotAllocateTwice)
{
    MOCKER(&ubse::mem::util::GetCurNodeId).stubs().will(returnValue(MIGRATE_DST_NODE_ID));
    MOCKER(&ShareExportExecute).expects(once()).will(invoke(&ExportAndSaveLedgerStub));
    UbseMemShareBorrowExportObj req{};
    req.req.name = "migrate_create_retry";
    req.req.requestId = 0;
    UbseMemDebtNumaInfo numa{};
    numa.nodeId = MIGRATE_DST_NODE_ID;
    req.algoResult.exportNumaInfos.push_back(numa);
    UbseMemShareBorrowExportObj first{};
    UbseMemShareBorrowExportObj second{};
    auto& service = UbseMemShmMigrateService::GetInstance();
    service.Export(req, first);
    ASSERT_EQ(UBSE_OK, first.status.errCode);
    ASSERT_EQ(UBSE_MEM_EXPORT_SUCCESS, first.status.state);
    req.req.requestId = 123; // 借用请求ID变化不影响同一资源的去重。
    service.Export(req, second);
    ASSERT_EQ(UBSE_OK, second.status.errCode);
    ASSERT_EQ(UBSE_MEM_EXPORT_SUCCESS, second.status.state);
    EXPECT_EQ(first.status.errCode, second.status.errCode);
    EXPECT_EQ(first.req.requestId, second.req.requestId);
}

using MappedBeginPtr = ubse::simpo::util::UbseAutoSimpoPtr<UbseMemMigrateBeginExportReq>;
using MappedCopyPtr = ubse::simpo::util::UbseAutoSimpoPtr<UbseMemMigrateDataCopyReq>;
using MappedEndPtr = ubse::simpo::util::UbseAutoSimpoPtr<UbseMemMigrateEndExportReq>;
std::atomic<bool> g_targetEndHeld{false};

HeldMigrateResult g_targetEndResultInfo{};
std::atomic<uint32_t> g_mappedImportRemovals{0};
std::atomic<uint32_t> g_mappedExportCommits{0};
// begin发送计数：替代mockcpp的expects(exactly(n))，异步驱动下expectation匹配非线程安全。
std::atomic<uint32_t> g_mappedBeginSends{0};

UbseResult MappedImportAccountStub(SchedulerImpl*, UbseMemShareBorrowImportObj& obj)
{
    if (obj.importNodeId == MIGRATE_DST_NODE_ID && obj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        ++g_mappedImportRemovals;
    }
    return UBSE_OK;
}

UbseResult MappedExportAccountFailOnceStub(SchedulerImpl* impl, UbseMemShareBorrowExportObj& obj)
{
    if (obj.status.state == UBSE_MEM_EXPORT_SUCCESS && !obj.algoResult.exportNumaInfos.empty() &&
        obj.algoResult.exportNumaInfos.front().nodeId == MIGRATE_DST_NODE_ID) {
        if (++g_mappedExportCommits == 1) {
            return UBSE_ERROR;
        }
    }
    return MigrateScheduleStub(impl, obj);
}

UbseResult MappedBeginRpcStub(UbseComModule*, const SendParam&, MappedBeginPtr& req, UbseBaseMessagePtr& ack,
                              const bool withCopy)
{
    ++g_mappedBeginSends;
    const auto resultInfo = req->GetUbseMesgInfo().rpcInfo;
    const auto request = req->GetUbseMesgInfo();
    UbseMemMigrateBeginExportResp response{};
    const auto local = GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    response.block.memId = local->status.importResults.at(request.rpcInfo.blockIndex).memId;
    response.block.desc.length = MIGRATE_SIZE_BYTES / MIGRATE_EXPECT_BLOCK_COUNT;
    response.block.desc.addr = 0x200000 + request.rpcInfo.blockIndex * response.block.desc.length;
    response.block.memIdStatus = UB_MEM_SWITCHING;
    response.rpcInfo = resultInfo;
    const auto executor = ubse::mem::util::GetExecutor("ubseMemController");
    executor->Execute([response]() { UbseMemShmMigrateService::GetInstance().BeginExportResult(response); });
    return UBSE_OK;
}

UbseResult MappedBeginCheckFaultStub(UbseComModule* module, const SendParam& param, MappedBeginPtr& req,
                                     UbseBaseMessagePtr& ack, const bool withCopy)
{
    // begin_export回包前尚无当前块设备，切换状态由后续copy检查。
    return MappedBeginRpcStub(module, param, req, ack, withCopy);
}

UbseResult MappedCopyRpcStub(UbseComModule*, const SendParam&, MappedCopyPtr& req, UbseBaseMessagePtr& ack,
                             const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::Copy};
    return ReportMigrateResult(resultInfo, UbseMemMigrateOpResp{UBSE_OK});
}

UbseResult MappedEndHoldLastBlockStub(UbseComModule*, const SendParam&, MappedEndPtr& req, UbseBaseMessagePtr& ack,
                                      const bool withCopy)
{
    const HeldMigrateResult resultInfo{req->GetUbseMesgInfo().rpcInfo, TestResultKind::End};
    if (req->GetUbseMesgInfo().success == 1 &&
        req->GetUbseMesgInfo().rpcInfo.blockIndex == MIGRATE_EXPECT_BLOCK_COUNT - 1) {
        g_targetEndResultInfo = resultInfo;
        g_targetEndHeld.store(true, std::memory_order_release);
        return UBSE_OK;
    }
    return ReportMigrateResult(resultInfo, UbseMemMigrateOpResp{UBSE_OK});
}

UbseResult MappedFailCommitThenRollback(UbseComModule*, const SendParam&, MappedEndPtr& req, UbseBaseMessagePtr&,
                                        const bool)
{
    return ReportMigrateResult(
        {req->GetUbseMesgInfo().rpcInfo, TestResultKind::End},
        UbseMemMigrateOpResp{req->GetUbseMesgInfo().success == 1 ? UBSE_MMI_OBMM_OP_FAILED : UBSE_OK});
}

UbseResult MappedEndCheckFaultStub(UbseComModule*, const SendParam&, MappedEndPtr& req, UbseBaseMessagePtr&, const bool)
{
    ExpectMigrateFaultStatus(req->GetUbseMesgInfo().rpcInfo.blockIndex);
    return ReportMigrateResult({req->GetUbseMesgInfo().rpcInfo, TestResultKind::End}, UbseMemMigrateOpResp{UBSE_OK});
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedFailedFirstCommitThenRollbackAllowsRelease)
{
    MigrateFaultQueryMockSet();
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    PutMigrateImportObj("13");
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto begin = &UbseComModule::RpcSend<MappedBeginPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(begin).stubs().will(invoke(&MappedBeginRpcStub));
    const auto end = &UbseComModule::RpcSend<MappedEndPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(end).stubs().will(invoke(&MappedFailCommitThenRollback));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_TRUE(snapshot.rebuildRequired);
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_EQ(UB_MEM_HEALTHY, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->exportObmmInfo.front().memIdStatus);
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME, true));
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedLaterRollbackClearsCurrentStatusAndKeepsCommittedTarget)
{
    MigrateFaultQueryMockSet();
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    PutMigrateImportObj("13");
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto begin = &UbseComModule::RpcSend<MappedBeginPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(begin).stubs().will(invoke(&MappedBeginCheckFaultStub));
    const auto end = &UbseComModule::RpcSend<MappedEndPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(end).stubs().will(invoke(&MappedEndCheckFaultStub));
    MigratePhase2RpcMockSet(&MigrateBeginCheckFaultStub, &MigrateCopyFailSecondBlockStub,
                            &MigrateSwitchEndImportRpcOkStub);

    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_EQ(MIGRATE_FAIL_COPY, snapshot.failReason);
    EXPECT_EQ(1U, snapshot.completedBlocks.value_or(0));
    ExpectMigrateFaultStatus(std::nullopt);
    const auto target = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, target);
    EXPECT_NE(0U, target->status.exportObmmInfo[0].desc.addr);
    for (size_t index = 1; index < target->status.exportObmmInfo.size(); ++index) {
        EXPECT_EQ(0U, target->status.exportObmmInfo[index].desc.addr);
        EXPECT_EQ(0U, target->status.exportObmmInfo[index].desc.length);
    }
    for (const auto& nodeId : {MIGRATE_DST_NODE_ID, std::string("13")}) {
        const auto local = GetShareImportObj(nodeId, SHM_NAME);
        ASSERT_NE(nullptr, local);
        EXPECT_EQ(target->status.exportObmmInfo[0].memId, local->exportObmmInfo[0].memId);
        EXPECT_EQ(ShmMigrateImportInfo(target->migrateInfos[0]), local->migrateInfos[0]);
        EXPECT_EQ(2002U, local->exportObmmInfo[1].memId);
        EXPECT_EQ(MakeShmMigrateInfo(0, 1, 0, false), local->migrateInfos[1]);
        EXPECT_TRUE(local->realExe);
        for (const auto& block : local->exportObmmInfo) {
            EXPECT_EQ(UB_MEM_HEALTHY, block.memIdStatus);
        }
    }
}

UbseResult CountOldCleanupStub(UbseComModule* module, const SendParam& param, MigrateExportReqPtr& req,
                               UbseBaseMessagePtr& ack, const bool withCopy)
{
    return MigrateExportRpcSuccessStub(module, param, req, ack, withCopy);
}

namespace {
UbseMemMigrateBeginExportResp g_heldMappedBegin{};
HeldMigrateResult g_heldMappedRollback{};
std::atomic<bool> g_mappedRollbackHeld{false};

UbseResult HoldMappedBegin(UbseComModule*, const SendParam& param, MappedBeginPtr& req, UbseBaseMessagePtr&, const bool)
{
    EXPECT_EQ(static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_EXPORT),
              param.GetOpCode());
    const auto request = req->GetUbseMesgInfo();
    g_heldMappedBegin = {};
    g_heldMappedBegin.rpcInfo = request.rpcInfo;
    const auto local = GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    g_heldMappedBegin.block.memId = local->status.importResults.at(request.rpcInfo.blockIndex).memId;
    g_heldMappedBegin.block.desc.length = MIGRATE_SIZE_BYTES / MIGRATE_EXPECT_BLOCK_COUNT;
    g_heldMappedBegin.block.desc.addr = 0x200000;
    g_heldMappedBegin.block.memIdStatus = UB_MEM_SWITCHING;
    return UBSE_OK;
}

UbseResult HoldMappedRollback(UbseComModule*, const SendParam&, MappedEndPtr& req, UbseBaseMessagePtr&, const bool)
{
    EXPECT_EQ(0U, req->GetUbseMesgInfo().success);
    g_heldMappedRollback = {req->GetUbseMesgInfo().rpcInfo, TestResultKind::End};
    g_mappedRollbackHeld.store(true, std::memory_order_release);
    return UBSE_OK;
}

void CheckMappedBeginCancellation(bool cleanupSuccess)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    PutMigrateImportObj("14"); // 尚未begin_import的节点不得收到回退。
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    MigrateComMockSet();
    const auto begin = &UbseComModule::RpcSend<MappedBeginPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(begin).stubs().will(invoke(&HoldMappedBegin));
    const auto end = &UbseComModule::RpcSend<MappedEndPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(end).stubs().will(invoke(&HoldMappedRollback));
    const auto importBegin = &UbseComModule::RpcSend<UbseMemMigrateSwitchBeginImportReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(importBegin).expects(never());
    const auto importEnd = &UbseComModule::RpcSend<UbseMemMigrateSwitchEndImportReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(importEnd).expects(never());
    g_mappedRollbackHeld.store(false);
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_EXPORTING, snapshot.state);
    // 同样的关联字段通过import入口返回不能完成export阶段。
    UbseMemMigrateOpResp wrongOperation{};
    wrongOperation.rpcInfo = g_heldMappedBegin.rpcInfo;
    service.BeginImportResult(wrongOperation);
    UbseMemMigrateCancelResp canceled{};
    service.Cancel(SHM_NAME, canceled);
    ASSERT_EQ(UBSE_OK, canceled.errCode);
    service.BeginExportResult(g_heldMappedBegin);
    service.BeginExportResult(g_heldMappedBegin); // 重复结果不得二次推进。
    ASSERT_TRUE(WaitHeld(g_mappedRollbackHeld));
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_EXPORTING, snapshot.state);
    const auto pending = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, pending);
    EXPECT_EQ(UBSE_MEM_EXPORT_RUNNING, pending->status.state);
    EXPECT_EQ(g_heldMappedBegin.block.desc.length, pending->status.exportObmmInfo[0].desc.length);
    ReportMigrateResult(g_heldMappedRollback, UbseMemMigrateOpResp{cleanupSuccess ? UBSE_OK : UBSE_ERROR});
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(cleanupSuccess ? UBS_MEM_MIGRATE_STATE_CANCELED : UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_EQ(cleanupSuccess ? UBSE_OK : static_cast<uint32_t>(MIGRATE_FAIL_ROLLBACK_EXPORT), snapshot.failReason);
    EXPECT_FALSE(snapshot.completedBlocks.has_value());
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME, true));
    EXPECT_NE(nullptr, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
    EXPECT_EQ(cleanupSuccess, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME) == nullptr);
}
} // namespace

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedBeginCancelWaitsForTargetRollback)
{
    CheckMappedBeginCancellation(true);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedBeginCancelCleanupFailureRetainsLedger)
{
    CheckMappedBeginCancellation(false);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateBeginExportFailureHasOwnReason)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto begin = &UbseComModule::RpcSend<MappedBeginPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(begin).stubs().will(invoke(&HoldMappedBegin));
    const auto end = &UbseComModule::RpcSend<MappedEndPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(end).stubs().will(invoke(&HoldMappedRollback));
    g_mappedRollbackHeld.store(false);
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    g_heldMappedBegin.errCode = UBSE_MMI_OBMM_OP_FAILED;
    service.BeginExportResult(g_heldMappedBegin);
    ASSERT_TRUE(WaitHeld(g_mappedRollbackHeld));
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_heldMappedRollback, UbseMemMigrateOpResp{UBSE_OK}));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateGetResp progress{};
    service.Get(SHM_NAME, progress);
    EXPECT_EQ(MIGRATE_FAIL_BEGIN_EXPORT, progress.failReason);
    EXPECT_NE(std::string::npos, progress.errMsg.find("begin_export"));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedEmptyImportTimerFailureDoesNotStartCopy)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto begin = &UbseComModule::RpcSend<MappedBeginPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(begin).expects(never());
    const auto copy = &UbseComModule::RpcSend<MappedCopyPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(copy).expects(never());
    const auto end = &UbseComModule::RpcSend<MappedEndPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(end).expects(once()).will(invoke(&HoldMappedRollback));
    MOCKER(&ubse::timer::UbseTimerHandlerRegister).reset();
    MOCKER(&ubse::timer::UbseTimerHandlerRegister)
        .stubs()
        .will(invoke(&MigrateTimerRegisterStub))
        .then(returnValue(UBSE_ERROR)) // begin_export之后没有其余导入节点，空begin_import仍须保留阶段失败。
        .then(invoke(&MigrateTimerRegisterStub));
    g_mappedRollbackHeld = false;
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_mappedRollbackHeld));
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_heldMappedRollback, UbseMemMigrateOpResp{UBSE_OK}));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_EQ(MIGRATE_FAIL_TIMER, snapshot.failReason);
    EXPECT_EQ(0U, snapshot.completedBlocks.value_or(0));
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    const auto local = GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, local);
    EXPECT_EQ(2001U, local->exportObmmInfo.front().memId);
    EXPECT_EQ(MakeShmMigrateInfo(0, 0, 0, false), local->migrateInfos.front());
    EXPECT_EQ(UB_MEM_HEALTHY, local->exportObmmInfo.front().memIdStatus);
    EXPECT_TRUE(local->realExe);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedLastTargetEndGatesIdentityConversion)
{
    MigrateFaultQueryMockSet();
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    MigrateScheduleMockSet();
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    MigrateComMockSet();
    const auto beginFunc = &UbseComModule::RpcSend<MappedBeginPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(beginFunc).stubs().will(invoke(&MappedBeginCheckFaultStub));
    const auto copyFunc = &UbseComModule::RpcSend<MappedCopyPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(copyFunc).stubs().will(invoke(&MigrateCopyCheckFaultStub));
    const auto endFunc = &UbseComModule::RpcSend<MappedEndPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(endFunc).stubs().will(invoke(&MappedEndHoldLastBlockStub));
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&CountOldCleanupStub));
    g_targetEndHeld.store(false);
    g_oldCleanupCount.store(0);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_targetEndHeld));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_DATA_COPYING, snapshot.state);
    EXPECT_EQ(MIGRATE_EXPECT_BLOCK_COUNT - 1, snapshot.completedBlocks.value_or(0));
    EXPECT_EQ(0U, g_oldCleanupCount.load());
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
    EXPECT_NE(nullptr, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_EQ(UBSE_MEM_EXPORT_RUNNING, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->status.state);
    ExpectMigrateFaultStatus(MIGRATE_EXPECT_BLOCK_COUNT - 1);
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_targetEndResultInfo, UbseMemMigrateOpResp{UBSE_OK}));
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_targetEndResultInfo, UbseMemMigrateOpResp{UBSE_OK}));
    g_targetEndResultInfo = {};
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
    EXPECT_EQ(1U, g_oldCleanupCount.load());
    ASSERT_NE(nullptr, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_FALSE(GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->realExe);
    EXPECT_EQ(MIGRATE_DST_NODE_ID, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->req.baseNodeId);
    g_migrateExecutor->Wait();
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->status.state);
    ExpectMigrateFaultStatus(std::nullopt);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedAccountFailureDoesNotGateSuccessOrRetry)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Accept(BuildMigrateReq(DST_SLOT_ID)));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowImportObj>)
        .stubs()
        .will(invoke(&MappedImportAccountStub));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowExportObj>)
        .stubs()
        .will(invoke(&MappedExportAccountFailOnceStub));
    g_mappedImportRemovals.store(0);
    g_mappedExportCommits.store(0);
    MigrateComMockSet();
    const auto beginFunc = &UbseComModule::RpcSend<MappedBeginPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(beginFunc).stubs().will(invoke(&MappedBeginRpcStub));
    const auto copyFunc = &UbseComModule::RpcSend<MappedCopyPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(copyFunc).stubs().will(invoke(&MappedCopyRpcStub));
    const auto endFunc = &UbseComModule::RpcSend<MappedEndPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(endFunc).stubs().will(invoke(&MappedEndHoldLastBlockStub));
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&CountOldCleanupStub));
    g_targetEndHeld.store(false);
    g_oldCleanupCount.store(0);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_targetEndHeld));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_DATA_COPYING, snapshot.state);
    EXPECT_EQ(MIGRATE_EXPECT_BLOCK_COUNT - 1, snapshot.completedBlocks.value_or(0));
    EXPECT_EQ(0U, g_oldCleanupCount.load());
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
    EXPECT_NE(nullptr, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_EQ(UBSE_MEM_EXPORT_RUNNING, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->status.state);
    g_targetEndHeld.store(false);
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_targetEndResultInfo, UbseMemMigrateOpResp{UBSE_OK}));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
    EXPECT_EQ(0U, snapshot.failReason);
    EXPECT_EQ(1U, g_mappedImportRemovals.load());
    EXPECT_EQ(1U, g_mappedExportCommits.load());
    ASSERT_NE(nullptr, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_FALSE(GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->realExe);
    EXPECT_EQ(MIGRATE_DST_NODE_ID, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->req.baseNodeId);
    const auto committedTarget = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, committedTarget);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, committedTarget->status.state);
    EXPECT_EQ(1U, g_oldCleanupCount.load());
    PumpMigrateTimers();
    EXPECT_FALSE(g_targetEndHeld.load());
    EXPECT_EQ(1U, g_mappedImportRemovals.load());
    EXPECT_EQ(1U, g_mappedExportCommits.load());
    EXPECT_FALSE(snapshot.rebuildRequired);
}

namespace {
std::atomic<bool> g_otherImportEndHeld{false};
HeldMigrateResult g_otherImportEndInfo{};

UbseResult MappedEndRpcOkStub(UbseComModule*, const SendParam&, MappedEndPtr& req, UbseBaseMessagePtr&, const bool)
{
    EXPECT_EQ(1U, req->GetUbseMesgInfo().success); // 已提交目标不得再收到end(0)。
    return ReportMigrateResult({req->GetUbseMesgInfo().rpcInfo, TestResultKind::End}, UbseMemMigrateOpResp{UBSE_OK});
}

UbseResult HoldLastMappedImportEnd(UbseComModule* module, const SendParam& param,
                                   UbseMemMigrateSwitchEndImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                   const bool withCopy)
{
    if (req->GetUbseMesgInfo().success == 1 &&
        req->GetUbseMesgInfo().rpcInfo.blockIndex == MIGRATE_EXPECT_BLOCK_COUNT - 1) {
        g_otherImportEndInfo = {req->GetUbseMesgInfo().rpcInfo, TestResultKind::End};
        g_otherImportEndHeld.store(true, std::memory_order_release);
        return UBSE_OK;
    }
    return MigrateSwitchEndImportRpcOkStub(module, param, req, ack, withCopy);
}

void CheckMappedTargetCommittedBeforeOtherEnd(bool otherEndSuccess)
{
    MigrateFaultQueryMockSet();
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    PutMigrateImportObj("13");
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    MigrateScheduleMockSet();
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowImportObj>)
        .stubs()
        .will(invoke(&MappedImportAccountStub));
    MigrateComMockSet();
    const auto begin = &UbseComModule::RpcSend<MappedBeginPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(begin).stubs().will(invoke(&MappedBeginRpcStub));
    const auto copy = &UbseComModule::RpcSend<MappedCopyPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(copy).stubs().will(invoke(&MappedCopyRpcStub));
    const auto end = &UbseComModule::RpcSend<MappedEndPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(end).stubs().will(invoke(&MappedEndRpcOkStub));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub, &HoldLastMappedImportEnd);
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&CountOldCleanupStub));
    g_otherImportEndHeld.store(false);
    g_oldCleanupCount.store(0);
    g_mappedImportRemovals.store(0);
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_otherImportEndHeld));
    ASSERT_NE(nullptr, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_FALSE(GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->realExe);
    EXPECT_EQ(MIGRATE_DST_NODE_ID, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->req.baseNodeId);
    const auto target = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, target);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, target->status.state);
    ExpectMigrateFaultStatus(std::nullopt);
    EXPECT_EQ(1U, g_mappedImportRemovals.load());
    EXPECT_EQ(0U, g_oldCleanupCount.load());
    ASSERT_EQ(UBSE_OK,
              ReportMigrateResult(g_otherImportEndInfo, UbseMemMigrateOpResp{otherEndSuccess ? UBSE_OK : UBSE_ERROR}));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(otherEndSuccess ? UBS_MEM_MIGRATE_STATE_SUCCESS : UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    ASSERT_NE(nullptr, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_FALSE(GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->realExe);
    EXPECT_EQ(MIGRATE_DST_NODE_ID, GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->req.baseNodeId);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME)->status.state);
    EXPECT_EQ(otherEndSuccess ? 1U : 0U, g_oldCleanupCount.load());
    EXPECT_EQ(1U, g_mappedImportRemovals.load());
}
} // namespace

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedTargetCommitsBeforeOtherImportEnd)
{
    CheckMappedTargetCommittedBeforeOtherEnd(true);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMappedOtherEndFailureKeepsCommittedTarget)
{
    CheckMappedTargetCommittedBeforeOtherEnd(false);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateNewExportHasNoStageTimer)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&MigrateExportHoldCreateStub));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    g_exportHeld.store(false);
    g_migratePhaseOrder.clear();
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_exportHeld));
    EXPECT_TRUE(g_exportResultInfo.name.empty());
    {
        std::lock_guard<std::mutex> lock(g_migrateTimersMutex);
        EXPECT_TRUE(g_migrateTimers.empty());
    }
    auto wrong = g_exportHeldResponse;
    wrong.algoResult.exportNumaInfos.front().nodeId = "98";
    ASSERT_EQ(UBSE_OK, ReportExportResult(g_exportResultInfo, wrong));
    wrong = g_exportHeldResponse;
    wrong.algoResult.exportNumaInfos.front().nodeId = "99";
    ASSERT_EQ(UBSE_OK, ReportExportResult(g_exportResultInfo, wrong));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_TRUE(g_migratePhaseOrder.empty());
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_EXPORTING, snapshot.state);
    ASSERT_EQ(UBSE_OK, ReportExportResult(g_exportResultInfo, g_exportHeldResponse));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateLateExportAfterSendFailureOnlyCleansUp)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&MigrateExportHoldCreateStub));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    g_exportHeld.store(false);
    g_migratePhaseOrder.clear();
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_exportHeld));
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    auto& service = UbseMemShmMigrateService::GetInstance();
    mgr.SetRebuildRequired(SHM_NAME, true);
    mgr.FinishSession(SHM_NAME, UBS_MEM_MIGRATE_STATE_FAILED, MIGRATE_FAIL_RPC);
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME, true));
    ASSERT_EQ(UBSE_OK, ReportExportResult({}, g_exportHeldResponse));
    g_migrateExecutor->Wait(); // 迟到结果完成普通清理后，原资源可再次受理迁移。
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    EXPECT_TRUE(g_migratePhaseOrder.empty());
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_SRC_NODE_ID, SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_FALSE(snapshot.completedBlocks.has_value());
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME));
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_OK, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_INIT, snapshot.state);
}

std::atomic<uint32_t> g_cancelExportAccountNotifications{0};

UbseResult CancelExportAccountStub(SchedulerImpl*, UbseMemShareBorrowExportObj& obj)
{
    if (obj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        ++g_cancelExportAccountNotifications;
        const auto ledger = GetShareExportObj(MIGRATE_DST_NODE_ID, obj.req.name);
        EXPECT_NE(nullptr, ledger);
        if (ledger != nullptr) {
            EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, ledger->status.state);
            EXPECT_EQ(obj.status.exportObmmInfo.size(), ledger->status.exportObmmInfo.size());
        }
    }
    return UBSE_OK;
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateExportResultAccountsWithoutSession)
{
    UbseMemShareBorrowExportObj result{};
    result.req.name = SHM_NAME;
    result.status.state = UBSE_MEM_EXPORT_SUCCESS;
    result.algoResult.exportNumaInfos.push_back({MIGRATE_DST_NODE_ID});
    g_cancelExportAccountNotifications.store(0);
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowExportObj>)
        .expects(once())
        .will(invoke(&CancelExportAccountStub));
    auto& service = UbseMemShmMigrateService::GetInstance();
    service.NewExportResult(result);
    service.NewExportResult(result);
    const auto ledger = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, ledger);
    EXPECT_TRUE(ledger->isCreateReportReceived);
    EXPECT_EQ(1U, g_cancelExportAccountNotifications.load());
    EXPECT_EQ(nullptr, UbseMemShmMigrateSessionManager::GetInstance().GetSession(SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCancelWaitsForNewExportCallback)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    g_cancelExportAccountNotifications.store(0);
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowExportObj>)
        .stubs()
        .will(invoke(&CancelExportAccountStub));
    MigrateComMockSet();
    const auto exportFunc = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(exportFunc).stubs().will(invoke(&MigrateExportHoldCreateStub));
    g_exportHeld.store(false);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_exportHeld));
    UbseMemMigrateCancelResp cancelResp{};
    UbseMemShmMigrateService::GetInstance().Cancel(SHM_NAME, cancelResp);
    ASSERT_EQ(UBSE_OK, cancelResp.errCode);
    UbseMemShmMigrateSession snapshot{};
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_EXPORTING, snapshot.state);
    EXPECT_TRUE(snapshot.cancelRequested);
    EXPECT_NE(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
    ASSERT_EQ(UBSE_OK, ReportExportResult(g_exportResultInfo, g_exportHeldResponse));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    ASSERT_TRUE(mgr.ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_CANCELED, snapshot.state);
    EXPECT_EQ(UBSE_OK, snapshot.failReason);
    EXPECT_EQ(1U, g_cancelExportAccountNotifications.load());
    EXPECT_FALSE(snapshot.rebuildRequired);
    EXPECT_EQ(nullptr, GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME));
}

namespace {
uint32_t g_masterExportAttempts = 0;
uint32_t g_masterFailCount = 0;
UbseMemMigrateRpcInfo g_firstMasterRequest{};
uint64_t g_firstMasterRequestId = 0;

UbseResult RetryTestMasterExportSend(UbseComModule* module, const SendParam& param, MigrateExportReqPtr& request,
                                     UbseBaseMessagePtr& ack, const bool withCopy)
{
    const auto& data = request->GetUbseMesgInfo();
    {
        EXPECT_EQ(MIGRATE_DST_NODE_ID, param.GetRemoteId());
        if (g_masterExportAttempts++ == 0) {
            g_firstMasterRequestId = data.req.requestId;
        }
        EXPECT_EQ(g_firstMasterRequestId, data.req.requestId);
        if (g_masterExportAttempts <= g_masterFailCount) {
            return UBSE_ERROR;
        }
    }
    return MigrateExportRpcSuccessStub(module, param, request, ack, withCopy);
}
} // namespace

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMasterSendRetriesThenSucceeds)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    g_masterExportAttempts = 0;
    g_masterFailCount = 2;
    const auto send = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(send).stubs().will(invoke(&RetryTestMasterExportSend));
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcOkStub,
                            &MigrateSwitchEndImportRpcOkStub);
    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    EXPECT_EQ(3U, g_masterExportAttempts);
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMasterSendRetryExhausted)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateComMockSet();
    g_masterExportAttempts = 0;
    g_masterFailCount = SEND_RETRY_TIMES;
    const auto send = &UbseComModule::RpcSend<MigrateExportReqPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(send).stubs().will(invoke(&RetryTestMasterExportSend));
    EXPECT_EQ(UBSE_ERROR, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    EXPECT_EQ(SEND_RETRY_TIMES, g_masterExportAttempts);
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_TRUE(snapshot.rebuildRequired);
}

namespace {
std::atomic<bool> g_copyNodeEndHeld{false};
std::atomic<uint32_t> g_otherNodeEndCalls{0};
HeldMigrateResult g_copyNodeEndInfo{};
UbseResult HoldCopyNodeEnd(UbseComModule* module, const SendParam& param, UbseMemMigrateSwitchEndImportReqSimpoPtr& req,
                           UbseBaseMessagePtr& ack, const bool copy)
{
    const auto& data = req->GetUbseMesgInfo();
    if (data.success == 1 && data.rpcInfo.blockIndex == 0) {
        if (param.GetRemoteId() == g_copyTargetNodeId) {
            g_copyNodeEndInfo = {data.rpcInfo, TestResultKind::End};
            g_copyNodeEndHeld.store(true, std::memory_order_release);
            return UBSE_OK;
        }
        ++g_otherNodeEndCalls;
    }
    return MigrateSwitchEndImportRpcOkStub(module, param, req, ack, copy);
}
} // namespace

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateAEndsCopyNodeBeforeReleasingOtherNodes)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    PutMigrateImportObj("15");
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    g_copyNodeEndHeld = false;
    g_otherNodeEndCalls = 0;
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcCaptureStub, &HoldCopyNodeEnd);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_copyNodeEndHeld));
    EXPECT_EQ(0U, g_otherNodeEndCalls.load());
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_copyNodeEndInfo, UbseMemMigrateOpResp{UBSE_OK}));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    EXPECT_EQ(1U, g_otherNodeEndCalls.load());
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_SUCCESS, snapshot.state);
}

namespace {
std::vector<std::string> g_rollbackNodes;
std::string g_committedOtherNode;
HeldMigrateResult g_failedOtherEnd;
std::atomic<bool> g_failedOtherEndHeld{false};

UbseResult CaptureRollback(UbseComModule* module, const SendParam& param, UbseMemMigrateSwitchEndImportReqSimpoPtr& req,
                           UbseBaseMessagePtr& ack, const bool copy)
{
    if (req->GetUbseMesgInfo().success == 0) {
        g_rollbackNodes.push_back(param.GetRemoteId());
    }
    return MigrateSwitchEndImportRpcOkStub(module, param, req, ack, copy);
}

UbseResult HoldLastImportEndFailure(UbseComModule* module, const SendParam& param,
                                    UbseMemMigrateSwitchEndImportReqSimpoPtr& req, UbseBaseMessagePtr& ack,
                                    const bool copy)
{
    const auto& data = req->GetUbseMesgInfo();
    if (data.success == 1 && param.GetRemoteId() != g_copyTargetNodeId) {
        if (g_committedOtherNode.empty()) {
            g_committedOtherNode = param.GetRemoteId();
        } else {
            g_failedOtherEnd = {data.rpcInfo, TestResultKind::End};
            g_failedOtherEndHeld = true;
            return UBSE_OK;
        }
    }
    return CaptureRollback(module, param, req, ack, copy);
}
} // namespace

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCompletionQueueFailureStillSendsRollback)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    g_beginHeld = false;
    g_rollbackNodes.clear();
    MigratePhase2RpcMockSet(&MigrateBeginHoldFirstStub, &MigrateDataCopyRpcOkStub, &CaptureRollback);
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_beginHeld));
    g_migrateExecutor->Wait();

    UbseMemMigrateOpResp failure{UBSE_MMI_OBMM_OP_FAILED};
    failure.rpcInfo = g_beginResultInfo.rpcInfo;
    g_migrateExecutorUnavailable = true;
    service.BeginImportResult(failure);
    g_migrateExecutorUnavailable = false;
    EXPECT_TRUE(g_rollbackNodes.empty());
    EXPECT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().HasActiveSession(SHM_NAME));

    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME)); // 由原阶段timer重新投递完成事件。
    EXPECT_EQ(std::vector<std::string>{"13"}, g_rollbackNodes);
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME, true));
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateRollbackSkipsAllConfirmedCommittedImports)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    PutMigrateImportObj("14");
    PutMigrateImportObj("15");
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    g_rollbackNodes.clear();
    g_committedOtherNode.clear();
    g_failedOtherEndHeld = false;
    MigratePhase2RpcMockSet(&MigrateSwitchBeginImportRpcOkStub, &MigrateDataCopyRpcCaptureStub,
                            &HoldLastImportEndFailure);
    ASSERT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().Execute(SHM_NAME));
    ASSERT_TRUE(WaitHeld(g_failedOtherEndHeld));
    g_migrateExecutor->Wait(); // 先确认另外两个节点的成功结果已落账。
    ASSERT_EQ(UBSE_OK, ReportMigrateResult(g_failedOtherEnd, UbseMemMigrateOpResp{UBSE_ERROR}));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    EXPECT_EQ(std::vector<std::string>{g_failedOtherEnd.rpcInfo.srcNodeId}, g_rollbackNodes);
    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().CheckOperationAllowed(SHM_NAME, true));
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateRebuildRestrictionEndsOnlyAfterAllResourcesReleased)
{
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj("13");
    auto& sessions = UbseMemShmMigrateSessionManager::GetInstance();
    ASSERT_TRUE(sessions.GetOrCreateSession(SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1).second);
    sessions.SetRebuildRequired(SHM_NAME, true);
    sessions.FinishSession(SHM_NAME, UBS_MEM_MIGRATE_STATE_FAILED, MIGRATE_FAIL_COPY);
    auto& service = UbseMemShmMigrateService::GetInstance();
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME, true));
    auto& ledger = UbseMemDebtLedger::GetInstance();
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(MIGRATE_SRC_NODE_ID, SHM_NAME);
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
    ledger.GetDebtMap<UbseMemShareBorrowImportObj>().RemoveResource("13", SHM_NAME);
    auto deleting = BuildMigrateExportObj(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES);
    deleting.status.state = UBSE_MEM_EXPORT_DESTROYING;
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(MIGRATE_SRC_NODE_ID, SHM_NAME, deleting);
    EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(MIGRATE_SRC_NODE_ID, SHM_NAME);
    EXPECT_EQ(UBSE_OK, service.CheckOperationAllowed(SHM_NAME));
    EXPECT_TRUE(sessions.GetOrCreateSession(SHM_NAME, MIGRATE_SRC_NODE_ID, MIGRATE_DST_NODE_ID, 1).second);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateTerminalRebuildFlagAllowsNormalDelete)
{
    MOCKER(IsMemShareFeatureSupported).stubs().will(returnValue(true));
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<com::UbseComModule> module;
    BuildOperationSuccessMock(module);
    MasterExportCallbackMockSet();
    UbseMemReturnReq req{};
    req.name = SHM_NAME;
    req.importNodeId = NODE_ONE;
    auto& service = UbseMemShmMigrateService::GetInstance();
    for (const auto state :
         {UBS_MEM_MIGRATE_STATE_SUCCESS, UBS_MEM_MIGRATE_STATE_FAILED, UBS_MEM_MIGRATE_STATE_CANCELED}) {
        for (const bool rebuildRequired : {false, true}) {
            SCOPED_TRACE(testing::Message() << "state=" << state << ", rebuild=" << rebuildRequired);
            UbseMemShareBorrowExportObj exportObj{};
            ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
            SetMigrationSessionForRelease(state, rebuildRequired);
            if (rebuildRequired) {
                EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.CheckOperationAllowed(SHM_NAME));
                EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
                UbseMemOperationResp restricted{};
                UbseMemShareBorrowReq borrow{};
                borrow.name = SHM_NAME;
                EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, UbseMemShareBorrow(borrow, restricted));
                UbseMemShareAttachReq attach{};
                attach.name = SHM_NAME;
                EXPECT_EQ(UBSE_ERR_MIGRATE_REBUILD_REQUIRED, UbseMemShareAttach(attach, restricted));
            }
            UbseMemOperationResp resp{};
            EXPECT_EQ(UBSE_OK, UbseMemShareReturn(req, resp, NODE_ONE));
            const auto deleting = GetShareExportObj(NODE_TWO, SHM_NAME);
            ASSERT_NE(nullptr, deleting);
            EXPECT_EQ(UBSE_MEM_EXPORT_DESTROYING, deleting->status.state);
        }
    }
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateFailedUnconfirmedCleanupAllowsNormalDetach)
{
    MOCKER(IsMemShareFeatureSupported).stubs().will(returnValue(true));
    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    MasterExportCallbackMockSet();
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_SUCCESS, UBSE_MEM_IMPORT_SUCCESS);
    PutShareImportObj(NODE_ONE, SHM_NAME, importObj);
    SetMigrationSessionForRelease(UBS_MEM_MIGRATE_STATE_FAILED, true);
    UbseMemShareDetachReq req{};
    req.name = SHM_NAME;
    req.unImportNodeId = NODE_ONE;
    req.udsInfo = exportObj.req.udsInfo;
    UbseMemOperationResp resp{};
    EXPECT_EQ(UBSE_OK, UbseMemShareDetach(req, resp, NODE_ONE));
    const auto detaching = GetShareImportObj(NODE_ONE, SHM_NAME);
    ASSERT_NE(nullptr, detaching);
    EXPECT_EQ(UBSE_MEM_IMPORT_DESTROYING, detaching->status.state);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateConsecutiveRoundsCommitReferencesAndMarkers)
{
    UbseMemShareBorrowImportObj obj{};
    obj.status.importResults.resize(2);
    obj.status.decoderResult.resize(2);
    obj.exportObmmInfo.resize(2);
    obj.migrateInfos.resize(2);
    obj.req.baseNodeId = "1";
    for (uint32_t round = 1; round <= 2; ++round) {
        const auto previousSecondMemId = obj.exportObmmInfo[1].memId;
        const auto previousSecondMarker = obj.migrateInfos[1];
        UbseMemMigrateRpcInfo info{};
        info.exportMemId = 100 + round;
        info.migrateInfo = MakeShmMigrateInfo(round, 0, round, true);
        UbseMemMigrateBlockData block{};
        block.exportInfo.memId = info.exportMemId;
        block.decoder = ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult{};
        block.targetLayout.exportNumaInfos = {{std::to_string(round + 1), 1, 2, 1024}};
        migrate_detail::ApplyEndSuccess(obj, info, block);
        EXPECT_EQ(info.exportMemId, obj.exportObmmInfo[0].memId);
        EXPECT_EQ(ShmMigrateImportInfo(info.migrateInfo), obj.migrateInfos[0]);
        EXPECT_EQ(previousSecondMemId, obj.exportObmmInfo[1].memId);
        EXPECT_EQ(previousSecondMarker, obj.migrateInfos[1]);
        EXPECT_EQ(std::to_string(round), obj.req.baseNodeId);
        info.blockIndex = 1;
        info.migrateInfo = MakeShmMigrateInfo(round, 1, round, true);
        migrate_detail::ApplyEndSuccess(obj, info, block);
        EXPECT_EQ(info.exportMemId, obj.exportObmmInfo[1].memId);
        EXPECT_EQ(ShmMigrateImportInfo(info.migrateInfo), obj.migrateInfos[1]);
        EXPECT_EQ(std::to_string(round + 1), obj.req.baseNodeId);
    }
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateMissingLocalLedgerDoesNotInventExecutionRecord)
{
    MOCKER(&ubse::mem::util::GetCurNodeId).stubs().will(returnValue(std::string("1")));
    MOCKER(&ubse::election::UbseGetMasterNodeId).stubs().with(outBound(std::string("2"))).will(returnValue(UBSE_OK));
    UbseMemMigrateRpcInfo info{"migrate_missing_ledger", 0, "1", 91827, MakeShmMigrateInfo(1, 0, 3, true), true};
    auto& service = UbseMemShmMigrateService::GetInstance();
    UbseMemMigrateOpResp result{};
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, service.SwitchEndImport({0, info}, "2", result));
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, service.SwitchEndImport({0, info}, "2", result));
    EXPECT_EQ(nullptr,
              UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource("1", info.name));
}

namespace {
enum class CommitFailureKind
{
    LOST_REPLY,
    BUSINESS_ERROR
};
CommitFailureKind g_importCommitFailure = CommitFailureKind::LOST_REPLY;
bool g_committedRollbackHasBlock = true;
bool g_otherRollbackSucceeds = true;
uint32_t g_committedRollbackIndex = 0;
std::atomic<uint32_t> g_lostCommitSendAttempts{0};

UbseMemMigrateOpResp CommittedRollbackResponse(const UbseMemMigrateRpcInfo& info, bool exportTarget)
{
    UbseMemMigrateOpResp result{UBSE_ERR_MIGRATE_NOT_ALLOWED};
    result.committed = true;
    if (!g_committedRollbackHasBlock) {
        return result;
    }
    if (exportTarget) {
        const auto target = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
        EXPECT_NE(nullptr, target);
        if (target) {
            result.block = UbseMemMigrateBlockData{target->status.exportObmmInfo[info.blockIndex], std::nullopt,
                                                   target->algoResult};
        }
    } else {
        const auto local = GetShareImportObj(info.srcNodeId, info.name);
        EXPECT_TRUE(local && local->switchContext && local->switchContext->target);
        if (local && local->switchContext) {
            result.block = local->switchContext->target;
        }
    }
    if (result.block) {
        result.block->exportInfo.memIdStatus = UB_MEM_HEALTHY;
    }
    return result;
}

UbseResult ImportCommitFailureThenRollbackStub(UbseComModule*, const SendParam& param,
                                               UbseMemMigrateSwitchEndImportReqSimpoPtr& req, UbseBaseMessagePtr&,
                                               const bool)
{
    const auto& data = req->GetUbseMesgInfo();
    EXPECT_EQ(0U, data.rpcInfo.blockIndex);
    if (data.success == 1) {
        EXPECT_EQ(g_copyTargetNodeId, param.GetRemoteId());
        ++g_lostCommitSendAttempts;
        if (g_importCommitFailure == CommitFailureKind::BUSINESS_ERROR) {
            UbseMemMigrateOpResp result{UBSE_ERR_INTERNAL};
            result.committed = true;
            return ReportMigrateResult({data.rpcInfo, TestResultKind::End}, result);
        }
        return UBSE_ERR_TIMEOUT; // 设备已提交，但请求确认和业务结果均丢失。
    }
    g_rollbackNodes.push_back(param.GetRemoteId());
    const auto result = param.GetRemoteId() == g_copyTargetNodeId ?
                            CommittedRollbackResponse(data.rpcInfo, false) :
                            UbseMemMigrateOpResp{g_otherRollbackSucceeds ? UBSE_OK : UBSE_MMI_OBMM_OP_FAILED};
    return ReportMigrateResult({data.rpcInfo, TestResultKind::End}, result);
}

void CheckImportCommittedRollback(bool otherImport, bool hasBlock, bool otherRollbackSucceeds = true,
                                  CommitFailureKind failureKind = CommitFailureKind::LOST_REPLY)
{
    MigrateFaultQueryMockSet();
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    MigrateNodeMockSetForExecute();
    AcceptMigrateOnly();
    if (otherImport) {
        PutMigrateImportObj("14");
    }
    MigrateScheduleMockSet();
    MigrateRpcSuccessMockSet();
    MigratePhase2RpcMockSet(&MigrateBeginCheckFaultStub, &MigrateCopyCheckFaultStub,
                            &ImportCommitFailureThenRollbackStub);
    g_committedRollbackHasBlock = hasBlock;
    g_otherRollbackSucceeds = otherRollbackSucceeds;
    g_importCommitFailure = failureKind;
    g_lostCommitSendAttempts = 0;
    g_rollbackNodes.clear();
    g_migratePhaseOrder.clear();
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    const bool businessFailure = failureKind == CommitFailureKind::BUSINESS_ERROR;
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_EQ(businessFailure ? MIGRATE_FAIL_END_IMPORT : MIGRATE_FAIL_RPC, snapshot.failReason);
    EXPECT_EQ(0U, snapshot.completedBlocks.value_or(0));
    EXPECT_TRUE(snapshot.rebuildRequired);
    EXPECT_EQ(businessFailure ? 1U : SEND_RETRY_TIMES, g_lostCommitSendAttempts.load());
    for (const auto step : g_migratePhaseOrder) {
        EXPECT_LT(step, 3U); // 失败收尾不能开始下一块。
    }
    if (!hasBlock) {
        ASSERT_EQ(1U, g_rollbackNodes.size());
        EXPECT_EQ(g_copyTargetNodeId, g_rollbackNodes.front());
        const auto rejected = GetShareImportObj(g_copyTargetNodeId, SHM_NAME);
        ASSERT_NE(nullptr, rejected);
        ASSERT_TRUE(rejected->switchContext);
        EXPECT_FALSE(rejected->switchContext->endDecision.has_value());
        EXPECT_TRUE(rejected->switchContext->target.has_value());
        EXPECT_EQ(UB_MEM_SWITCHING, rejected->exportObmmInfo[0].memIdStatus);
        EXPECT_EQ(2001U, rejected->exportObmmInfo[0].memId);
        EXPECT_EQ(0U, ShmMigrateVersion(rejected->migrateInfos[0]));
        ExpectMigrateFaultStatus(0U);
        return;
    }
    ASSERT_EQ(otherImport ? 2U : 1U, g_rollbackNodes.size());
    EXPECT_EQ(g_copyTargetNodeId, g_rollbackNodes.front());
    ExpectMigrateFaultStatus(otherImport && !otherRollbackSucceeds ? std::optional<uint32_t>(0) : std::nullopt);
    const auto copyNode = GetShareImportObj(g_copyTargetNodeId, SHM_NAME);
    ASSERT_NE(nullptr, copyNode);
    ASSERT_TRUE(copyNode->switchContext);
    EXPECT_EQ(true, copyNode->switchContext->endDecision);
    EXPECT_EQ(UB_MEM_HEALTHY, copyNode->exportObmmInfo[0].memIdStatus);
    EXPECT_EQ(1U, copyNode->exportObmmInfo[0].memId);
    EXPECT_EQ(1U, ShmMigrateVersion(copyNode->migrateInfos[0]));
    if (otherImport) {
        const auto other = GetShareImportObj(g_copyTargetNodeId == "13" ? "14" : "13", SHM_NAME);
        ASSERT_NE(nullptr, other);
        EXPECT_EQ(2001U, other->exportObmmInfo[0].memId);
        EXPECT_EQ(otherRollbackSucceeds ? UB_MEM_HEALTHY : UB_MEM_SWITCHING, other->exportObmmInfo[0].memIdStatus);
    }
}

UbseResult ExportCommitReplyLostThenRejectRollback(UbseComModule*, const SendParam&, MappedEndPtr& req,
                                                   UbseBaseMessagePtr&, const bool)
{
    const auto& data = req->GetUbseMesgInfo();
    if (data.rpcInfo.blockIndex != g_committedRollbackIndex) {
        EXPECT_LT(data.rpcInfo.blockIndex, g_committedRollbackIndex);
        EXPECT_EQ(1U, data.success);
        return ReportMigrateResult({data.rpcInfo, TestResultKind::End}, UbseMemMigrateOpResp{UBSE_OK});
    }
    if (data.success == 1) {
        ++g_lostCommitSendAttempts;
        return UBSE_ERR_TIMEOUT;
    }
    g_rollbackNodes.push_back(data.rpcInfo.srcNodeId);
    return ReportMigrateResult({data.rpcInfo, TestResultKind::End}, CommittedRollbackResponse(data.rpcInfo, true));
}

UbseResult MappedExportAccountCountStub(SchedulerImpl* impl, UbseMemShareBorrowExportObj& obj)
{
    if (obj.status.state == UBSE_MEM_EXPORT_SUCCESS && !obj.algoResult.exportNumaInfos.empty() &&
        obj.algoResult.exportNumaInfos.front().nodeId == MIGRATE_DST_NODE_ID) {
        ++g_mappedExportCommits;
    }
    return MigrateScheduleStub(impl, obj);
}

void CheckExportCommittedRollback(uint32_t blockIndex, bool hasBlock)
{
    MigrateFaultQueryMockSet();
    BuildMigrateExportObjWithBlocks(MIGRATE_SRC_NODE_ID, MIGRATE_SIZE_BYTES, MIGRATE_EXPECT_BLOCK_COUNT);
    PutMigrateImportObj(MIGRATE_DST_NODE_ID);
    const auto oldTargetImport = GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, oldTargetImport);
    MigrateNodeMockSetForExecute();
    MOCKER(&ShareBorrowSchedule).stubs().will(invoke(&MigratePreparedScheduleStub));
    MOCKER(&AsyncMemShmMigrateProcessor).stubs().will(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    ASSERT_EQ(UBSE_OK, service.Accept(BuildMigrateReq(DST_SLOT_ID)));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowExportObj>)
        .stubs()
        .will(invoke(&MappedExportAccountCountStub));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemShareBorrowImportObj>)
        .stubs()
        .will(invoke(&MappedImportAccountStub));
    MigrateComMockSet();
    const auto begin = &UbseComModule::RpcSend<MappedBeginPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(begin).stubs().will(invoke(&MappedBeginCheckFaultStub));
    const auto copy = &UbseComModule::RpcSend<MappedCopyPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(copy).stubs().will(invoke(&MappedCopyRpcStub));
    const auto end = &UbseComModule::RpcSend<MappedEndPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(end).stubs().will(invoke(&ExportCommitReplyLostThenRejectRollback));
    g_committedRollbackIndex = blockIndex;
    g_committedRollbackHasBlock = hasBlock;
    g_lostCommitSendAttempts = 0;
    g_mappedImportRemovals = 0;
    g_mappedExportCommits = 0;
    g_mappedBeginSends = 0;
    g_rollbackNodes.clear();
    ASSERT_EQ(UBSE_OK, service.Execute(SHM_NAME));
    ASSERT_TRUE(WaitMigrateTerminal(SHM_NAME));
    EXPECT_EQ(blockIndex + 1, g_mappedBeginSends.load());
    UbseMemShmMigrateSession snapshot{};
    ASSERT_TRUE(UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(SHM_NAME, snapshot));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_FAILED, snapshot.state);
    EXPECT_EQ(MIGRATE_FAIL_RPC, snapshot.failReason);
    EXPECT_EQ(blockIndex, snapshot.completedBlocks.value_or(0));
    EXPECT_TRUE(snapshot.rebuildRequired);
    EXPECT_EQ(SEND_RETRY_TIMES, g_lostCommitSendAttempts.load());
    EXPECT_EQ(std::vector<std::string>{MIGRATE_DST_NODE_ID}, g_rollbackNodes);
    const auto target = GetShareExportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    const auto local = GetShareImportObj(MIGRATE_DST_NODE_ID, SHM_NAME);
    ASSERT_NE(nullptr, target);
    ASSERT_NE(nullptr, local);
    EXPECT_NE(0U, target->status.exportObmmInfo[blockIndex].desc.addr);
    EXPECT_EQ(MIGRATE_SIZE_BYTES / MIGRATE_EXPECT_BLOCK_COUNT, target->status.exportObmmInfo[blockIndex].desc.length);
    EXPECT_TRUE(ShmMigrateIsExport(target->migrateInfos[blockIndex]));
    for (uint32_t index = 0; index < blockIndex; ++index) {
        EXPECT_EQ(target->status.exportObmmInfo[index].memId, local->exportObmmInfo[index].memId);
        EXPECT_EQ(ShmMigrateImportInfo(target->migrateInfos[index]), local->migrateInfos[index]);
    }
    if (!hasBlock) {
        EXPECT_EQ(UB_MEM_SWITCHING, target->status.exportObmmInfo[blockIndex].memIdStatus);
        EXPECT_EQ(UB_MEM_SWITCHING, local->exportObmmInfo[blockIndex].memIdStatus);
        EXPECT_EQ(oldTargetImport->exportObmmInfo[blockIndex].memId, local->exportObmmInfo[blockIndex].memId);
        EXPECT_EQ(oldTargetImport->migrateInfos[blockIndex], local->migrateInfos[blockIndex]);
        ASSERT_TRUE(local->switchContext);
        EXPECT_FALSE(local->switchContext->endDecision.has_value());
        EXPECT_TRUE(local->realExe);
        EXPECT_EQ(UBSE_MEM_EXPORT_RUNNING, target->status.state);
        EXPECT_EQ(0U, g_mappedImportRemovals.load());
        EXPECT_EQ(0U, g_mappedExportCommits.load());
        return;
    }
    EXPECT_EQ(UB_MEM_HEALTHY, target->status.exportObmmInfo[blockIndex].memIdStatus);
    EXPECT_EQ(UB_MEM_HEALTHY, local->exportObmmInfo[blockIndex].memIdStatus);
    ASSERT_TRUE(local->switchContext);
    EXPECT_EQ(true, local->switchContext->endDecision);
    ExpectMigrateFaultStatus(std::nullopt);
    EXPECT_EQ(target->status.exportObmmInfo[blockIndex].memId, local->exportObmmInfo[blockIndex].memId);
    EXPECT_EQ(ShmMigrateImportInfo(target->migrateInfos[blockIndex]), local->migrateInfos[blockIndex]);
    const bool lastBlock = blockIndex + 1 == MIGRATE_EXPECT_BLOCK_COUNT;
    EXPECT_EQ(!lastBlock, local->realExe);
    EXPECT_EQ(lastBlock ? UBSE_MEM_EXPORT_SUCCESS : UBSE_MEM_EXPORT_RUNNING, target->status.state);
    EXPECT_EQ(lastBlock ? 1U : 0U, g_mappedImportRemovals.load());
    EXPECT_EQ(lastBlock ? 1U : 0U, g_mappedExportCommits.load());
    if (lastBlock) {
        EXPECT_EQ(MIGRATE_DST_NODE_ID, local->req.baseNodeId);
    }
}
} // namespace

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCommittedCopyRollbackClearsSingleImportStatus)
{
    CheckImportCommittedRollback(false, true);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCommittedCopyRollbackContinuesOtherImports)
{
    CheckImportCommittedRollback(true, true);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCommittedCopyRollbackWithoutBlockRejectsLedgerUpdate)
{
    CheckImportCommittedRollback(false, false);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCommittedCopyRollbackKeepsUnconfirmedOtherImportSwitching)
{
    CheckImportCommittedRollback(true, true, false);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateIncompleteCopyCommitDoesNotConfirmEnd)
{
    CheckImportCommittedRollback(true, false, true, CommitFailureKind::BUSINESS_ERROR);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCommittedTargetRollbackWithoutBlockRejectsLedgerUpdate)
{
    CheckExportCommittedRollback(0, false);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateCommittedLastTargetRollbackConvertsIdentityAndAccounts)
{
    CheckExportCommittedRollback(MIGRATE_EXPECT_BLOCK_COUNT - 1, true);
}

TEST_F(TestUbseMemControllerMigrateApi, ShmMigrateIncompleteTargetResultPreservesExistingLedger)
{
    CheckExportCommittedRollback(MIGRATE_EXPECT_BLOCK_COUNT - 1, false);
}

} // namespace ubse::mem_controller::share::ut
