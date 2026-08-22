/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "mockcpp/mokc.h"
#include "over_commit_pid_fault_error_util.h"
#include "over_commit_pid_fault_handler.cpp"
#include "over_commit_pid_fault_handler.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::over_commit {

using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;

class TestPidFaultErrorCodeHandler : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

// 构造执行请求的序列化buffer（调用方负责freeFunc释放）
static UbseByteBuffer BuildExecuteReqBuffer(const FaultPidExecuteRequest& request)
{
    RmrsOutStream builder;
    FaultPidExecuteRequestSerialization(builder, request);
    UbseByteBuffer req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t* data) {
                              delete[] data;
                          }};
    return req;
}

// ==================== Step1: 直接归还失败 → n=7 RETURN_MEM ====================

/*
 * 用例描述：直接归还borrowId失败（非NOT_EXIST），响应聚合码为归还内存错误
 * 前置条件：MemFreeWithOps打桩失败
 * 步骤：下发仅含directReturnBorrowIds的请求，调用PidExecuteRecvHandler
 * 预期：返回值与响应retCode均为MEM_POOLING_FAULT_RETURN_MEM_ERROR，且未记录已归还
 */
TEST_F(TestPidFaultErrorCodeHandler, PidExecuteRecvHandler_DirectReturnFail_ReturnReturnMemError)
{
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(MemBorrowExecutor::*)(const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    FaultPidExecuteRequest request;
    request.faultNodeId = "node1";
    request.directReturnBorrowIds = {"bid-direct"};
    UbseByteBuffer req = BuildExecuteReqBuffer(request);
    UbseByteBuffer resp{};

    uint32_t ret = PidFaultHandler::PidExecuteRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_FAULT_RETURN_MEM_ERROR);

    FaultPidExecuteResponse response;
    RmrsInStream in(resp.data, resp.len);
    ASSERT_EQ(FaultPidExecuteResponseDeserialization(in, response), MEM_POOLING_OK);
    EXPECT_EQ(response.retCode, MEM_POOLING_FAULT_RETURN_MEM_ERROR);
    EXPECT_TRUE(response.freedBorrowIds.empty());

    resp.freeFunc(resp.data);
    req.freeFunc(req.data);
}

// ==================== Step2: smap探活失败task停BORROWED → n=2 MIGRATE ====================

/*
 * 用例描述：smap迁移能力不可用，BORROWED task本轮无法迁移，per-task码归为迁移失败
 * 前置条件：GetSmapMigratePidRemoteNumaFunc打桩返回nullptr
 * 步骤：下发单个BORROWED task的执行请求，调用PidExecuteRecvHandler
 * 预期：响应retCode为MEM_POOLING_FAULT_MIGRATE_ERROR，task停在BORROWED等下轮RESUME
 */
TEST_F(TestPidFaultErrorCodeHandler, PidExecuteRecvHandler_MigrateUnavailable_ReturnMigrateError)
{
    smap::SmapMigratePidRemoteNumaFunc nullFunc = nullptr;
    MOCKER_CPP(&smap::SmapModule::GetSmapMigratePidRemoteNumaFunc, smap::SmapMigratePidRemoteNumaFunc(*)())
        .stubs()
        .will(returnValue(nullFunc));

    FaultPidExecuteRequest request;
    request.faultNodeId = "node1";
    MigrationTask task;
    task.taskId = "task1";
    task.phase = TaskPhase::BORROWED;
    task.pids = {100};
    task.newBorrowId = "bid-new";
    task.newRemoteNumaId = 1;
    request.tasks.push_back(task);
    UbseByteBuffer req = BuildExecuteReqBuffer(request);
    UbseByteBuffer resp{};

    uint32_t ret = PidFaultHandler::PidExecuteRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_FAULT_MIGRATE_ERROR);

    FaultPidExecuteResponse response;
    RmrsInStream in(resp.data, resp.len);
    ASSERT_EQ(FaultPidExecuteResponseDeserialization(in, response), MEM_POOLING_OK);
    EXPECT_EQ(response.retCode, MEM_POOLING_FAULT_MIGRATE_ERROR);
    ASSERT_EQ(response.taskResults.size(), 1U);
    EXPECT_EQ(response.taskResults[0].taskId, "task1");
    EXPECT_EQ(response.taskResults[0].retCode, MEM_POOLING_FAULT_MIGRATE_ERROR);
    EXPECT_EQ(response.taskResults[0].completedPhase, TaskPhase::BORROWED);

    resp.freeFunc(resp.data);
    req.freeFunc(req.data);
}

// ==================== 出口聚合: 空请求全成功 → OK ====================

/*
 * 用例描述：无任何task与直接归还时失败码集合为空，聚合结果为OK
 * 前置条件：无
 * 步骤：下发空请求，调用PidExecuteRecvHandler
 * 预期：返回值与响应retCode均为MEM_POOLING_OK
 */
TEST_F(TestPidFaultErrorCodeHandler, PidExecuteRecvHandler_EmptyRequest_ReturnOk)
{
    FaultPidExecuteRequest request;
    request.faultNodeId = "node1";
    UbseByteBuffer req = BuildExecuteReqBuffer(request);
    UbseByteBuffer resp{};

    uint32_t ret = PidFaultHandler::PidExecuteRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_OK);

    FaultPidExecuteResponse response;
    RmrsInStream in(resp.data, resp.len);
    ASSERT_EQ(FaultPidExecuteResponseDeserialization(in, response), MEM_POOLING_OK);
    EXPECT_EQ(response.retCode, MEM_POOLING_OK);
    EXPECT_TRUE(response.taskResults.empty());

    resp.freeFunc(resp.data);
    req.freeFunc(req.data);
}

// ==================== MigrateSingleTask: 跟随纳管模式per-pid下发 + 迁移后纳管移除 ====================

// 捕获逐pid下发的迁移msg
static std::vector<smap::MigrateEscapeMsg> gCapturedMigrateMsgs;
MpResult MockSmapMigrateMultiNumaCapture(smap::MigrateEscapeMsg& msg)
{
    gCapturedMigrateMsgs.push_back(msg);
    return MEM_POOLING_OK;
}

// mock各源numa上的pid纳管配置查询
static std::unordered_map<uint16_t, std::unordered_map<pid_t, smap::ProcessPayload>> gMockManagedByNuma;
MpResult MockGetVmRatioOnFaultNumaBySmap(const int16_t faultNumaId,
                                         std::unordered_map<pid_t, smap::ProcessPayload>& processPayloadMap)
{
    auto it = gMockManagedByNuma.find(static_cast<uint16_t>(faultNumaId));
    if (it != gMockManagedByNuma.end()) {
        processPayloadMap = it->second;
    }
    return MEM_POOLING_OK;
}

// 记录smap迁移开关调用序列（0禁用/1启用），验证先禁用→迁移→再启用的顺序；
// gEnableHelperRet控制返回值（mockcpp的hook在verify后不解除，行为切换必须走开关而非重复注册）
static std::vector<int> gEnableCallSeq;
static int gEnableHelperRet = 0;
int MockSmapEnableProcessMigrateCapture(pid_t* pidArr, int len, int enable, int flags)
{
    (void)pidArr;
    (void)len;
    (void)flags;
    gEnableCallSeq.push_back(enable);
    return gEnableHelperRet;
}

// 捕获故障numa纳管移除调用（pid列表+目标numa），返回值可控
static std::vector<std::pair<std::vector<pid_t>, int16_t>> gRemoveCalls;
static MpResult gRemoveRet = MEM_POOLING_OK;
MpResult MockSmapRemovePidsCapture(const std::vector<pid_t>& pids, int16_t remoteNumaId)
{
    gRemoveCalls.emplace_back(pids, remoteNumaId);
    return gRemoveRet;
}
using SmapRemovePidsHelperFunc = MpResult (*)(const std::vector<pid_t>&, int16_t);

void MockMigrateSmapDeps()
{
    gEnableHelperRet = 0;
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, int, int, int))
        .stubs()
        .will(invoke(MockSmapEnableProcessMigrateCapture));
    MOCKER_CPP(&MpSmapHelper::SmapMigratePidMultiRemoteNumaHelperWithRetry, MpResult(*)(smap::MigrateEscapeMsg&))
        .stubs()
        .will(invoke(MockSmapMigrateMultiNumaCapture));
    MOCKER_CPP(&MpSmapHelper::GetVmRatioOnFaultNumaBySmap,
               MpResult(*)(const int16_t, std::unordered_map<pid_t, smap::ProcessPayload>&))
        .stubs()
        .will(invoke(MockGetVmRatioOnFaultNumaBySmap));
    MOCKER_CPP(&MpSmapHelper::SmapRemovePidsHelper, SmapRemovePidsHelperFunc)
        .stubs()
        .will(invoke(MockSmapRemovePidsCapture));
}

/*
 * 用例描述：容器task聚合两个pid的故障numa占用，逐pid按各自纳管配置下发（非task级聚合，非固定memsize模式）
 * 前置条件：pid100为ratio纳管(ratio=30)，pid200为memsize纳管(memSize=2048KB)
 * 预期：两次下发各自跟随纳管模式与纳管值，下发内容与实采KB无关
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateSingleTask_FollowManagedModePerPid)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_OK;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    smap::ProcessPayload p200{};
    p200.pid = 200;
    p200.migrateMode = static_cast<uint8_t>(MIG_MEMSIZE_MODE);
    p200.memSize = 2048;
    gMockManagedByNuma[5] = {{100, p100}, {200, p200}};
    MockMigrateSmapDeps();

    MigrationTask task;
    task.taskId = "task1";
    task.pids = {100, 200};
    task.newRemoteNumaId = 9;
    NumaMemUsage u1;
    u1.pid = 100;
    u1.numaId = 5;
    u1.isLocal = false;
    u1.usedMemKB = 1024;
    NumaMemUsage u2;
    u2.pid = 200;
    u2.numaId = 5;
    u2.isLocal = false;
    u2.usedMemKB = 2048;
    task.faultNumaUsages = {u1, u2};

    EXPECT_EQ(MigrateSingleTask(task), TaskPhase::REMOVED);
    ASSERT_EQ(gCapturedMigrateMsgs.size(), 2U);
    std::unordered_map<pid_t, smap::MigrateEscapePayload> sent;
    for (const auto& msg : gCapturedMigrateMsgs) {
        ASSERT_EQ(msg.count, 1);
        EXPECT_EQ(msg.payload[0].destNid, 9);
        EXPECT_EQ(msg.payload[0].srcNid, 5);
        sent[msg.payload[0].pid] = msg.payload[0];
    }
    ASSERT_EQ(sent.size(), 2U);
    EXPECT_EQ(sent[100].migrateMode, MIG_RATIO_MODE);
    EXPECT_EQ(sent[100].ratio, 30);
    EXPECT_EQ(sent[200].migrateMode, MIG_MEMSIZE_MODE);
    EXPECT_EQ(sent[200].memSize, 2048U);
    // 迁移成功后移除故障numa纳管（防冷热流动写回），再恢复enable
    ASSERT_EQ(gRemoveCalls.size(), 1U);
    EXPECT_EQ(gRemoveCalls[0].second, 5);
    EXPECT_EQ(gRemoveCalls[0].first.size(), 2U);
    // smap约束: 迁移前必须先禁用pid冷热迁移，纳管移除后才恢复启用
    ASSERT_EQ(gEnableCallSeq.size(), 2U);
    EXPECT_EQ(gEnableCallSeq[0], 0);
    EXPECT_EQ(gEnableCallSeq[1], 1);
}

/*
 * 用例描述：迁移成功但故障numa纳管移除失败
 * 预期：停MIGRATED（下轮只做remove），不恢复enable（pid保持禁用态），不进入归还
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateSingleTask_RemoveFail_StaysMigrated)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_ERROR;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateSmapDeps();

    MigrationTask task;
    task.taskId = "task-remove-fail";
    task.pids = {100};
    task.newRemoteNumaId = 9;
    NumaMemUsage u1;
    u1.pid = 100;
    u1.numaId = 5;
    u1.isLocal = false;
    u1.usedMemKB = 1024;
    task.faultNumaUsages = {u1};

    EXPECT_EQ(MigrateSingleTask(task), TaskPhase::MIGRATED);
    ASSERT_EQ(gCapturedMigrateMsgs.size(), 1U);
    ASSERT_EQ(gRemoveCalls.size(), 1U);
    // remove失败不恢复enable: 开关序列只有初始禁用
    ASSERT_EQ(gEnableCallSeq.size(), 1U);
    EXPECT_EQ(gEnableCallSeq[0], 0);
}

/*
 * 用例描述：迁移前禁用smap迁移开关失败（smap约束: 启用状态下无法迁移）
 * 预期：不下发任何迁移请求，记失败保持BORROWED下轮重试，也不再调用启用
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateSingleTask_DisableFailed_NoMigrate)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateSmapDeps();
    // 通过开关切换禁用失败（重复注册同一函数的mock不生效）
    gEnableHelperRet = -1;

    MigrationTask task;
    task.taskId = "task7";
    task.pids = {100};
    task.newRemoteNumaId = 9;
    NumaMemUsage u1;
    u1.pid = 100;
    u1.numaId = 5;
    u1.isLocal = false;
    u1.usedMemKB = 1024;
    task.faultNumaUsages = {u1};

    EXPECT_EQ(MigrateSingleTask(task), TaskPhase::NONE);
    EXPECT_TRUE(gCapturedMigrateMsgs.empty());
    ASSERT_EQ(gEnableCallSeq.size(), 1U);
    EXPECT_EQ(gEnableCallSeq[0], 0);
}

/*
 * 用例描述：pid在task.pids中但无占用明细时跳过，有明细且已纳管的pid正常下发
 * 预期：仅下发有明细且已纳管的pid，整体成功推进到REMOVED
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateSingleTask_SkipPidWithoutUsage)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_OK;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 50;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateSmapDeps();

    MigrationTask task;
    task.taskId = "task2";
    task.pids = {100, 200};
    task.newRemoteNumaId = 9;
    NumaMemUsage u1;
    u1.pid = 100;
    u1.numaId = 5;
    u1.isLocal = false;
    u1.usedMemKB = 4096;
    task.faultNumaUsages = {u1};

    EXPECT_EQ(MigrateSingleTask(task), TaskPhase::REMOVED);
    ASSERT_EQ(gCapturedMigrateMsgs.size(), 1U);
    EXPECT_EQ(gCapturedMigrateMsgs[0].payload[0].pid, 100);
    EXPECT_EQ(gCapturedMigrateMsgs[0].payload[0].migrateMode, MIG_RATIO_MODE);
    EXPECT_EQ(gCapturedMigrateMsgs[0].payload[0].ratio, 50);
}

/*
 * 用例描述：task.pids中的pid全部无占用明细，无可迁移payload
 * 预期：记失败（保持BORROWED下轮重试），不下发任何迁移请求
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateSingleTask_NoUsageForAnyPid_Fail)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    MockMigrateSmapDeps();

    MigrationTask task;
    task.taskId = "task3";
    task.pids = {100};
    task.newRemoteNumaId = 9;

    EXPECT_EQ(MigrateSingleTask(task), TaskPhase::NONE);
    EXPECT_TRUE(gCapturedMigrateMsgs.empty());
    // 无占用明细在禁用前提前返回，不应触碰smap迁移开关
    EXPECT_TRUE(gEnableCallSeq.empty());
}

/*
 * 用例描述：pid有采集明细但不在smap纳管配置中，无纳管占用预期
 * 预期：跳过该pid记失败（保持BORROWED下轮重试），不下发迁移请求；
 *       迁移失败不恢复enable（pid停留故障numa，禁用态保持）
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateSingleTask_PidNotInManagedConfig_Fail)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    // numa5纳管配置为空（pid100不在其中）
    MockMigrateSmapDeps();

    MigrationTask task;
    task.taskId = "task5";
    task.pids = {100};
    task.newRemoteNumaId = 9;
    NumaMemUsage u1;
    u1.pid = 100;
    u1.numaId = 5;
    u1.isLocal = false;
    u1.usedMemKB = 1024;
    task.faultNumaUsages = {u1};

    EXPECT_EQ(MigrateSingleTask(task), TaskPhase::NONE);
    EXPECT_TRUE(gCapturedMigrateMsgs.empty());
    ASSERT_EQ(gEnableCallSeq.size(), 1U);
    EXPECT_EQ(gEnableCallSeq[0], 0);
}

/*
 * 用例描述：纳管配置无效（ratio模式但ratio=0），下发会被smap报参数错误
 * 预期：跳过该纳管条目，无可下发payload记失败保持BORROWED
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateSingleTask_InvalidManagedQuota_Fail)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 0;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateSmapDeps();

    MigrationTask task;
    task.taskId = "task6";
    task.pids = {100};
    task.newRemoteNumaId = 9;
    NumaMemUsage u1;
    u1.pid = 100;
    u1.numaId = 5;
    u1.isLocal = false;
    u1.usedMemKB = 1024;
    task.faultNumaUsages = {u1};

    EXPECT_EQ(MigrateSingleTask(task), TaskPhase::NONE);
    EXPECT_TRUE(gCapturedMigrateMsgs.empty());
}

/*
 * 用例描述：RESUME自MIGRATED的纳管移除重试（上轮迁移成功但remove失败）
 * 预期：全部源numa移除成功→恢复enable→REMOVED；任一失败→保持禁用态→MIGRATED
 */
TEST_F(TestPidFaultErrorCodeHandler, RemoveRetry_AllSuccess_Removed)
{
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_OK;
    gEnableCallSeq.clear();
    MOCKER_CPP(&MpSmapHelper::SmapRemovePidsHelper, SmapRemovePidsHelperFunc)
        .stubs()
        .will(invoke(MockSmapRemovePidsCapture));
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, int, int, int))
        .stubs()
        .will(invoke(MockSmapEnableProcessMigrateCapture));

    MigrationTask task;
    task.taskId = "task-retry";
    task.pids = {100};
    NumaMemUsage u1;
    u1.pid = 100;
    u1.numaId = 5;
    u1.isLocal = false;
    u1.usedMemKB = 1024;
    task.faultNumaUsages = {u1};

    EXPECT_EQ(RemoveSingleTaskFaultNumaManaged(task), TaskPhase::REMOVED);
    ASSERT_EQ(gRemoveCalls.size(), 1U);
    EXPECT_EQ(gRemoveCalls[0].second, 5);
    ASSERT_EQ(gEnableCallSeq.size(), 1U);
    EXPECT_EQ(gEnableCallSeq[0], 1);
}

TEST_F(TestPidFaultErrorCodeHandler, RemoveRetry_RemoveFail_StaysMigrated)
{
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_ERROR;
    gEnableCallSeq.clear();
    MOCKER_CPP(&MpSmapHelper::SmapRemovePidsHelper, SmapRemovePidsHelperFunc)
        .stubs()
        .will(invoke(MockSmapRemovePidsCapture));
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, int, int, int))
        .stubs()
        .will(invoke(MockSmapEnableProcessMigrateCapture));

    MigrationTask task;
    task.taskId = "task-retry-fail";
    task.pids = {100};
    NumaMemUsage u1;
    u1.pid = 100;
    u1.numaId = 5;
    u1.isLocal = false;
    u1.usedMemKB = 1024;
    task.faultNumaUsages = {u1};

    EXPECT_EQ(RemoveSingleTaskFaultNumaManaged(task), TaskPhase::MIGRATED);
    // remove失败不恢复enable
    EXPECT_TRUE(gEnableCallSeq.empty());
}

// ==================== MigrateTaskGroup: 虚机场景借来内存分大页 ====================

// 捕获幂等分大页调用（numaId与借用量Byte）
static std::vector<std::pair<uint16_t, uint64_t>> gHugeAllocCalls;
static MpResult gHugeAllocRet = MEM_POOLING_OK;
MpResult MockIdempotentAllocateHugePages(MpSmapHelper* self, uint64_t numaId, uint64_t borrowSize)
{
    (void)self;
    gHugeAllocCalls.emplace_back(static_cast<uint16_t>(numaId), borrowSize);
    return gHugeAllocRet;
}
using IdempotentAllocateHugePagesFunc = MpResult (*)(MpSmapHelper*, uint64_t, uint64_t);
using GetSceneTypeFunc = MpSceneType (*)(const MpConfiguration*);
using GetPageTypeFunc = PageType (*)(MpConfiguration*);
using SetSmapRemoteNumaInfoFunc = MpResult (*)(const int16_t&, const std::vector<MemBorrowInfoWithSrc>&);

// 捕获smap借用信息设置调用（首参srcNumaId与infos）
static std::vector<std::pair<int16_t, std::vector<MemBorrowInfoWithSrc>>> gSetSmapCalls;

PageType MockGetPageType2M(MpConfiguration* self)
{
    (void)self;
    return PageType::PAGE_2M;
}

MpResult MockSetSmapRemoteNumaInfoOk(const int16_t& srcNumaId, const std::vector<MemBorrowInfoWithSrc>& infos)
{
    gSetSmapCalls.emplace_back(srcNumaId, infos);
    return MEM_POOLING_OK;
}

void MockMigrateGroupDeps()
{
    // const成员函数invoke的this参数匹配有问题，用returnValue（对齐存量用例惯例）
    MOCKER_CPP(&MpConfiguration::GetSceneType, GetSceneTypeFunc).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(&MpConfiguration::GetPageType, GetPageTypeFunc).stubs().will(invoke(MockGetPageType2M));
    MOCKER_CPP(&MpSmapHelper::IdempotentAllocateHugePages, IdempotentAllocateHugePagesFunc)
        .stubs()
        .will(invoke(MockIdempotentAllocateHugePages));
    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo, SetSmapRemoteNumaInfoFunc)
        .stubs()
        .will(invoke(MockSetSmapRemoteNumaInfoOk));
    MockMigrateSmapDeps();
}

MigrationTask BuildMigratableTask(const std::string& taskId, uint64_t borrowSizeKB)
{
    MigrationTask task;
    task.taskId = taskId;
    task.pids = {100};
    task.newBorrowId = "bid-new";
    task.newRemoteNumaId = 9;
    task.newBorrowSizeKB = borrowSizeKB;
    NumaMemUsage usage;
    usage.pid = 100;
    usage.numaId = 5;
    usage.isLocal = false;
    usage.usedMemKB = borrowSizeKB;
    task.faultNumaUsages = {usage};
    return task;
}

/*
 * 用例描述：虚机场景(PAGE_2M)迁移组先按聚合借用量幂等分大页，成功后再设置smap远端信息并迁移
 * 预期：分大页被调用一次且参数为目标numa与借用量(Byte)，task迁移成功
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_VirtualScene_AllocateHugePagesBeforeMigrate)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gHugeAllocCalls.clear();
    gHugeAllocRet = MEM_POOLING_OK;
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_OK;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateGroupDeps();

    MigrationTask task = BuildMigratableTask("task-huge-1", 8192);
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    EXPECT_EQ(taskPhases["task-huge-1"], TaskPhase::REMOVED);
    // 分大页参数: 目标numa=9, 借用量8192KB=8388608Byte
    ASSERT_EQ(gHugeAllocCalls.size(), 1U);
    EXPECT_EQ(gHugeAllocCalls[0].first, 9);
    EXPECT_EQ(gHugeAllocCalls[0].second, 8192ULL * 1024);
    ASSERT_EQ(gCapturedMigrateMsgs.size(), 1U);
    EXPECT_EQ(gCapturedMigrateMsgs[0].payload[0].destNid, 9);
}

/*
 * 用例描述：虚机场景分大页失败
 * 预期：整组跳过不下发迁移，task保持BORROWED下轮RESUME（幂等分配保证下轮不重复加大页）
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_HugePageAllocFail_GroupSkipped)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gHugeAllocCalls.clear();
    gHugeAllocRet = MEM_POOLING_ERROR;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateGroupDeps();

    MigrationTask task = BuildMigratableTask("task-huge-2", 8192);
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    EXPECT_TRUE(taskPhases.empty());
    ASSERT_EQ(gHugeAllocCalls.size(), 1U);
    EXPECT_TRUE(gCapturedMigrateMsgs.empty());
}

/*
 * 用例描述：容器场景不管分大页
 * 预期：不调用分大页，直接设置smap远端信息并迁移
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_ContainerScene_NoHugePages)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gHugeAllocCalls.clear();
    gHugeAllocRet = MEM_POOLING_OK;
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_OK;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
    MOCKER_CPP(&MpConfiguration::GetSceneType, GetSceneTypeFunc)
        .stubs()
        .will(returnValue(MpSceneType::CONTAINER_SCENE));
    MOCKER_CPP(&MpConfiguration::GetPageType, GetPageTypeFunc).stubs().will(invoke(MockGetPageType2M));
    MOCKER_CPP(&MpSmapHelper::IdempotentAllocateHugePages, IdempotentAllocateHugePagesFunc)
        .stubs()
        .will(invoke(MockIdempotentAllocateHugePages));
    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo, SetSmapRemoteNumaInfoFunc)
        .stubs()
        .will(invoke(MockSetSmapRemoteNumaInfoOk));
    MockMigrateSmapDeps();

    MigrationTask task = BuildMigratableTask("task-huge-3", 8192);
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    EXPECT_TRUE(gHugeAllocCalls.empty());
    ASSERT_EQ(gCapturedMigrateMsgs.size(), 1U);
}

/*
 * 用例描述：设置smap借用信息按task归属本地numa（多本地取首个）传真实srcNumaId
 * 预期：SetSmapRemoteNumaInfo首参为本地numa而非-1通配，info.srcNumaId同步为真实本地numa
 * （smap按(srcNid,destNid)累加，-1与具体numa各记一笔会叠加，归还时通配记录无法核减导致迁不回）
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_SmapInfoUsesRealLocalNuma)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gHugeAllocCalls.clear();
    gSetSmapCalls.clear();
    gHugeAllocRet = MEM_POOLING_OK;
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_OK;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateGroupDeps();

    MigrationTask task = BuildMigratableTask("task-numa-1", 1024);
    task.localNumaIds = {2, 3}; // 多本地取首个
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    ASSERT_EQ(gSetSmapCalls.size(), 1U);
    EXPECT_EQ(gSetSmapCalls[0].first, 2);
    ASSERT_EQ(gSetSmapCalls[0].second.size(), 1U);
    EXPECT_EQ(gSetSmapCalls[0].second[0].srcNumaId, 2U);
    EXPECT_EQ(gSetSmapCalls[0].second[0].presentNumaId, 9U);
    EXPECT_EQ(gSetSmapCalls[0].second[0].borrowSize, 1024U);
}

/*
 * 用例描述：同组两笔借用归属不同本地numa
 * 预期：按本地numa分桶各设一笔（numa升序），各携带自身借用量，不叠加为一笔
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_SmapInfoSplitByLocalNuma)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gHugeAllocCalls.clear();
    gSetSmapCalls.clear();
    gHugeAllocRet = MEM_POOLING_OK;
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_OK;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateGroupDeps();

    MigrationTask task1 = BuildMigratableTask("task-numa-2a", 1024);
    task1.newBorrowId = "bid-numa2";
    task1.localNumaIds = {2};
    MigrationTask task2 = BuildMigratableTask("task-numa-2b", 2048);
    task2.newBorrowId = "bid-numa3";
    task2.localNumaIds = {3};
    std::vector<const MigrationTask*> group = {&task1, &task2};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 2U);
    ASSERT_EQ(gSetSmapCalls.size(), 2U);
    EXPECT_EQ(gSetSmapCalls[0].first, 2);
    EXPECT_EQ(gSetSmapCalls[0].second[0].borrowSize, 1024U);
    EXPECT_EQ(gSetSmapCalls[1].first, 3);
    EXPECT_EQ(gSetSmapCalls[1].second[0].borrowSize, 2048U);
}

/*
 * 用例描述：task无本地numa信息（异常数据）
 * 预期：兜底-1通配不劣化于存量行为（WARN可观测），迁移仍下发
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_NoLocalNumaFallsBackToWildcard)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gHugeAllocCalls.clear();
    gSetSmapCalls.clear();
    gHugeAllocRet = MEM_POOLING_OK;
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_OK;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateGroupDeps();

    MigrationTask task = BuildMigratableTask("task-numa-3", 1024);
    task.localNumaIds.clear();
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    ASSERT_EQ(gSetSmapCalls.size(), 1U);
    EXPECT_EQ(gSetSmapCalls[0].first, -1);
}

// ==================== MigrateTaskGroup: smap预算登记聚合账本存量借用量 ====================

using GetDebtInfosWithRetryFunc = MpResult (*)(std::vector<UbseNumaMemoryDebtInfo>&);
static std::vector<UbseNumaMemoryDebtInfo> gHandlerMockDebtInfos;
static MpResult gHandlerGetDebtRet = MEM_POOLING_OK;
MpResult MockHandlerGetDebtInfos(std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    debtInfos = gHandlerMockDebtInfos;
    return gHandlerGetDebtRet;
}

MpResult MockSetSmapRemoteNumaInfoFail(const int16_t& srcNumaId, const std::vector<MemBorrowInfoWithSrc>& infos)
{
    gSetSmapCalls.emplace_back(srcNumaId, infos);
    return MEM_POOLING_ERROR;
}

// 构造容器/虚机借用协议debt（usrInfo前2字节=int16本地numaId，非process_mem协议）
UbseNumaMemoryDebtInfo BuildVmProtocolDebt(const std::string& name, int16_t localNuma, int64_t remoteNuma,
                                           uint64_t sizeKB)
{
    UbseNumaMemoryDebtInfo debt{};
    debt.name = name;
    debt.borrowNodeId = "mock_node_id";
    debt.remoteNumaId = remoteNuma;
    debt.size = sizeKB * 1024;
    debt.state = UbseMemStage::UBSE_EXIST;
    (void)memcpy_s(debt.usrInfo, sizeof(localNuma), &localNuma, sizeof(localNuma));
    return debt;
}

void MockMigrateGroupWithLedger()
{
    MockMigrateGroupDeps();
    MOCKER_CPP(&MemBorrowExecutor::GetDebtInfosWithRetry, GetDebtInfosWithRetryFunc)
        .stubs()
        .will(invoke(MockHandlerGetDebtInfos));
}

void PrepareMigrateGroupBase()
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gHugeAllocCalls.clear();
    gSetSmapCalls.clear();
    gHugeAllocRet = MEM_POOLING_OK;
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_OK;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
}

/*
 * 用例描述：新借用并入同号存量呈现numa（存量借用不在本轮borrowId中）
 * 预期：登记量=账本存量+本次借用量（防覆盖缩水存量预算导致迁移-92），srcNumaId为归属本地numa
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_RegisterAggregatesLedgerDebt)
{
    PrepareMigrateGroupBase();
    gHandlerMockDebtInfos = {BuildVmProtocolDebt("bid-old", 2, 9, 2048)};
    gHandlerGetDebtRet = MEM_POOLING_OK;
    MockMigrateGroupWithLedger();

    MigrationTask task = BuildMigratableTask("task-ledger-1", 1024);
    task.localNumaIds = {2};
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    ASSERT_EQ(gSetSmapCalls.size(), 1U);
    EXPECT_EQ(gSetSmapCalls[0].first, 2);
    ASSERT_EQ(gSetSmapCalls[0].second.size(), 1U);
    EXPECT_EQ(gSetSmapCalls[0].second[0].srcNumaId, 2U);
    EXPECT_EQ(gSetSmapCalls[0].second[0].presentNumaId, 9U);
    EXPECT_EQ(gSetSmapCalls[0].second[0].borrowSize, 3072U); // 存量2048 + 本次1024
}

/*
 * 用例描述：虚机场景新借用并入同号存量呈现numa（账本有存量借用）
 * 预期：分大页口径=累计借用量（存量+本次），防按本次借用量分大页被存量已分大页
 * 达标判据误跳过，导致合并后大页不足迁移数据无处落
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_HugePagesUseCumulativeLedgerSize)
{
    PrepareMigrateGroupBase();
    gHandlerMockDebtInfos = {BuildVmProtocolDebt("bid-old", 2, 9, 2048)};
    gHandlerGetDebtRet = MEM_POOLING_OK;
    MockMigrateGroupWithLedger();

    MigrationTask task = BuildMigratableTask("task-huge-ledger", 1024);
    task.localNumaIds = {2};
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    ASSERT_EQ(gHugeAllocCalls.size(), 1U);
    EXPECT_EQ(gHugeAllocCalls[0].first, 9);
    EXPECT_EQ(gHugeAllocCalls[0].second, 3072ULL * 1024); // 存量2048 + 本次1024 = 3072KB
}

/*
 * 用例描述：本轮borrowId已入账本（借用刚完成即查账本）
 * 预期：不重复计入，登记量=账本累计量
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_RegisterThisRoundInLedger_NoDoubleCount)
{
    PrepareMigrateGroupBase();
    gHandlerMockDebtInfos = {BuildVmProtocolDebt("bid-new", 2, 9, 1024)};
    gHandlerGetDebtRet = MEM_POOLING_OK;
    MockMigrateGroupWithLedger();

    MigrationTask task = BuildMigratableTask("task-ledger-2", 1024);
    task.localNumaIds = {2};
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    ASSERT_EQ(gSetSmapCalls.size(), 1U);
    EXPECT_EQ(gSetSmapCalls[0].second[0].borrowSize, 1024U); // 不叠加为2048
}

/*
 * 用例描述：裸机process_mem协议debt（usrInfo前4字节=pluginId，srcNuma字段存本地numa）
 * 预期：按协议解析srcNuma归属分桶，与本次借用桶各自登记
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_RegisterParsesProcessMemProtocolDebt)
{
    PrepareMigrateGroupBase();
    UbseNumaMemoryDebtInfo debt{};
    debt.name = "bid-baremetal";
    debt.borrowNodeId = "mock_node_id";
    debt.remoteNumaId = 9;
    debt.size = 2048 * 1024;
    debt.state = UbseMemStage::UBSE_EXIST;
    process_mem::def::ProcessMemUsrInfo usr{}; // pluginId默认PROCESS_MEM
    usr.srcNuma = 3;
    (void)memcpy_s(debt.usrInfo, sizeof(usr), &usr, sizeof(usr));
    gHandlerMockDebtInfos = {debt};
    gHandlerGetDebtRet = MEM_POOLING_OK;
    MockMigrateGroupWithLedger();

    MigrationTask task = BuildMigratableTask("task-ledger-3", 1024);
    task.localNumaIds = {2};
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    ASSERT_EQ(gSetSmapCalls.size(), 2U); // 本地numa升序: 2本次桶、3存量桶
    EXPECT_EQ(gSetSmapCalls[0].first, 2);
    EXPECT_EQ(gSetSmapCalls[0].second[0].borrowSize, 1024U);
    EXPECT_EQ(gSetSmapCalls[1].first, 3);
    EXPECT_EQ(gSetSmapCalls[1].second[0].borrowSize, 2048U);
}

/*
 * 用例描述：账本中无效debt（删除中/其他呈现numa/其他节点借入）不参与聚合
 * 预期：登记量仅本次借用量
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_RegisterFiltersInvalidDebt)
{
    PrepareMigrateGroupBase();
    auto deletingDebt = BuildVmProtocolDebt("bid-deleting", 2, 9, 4096);
    deletingDebt.state = UbseMemStage::UBSE_DELETING;
    auto otherNumaDebt = BuildVmProtocolDebt("bid-other-numa", 2, 10, 4096);
    auto otherNodeDebt = BuildVmProtocolDebt("bid-other-node", 2, 9, 8192);
    otherNodeDebt.borrowNodeId = "other_node";
    gHandlerMockDebtInfos = {deletingDebt, otherNumaDebt, otherNodeDebt};
    gHandlerGetDebtRet = MEM_POOLING_OK;
    MockMigrateGroupWithLedger();

    MigrationTask task = BuildMigratableTask("task-ledger-4", 1024);
    task.localNumaIds = {2};
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    ASSERT_EQ(gSetSmapCalls.size(), 1U);
    EXPECT_EQ(gSetSmapCalls[0].second[0].borrowSize, 1024U);
}

/*
 * 用例描述：账本查询失败
 * 预期：降级仅登记本次借用量（存量行为），迁移仍下发
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_RegisterLedgerQueryFail_FallbackThisRound)
{
    PrepareMigrateGroupBase();
    gHandlerMockDebtInfos = {};
    gHandlerGetDebtRet = MEM_POOLING_ERROR;
    MockMigrateGroupWithLedger();

    MigrationTask task = BuildMigratableTask("task-ledger-5", 1024);
    task.localNumaIds = {2};
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    ASSERT_EQ(gSetSmapCalls.size(), 1U);
    EXPECT_EQ(gSetSmapCalls[0].second[0].borrowSize, 1024U);
    ASSERT_EQ(gCapturedMigrateMsgs.size(), 1U);
}

/*
 * 用例描述：smap预算登记失败（登记失败迁移必-92）
 * 预期：整组跳过不下发迁移，task保持BORROWED下轮RESUME
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_RegisterSmapFail_GroupSkipped)
{
    PrepareMigrateGroupBase();
    gHandlerMockDebtInfos = {};
    gHandlerGetDebtRet = MEM_POOLING_OK;
    MOCKER_CPP(&MpConfiguration::GetSceneType, GetSceneTypeFunc).stubs().will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(&MpConfiguration::GetPageType, GetPageTypeFunc).stubs().will(invoke(MockGetPageType2M));
    MOCKER_CPP(&MpSmapHelper::IdempotentAllocateHugePages, IdempotentAllocateHugePagesFunc)
        .stubs()
        .will(invoke(MockIdempotentAllocateHugePages));
    MOCKER_CPP(&MpSmapHelper::SetSmapRemoteNumaInfo, SetSmapRemoteNumaInfoFunc)
        .stubs()
        .will(invoke(MockSetSmapRemoteNumaInfoFail));
    MOCKER_CPP(&MemBorrowExecutor::GetDebtInfosWithRetry, GetDebtInfosWithRetryFunc)
        .stubs()
        .will(invoke(MockHandlerGetDebtInfos));
    MockMigrateSmapDeps();

    MigrationTask task = BuildMigratableTask("task-ledger-6", 1024);
    task.localNumaIds = {2};
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    EXPECT_TRUE(taskPhases.empty());
    ASSERT_EQ(gSetSmapCalls.size(), 1U);
    EXPECT_TRUE(gCapturedMigrateMsgs.empty()); // 不空跑迁移
}

/*
 * 用例描述：同组task迁移成功但纳管移除失败
 * 预期：该task停MIGRATED进结果集（下轮只做remove），组内不阻断其他task推进
 */
TEST_F(TestPidFaultErrorCodeHandler, MigrateTaskGroup_RemoveFailTaskStaysMigrated)
{
    gCapturedMigrateMsgs.clear();
    gMockManagedByNuma.clear();
    gEnableCallSeq.clear();
    gHugeAllocCalls.clear();
    gHugeAllocRet = MEM_POOLING_OK;
    gRemoveCalls.clear();
    gRemoveRet = MEM_POOLING_ERROR;
    smap::ProcessPayload p100{};
    p100.pid = 100;
    p100.migrateMode = static_cast<uint8_t>(MIG_RATIO_MODE);
    p100.ratio = 30;
    gMockManagedByNuma[5] = {{100, p100}};
    MockMigrateGroupDeps();

    MigrationTask task = BuildMigratableTask("task-stay-mig", 1024);
    std::vector<const MigrationTask*> group = {&task};
    auto taskPhases = MigrateTaskGroup(9, group);

    ASSERT_EQ(taskPhases.size(), 1U);
    EXPECT_EQ(taskPhases["task-stay-mig"], TaskPhase::MIGRATED);
}

/*
 * 用例描述：NumaMemUsage新增pid字段随序列化透传
 * 预期：反序列化后pid不丢失（master下发→借入节点迁移据此按pid下发）；
 *       newBorrowSizeKB同步透传（借入节点设置smap远端numa借用信息免查账本）
 */
TEST_F(TestPidFaultErrorCodeHandler, NumaMemUsage_PidSurvivesSerialization)
{
    MigrationTask task;
    task.taskId = "task4";
    task.pids = {1234};
    task.newBorrowId = "bid-new";
    task.newRemoteNumaId = 9;
    task.newBorrowSizeKB = 8192;
    NumaMemUsage usage;
    usage.pid = 1234;
    usage.numaId = 7;
    usage.isLocal = false;
    usage.pageSizeKB = 64;
    usage.usedMemKB = 8192;
    task.faultNumaUsages = {usage};

    RmrsOutStream out;
    MigrationTaskSerialization(out, task);
    MigrationTask parsed;
    RmrsInStream in(out.GetBufferPointer(), out.GetSize());
    ASSERT_EQ(MigrationTaskDeserialization(in, parsed), MEM_POOLING_OK);
    ASSERT_EQ(parsed.faultNumaUsages.size(), 1U);
    EXPECT_EQ(parsed.faultNumaUsages[0].pid, 1234);
    EXPECT_EQ(parsed.faultNumaUsages[0].numaId, 7);
    EXPECT_EQ(parsed.faultNumaUsages[0].usedMemKB, 8192U);
    EXPECT_EQ(parsed.newBorrowId, "bid-new");
    EXPECT_EQ(parsed.newRemoteNumaId, 9);
    EXPECT_EQ(parsed.newBorrowSizeKB, 8192U);
}

// ==================== CollectVmPidMemInfos: 禁用冷热迁移后实采稳态占用 ====================

// 注入式mock: 实采VM列表与per-numa纳管payload/smap查询结果由用例预设；
// 首次/二次实采可分别注入（二次为空时复用首次结果），验证稳态值替代瞬时值
static std::vector<mempooling::exportV2::VmDomainInfo> gVmCollectResult;
static std::vector<mempooling::exportV2::VmDomainInfo> gVmCollectResultStable;
static MpResult gVmCollectRet = MEM_POOLING_OK;
static MpResult gVmCollectRetStable = MEM_POOLING_OK;
static uint32_t gVmCollectCallCount = 0;
MpResult MockGetVmInfoForPidCollect(std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos)
{
    gVmCollectCallCount++;
    if (gVmCollectCallCount > 1) {
        vmDomainInfos = gVmCollectResultStable.empty() ? gVmCollectResult : gVmCollectResultStable;
        return gVmCollectRetStable;
    }
    vmDomainInfos = gVmCollectResult;
    return gVmCollectRet;
}

static std::unordered_map<int, std::vector<smap::ProcessPayload>> gSmapManagedByNumaForCollect;
static std::unordered_set<int> gSmapFailNumasForCollect;
MpResult MockSmapQueryProcessConfigForCollect(int nid, std::vector<smap::ProcessPayload>& payloadList)
{
    if (gSmapFailNumasForCollect.count(nid) > 0) {
        return MEM_POOLING_ERROR;
    }
    auto it = gSmapManagedByNumaForCollect.find(nid);
    if (it != gSmapManagedByNumaForCollect.end()) {
        payloadList = it->second;
    }
    return MEM_POOLING_OK;
}

// 构造单VM: 本地numa0(3000KB) + 故障远端numa5(实采占用collectedKB，波谷值)
static mempooling::exportV2::VmDomainInfo BuildVmForCollect(pid_t pid, uint16_t faultNuma, uint64_t collectedKB)
{
    mempooling::exportV2::VmDomainInfo vm;
    vm.metaData.pid = pid;
    vm.metaData.name = "vm" + std::to_string(pid);
    vm.numaInfo[0] = {.numaId = 0, .pageSize = 2048, .usedMem = 3000, .socketId = 0, .isLocal = true};
    vm.numaInfo[static_cast<int16_t>(faultNuma)] = {.numaId = static_cast<int16_t>(faultNuma),
                                                    .pageSize = 2048,
                                                    .usedMem = static_cast<int64_t>(collectedKB),
                                                    .socketId = -1,
                                                    .isLocal = false};
    return vm;
}

static smap::ProcessPayload BuildManagedPayload(pid_t pid, uint8_t mode, uint16_t ratio, uint64_t memSize)
{
    smap::ProcessPayload payload{};
    payload.pid = pid;
    payload.migrateMode = mode;
    payload.ratio = ratio;
    payload.memSize = memSize;
    return payload;
}

// 采集阶段等待在途迁移收敛的调用计数（mock成空操作避免用例真睡3.5s）
static uint32_t gWaitQuiesceCalls = 0;
void MockWaitSmapMigrateQuiesceNoop()
{
    gWaitQuiesceCalls++;
}
using WaitSmapMigrateQuiesceFunc = void (*)();

void MockVmCollectDeps()
{
    gVmCollectCallCount = 0;
    gVmCollectResultStable.clear();
    gVmCollectRetStable = MEM_POOLING_OK;
    gEnableCallSeq.clear();
    gEnableHelperRet = 0;
    gWaitQuiesceCalls = 0;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo>&))
        .stubs()
        .will(invoke(MockGetVmInfoForPidCollect));
    MOCKER_CPP(&MpSmapHelper::SmapQueryProcessConfigHelper, MpResult(*)(int, std::vector<smap::ProcessPayload>&))
        .stubs()
        .will(invoke(MockSmapQueryProcessConfigForCollect));
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, int, int, int))
        .stubs()
        .will(invoke(MockSmapEnableProcessMigrateCapture));
    MOCKER_CPP(&MpSmapHelper::WaitSmapMigrateQuiesce, WaitSmapMigrateQuiesceFunc)
        .stubs()
        .will(invoke(MockWaitSmapMigrateQuiesceNoop));
}

/*
 * 用例描述：smap成功且有纳管，首次实采为波谷值，禁用+等待后二次实采为稳态值
 * 预期：迁移需求取稳态实采值(1500)而非波谷值(1000)也不按ratio折算；
 *       disable(enable=0)与等待在途迁移收敛各调用一次
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectVm_StableCollect_UsesSecondSnapshot)
{
    MockVmCollectDeps();
    gVmCollectRet = MEM_POOLING_OK;
    gVmCollectResult = {BuildVmForCollect(100, 5, 1000)};
    gVmCollectResultStable = {BuildVmForCollect(100, 5, 1500)};
    gSmapFailNumasForCollect = {};
    gSmapManagedByNumaForCollect = {{5, {BuildManagedPayload(100, static_cast<uint8_t>(MIG_RATIO_MODE), 30, 0)}}};

    FaultPidQueryResponse response;
    ASSERT_EQ(CollectVmPidMemInfos({5}, response), MEM_POOLING_OK);
    ASSERT_EQ(response.pidMemDistribution.size(), 1U);
    ASSERT_EQ(response.pidMemDistribution[0].faultNumaUsages.size(), 1U);
    EXPECT_EQ(response.pidMemDistribution[0].faultNumaUsages[0].usedMemKB, 1500U);
    EXPECT_TRUE(response.pendingFaultNumaIds.empty());
    ASSERT_EQ(gEnableCallSeq.size(), 1U);
    EXPECT_EQ(gEnableCallSeq[0], 0);
    EXPECT_EQ(gWaitQuiesceCalls, 1U);
}

/*
 * 用例描述：memsize纳管同样纳入处理（占用以实采为准，纳管模式不参与量化）
 * 预期：不pending，稳态实采值写入迁移需求
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectVm_MemsizeManaged_Supported)
{
    gVmCollectRet = MEM_POOLING_OK;
    gVmCollectResult = {BuildVmForCollect(100, 5, 2048)};
    gSmapFailNumasForCollect = {};
    gSmapManagedByNumaForCollect = {{5, {BuildManagedPayload(100, static_cast<uint8_t>(MIG_MEMSIZE_MODE), 0, 2048)}}};
    MockVmCollectDeps();

    FaultPidQueryResponse response;
    ASSERT_EQ(CollectVmPidMemInfos({5}, response), MEM_POOLING_OK);
    ASSERT_EQ(response.pidMemDistribution.size(), 1U);
    ASSERT_EQ(response.pidMemDistribution[0].faultNumaUsages.size(), 1U);
    EXPECT_EQ(response.pidMemDistribution[0].faultNumaUsages[0].usedMemKB, 2048U);
    EXPECT_TRUE(response.pendingFaultNumaIds.empty());
}

/*
 * 用例描述：禁用冷热迁移失败
 * 预期：本轮不采稳态值，返回采集错误等下轮重试（不pending，下轮重新全流程）
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectVm_DisableFailed_CollectError)
{
    gVmCollectRet = MEM_POOLING_OK;
    gVmCollectResult = {BuildVmForCollect(100, 5, 1000)};
    gSmapFailNumasForCollect = {};
    gSmapManagedByNumaForCollect = {{5, {BuildManagedPayload(100, static_cast<uint8_t>(MIG_RATIO_MODE), 30, 0)}}};
    MockVmCollectDeps();
    // 通过开关切换禁用失败（重复注册同一函数的mock不生效）
    gEnableHelperRet = -1;

    FaultPidQueryResponse response;
    EXPECT_EQ(CollectVmPidMemInfos({5}, response), MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
    EXPECT_TRUE(response.pidMemDistribution.empty());
    EXPECT_EQ(gWaitQuiesceCalls, 0U);
}

/*
 * 用例描述：smap查询失败但实采占用为0
 * 预期：走直接归还路径——不进采集结果也不进pending（task_builder按“无pid使用”归还）
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectVm_SmapFailNoCollectedUsage_DirectReturn)
{
    gVmCollectRet = MEM_POOLING_OK;
    gVmCollectResult = {};
    gSmapFailNumasForCollect = {5};
    gSmapManagedByNumaForCollect = {};
    MockVmCollectDeps();

    FaultPidQueryResponse response;
    ASSERT_EQ(CollectVmPidMemInfos({5}, response), MEM_POOLING_OK);
    EXPECT_TRUE(response.pidMemDistribution.empty());
    EXPECT_TRUE(response.pendingFaultNumaIds.empty());
}

/*
 * 用例描述：smap查询失败且实采占用非0
 * 预期：进pendingFaultNumaIds等下轮恢复（不建task也不归还，避免数据不确定时误操作）
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectVm_SmapFailWithCollectedUsage_Pending)
{
    gVmCollectRet = MEM_POOLING_OK;
    gVmCollectResult = {BuildVmForCollect(100, 5, 1024)};
    gSmapFailNumasForCollect = {5};
    gSmapManagedByNumaForCollect = {};
    MockVmCollectDeps();

    FaultPidQueryResponse response;
    ASSERT_EQ(CollectVmPidMemInfos({5}, response), MEM_POOLING_OK);
    EXPECT_TRUE(response.pidMemDistribution.empty());
    ASSERT_EQ(response.pendingFaultNumaIds.size(), 1U);
    EXPECT_EQ(response.pendingFaultNumaIds[0], 5);
}

/*
 * 用例描述：smap成功但无有效纳管配额（ratio=0）
 * 预期：无预期远端占用，不进采集结果也不进pending（走直接归还）
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectVm_SmapNoQuota_DirectReturn)
{
    gVmCollectRet = MEM_POOLING_OK;
    gVmCollectResult = {BuildVmForCollect(100, 5, 1024)};
    gSmapFailNumasForCollect = {};
    gSmapManagedByNumaForCollect = {{5, {BuildManagedPayload(100, static_cast<uint8_t>(MIG_RATIO_MODE), 0, 0)}}};
    MockVmCollectDeps();

    FaultPidQueryResponse response;
    ASSERT_EQ(CollectVmPidMemInfos({5}, response), MEM_POOLING_OK);
    EXPECT_TRUE(response.pidMemDistribution.empty());
    EXPECT_TRUE(response.pendingFaultNumaIds.empty());
}

/*
 * 用例描述：smap纳管名单里的pid稳态实采不存在（VM可能已停，稳态下即无数据）
 * 预期：不写迁移需求也不pending，所在numa由task_builder按无占用走直接归还
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectVm_ManagedPidGoneInStable_DirectReturn)
{
    gVmCollectRet = MEM_POOLING_OK;
    gVmCollectResult = {BuildVmForCollect(100, 5, 100)};
    gSmapFailNumasForCollect = {};
    // 纳管的是pid=200，实采里只有pid=100
    gSmapManagedByNumaForCollect = {{5, {BuildManagedPayload(200, static_cast<uint8_t>(MIG_RATIO_MODE), 30, 0)}}};
    MockVmCollectDeps();

    FaultPidQueryResponse response;
    ASSERT_EQ(CollectVmPidMemInfos({5}, response), MEM_POOLING_OK);
    EXPECT_TRUE(response.pidMemDistribution.empty());
    EXPECT_TRUE(response.pendingFaultNumaIds.empty());
}

/*
 * 用例描述：禁用成功后二次稳态实采失败
 * 预期：只pending有纳管pid的故障numa，返回采集错误；禁用态保持不恢复
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectVm_StableCollectFail_PendingManagedNumas)
{
    MockVmCollectDeps();
    gVmCollectRet = MEM_POOLING_OK;
    // 稳态实采失败注入（须在MockVmCollectDeps之后，避免被其重置）
    gVmCollectRetStable = MEM_POOLING_ERROR;
    gVmCollectResult = {BuildVmForCollect(100, 5, 1000)};
    gSmapFailNumasForCollect = {};
    gSmapManagedByNumaForCollect = {{5, {BuildManagedPayload(100, static_cast<uint8_t>(MIG_RATIO_MODE), 30, 0)}}};

    FaultPidQueryResponse response;
    EXPECT_EQ(CollectVmPidMemInfos({5}, response), MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
    ASSERT_EQ(response.pendingFaultNumaIds.size(), 1U);
    EXPECT_EQ(response.pendingFaultNumaIds[0], 5);
}

/*
 * 用例描述：实采失败（GetVmInfoImmediately失败）
 * 预期：返回采集错误，整节点等恢复（不触发任何归还/迁移）
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectVm_CollectFail_ResourceCollectError)
{
    gVmCollectRet = MEM_POOLING_ERROR;
    gVmCollectResult = {};
    gSmapFailNumasForCollect = {};
    gSmapManagedByNumaForCollect = {};
    MockVmCollectDeps();

    FaultPidQueryResponse response;
    EXPECT_EQ(CollectVmPidMemInfos({5}, response), MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
}

// ==================== 容器采集: 禁用失败阻断本轮 ====================

/*
 * 用例描述：容器场景smap发现纳管pid后禁用冷热迁移失败
 * 预期：本轮不采集，返回采集错误等下轮重试（不进入实采与等待）
 */
TEST_F(TestPidFaultErrorCodeHandler, CollectContainer_DisableFailed_CollectError)
{
    gSmapFailNumasForCollect = {};
    gSmapManagedByNumaForCollect = {{5, {BuildManagedPayload(100, static_cast<uint8_t>(MIG_RATIO_MODE), 30, 0)}}};
    MockVmCollectDeps();
    // 通过开关切换禁用失败（重复注册同一函数的mock不生效）
    gEnableHelperRet = -1;

    FaultPidQueryResponse response;
    EXPECT_EQ(CollectContainerPidMemInfos({5}, response), MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
    EXPECT_TRUE(response.pidMemDistribution.empty());
    EXPECT_EQ(gWaitQuiesceCalls, 0U);
}

// ==================== ExecuteConditionalReturn: 归还门槛要求纳管已移除 ====================

/*
 * 用例描述：共享borrowId防误还的门槛从MIGRATED抬升到REMOVED
 * 预期：MIGRATED（纳管未移除）task的旧borrow不归还且不推进；
 *       REMOVED task的旧borrow归还后进COMPLETED
 */
TEST_F(TestPidFaultErrorCodeHandler, ExecuteConditionalReturn_RequiresRemovedBeforeFree)
{
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(MemBorrowExecutor::*)(const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MigrationTask migTask;
    migTask.taskId = "t-mig";
    migTask.relatedBorrowIds = {"bid-a"};
    MigrationTask removedTask;
    removedTask.taskId = "t-removed";
    removedTask.relatedBorrowIds = {"bid-b"};
    // newBorrowId留空避开重定向持久化

    std::unordered_map<std::string, TaskPhase> taskPhases = {{"t-mig", TaskPhase::MIGRATED},
                                                             {"t-removed", TaskPhase::REMOVED}};
    std::vector<std::string> freedBorrowIds;
    ExecuteConditionalReturn({migTask, removedTask}, taskPhases, freedBorrowIds);

    ASSERT_EQ(freedBorrowIds.size(), 1U);
    EXPECT_EQ(freedBorrowIds[0], "bid-b");
    EXPECT_EQ(taskPhases["t-mig"], TaskPhase::MIGRATED);
    EXPECT_EQ(taskPhases["t-removed"], TaskPhase::COMPLETED);
}

} // namespace mempooling::over_commit
