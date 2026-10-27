/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// RecoverLocalAttachments UT（reliable_design.md §9.1 UT9~14）：
// 探测命中零调用、OS重启幂等重挂（NS + 聚合设备Assemble）、master账本恢复、
// agent重报自愈/重试保留、PENDING残留收敛、恢复与SDK并发resourceLock序列化

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <thread>

#include "framework/misc/ubse_logging_lock_guard.h"
#include "test_ubse_ssu_service_imp_fixture.h"
#include "ubse_ssu_attach_record_store.h"
#include "ubse_ssu_mount_ops.h"

namespace ubse::ssu::service::ut {

using namespace ubse::ssu::persist;
using namespace ubse::ssu::debt;

namespace {

uint32_t g_storeFileSeq = 0;

// Probe/Assemble/Stop 计数（mock invoke中递增）
std::atomic<int> g_probeCallCount{0};
std::atomic<int> g_assembleCallCount{0};
std::atomic<int> g_stopCallCount{0};

void ResetBlockDeviceMockCounters()
{
    g_probeCallCount.store(0);
    g_assembleCallCount.store(0);
    g_stopCallCount.store(0);
}

int CountProbe(UbseSsuAdapterImpl *self, const std::string &devName, UbseSsuAddressingType addressingType)
{
    (void)self;
    (void)devName;
    (void)addressingType;
    g_probeCallCount.fetch_add(1);
    return 1; // 元数据残留 → Assemble 分支
}

uint32_t CountAssemble(UbseSsuAdapterImpl *self, const std::string &devName,
                       const std::vector<std::string> &devicePathList, std::string &devicePath,
                       UbseSsuAddressingType addressingType)
{
    (void)self;
    (void)devName;
    (void)devicePathList;
    (void)addressingType;
    g_assembleCallCount.fetch_add(1);
    devicePath = "/dev/ssu/" + devName;
    return UBSE_OK;
}

uint32_t CountStop(UbseSsuAdapterImpl *self, const std::string &devName)
{
    (void)self;
    (void)devName;
    g_stopCallCount.fetch_add(1);
    return UBSE_OK;
}

// ====== 两阶段恢复（阶段1本地恢复/阶段2账本对账）辅助mock ======

// 阶段2重报收集：记录上报的空间名序列；g_onStatusReport在每次上报前触发（注入测试副作用）
std::vector<std::string> g_reportedNames;
std::function<void(const std::string &)> g_onStatusReport;

void ResetReportCollector()
{
    g_reportedNames.clear();
    g_onStatusReport = nullptr;
}

uint32_t CollectStatusUpdateRpcSend(UbseRpcEndpoint *self, const std::string &targetNodeId,
                                    const UbseRpcMessage &req, UbseRpcMessage &resp)
{
    (void)self;
    (void)targetNodeId;
    const auto *statusReq = dynamic_cast<const UbseSsuStatusReqMsg *>(&req);
    if (statusReq == nullptr) {
        return UBSE_ERROR;
    }
    const auto &name = statusReq->GetStatusUpdateReq().requestName;
    if (g_onStatusReport) {
        g_onStatusReport(name);
    }
    g_reportedNames.push_back(name);
    UbseSsuSyncRespMsg rspMsg(UBSE_OK);
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    if (rspMsg.Serialize(buffer, bufferSize) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return resp.Deserialize(buffer.get(), bufferSize);
}

void SetupReportCollector()
{
    MOCKER_CPP(&UbseRpcEndpoint::UbseRpcSend).reset();
    MOCKER_CPP(&UbseRpcEndpoint::UbseRpcSend).stubs().will(invoke(CollectStatusUpdateRpcSend));
}

// 阶段1重取守卫UT的Probe钩子：处理空间A的阶段1重挂时删除空间B
// （模拟B的SDK detach先于恢复流程处理到B之前完成）
int GuardRemoveBProbe(UbseSsuAdapterImpl *self, const std::string &devName, UbseSsuAddressingType addressingType)
{
    (void)self;
    (void)devName;
    (void)addressingType;
    UbseSsuAttachRecordStore::GetInstance().Remove("space_guard_b");
    return 1; // 元数据残留 → Assemble分支
}

// 阶段2重取守卫UT的报告钩子：上报空间A时删除空间B
// （模拟B在阶段1之后、阶段2重报之前被free自愈/SDK detach删除）
void HookRemoveBOnReportA(const std::string &name)
{
    if (name == "space_rep_a") {
        UbseSsuAttachRecordStore::GetInstance().Remove("space_rep_b");
    }
}

} // namespace

class TestUbseSsuServiceImpRecover : public UbseSsuServiceImpTestBase {
public:
    void SetUp() override
    {
        UbseSsuServiceImpTestBase::SetUp();
        SetupAdapterFuncs();
        ResetBlockDeviceMockCounters();
        // 重报重试间隔提速为1ms（默认2s，UT不可接受）
        setenv("UBSE_SSU_RECOVER_RETRY_INTERVAL_MS", "1", 1);
        // store指向独立临时文件（Init清空内存视图，测试间互不影响）
        recordFile_ = "/tmp/ubse_ssut_recover_" + std::to_string(++g_storeFileSeq) + ".json";
        setenv("UBSE_SSU_ATTACH_RECORD_FILE", recordFile_.c_str(), 1);
        UbseSsuAttachRecordStore::GetInstance().Init();
        // 模块就绪标志默认置位：非门禁测试保持"Start已完成"语义（阶段2不等待）；
        // 门禁测试（UT16-*）自行Reset构造"事件早于模块Start"场景
        UbseSsuServiceImp::MarkModuleReady();
    }

    void TearDown() override
    {
        ResetReportCollector();
        UbseSsuDebtLedger::GetInstance().SetReady(false);
        UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr;
        unsetenv("UBSE_SSU_ATTACH_RECORD_FILE");
        unlink(recordFile_.c_str());
        UbseSsuServiceImp::ResetModuleReady();
        UbseSsuServiceImpTestBase::TearDown();
    }

protected:
    // 记录的nsDevPath无需过白名单：Upsert直接进内存视图（不走反序列化校验），
    // 探测用access()，因此可指向临时目录下的真实文件
    static UbseSsuAttachRecord MakeRecoverRecord(const std::string &name, const std::string &nsDevPath,
                                                 bool withBlockDevice = false,
                                                 UbseSsuAttachRecordState state = UbseSsuAttachRecordState::ATTACHED)
    {
        UbseSsuAttachRecord record{};
        record.name = name;
        record.strategy = withBlockDevice ? UbseSsuAllocStrategy::LINEAR : UbseSsuAllocStrategy::NORMAL;
        record.recordState = state;
        record.devName = withBlockDevice ? "agg_dev_r" : "";
        record.nqn = "";
        record.identity.uid = 100;
        record.identity.userName = "test_user";
        if (withBlockDevice) {
            record.blockDeviceOptions = UbseCreateBlockDeviceOptions{};
            record.blockDeviceOptions->addressingType = UbseSsuAddressingType::LINEAR;
            record.blockDeviceOptions->raidLevel = UbseSsuRaidLevel::RAID0;
            record.blockDeviceOptions->chunkSize = 512;
        }

        UbseSsuNameSpaceInfo ns{};
        ns.tgtEid = "eid_recover_0001"; // EID_SIZE=16，长度不足会被BuildNamespaceInfoForBasic拒绝
        ns.tgtNqn = "nqn.tgt.recover";
        ns.nsUuid = "uuid-recover";
        ns.namespaceId = 1;
        ns.nsDevPath = nsDevPath;
        ns.nsSize = 4096;
        ns.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
        record.nsList.push_back(ns);

        UbseSsuNsVerifyInfo verify{};
        verify.defaultNqn = "nqn.recover.default";
        verify.jettyId = 1;
        verify.guid = "guid-recover";
        record.nsVerifyList.push_back(verify);
        record.attachedAt = 1700000000;
        return record;
    }

    static void TouchFile(const std::string &path)
    {
        FILE *file = fopen(path.c_str(), "w");
        if (file != nullptr) {
            fclose(file);
        }
    }

    // master对账用：预置Rebuild后状态的账本条目（CREATED、attachNodes空）
    static void PutRebuiltLedgerEntry(const std::string &name, UbseSsuAllocStrategy strategy)
    {
        UbseSsuLedgerEntry entry{};
        entry.name = name;
        entry.allocReq.strategy = strategy;
        entry.state = UbseSsuNsState::CREATED;
        UbseSsuDebtLedger::GetInstance().Put(name, std::make_shared<const UbseSsuLedgerEntry>(entry));
    }

    static std::vector<UbseSsuAttachRecord> GetRecords()
    {
        return UbseSsuAttachRecordStore::GetInstance().GetAll();
    }

protected:
    std::string recordFile_;
};

// UT9：进程重启模拟——探测全命中（文件存在）→ 不调用任何挂载原语
TEST_F(TestUbseSsuServiceImpRecover, AllMountedProbeHitNoAdapterCall)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_probe_hit_dev";
    TouchFile(devPath);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("space_probe_hit", devPath)),
              UBSE_OK);
    // 账本就绪（fixture默认）下master对账需命中条目，否则走"空间已释放"清理路径调detach
    PutRebuiltLedgerEntry("space_probe_hit", UbseSsuAllocStrategy::NORMAL);

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    // 零挂载原语调用（attach/detach均未发生）
    EXPECT_EQ(g_attachCallCount.load(), 0);
    EXPECT_EQ(g_detachCallCount.load(), 0);
    unlink(devPath.c_str());
}

// UT10：OS重启模拟——探测全缺失 → 逐NS调用attach原语；补齐后重复执行全部跳过（幂等）
TEST_F(TestUbseSsuServiceImpRecover, OsRestartRemountIdempotent)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_remount_dev";
    unlink(devPath.c_str()); // 确保缺失
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("space_remount", devPath)),
              UBSE_OK);
    // 同UT9：账本就绪后对账需命中条目，避免误走清理路径调detach
    PutRebuiltLedgerEntry("space_remount", UbseSsuAllocStrategy::NORMAL);

    // 第一次：探测缺失 → 调用attach原语重挂
    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(g_attachCallCount.load(), 1);
    EXPECT_EQ(g_detachCallCount.load(), 0);

    // 模拟重挂后设备出现，第二次恢复全部跳过
    TouchFile(devPath);
    g_attachCallCount.store(0);
    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(g_attachCallCount.load(), 0); // 幂等：探测命中不再调用原语
    unlink(devPath.c_str());
}

// UT10补充：LINEAR聚合设备缺失 → SetupBlockDeviceWithProbe走Probe命中→Assemble分支
TEST_F(TestUbseSsuServiceImpRecover, OsRestartRemountAssembleAggregateDevice)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_assemble_dev";
    TouchFile(devPath); // NS已挂载，仅聚合块设备缺失
    auto record = MakeRecoverRecord("space_assemble", devPath, true);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(record), UBSE_OK);

    // mock Probe命中（元数据残留）→ Assemble
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::ProbeBlockDevice).stubs().will(invoke(CountProbe));
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::AssembleBlockDevice).stubs().will(invoke(CountAssemble));

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(g_probeCallCount.load(), 1);
    EXPECT_EQ(g_assembleCallCount.load(), 1);
    EXPECT_EQ(g_attachCallCount.load(), 0); // NS探测命中无需重挂
    unlink(devPath.c_str());
}

// UT11：master账本恢复——Rebuild后CREATED/attachNodes空 → 恢复后ATTACHED/attachNodes={self}/devName回填
TEST_F(TestUbseSsuServiceImpRecover, MasterLedgerReconcileAfterRebuild)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_reconcile_dev";
    TouchFile(devPath);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("space_reconcile", devPath, true)),
              UBSE_OK);
    PutRebuiltLedgerEntry("space_reconcile", UbseSsuAllocStrategy::LINEAR);
    UbseSsuDebtLedger::GetInstance().SetReady(true);

    // 聚合设备缺失走重挂路径：mock Probe命中（元数据残留）→ Assemble，避免真实LVM/mdadm命令
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::ProbeBlockDevice).stubs().will(invoke(CountProbe));
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::AssembleBlockDevice).stubs().will(invoke(CountAssemble));

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("space_reconcile");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
    EXPECT_EQ(entry->attachNodes.size(), 1u);
    EXPECT_NE(entry->attachNodes.find("master_node_1"), entry->attachNodes.end()); // 遗留1：attacher恢复
    EXPECT_EQ(entry->devName, "agg_dev_r");
    unlink(devPath.c_str());
}

// UT11补充：账本未就绪（ledgerReady_为假）→ 对账挂起，账本不被修改，记录保留
TEST_F(TestUbseSsuServiceImpRecover, MasterLedgerNotReadyDeferReconcile)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_defer_dev";
    TouchFile(devPath);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("space_defer", devPath, true)),
              UBSE_OK);
    PutRebuiltLedgerEntry("space_defer", UbseSsuAllocStrategy::LINEAR);
    UbseSsuDebtLedger::GetInstance().SetReady(false); // 首次采集未完成

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("space_defer");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED); // 未对账
    EXPECT_TRUE(entry->attachNodes.empty());
    EXPECT_EQ(GetRecords().size(), 1u); // 记录保留，待回调链补执行
    unlink(devPath.c_str());
}

// UT11补充：账本就绪但查无该空间（space已被free）→ master本地自愈清理+记录删除
TEST_F(TestUbseSsuServiceImpRecover, MasterLedgerEntryGoneSelfHeal)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_gone_dev";
    TouchFile(devPath);
    auto record = MakeRecoverRecord("space_gone", devPath, true);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(record), UBSE_OK);
    // 不放账本条目 = space已被free
    UbseSsuDebtLedger::GetInstance().SetReady(true);

    auto &impl = UbseSsuAdapterImpl::GetInstance();
    // 聚合设备缺失走重挂路径：mock Probe/Assemble避免真实LVM/mdadm命令，重挂成功后对账触发自愈
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::ProbeBlockDevice).stubs().will(invoke(CountProbe));
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::AssembleBlockDevice).stubs().will(invoke(CountAssemble));
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::StopBlockDevice).stubs().will(invoke(CountStop));

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(GetRecords().size(), 0u); // 记录删除
    EXPECT_EQ(g_stopCallCount.load(), 1); // 聚合设备停止
    EXPECT_EQ(g_detachCallCount.load(), 1); // NS卸载
    unlink(devPath.c_str());
}

// UT12：agent重报收到SPACE_NOT_FOUND → 本地清理+记录删除
TEST_F(TestUbseSsuServiceImpRecover, AgentReportSpaceNotFoundSelfHeal)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_agent_gone_dev";
    TouchFile(devPath);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("space_agent_gone", devPath)),
              UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    g_mockStatusUpdateResult = UBSE_SSU_ERROR_SPACE_NOT_FOUND;

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(g_statusUpdateCallCount, 1u);
    EXPECT_EQ(GetRecords().size(), 0u); // 记录删除
    EXPECT_EQ(g_detachCallCount.load(), 1); // 本地NS清理
    unlink(devPath.c_str());
}

// UT12补充：MASTER_NOT_READY纯重试，重试耗尽后保留记录（下次事件收敛）
TEST_F(TestUbseSsuServiceImpRecover, AgentReportNotReadyRetryKeepsRecord)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_retry_dev";
    TouchFile(devPath);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("space_retry", devPath)),
              UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    g_mockStatusUpdateResult = UBSE_SSU_ERROR_MASTER_NOT_READY;

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(g_statusUpdateCallCount, 5u); // 5次重试
    EXPECT_EQ(GetRecords().size(), 1u); // 记录保留
    EXPECT_EQ(g_detachCallCount.load(), 0); // 不自愈清理
    unlink(devPath.c_str());
}

// UT13：PENDING残留——全部已挂载则收敛ATTACHED；未挂载则删除
TEST_F(TestUbseSsuServiceImpRecover, PendingRecordConvergeOrDiscard)
{
    // 场景1：PENDING但探测全命中（崩溃发生在转ATTACHED前）→ 收敛ATTACHED
    const std::string devPathHit = "/tmp/ubse_ssut_recover_pending_hit";
    TouchFile(devPathHit);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_pending_hit", devPathHit, false, UbseSsuAttachRecordState::PENDING)),
              UBSE_OK);
    UbseSsuDebtLedger::GetInstance().SetReady(false); // 不关心对账分支

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    auto records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].recordState, UbseSsuAttachRecordState::ATTACHED);

    // 场景2：PENDING且未挂载完成 → 记录作废删除
    const std::string devPathMiss = "/tmp/ubse_ssut_recover_pending_miss";
    unlink(devPathMiss.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_pending_miss", devPathMiss, false, UbseSsuAttachRecordState::PENDING)),
              UBSE_OK);

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    records = GetRecords();
    ASSERT_EQ(records.size(), 1u); // 仅剩场景1的记录
    EXPECT_EQ(records[0].name, "space_pending_hit");
    unlink(devPathHit.c_str());
}

// UT14：恢复与SDK并发——per-name resourceLock序列化：锁被持有时恢复不执行挂载原语，释放后完成
TEST_F(TestUbseSsuServiceImpRecover, ResourceLockSerializesWithConcurrentHolder)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_lock_dev";
    unlink(devPath.c_str()); // 探测缺失，触发RemountLocal
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("space_lock", devPath)),
              UBSE_OK);
    UbseSsuDebtLedger::GetInstance().SetReady(false);

    // 模拟SDK并发操作：另一线程持有同名resourceLock
    std::atomic<bool> releaseLock{false};
    std::thread holder([&releaseLock]() {
        auto lock = ubse::utils::UbseLoggingLockGuard("space_lock");
        while (!releaseLock.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    // 确保持锁生效后启动恢复
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::thread recoverThread([this]() { service_.RecoverLocalAttachments(); });

    // 恢复应阻塞在resourceLock上，挂载原语未执行
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(g_attachCallCount.load(), 0);

    // 释放锁，恢复继续完成重挂
    releaseLock.store(true);
    holder.join();
    recoverThread.join();
    EXPECT_EQ(g_attachCallCount.load(), 1);
}

// ====== 两阶段恢复（阶段1本地恢复/阶段2账本对账）======

// UT15-1：阶段1锁内重取守卫——处理空间A（阶段1重挂路径经Probe钩子）时空间B被并发删除
// （模拟B的SDK detach先于恢复完成），B不再被复活重挂：
// 旧实现沿用GetAll快照会对B执行重挂原语并把已卸载空间重报账本（复活竞态）
TEST_F(TestUbseSsuServiceImpRecover, RecordRemovedBeforeLocalRecoverySkipped)
{
    // A：LINEAR，NS设备在位仅聚合设备缺失 → 阶段1重挂走Probe钩子（注入删除B的副作用）
    const std::string devPathA = "/tmp/ubse_ssut_recover_guard1_a";
    TouchFile(devPathA);
    // B：NORMAL，NS设备缺失 → 若被复活将调用attach原语
    const std::string devPathB = "/tmp/ubse_ssut_recover_guard1_b";
    unlink(devPathB.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_guard_a", devPathA, true)), UBSE_OK);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_guard_b", devPathB)), UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::ProbeBlockDevice).stubs().will(invoke(GuardRemoveBProbe));
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::AssembleBlockDevice).stubs().will(invoke(CountAssemble));

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    // B未被复活：重挂原语零调用（A的NS在位也无需attach）
    EXPECT_EQ(g_attachCallCount.load(), 0);
    // 仅A进入阶段2重报；B锁内重取不存在被跳过
    EXPECT_EQ(g_reportedNames, std::vector<std::string>({"space_guard_a"}));
    auto records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].name, "space_guard_a");
    unlink(devPathA.c_str());
}

// UT15-2：阶段2锁内重取守卫——阶段1后、重报前被删除的空间不再重报：
// 上报空间A的钩子中删除空间B（模拟B被free自愈/SDK detach删除），B的重报被跳过
TEST_F(TestUbseSsuServiceImpRecover, RecordRemovedBeforeReportSkipped)
{
    // A：NS设备在位（探测命中，零原语调用）；B：NS设备缺失（阶段1重挂一次）
    const std::string devPathA = "/tmp/ubse_ssut_recover_guard2_a";
    TouchFile(devPathA);
    const std::string devPathB = "/tmp/ubse_ssut_recover_guard2_b";
    unlink(devPathB.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_rep_a", devPathA)), UBSE_OK);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_rep_b", devPathB)), UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();
    g_onStatusReport = HookRemoveBOnReportA;

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    // 仅B在阶段1被重挂（A探测命中零调用）
    EXPECT_EQ(g_attachCallCount.load(), 1);
    // B在重报前被删除：阶段2重取不存在，不重报（旧实现沿用快照会上报A、B两条）
    EXPECT_EQ(g_reportedNames, std::vector<std::string>({"space_rep_a"}));
    auto records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].name, "space_rep_a");
    unlink(devPathA.c_str());
}

// UT15-3：两阶段汇总语义——仅本地就绪（ATTACHED）空间进入阶段2重报：
// SUSPECT重挂仍失败不上报（保留SUSPECT等下轮重试），PENDING未完成作废不上报，ATTACHED正常重报
TEST_F(TestUbseSsuServiceImpRecover, TwoPhaseSyncOnlyAttachedReported)
{
    const std::string devPathOk = "/tmp/ubse_ssut_recover_2p_ok";
    TouchFile(devPathOk); // A探测命中
    const std::string devPathSus = "/tmp/ubse_ssut_recover_2p_sus";
    unlink(devPathSus.c_str()); // B缺失（SUSPECT持续重挂但失败）
    const std::string devPathPend = "/tmp/ubse_ssut_recover_2p_pend";
    unlink(devPathPend.c_str()); // C挂载未完成（作废）

    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_2p_ok", devPathOk)), UBSE_OK);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_2p_sus", devPathSus, false, UbseSsuAttachRecordState::SUSPECT)),
              UBSE_OK);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_2p_pend", devPathPend, false, UbseSsuAttachRecordState::PENDING)),
              UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();
    // B为SUSPECT且探测缺失：重挂会继续尝试但失败（A探测命中/PENDING作废均不调attach原语）
    g_attachFailAfter.store(0);
    g_attachFailRet.store(UBSE_SSU_ERROR_NS_NOT_FOUND);

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    // 仅A重报；SUSPECT/PENDING均不进入阶段2
    EXPECT_EQ(g_reportedNames, std::vector<std::string>({"space_2p_ok"}));
    EXPECT_EQ(g_attachCallCount.load(), 1); // SUSPECT仍重挂（失败1次）、PENDING作废不重挂
    auto records = GetRecords();
    ASSERT_EQ(records.size(), 2u); // PENDING作废删除
    bool susKept = false;
    bool okKept = false;
    for (const auto &record : records) {
        if (record.name == "space_2p_sus") {
            EXPECT_EQ(record.recordState, UbseSsuAttachRecordState::SUSPECT); // 保留原状态
            susKept = true;
        } else if (record.name == "space_2p_ok") {
            okKept = true;
        }
    }
    EXPECT_TRUE(susKept);
    EXPECT_TRUE(okKept);
    unlink(devPathOk.c_str());
}

// ====== 模块就绪门禁（MASTER_ONLINE事件早于模块Start的场景）======

// UT16-1：阶段2等待模块就绪——MASTER_ONLINE先于Start到达（标志未置位）：
// 阶段1本地探测照常完成，重报被阻塞在门禁上；Start完成（置位）后放行上报。
// 旧实现5次重报（5×2s）全部撞在endpoint未注册上，账本最长延迟300s才被运营对账补齐
TEST_F(TestUbseSsuServiceImpRecover, Phase2WaitsModuleReadyBeforeReport)
{
    UbseSsuServiceImp::ResetModuleReady(); // 构造"模块Start未完成"
    // 门禁等待轮询复用重试间隔（15次×间隔）： SetUp已设1ms，本用例提高到100ms，
    // 保证断言窗口（200ms）内确定处于阻塞等待期而非超时放行（SetUp每用例重设，无交叉污染）
    setenv("UBSE_SSU_RECOVER_RETRY_INTERVAL_MS", "100", 1);
    const std::string devPath = "/tmp/ubse_ssut_recover_gate";
    TouchFile(devPath); // NS设备在位：阶段1探测命中，零挂载原语
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_gate", devPath)), UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();

    std::thread recoverThread([this]() { service_.RecoverLocalAttachments(); });
    // 阶段1已完成（等待期间不再有本地动作），阶段2阻塞等待模块就绪：零上报
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(g_reportedNames.empty());

    UbseSsuServiceImp::MarkModuleReady(); // 模块Start完成
    recoverThread.join();
    EXPECT_EQ(g_reportedNames, std::vector<std::string>({"space_gate"}));
    unlink(devPath.c_str());
}

// UT16-2：等待超时放行兜底——模块Start始终未完成（挂死/失败），等待耗尽后
// 阶段2仍放行执行重报：不能被启动异常无限拖延（重试+300s运营对账兜底语义保留）
TEST_F(TestUbseSsuServiceImpRecover, Phase2ProceedsAfterModuleReadyWaitTimeout)
{
    UbseSsuServiceImp::ResetModuleReady();
    const std::string devPath = "/tmp/ubse_ssut_recover_gate_to";
    TouchFile(devPath);
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_gate_to", devPath)), UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();

    // 等待15次×1ms（env已提速）耗尽后放行，重报照常发生
    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(g_reportedNames, std::vector<std::string>({"space_gate_to"}));
    unlink(devPath.c_str());
}

// ====== v1.1 §14 恢复窗口竞态防护（reliable_design.md §9.1 UT19） ======

namespace {

// 独占策略账本条目（可指定restoredFromHardware），allocReq/allocResult策略一致供verify校验
UbseSsuLedgerEntry MakeExclusiveEntry(const std::string &name, bool restoredFromHardware,
                                      UbseSsuAllocStrategy strategy = UbseSsuAllocStrategy::LINEAR)
{
    UbseSsuLedgerEntry entry{};
    entry.name = name;
    entry.allocReq.strategy = strategy;
    entry.allocResult.strategy = strategy;
    entry.state = UbseSsuNsState::CREATED;
    entry.restoredFromHardware = restoredFromHardware;
    return entry;
}

ubse::ssu::message::UbseSsuAttachDetachVerifyOption AttachOption(UbseSsuAllocStrategy strategy)
{
    ubse::ssu::message::UbseSsuAttachDetachVerifyOption option;
    option.isAttach = true;
    option.expectedStrategy = strategy;
    return option;
}

} // namespace

// UT19-1：账本未就绪门禁——verify/alloc/free入口返回MASTER_NOT_READY（与HandleStatusReceiver
// 门禁对称），而非误导性的SPACE_NOT_FOUND/NO_NEED_FREE
TEST_F(TestUbseSsuServiceImpRecover, LedgerNotReadyGateBlocksVerifyAllocFree)
{
    UbseSsuDebtLedger::GetInstance().SetReady(false);
    PutRebuiltLedgerEntry("space_gate", UbseSsuAllocStrategy::LINEAR);

    auto resp = ubse::ssu::message::UbseSsuAttachDetachVerifyResp{};
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_gate", MakeIdentity(), "node_other",
                                                      AttachOption(UbseSsuAllocStrategy::LINEAR), resp),
              UBSE_SSU_ERROR_MASTER_NOT_READY);

    UbseSsuAllocResult result{};
    EXPECT_EQ(service_.ExecuteAlloc(MakeAllocReq("space_gate", 4096, 2), MakeIdentity(), result),
              UBSE_SSU_ERROR_MASTER_NOT_READY);

    EXPECT_EQ(service_.ExecuteFree("space_gate", "", MakeIdentity()), UBSE_SSU_ERROR_MASTER_NOT_READY);
}

// UT19-2：恢复收敛窗口守卫——Rebuild硬件恢复的独占空间在窗口内拒绝其他节点首个attach；
// 同窗口新建（非硬件恢复）条目与NORMAL硬件恢复空间不受限。
// 注：窗口过期（60s后放行）依赖真实时钟，UT不模拟，由代码路径restoredFromHardware=false等价覆盖
TEST_F(TestUbseSsuServiceImpRecover, ReconcileWindowGuardsExclusiveRestoredSpace)
{
    // 构造带customData的NS写入collector缓存，经RebuildLedgerFromDevList生成restoredFromHardware条目
    auto exclNs = MakeNsForCache("eid_win_excl_0001", "nqn.win.excl", 1);
    std::strncpy(exclNs.customData.name, "space_win_excl", sizeof(exclNs.customData.name) - 1);
    exclNs.customData.nsNum = 1;
    exclNs.customData.totalBytes = 4096;
    exclNs.customData.allocStrategy = static_cast<uint8_t>(UbseSsuAllocStrategy::LINEAR);
    AddNsToCollectorCache(exclNs);
    auto normalNs = MakeNsForCache("eid_win_norm_0001", "nqn.win.norm", 1);
    std::strncpy(normalNs.customData.name, "space_win_norm", sizeof(normalNs.customData.name) - 1);
    normalNs.customData.nsNum = 1;
    normalNs.customData.totalBytes = 4096;
    normalNs.customData.allocStrategy = static_cast<uint8_t>(UbseSsuAllocStrategy::NORMAL);
    AddNsToCollectorCache(normalNs);

    // master角色（基类默认mock）下Rebuild：条目CREATED+restoredFromHardware=true+收敛窗口激活
    service_.RebuildLedgerFromDevList();
    ASSERT_TRUE(UbseSsuDebtLedger::GetInstance().IsInReconcileWindow());
    ASSERT_TRUE(UbseSsuDebtLedger::GetInstance().Get("space_win_excl") != nullptr);

    auto resp = ubse::ssu::message::UbseSsuAttachDetachVerifyResp{};
    // 窗口内其他节点对硬件恢复独占空间的首个attach被拒（SDK重试收敛）
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_win_excl", MakeIdentity(), "node_other",
                                                      AttachOption(UbseSsuAllocStrategy::LINEAR), resp),
              UBSE_SSU_ERROR_MASTER_NOT_READY);

    // 非硬件恢复条目（等价于窗口内新分配空间）不受窗口限制
    auto freshEntry = MakeExclusiveEntry("space_win_new", false);
    UbseSsuDebtLedger::GetInstance().Put("space_win_new",
                                         std::make_shared<const UbseSsuLedgerEntry>(freshEntry));
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_win_new", MakeIdentity(), "node_other",
                                                      AttachOption(UbseSsuAllocStrategy::LINEAR), resp),
              UBSE_OK);

    // NORMAL多节点共挂无双重挂载竞态，硬件恢复空间窗口内attach不受限
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_win_norm", MakeIdentity(), "node_other",
                                                      AttachOption(UbseSsuAllocStrategy::NORMAL), resp),
              UBSE_OK);
}

// UT19-3：疑似本地残留挂载守卫——本机存在NS设备但无挂载记录的独占空间（记录丢失、attacher未知），
// master恢复后反查探测命中并拒绝attach；摘除标记后放行
TEST_F(TestUbseSsuServiceImpRecover, SuspectedLocalMountRejectsAttach)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_suspect_dev";
    TouchFile(devPath); // 模拟本机残留的NS设备节点

    auto entry = MakeExclusiveEntry("space_suspect", false);
    // NS写入缓存，账本NS信息经MakeNameSpaceInfo转换uuid（StrToUuid），保证verify时ns匹配命中
    auto cacheNs = MakeNsForCache("eid_suspect_0001", "nqn.suspect", 1);
    auto ns = MakeNameSpaceInfo(cacheNs);
    ns.nsDevPath = devPath; // 反查探测将命中（access==0）
    entry.allocResult.nameSpaceList.push_back(ns);
    UbseSsuDebtLedger::GetInstance().Put("space_suspect", std::make_shared<const UbseSsuLedgerEntry>(entry));
    AddNsToCollectorCache(cacheNs);

    // master角色恢复（本地无该空间挂载记录）→ 反查探测命中，记入疑似集合
    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_TRUE(service_.suspectedMountTable_.Contains("space_suspect"));

    auto resp = ubse::ssu::message::UbseSsuAttachDetachVerifyResp{};
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_suspect", MakeIdentity(), "node_other",
                                                      AttachOption(UbseSsuAllocStrategy::LINEAR), resp),
              UBSE_SSU_ERROR_MOUNT_SUSPECTED);

    // 人工核对后的自愈出口（free成功路径会自动摘除）：摘除后verify放行
    service_.suspectedMountTable_.Erase("space_suspect");
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_suspect", MakeIdentity(), "node_other",
                                                      AttachOption(UbseSsuAllocStrategy::LINEAR), resp),
              UBSE_OK);
    unlink(devPath.c_str());
}

// UT19-4：重报污染防护——LINEAR空间ATTACHED上报时attachNodes已含其他节点则拒收
// （ATTACHER_MISMATCH），维持attacher单节点不变式；同节点重报保持幂等消化。
// 注：HandleStatusReceiver为rpc_processor内部handler，经test_ubse_ssu_rpc_handler套件覆盖，
// 此处校验账本侧守卫的入口条件（不直接调handler）
TEST_F(TestUbseSsuServiceImpRecover, AttacherInvariantLedgerGuardPreconditions)
{
    auto entry = MakeExclusiveEntry("space_attacher", false);
    entry.state = UbseSsuNsState::ATTACHED;
    entry.attachNodes.insert("node_attacher");
    UbseSsuDebtLedger::GetInstance().Put("space_attacher", std::make_shared<const UbseSsuLedgerEntry>(entry));

    auto resp = ubse::ssu::message::UbseSsuAttachDetachVerifyResp{};
    // attacher自身重复attach → 幂等alreadyAttached
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_attacher", MakeIdentity(), "node_attacher",
                                                      AttachOption(UbseSsuAllocStrategy::LINEAR), resp),
              UBSE_OK);
    EXPECT_TRUE(resp.alreadyAttached);

    // 其他节点attach → ATTACHER_MISMATCH（既有强校验语义，重报污染防护与之闭环）
    resp = ubse::ssu::message::UbseSsuAttachDetachVerifyResp{};
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_attacher", MakeIdentity(), "node_other",
                                                      AttachOption(UbseSsuAllocStrategy::LINEAR), resp),
              UBSE_SSU_ERROR_ATTACHER_MISMATCH);
}

// ====== v1.1 §12.2 设备故障FAULTED状态迁移（reliable_design.md §9.1 UT20） ======

namespace {

// 构造含指定空间名NS的采集设备列表（模拟该空间硬件在线）
std::vector<UbseSsuDevInfo> MakeDevListWithSpace(const std::string &eid, const std::string &spaceName)
{
    auto ns = UbseSsuServiceImpTestBase::MakeNsForCache(eid, "nqn.fault.recover", 1);
    std::strncpy(ns.customData.name, spaceName.c_str(), sizeof(ns.customData.name) - 1);
    ns.customData.name[sizeof(ns.customData.name) - 1] = '\0';
    UbseSsuDevInfo dev{};
    dev.subSystem.eid = eid;
    dev.subSystem.subNqn = "nqn.fault.recover";
    dev.nameSpaces.push_back(ns);
    return {dev};
}

} // namespace

// UT20-1：设备故障防抖与回正——空间连续SSU_FAULT_DEBOUNCE_CYCLES(3)个采集周期从硬件缺失
// 才置FAULTED（不足窗口不动作）；重新出现后计数清零，FAULTED回正CREATED（attachNodes空）
TEST_F(TestUbseSsuServiceImpRecover, SpaceFaultDebounceAndRecover)
{
    UbseSsuDebtLedger::GetInstance().Put("space_fault_a",
                                         std::make_shared<const UbseSsuLedgerEntry>(
                                             MakeExclusiveEntry("space_fault_a", false)));
    const std::vector<UbseSsuDevInfo> emptyList{};
    auto &ledger = UbseSsuDebtLedger::GetInstance();

    // 周期1~2：防抖不足，不迁移状态
    service_.ReconcileSpaceFaultStates(emptyList);
    service_.ReconcileSpaceFaultStates(emptyList);
    EXPECT_EQ(ledger.Get("space_fault_a")->state, UbseSsuNsState::CREATED);

    // 周期3：防抖窗口满足 → FAULTED
    service_.ReconcileSpaceFaultStates(emptyList);
    EXPECT_EQ(ledger.Get("space_fault_a")->state, UbseSsuNsState::FAULTED);

    // 设备重新上线：计数清零 + FAULTED回正CREATED（无attacher）
    service_.ReconcileSpaceFaultStates(MakeDevListWithSpace("eid_fault_a_0001", "space_fault_a"));
    EXPECT_EQ(ledger.Get("space_fault_a")->state, UbseSsuNsState::CREATED);

    // 再次缺失：计数已清零，需重新走满防抖窗口（不会立刻FAULTED）
    service_.ReconcileSpaceFaultStates(emptyList);
    EXPECT_EQ(ledger.Get("space_fault_a")->state, UbseSsuNsState::CREATED);
}

// UT20-2：FAULTED回正按attachNodes分支——attacher仍在挂载则回ATTACHED，否则回CREATED
TEST_F(TestUbseSsuServiceImpRecover, SpaceFaultRecoverToAttachedWhenAttacherHeld)
{
    auto entry = MakeExclusiveEntry("space_fault_b", false);
    entry.state = UbseSsuNsState::FAULTED;
    entry.attachNodes.insert("node_attacher");
    UbseSsuDebtLedger::GetInstance().Put("space_fault_b", std::make_shared<const UbseSsuLedgerEntry>(entry));

    service_.ReconcileSpaceFaultStates(MakeDevListWithSpace("eid_fault_b_0001", "space_fault_b"));
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_fault_b")->state, UbseSsuNsState::ATTACHED);
}

// UT20-3：不判故障的边界——CREATING（创建中NS未落硬件）/IDLE（失败回退）不参与对账；
// 账本未就绪时对账为no-op（防抖计数不前进）
TEST_F(TestUbseSsuServiceImpRecover, SpaceFaultSkipsCreatingIdleAndNotReady)
{
    auto creating = MakeExclusiveEntry("space_fault_c", false);
    creating.state = UbseSsuNsState::CREATING;
    UbseSsuDebtLedger::GetInstance().Put("space_fault_c", std::make_shared<const UbseSsuLedgerEntry>(creating));
    auto idle = MakeExclusiveEntry("space_fault_d", false);
    idle.state = UbseSsuNsState::IDLE;
    UbseSsuDebtLedger::GetInstance().Put("space_fault_d", std::make_shared<const UbseSsuLedgerEntry>(idle));

    const std::vector<UbseSsuDevInfo> emptyList{};
    for (int i = 0; i < SSU_FAULT_DEBOUNCE_CYCLES + 1; ++i) {
        service_.ReconcileSpaceFaultStates(emptyList);
    }
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_fault_c")->state, UbseSsuNsState::CREATING);
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_fault_d")->state, UbseSsuNsState::IDLE);

    // 账本未就绪：CREATED空间缺失不计数（后续就绪首周期从0开始防抖）
    UbseSsuDebtLedger::GetInstance().SetReady(false);
    UbseSsuDebtLedger::GetInstance().Put("space_fault_e",
                                         std::make_shared<const UbseSsuLedgerEntry>(
                                             MakeExclusiveEntry("space_fault_e", false)));
    for (int i = 0; i < SSU_FAULT_DEBOUNCE_CYCLES; ++i) {
        service_.ReconcileSpaceFaultStates(emptyList);
    }
    UbseSsuDebtLedger::GetInstance().SetReady(true);
    service_.ReconcileSpaceFaultStates(emptyList);
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_fault_e")->state, UbseSsuNsState::CREATED);
}

// ====== v1.1 §12.2 FAULTED拦截（reliable_design.md §9.1 UT21） ======

// UT21-1：FAULTED空间的attach/detach verify一律返回SPACE_FAULTED（新对外错误码1204）；
// free保留（人工止损出口）——FAULTED下不返回NEED_DETACH_BEFORE_FREE/STATE_INVALID，
// 硬件不在线的NS按已释放跳过，free成功移除账本条目
TEST_F(TestUbseSsuServiceImpRecover, FaultedSpaceBlocksVerifyButFreeAllowed)
{
    auto entry = MakeExclusiveEntry("space_fault_op", false);
    entry.state = UbseSsuNsState::FAULTED;
    entry.attachNodes.insert("node_attacher");
    // nsList留空：模拟硬件NS已不在设备缓存（故障设备整掉线），free按已释放跳过
    UbseSsuDebtLedger::GetInstance().Put("space_fault_op", std::make_shared<const UbseSsuLedgerEntry>(entry));

    // attach verify：attacher自身与其他节点均被拦截（FAULTED优先于幂等/attacher校验）
    auto resp = ubse::ssu::message::UbseSsuAttachDetachVerifyResp{};
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_fault_op", MakeIdentity(), "node_attacher",
                                                      AttachOption(UbseSsuAllocStrategy::LINEAR), resp),
              UBSE_SSU_ERROR_SPACE_FAULTED);
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_fault_op", MakeIdentity(), "node_other",
                                                      AttachOption(UbseSsuAllocStrategy::LINEAR), resp),
              UBSE_SSU_ERROR_SPACE_FAULTED);

    // detach verify：同样拦截
    auto detachOption = AttachOption(UbseSsuAllocStrategy::LINEAR);
    detachOption.isAttach = false;
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("space_fault_op", MakeIdentity(), "node_attacher",
                                                      detachOption, resp),
              UBSE_SSU_ERROR_SPACE_FAULTED);

    // free保留：FAULTED下允许止损（不要求先detach），账本条目移除
    EXPECT_EQ(service_.ExecuteFree("space_fault_op", "", MakeIdentity()), UBSE_OK);
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_fault_op"), nullptr);
}

// ====== v1.1 §12.3 agent重挂失败收敛升级（reliable_design.md §9.1 UT22） ======

// UT22-1：重挂连续失败SSU_REMOUNT_FAIL_SUSPECT_LIMIT(3)次→记录标记SUSPECT并告警，
// 但自动重挂不停止（每轮持续重试直到挂上，防冲击仅由重挂节奏限频保证）；
// 设备恢复（探测命中）→ 收敛回ATTACHED并清计数
TEST_F(TestUbseSsuServiceImpRecover, RemountFailConvergeToSuspectThenRecover)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_suspect_limit_dev";
    unlink(devPath.c_str()); // 确保探测缺失
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("space_remount_suspect", devPath)),
              UBSE_OK);
    // 预置账本条目：SUSPECT收敛回ATTACHED后master对账需命中条目，否则走"空间已释放"清理路径
    PutRebuiltLedgerEntry("space_remount_suspect", UbseSsuAllocStrategy::NORMAL);
    // 挂载原语持续失败（模拟本地NVMe链路故障）
    g_attachFailAfter.store(0);
    g_attachFailRet.store(UBSE_SSU_ERROR_NS_NOT_FOUND);

    auto getRecordState = []() {
        auto records = GetRecords();
        for (const auto &r : records) {
            if (r.name == "space_remount_suspect") {
                return r.recordState;
            }
        }
        return UbseSsuAttachRecordState::PENDING; // 不存在时（不应发生）返回占位值
    };

    // 前3轮：每轮重挂失败（各1次attach原语调用），第3轮达阈值标记SUSPECT
    for (int i = 0; i < SSU_REMOUNT_FAIL_SUSPECT_LIMIT; ++i) {
        EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
        EXPECT_EQ(g_attachCallCount.load(), i + 1);
    }
    EXPECT_EQ(getRecordState(), UbseSsuAttachRecordState::SUSPECT);

    // SUSPECT后仍持续自动重挂：探测仍缺失时继续调挂载原语，记录保留SUSPECT
    g_attachCallCount.store(0);
    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(g_attachCallCount.load(), 1);
    EXPECT_EQ(getRecordState(), UbseSsuAttachRecordState::SUSPECT);

    // 设备恢复（探测命中）：不调原语，收敛回ATTACHED
    TouchFile(devPath);
    g_attachCallCount.store(0);
    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(g_attachCallCount.load(), 0);
    EXPECT_EQ(getRecordState(), UbseSsuAttachRecordState::ATTACHED);

    // 收敛后计数已清零：再次故障需重新走满失败阈值（非SUSPECT状态仍会自动重挂）
    unlink(devPath.c_str());
    g_attachCallCount.store(0);
    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    EXPECT_EQ(g_attachCallCount.load(), 1);
    EXPECT_EQ(getRecordState(), UbseSsuAttachRecordState::ATTACHED); // 1次失败未达阈值
}

// ====== v1.1 §12.4 节点永久离场处理（reliable_design.md §9.1 UT23） ======

namespace {

// 回拨节点离线时刻，模拟离线持续超阈值（UT不等待真实SSU_NODE_OFFLINE_THRESHOLD_SECONDS）
void RewindOfflineSince(UbseSsuServiceImp &svc, const std::string &nodeId, std::chrono::seconds back)
{
    std::lock_guard<std::mutex> lock(svc.nodeOfflineTracker_.mtx_);
    svc.nodeOfflineTracker_.since_[nodeId] -= back;
}

// ATTACHED状态账本条目（可指定策略与挂载节点集合）
UbseSsuLedgerEntry MakeAttachedEntry(const std::string &name, UbseSsuAllocStrategy strategy,
                                     const std::unordered_set<std::string> &attachNodes)
{
    UbseSsuLedgerEntry entry{};
    entry.name = name;
    entry.allocReq.strategy = strategy;
    entry.allocResult.strategy = strategy;
    entry.state = UbseSsuNsState::ATTACHED;
    entry.attachNodes = attachNodes;
    return entry;
}

} // namespace

// UT23-1：NORMAL空间离线超阈值摘除——阈值内不处理；超阈值后从attachNodes摘除该节点，
// 摘空后空间级state回CREATED；仍有其他节点时保持ATTACHED
TEST_F(TestUbseSsuServiceImpRecover, NodeOfflineRemoveNormalAttacherAfterThreshold)
{
    const std::string nodeId = "node_gone";
    UbseSsuDebtLedger::GetInstance().Put("space_off_norm", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("space_off_norm", UbseSsuAllocStrategy::NORMAL, {nodeId, "node_stay"})));
    UbseSsuDebtLedger::GetInstance().Put("space_off_solo", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("space_off_solo", UbseSsuAllocStrategy::NORMAL, {nodeId})));

    service_.OnNodeDownEvent(nodeId);

    // 阈值内：不做离场处理，挂载关系保留
    service_.ReconcileOfflineNodes();
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_off_norm")->attachNodes.count(nodeId), 1U);
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_off_solo")->state, UbseSsuNsState::ATTACHED);

    // 离线持续超阈值：NORMAL空间摘除离线节点
    RewindOfflineSince(service_, nodeId, std::chrono::seconds(SSU_NODE_OFFLINE_THRESHOLD_SECONDS + 1));
    service_.ReconcileOfflineNodes();
    const auto *normEntry = UbseSsuDebtLedger::GetInstance().Get("space_off_norm").get();
    EXPECT_EQ(normEntry->attachNodes.count(nodeId), 0U);
    EXPECT_EQ(normEntry->attachNodes.count("node_stay"), 1U);
    EXPECT_EQ(normEntry->state, UbseSsuNsState::ATTACHED);
    // 摘空后回CREATED
    const auto *soloEntry = UbseSsuDebtLedger::GetInstance().Get("space_off_solo").get();
    EXPECT_TRUE(soloEntry->attachNodes.empty());
    EXPECT_EQ(soloEntry->state, UbseSsuNsState::CREATED);

    // 已处理节点去重：重复对账不再次处理（processed标记在位）
    std::lock_guard<std::mutex> lock(service_.nodeOfflineTracker_.mtx_);
    EXPECT_EQ(service_.nodeOfflineTracker_.processed_.count(nodeId), 1U);
}

// UT23-2：独占空间attacher离线超阈值仅告警——不自动清理attachNodes（自动detach有数据面风险），
// 保持ATTACHED等人工介入
TEST_F(TestUbseSsuServiceImpRecover, NodeOfflineExclusiveSpaceKeepsAttacher)
{
    const std::string nodeId = "node_excl_gone";
    UbseSsuDebtLedger::GetInstance().Put("space_off_excl", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("space_off_excl", UbseSsuAllocStrategy::LINEAR, {nodeId})));

    service_.OnNodeDownEvent(nodeId);
    RewindOfflineSince(service_, nodeId, std::chrono::seconds(SSU_NODE_OFFLINE_THRESHOLD_SECONDS + 1));
    service_.ReconcileOfflineNodes();

    const auto *entry = UbseSsuDebtLedger::GetInstance().Get("space_off_excl").get();
    EXPECT_EQ(entry->attachNodes.count(nodeId), 1U);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);

    // 去重：重复对账保持不变（不重复告警/处理）
    service_.ReconcileOfflineNodes();
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_off_excl")->attachNodes.count(nodeId), 1U);
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_off_excl")->state, UbseSsuNsState::ATTACHED);
}

// UT23-3：节点回归清除离线记录——NODE_UP后不再做离场处理；空nodeId事件忽略；
// 重复NODE_DOWN不刷新首次离线时刻（幂等，离线时长从首次算起）
TEST_F(TestUbseSsuServiceImpRecover, NodeUpClearsOfflineRecordAndDownIdempotent)
{
    const std::string nodeId = "node_back";
    UbseSsuDebtLedger::GetInstance().Put("space_off_return", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("space_off_return", UbseSsuAllocStrategy::NORMAL, {nodeId})));

    // 空nodeId事件忽略
    service_.OnNodeDownEvent("");
    service_.ReconcileOfflineNodes();
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_off_return")->attachNodes.count(nodeId), 1U);

    // NODE_DOWN幂等：重复事件不刷新离线时刻（仍以首次为准）
    service_.OnNodeDownEvent(nodeId);
    auto firstSince = [&]() {
        std::lock_guard<std::mutex> lock(service_.nodeOfflineTracker_.mtx_);
        return service_.nodeOfflineTracker_.since_[nodeId];
    }();
    service_.OnNodeDownEvent(nodeId);
    {
        std::lock_guard<std::mutex> lock(service_.nodeOfflineTracker_.mtx_);
        EXPECT_EQ(service_.nodeOfflineTracker_.since_[nodeId], firstSince);
    }

    // 节点回归（超阈值前）：清除记录，后续对账不做离场处理
    service_.OnNodeUpEvent(nodeId);
    service_.ReconcileOfflineNodes();
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_off_return")->attachNodes.count(nodeId), 1U);
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_off_return")->state, UbseSsuNsState::ATTACHED);
    {
        std::lock_guard<std::mutex> lock(service_.nodeOfflineTracker_.mtx_);
        EXPECT_EQ(service_.nodeOfflineTracker_.since_.count(nodeId), 0U);
        EXPECT_EQ(service_.nodeOfflineTracker_.processed_.count(nodeId), 0U);
    }
}

// ====== v1.1 §13 运营期持续对账平账（reliable_design.md §9.1 UT24） ======

namespace {

// 摘要项（{name, recordState, devName}，不含nsList明细）
UbseSsuMountSummaryItem MakeSummaryItem(const std::string &name, UbseSsuAttachRecordState state,
                                        const std::string &devName = "")
{
    UbseSsuMountSummaryItem item;
    item.name = name;
    item.recordState = state;
    item.devName = devName;
    return item;
}

// 按name查平账项（diff遍历账本为无序map，输出顺序不定）
const UbseSsuReconcileItem *FindReconcileItem(const std::vector<UbseSsuReconcileItem> &items,
                                               const std::string &name)
{
    for (const auto &item : items) {
        if (item.name == name) {
            return &item;
        }
    }
    return nullptr;
}

// 从store按名查记录状态（不存在返回nullopt）
std::optional<UbseSsuAttachRecord> FindRecoverRecord(const std::string &name)
{
    for (const auto &record : TestUbseSsuServiceImpRecover::GetRecords()) {
        if (record.name == name) {
            return record;
        }
    }
    return std::nullopt;
}

} // namespace

// UT24-1：双向diff五类规则（§13.2，以账本为准）——
// 期望挂载而agent记录缺失→REMOUNT；agent ATTACHED而账本无→CLEANUP；
// 账本有但节点∉attachNodes且agent ATTACHED→REPORT；PENDING在途/SUSPECT人工介入跳过；
// FAULTED由§12机制处理跳过；独占devName属性级不一致仅WARN不产生平账项
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileDiffRules)
{
    const std::string nodeId = "nodeA";
    // 账本侧
    UbseSsuDebtLedger::GetInstance().Put("sp_remount", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("sp_remount", UbseSsuAllocStrategy::NORMAL, {nodeId}))); // 期望挂载无记录
    UbseSsuDebtLedger::GetInstance().Put("sp_ok", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("sp_ok", UbseSsuAllocStrategy::NORMAL, {nodeId}))); // 一致
    UbseSsuDebtLedger::GetInstance().Put("sp_pending", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("sp_pending", UbseSsuAllocStrategy::NORMAL, {nodeId}))); // agent在途
    UbseSsuDebtLedger::GetInstance().Put("sp_suspect", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("sp_suspect", UbseSsuAllocStrategy::NORMAL, {nodeId}))); // agent SUSPECT
    auto reportEntry = MakeAttachedEntry("sp_report", UbseSsuAllocStrategy::NORMAL, {"nodeB"});
    UbseSsuDebtLedger::GetInstance().Put("sp_report", std::make_shared<const UbseSsuLedgerEntry>(
        reportEntry)); // 节点∉attachNodes
    auto faultedEntry = MakeAttachedEntry("sp_faulted", UbseSsuAllocStrategy::NORMAL, {nodeId});
    faultedEntry.state = UbseSsuNsState::FAULTED;
    UbseSsuDebtLedger::GetInstance().Put("sp_faulted", std::make_shared<const UbseSsuLedgerEntry>(
        faultedEntry)); // FAULTED跳过
    auto driftEntry = MakeAttachedEntry("sp_devdrift", UbseSsuAllocStrategy::LINEAR, {nodeId});
    driftEntry.devName = "dev_ledger";
    UbseSsuDebtLedger::GetInstance().Put("sp_devdrift", std::make_shared<const UbseSsuLedgerEntry>(
        driftEntry)); // 独占devName属性级漂移

    // agent侧摘要（sp_cleanup不在账本=已free）
    UbseSsuMountSummaryResp summary;
    summary.items.push_back(MakeSummaryItem("sp_ok", UbseSsuAttachRecordState::ATTACHED));
    summary.items.push_back(MakeSummaryItem("sp_pending", UbseSsuAttachRecordState::PENDING));
    summary.items.push_back(MakeSummaryItem("sp_suspect", UbseSsuAttachRecordState::SUSPECT));
    summary.items.push_back(MakeSummaryItem("sp_report", UbseSsuAttachRecordState::ATTACHED));
    summary.items.push_back(MakeSummaryItem("sp_cleanup", UbseSsuAttachRecordState::ATTACHED));
    summary.items.push_back(MakeSummaryItem("sp_devdrift", UbseSsuAttachRecordState::ATTACHED, "dev_agent"));

    std::vector<UbseSsuReconcileItem> items;
    DiffNodeMountSummary(nodeId, summary, items);
    ASSERT_EQ(items.size(), 3U);
    // REMOUNT：账本期望挂载而agent记录缺失（轻量首发，NEED_FULL_INFO后master补齐重发）
    auto *remountItem = FindReconcileItem(items, "sp_remount");
    ASSERT_NE(remountItem, nullptr);
    EXPECT_EQ(remountItem->action, UbseSsuReconcileAction::REMOUNT);
    EXPECT_FALSE(remountItem->hasRecord);
    // REPORT：账本有但节点∉attachNodes而agent记录ATTACHED（非破坏性补报）
    auto *reportItem = FindReconcileItem(items, "sp_report");
    ASSERT_NE(reportItem, nullptr);
    EXPECT_EQ(reportItem->action, UbseSsuReconcileAction::REPORT);
    // CLEANUP：agent有记录而账本无该空间（已free）
    auto *cleanupItem = FindReconcileItem(items, "sp_cleanup");
    ASSERT_NE(cleanupItem, nullptr);
    EXPECT_EQ(cleanupItem->action, UbseSsuReconcileAction::CLEANUP);
    // 其余（一致/PENDING/SUSPECT/FAULTED/devName漂移）不产生平账项
    EXPECT_EQ(FindReconcileItem(items, "sp_ok"), nullptr);
    EXPECT_EQ(FindReconcileItem(items, "sp_pending"), nullptr);
    EXPECT_EQ(FindReconcileItem(items, "sp_suspect"), nullptr);
    EXPECT_EQ(FindReconcileItem(items, "sp_faulted"), nullptr);
    EXPECT_EQ(FindReconcileItem(items, "sp_devdrift"), nullptr);
}

// UT24-2：BuildRecordFromLedger——以账本allocResult+collector缓存补齐完整记录
// （verify信息从缓存NS回填，独占策略补默认块设备选项）；缓存缺失返回false（下轮重试）
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileBuildRecordFromLedger)
{
    auto cacheNs = MakeNsForCache("eid_build_00001", "nqn.build.recover", 1, 4096,
                                  std::string(GUID_SIZE, '\xCD'), std::string(UUID_SIZE, '\xCD'),
                                  100, "test_user", "nqn.build.default");
    AddNsToCollectorCache(cacheNs);
    auto entry = MakeAttachedEntry("sp_build", UbseSsuAllocStrategy::LINEAR, {"master_node_1"});
    entry.devName = "agg_build";
    entry.allocResult.nameSpaceList.push_back(MakeNameSpaceInfo(cacheNs));
    auto entryPtr = std::make_shared<const UbseSsuLedgerEntry>(entry);
    UbseSsuDebtLedger::GetInstance().Put("sp_build", entryPtr);

    const auto devMap = service_.collector_.GetCachedDevMap();
    UbseSsuAttachRecord record;
    ASSERT_TRUE(BuildRecordFromLedger(*entryPtr, devMap, record));
    EXPECT_EQ(record.name, "sp_build");
    EXPECT_EQ(record.strategy, UbseSsuAllocStrategy::LINEAR);
    EXPECT_EQ(record.recordState, UbseSsuAttachRecordState::ATTACHED);
    EXPECT_EQ(record.devName, "agg_build");
    ASSERT_EQ(record.nsList.size(), 1U);
    ASSERT_EQ(record.nsVerifyList.size(), 1U);
    EXPECT_EQ(record.nsVerifyList[0].defaultNqn, "nqn.build.default"); // ResolveNqn回落值（nqn留空）
    EXPECT_EQ(record.nsVerifyList[0].jettyId, cacheNs.subSystem.jettyId);
    // identity从NS customData恢复：留空会导致RPC serde字段校验失败（对账批次整体被拒）且落盘记录无法重载
    EXPECT_EQ(record.identity.uid, 100U);
    EXPECT_EQ(record.identity.userName, "test_user");
    EXPECT_TRUE(IsRecordFieldsValid(record)); // 与RPC serde共用校验，账本重建记录必须可下发/可落盘
    ASSERT_TRUE(record.blockDeviceOptions.has_value());
    EXPECT_EQ(record.blockDeviceOptions->addressingType, UbseSsuAddressingType::LINEAR);

    // 缓存缺失（采集抖动）：eid不在缓存中，放弃本条，下轮重试
    auto missingNs = MakeNsForCache("eid_missing_00001", "nqn.missing.recover", 1);
    auto missingEntry = MakeAttachedEntry("sp_cache_miss", UbseSsuAllocStrategy::LINEAR, {"master_node_1"});
    missingEntry.allocResult.nameSpaceList.push_back(MakeNameSpaceInfo(missingNs));
    auto missingPtr = std::make_shared<const UbseSsuLedgerEntry>(missingEntry);
    EXPECT_FALSE(BuildRecordFromLedger(*missingPtr, devMap, record));

    // 空nsList条目（Rebuild时全部NS异常仍入账本的边缘场景）：出口终检拒绝，放弃本条，
    // 否则带空nsList的记录下发后被serde字段校验拒收整个对账批次
    auto emptyPtr = std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("sp_empty_ns", UbseSsuAllocStrategy::NORMAL, {"master_node_1"}));
    EXPECT_FALSE(BuildRecordFromLedger(*emptyPtr, devMap, record));
}

// UT24-3：本机直平正向——账本无该空间（已free）本地残留记录 → 清理+删记录
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileSelfFreesLedgerGoneResidue)
{
    const std::string devPath = "/tmp/ubse_ssut_reconcile_residue_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("sp_residue", devPath)),
              UBSE_OK);
    // 不放账本条目 = space已被free

    service_.ReconcileSelfLocalMounts("master_node_1");
    EXPECT_EQ(GetRecords().size(), 0U); // 残留记录删除
    EXPECT_EQ(g_detachCallCount.load(), 1); // 本地NS清理
}

// UT24-4：本机直平REPORT分支——账本期望节点不含本机而本地ATTACHED（上报丢失/状态矛盾）
// → 本机直报幂等收敛（attachNodes补入本机、state收敛ATTACHED），记录保留
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileSelfReportsLedgerWithoutSelf)
{
    const std::string devPath = "/tmp/ubse_ssut_reconcile_report_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("sp_report_self", devPath)),
              UBSE_OK);
    PutRebuiltLedgerEntry("sp_report_self", UbseSsuAllocStrategy::NORMAL); // CREATED、attachNodes空

    service_.ReconcileSelfLocalMounts("master_node_1");
    const auto *entry = UbseSsuDebtLedger::GetInstance().Get("sp_report_self").get();
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
    EXPECT_EQ(entry->attachNodes.count("master_node_1"), 1U); // 直报收敛
    EXPECT_TRUE(FindRecoverRecord("sp_report_self").has_value()); // 记录保留
    EXPECT_EQ(g_attachCallCount.load(), 0); // 不重挂（非漂移场景）
    EXPECT_EQ(g_detachCallCount.load(), 0);
}

// UT24-5：本机直平漂移重挂——账本期望本机挂载、本地记录ATTACHED但探测缺失（误detach）
// → per-name锁内幂等重挂（复用RemountLocal）
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileSelfRemountsDriftedMount)
{
    const std::string devPath = "/tmp/ubse_ssut_reconcile_drift_dev";
    unlink(devPath.c_str()); // 探测缺失
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("sp_drift", devPath)),
              UBSE_OK);
    UbseSsuDebtLedger::GetInstance().Put("sp_drift", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("sp_drift", UbseSsuAllocStrategy::NORMAL, {"master_node_1"})));

    service_.ReconcileSelfLocalMounts("master_node_1");
    EXPECT_EQ(g_attachCallCount.load(), 1); // 幂等重挂
    EXPECT_TRUE(FindRecoverRecord("sp_drift").has_value()); // 记录保留
}

// UT24-6：本机直平SUSPECT记录跳过——运营直平通道对SUSPECT不做重挂/改状态（状态不被篡改），
// 持续自动重试由本地挂载定时器承担，本通道保留master裁决/人工核对语义
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileSelfSuspectRecordSkipped)
{
    const std::string devPath = "/tmp/ubse_ssut_reconcile_suspect_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("sp_suspect_self", devPath, false, UbseSsuAttachRecordState::SUSPECT)),
              UBSE_OK);
    UbseSsuDebtLedger::GetInstance().Put("sp_suspect_self", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("sp_suspect_self", UbseSsuAllocStrategy::NORMAL, {"master_node_1"})));

    service_.ReconcileSelfLocalMounts("master_node_1");
    EXPECT_EQ(g_attachCallCount.load(), 0); // 不自动重挂
    auto record = FindRecoverRecord("sp_suspect_self");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->recordState, UbseSsuAttachRecordState::SUSPECT); // 状态不被篡改
}

// UT24-7：疑似残留独占空间守卫（§14.2-4联动）——正向漂移与反向补记录均跳过自动动作，等人工核对
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileSelfSuspectedExclusiveSkips)
{
    const std::string devPath = "/tmp/ubse_ssut_reconcile_suspected_dev";
    unlink(devPath.c_str());
    // 正向：本地ATTACHED独占记录探测漂移，但空间在疑似集合
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("sp_suspected", devPath, true)),
              UBSE_OK);
    UbseSsuDebtLedger::GetInstance().Put("sp_suspected", std::make_shared<const UbseSsuLedgerEntry>(
        MakeAttachedEntry("sp_suspected", UbseSsuAllocStrategy::LINEAR, {"master_node_1"})));
    // 反向：账本期望本机挂载但本地记录缺失（记录文件损坏/误删），空间在疑似集合
    auto backEntry = MakeAttachedEntry("sp_suspected_back", UbseSsuAllocStrategy::LINEAR, {"master_node_1"});
    auto cacheNs = MakeNsForCache("eid_suspect_back1", "nqn.suspect.back", 1);
    backEntry.allocResult.nameSpaceList.push_back(MakeNameSpaceInfo(cacheNs));
    AddNsToCollectorCache(cacheNs);
    UbseSsuDebtLedger::GetInstance().Put("sp_suspected_back",
                                         std::make_shared<const UbseSsuLedgerEntry>(backEntry));

    // 记入疑似集合（模拟BuildSuspectedLocalMounts反查命中）
    {
        std::lock_guard<std::mutex> lock(service_.suspectedMountTable_.mtx_);
        service_.suspectedMountTable_.suspected_.insert("sp_suspected");
        service_.suspectedMountTable_.suspected_.insert("sp_suspected_back");
    }

    service_.ReconcileSelfLocalMounts("master_node_1");
    EXPECT_EQ(g_attachCallCount.load(), 0); // 正向不自动重挂
    EXPECT_EQ(g_probeCallCount.load(), 0);  // 反向不自动补挂
    EXPECT_TRUE(FindRecoverRecord("sp_suspected").has_value()); // 记录保留
    EXPECT_FALSE(FindRecoverRecord("sp_suspected_back").has_value()); // 不补建记录
}

// UT24-8：本机直平反向——账本期望本机挂载但本地记录缺失（记录文件损坏/误删）
// → 以账本+缓存补齐记录并重挂（BuildRecordFromLedger+RemountLocal+Upsert ATTACHED）
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileSelfBackfillsRecordFromLedger)
{
    const std::string devPath = "/tmp/ubse_ssut_reconcile_backfill_dev";
    unlink(devPath.c_str());
    auto cacheNs = MakeNsForCache("eid_backfill_001", "nqn.backfill", 1); // EID必须16字符，否则被adapter拒绝
    AddNsToCollectorCache(cacheNs);
    auto entry = MakeAttachedEntry("sp_backfill", UbseSsuAllocStrategy::NORMAL, {"master_node_1"});
    entry.allocResult.nameSpaceList.push_back(MakeNameSpaceInfo(cacheNs));
    UbseSsuDebtLedger::GetInstance().Put("sp_backfill", std::make_shared<const UbseSsuLedgerEntry>(entry));

    service_.ReconcileSelfLocalMounts("master_node_1");
    EXPECT_EQ(g_attachCallCount.load(), 1); // 重挂（nqn留空经ResolveNqn回落defaultNqn）
    auto record = FindRecoverRecord("sp_backfill");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->recordState, UbseSsuAttachRecordState::ATTACHED); // 补齐记录落库
    EXPECT_EQ(record->nsList.size(), 1U);
    ASSERT_EQ(record->nsVerifyList.size(), 1U);
    EXPECT_EQ(record->nsVerifyList[0].defaultNqn, "nqn.default");
}

// UT24-9：RunMountReconcile门禁——账本未就绪（首采未成功）跳过，不产生任何平账动作
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileRunSkipsWhenLedgerNotReady)
{
    const std::string devPath = "/tmp/ubse_ssut_reconcile_notready_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("sp_notready", devPath)),
              UBSE_OK);
    UbseSsuDebtLedger::GetInstance().SetReady(false);

    service_.RunMountReconcile();
    EXPECT_EQ(GetRecords().size(), 1U); // 残留记录保留（若执行会被清理）
    EXPECT_EQ(g_detachCallCount.load(), 0);
}

// UT24-10：RunMountReconcile门禁——恢复收敛窗口内（§14.2-2）跳过本轮，
// 避免与恢复期各节点重报竞争；窗口外正常执行
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileRunSkipsInRecoveryWindow)
{
    // 激活恢复收敛窗口（UT19-2模式：Rebuild生成restoredFromHardware条目）
    auto exclNs = MakeNsForCache("eid_recon_win_001", "nqn.recon.win", 1);
    std::strncpy(exclNs.customData.name, "sp_recon_win", sizeof(exclNs.customData.name) - 1);
    exclNs.customData.nsNum = 1;
    exclNs.customData.totalBytes = 4096;
    exclNs.customData.allocStrategy = static_cast<uint8_t>(UbseSsuAllocStrategy::LINEAR);
    AddNsToCollectorCache(exclNs);
    service_.RebuildLedgerFromDevList();
    ASSERT_TRUE(UbseSsuDebtLedger::GetInstance().IsInReconcileWindow());

    const std::string devPath = "/tmp/ubse_ssut_reconcile_window_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("sp_in_window", devPath)),
              UBSE_OK);
    // 账本无该空间（Rebuild仅含硬件恢复条目）= 已free残留；窗口内不得清理

    service_.RunMountReconcile();
    EXPECT_EQ(GetRecords().size(), 1U); // 窗口内跳过，残留记录保留
    EXPECT_EQ(g_detachCallCount.load(), 0);
}

// UT24-11：RunMountReconcile正常执行——本机直平生效（残留清理），
// 远端节点循环在UT环境天然空转（GetAllNodes为空）
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileRunExecutesSelfReconcile)
{
    const std::string devPath = "/tmp/ubse_ssut_reconcile_run_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(MakeRecoverRecord("sp_run", devPath)),
              UBSE_OK);
    ASSERT_FALSE(UbseSsuDebtLedger::GetInstance().IsInReconcileWindow()); // 无窗口

    service_.RunMountReconcile();
    EXPECT_EQ(GetRecords().size(), 0U); // 残留记录被本机直平清理
    EXPECT_EQ(g_detachCallCount.load(), 1);
}

// UT24-12：运营对账定时器启停幂等——重复Start不重复注册、Stop后可重启、未启动时Stop为no-op
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileTimerStartStopIdempotent)
{
    EXPECT_EQ(service_.StartMountReconcileTimer(), UBSE_OK);
    EXPECT_EQ(service_.StartMountReconcileTimer(), UBSE_OK); // CAS幂等：已启动直接返回
    EXPECT_TRUE(service_.mountReconcileFlags_.started_.load());
    service_.StopMountReconcileTimer();
    EXPECT_FALSE(service_.mountReconcileFlags_.started_.load());
    service_.StopMountReconcileTimer(); // 未启动Stop为no-op
    EXPECT_EQ(service_.StartMountReconcileTimer(), UBSE_OK); // 可重启
    service_.StopMountReconcileTimer();
}

// ====== v1.1 master本地挂载入口恢复期守卫（reliable_design.md §9.1 UT25，
// 与VerifyAttachDetachPrecondition对称） ======
// AttachSpace(NORMAL)/AttachLinearSpace(LINEAR)/AttachStripedSpace(STRIPED)的master本地路径
// 此前仅基础校验（状态/策略/attacher），恢复期四守卫只在verify RPC路径生效——master自身
// 挂载不设防存在双重挂载风险。本块覆盖：账本未就绪门禁（§14.2-1）、FAULTED拦截（§12.2，
// 返回1204而非泛化STATE_INVALID）、疑似残留（§14.2-4）、恢复收敛窗口（§14.2-2）

namespace {

// 守卫用例NS信息（挂载入口策略匹配所需；size统一4096适配条带化一致性校验）
UbseSsuNameSpaceInfo MakeGuardNsInfo(const std::string &eid, uint32_t nsId)
{
    UbseSsuNameSpaceInfo ns{};
    ns.tgtEid = eid; // 16字符，满足BuildNamespaceInfoForBasic长度校验
    ns.tgtNqn = "nqn.tgt.guard";
    ns.nsUuid = "uuid_guard_" + std::to_string(nsId);
    ns.namespaceId = nsId;
    ns.nsSize = 4096;
    ns.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    return ns;
}

void PutGuardEntry(const std::string &name, UbseSsuAllocStrategy strategy, UbseSsuNsState state,
                   bool restoredFromHardware = false, size_t nsCount = 0, bool syncNsToCache = false)
{
    UbseSsuLedgerEntry entry{};
    entry.name = name;
    entry.allocReq.strategy = strategy;
    entry.allocResult.strategy = strategy;
    entry.state = state;
    entry.restoredFromHardware = restoredFromHardware;
    for (uint32_t i = 1; i <= static_cast<uint32_t>(nsCount); ++i) {
        auto eid = "eid_grd_" + std::to_string(10000000 + i); // "eid_grd_10000001"恰16字符
        if (syncNsToCache) {
            // attach入口身份预校验先于守卫判断且需从缓存读取NS customData：
            // 缓存与账本NS须同源构造（uuid经StrToUuid规范格式才能与缓存比对命中），
            // uid/userName留空与请求默认identity（空值）天然匹配
            auto ns = UbseSsuServiceImpTestBase::MakeNsForCache(eid, "nqn.tgt.guard", i, 4096, std::string(16, '\xAB'),
                                                                std::string(16, '\xAB'), 0, "");
            UbseSsuServiceImpTestBase::AddNsToCollectorCache(ns);
            entry.allocResult.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns));
        } else {
            entry.allocResult.nameSpaceList.push_back(MakeGuardNsInfo(eid, i));
        }
    }
    UbseSsuDebtLedger::GetInstance().Put(name, std::make_shared<const UbseSsuLedgerEntry>(entry));
}

} // namespace

// UT25-1：账本未就绪门禁——三入口均回MASTER_NOT_READY（纯重试语义），
// 先于Get生效（条目存在也被拒），杜绝恢复期误报SPACE_NOT_FOUND误导SDK
TEST_F(TestUbseSsuServiceImpRecover, MasterLocalAttachLedgerNotReadyGate)
{
    UbseSsuDebtLedger::GetInstance().SetReady(false);
    PutGuardEntry("space_g_n", UbseSsuAllocStrategy::NORMAL, UbseSsuNsState::CREATED);
    PutGuardEntry("space_g_l", UbseSsuAllocStrategy::LINEAR, UbseSsuNsState::CREATED);
    PutGuardEntry("space_g_s", UbseSsuAllocStrategy::STRIPED, UbseSsuNsState::CREATED, false, 2);

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    UbseSsuSpaceReq normalReq;
    normalReq.name = "space_g_n";
    EXPECT_EQ(service_.AttachSpace(normalReq, nsDevPaths), UBSE_SSU_ERROR_MASTER_NOT_READY);
    UbseSsuLinearSpaceReq linearReq;
    linearReq.name = "space_g_l";
    EXPECT_EQ(service_.AttachLinearSpace(linearReq, nsDevPaths, devPath), UBSE_SSU_ERROR_MASTER_NOT_READY);
    UbseSsuStripedSpaceReq stripedReq;
    stripedReq.name = "space_g_s";
    EXPECT_EQ(service_.AttachStripedSpace(stripedReq, nsDevPaths, devPath), UBSE_SSU_ERROR_MASTER_NOT_READY);
}

// UT25-2：FAULTED拦截——设备故障空间三入口回SPACE_FAULTED(1204)，
// 而非泛化STATE_INVALID（SDK可据此提示"可free止损"）；回正后拦截自动解除（§12.1周期对账回正UT覆盖）
TEST_F(TestUbseSsuServiceImpRecover, MasterLocalAttachFaultedRejected)
{
    PutGuardEntry("space_f_n", UbseSsuAllocStrategy::NORMAL, UbseSsuNsState::FAULTED);
    PutGuardEntry("space_f_l", UbseSsuAllocStrategy::LINEAR, UbseSsuNsState::FAULTED);
    PutGuardEntry("space_f_s", UbseSsuAllocStrategy::STRIPED, UbseSsuNsState::FAULTED);

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    UbseSsuSpaceReq normalReq;
    normalReq.name = "space_f_n";
    EXPECT_EQ(service_.AttachSpace(normalReq, nsDevPaths), UBSE_SSU_ERROR_SPACE_FAULTED);
    UbseSsuLinearSpaceReq linearReq;
    linearReq.name = "space_f_l";
    EXPECT_EQ(service_.AttachLinearSpace(linearReq, nsDevPaths, devPath), UBSE_SSU_ERROR_SPACE_FAULTED);
    UbseSsuStripedSpaceReq stripedReq;
    stripedReq.name = "space_f_s";
    EXPECT_EQ(service_.AttachStripedSpace(stripedReq, nsDevPaths, devPath), UBSE_SSU_ERROR_SPACE_FAULTED);
}

// UT25-3：疑似残留守卫——独占空间记入疑似集合（本机有NS设备但记录丢失、attacher未知）期间，
// master本地attach同样拒绝（verify只拦RPC侧，本地不设防则master可抢先挂载残留独占设备）
TEST_F(TestUbseSsuServiceImpRecover, MasterLocalAttachSuspectedGuardExclusive)
{
    PutGuardEntry("space_sus_l", UbseSsuAllocStrategy::LINEAR, UbseSsuNsState::CREATED);
    PutGuardEntry("space_sus_s", UbseSsuAllocStrategy::STRIPED, UbseSsuNsState::CREATED, false, 2, true);
    {
        std::lock_guard<std::mutex> lock(service_.suspectedMountTable_.mtx_);
        service_.suspectedMountTable_.suspected_.insert("space_sus_l");
        service_.suspectedMountTable_.suspected_.insert("space_sus_s");
    }

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    // UbseSsuAllocIdentityInfo.uid无默认初始化器，须显式置空（uid=0）与守卫NS空customData匹配
    UbseSsuLinearSpaceReq linearReq;
    linearReq.name = "space_sus_l";
    linearReq.identity = UbseSsuAllocIdentityInfo{};
    EXPECT_EQ(service_.AttachLinearSpace(linearReq, nsDevPaths, devPath), UBSE_SSU_ERROR_MOUNT_SUSPECTED);
    UbseSsuStripedSpaceReq stripedReq;
    stripedReq.name = "space_sus_s";
    stripedReq.identity = UbseSsuAllocIdentityInfo{};
    EXPECT_EQ(service_.AttachStripedSpace(stripedReq, nsDevPaths, devPath), UBSE_SSU_ERROR_MOUNT_SUSPECTED);
}

// UT25-4：恢复收敛窗口守卫——窗口内硬件恢复的独占空间，master本地attach同样拒绝
// （master即"其他节点"之一，不设防则attacher重报到达前可抢先挂载）；NORMAL硬件恢复空间不受限
TEST_F(TestUbseSsuServiceImpRecover, MasterLocalAttachWindowGuardRestoredSpace)
{
    // Rebuild激活收敛窗口（UT19-2模式：带customData的NS经RebuildLedgerFromDevList生成硬件恢复条目）
    auto winNs = MakeNsForCache("eid_win3_act_001", "nqn.win3.act", 1);
    std::strncpy(winNs.customData.name, "space_win3_act", sizeof(winNs.customData.name) - 1);
    winNs.customData.nsNum = 1;
    winNs.customData.totalBytes = 4096;
    winNs.customData.allocStrategy = static_cast<uint8_t>(UbseSsuAllocStrategy::LINEAR);
    AddNsToCollectorCache(winNs);
    service_.RebuildLedgerFromDevList();
    ASSERT_TRUE(UbseSsuDebtLedger::GetInstance().IsInReconcileWindow());

    // Rebuild生成的LINEAR硬件恢复条目（CREATED+restoredFromHardware）：窗口内master本地attach被拒
    // （请求携带属主identity以通过前置身份校验，winNs customData为uid=100/test_user）
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    UbseSsuLinearSpaceReq linearReq;
    linearReq.name = "space_win3_act";
    linearReq.identity = MakeIdentity();
    EXPECT_EQ(service_.AttachLinearSpace(linearReq, nsDevPaths, devPath), UBSE_SSU_ERROR_MASTER_NOT_READY);

    // 同窗口手工STRIPED硬件恢复条目：同样被拒（覆盖striped入口接线，条带化参数校验已通过）
    PutGuardEntry("space_win3_s", UbseSsuAllocStrategy::STRIPED, UbseSsuNsState::CREATED, true, 2, true);
    UbseSsuStripedSpaceReq stripedReq;
    stripedReq.name = "space_win3_s";
    // UbseSsuAllocIdentityInfo.uid无默认初始化器，须显式置空（uid=0）与守卫NS空customData匹配
    stripedReq.identity = UbseSsuAllocIdentityInfo{};
    EXPECT_EQ(service_.AttachStripedSpace(stripedReq, nsDevPaths, devPath), UBSE_SSU_ERROR_MASTER_NOT_READY);

    // NORMAL多节点共挂无独占双重挂载竞态，不设该守卫（挂载继续走正常流程）
    PutGuardEntry("space_win3_n", UbseSsuAllocStrategy::NORMAL, UbseSsuNsState::CREATED, true);
    UbseSsuSpaceReq normalReq;
    normalReq.name = "space_win3_n";
    EXPECT_NE(service_.AttachSpace(normalReq, nsDevPaths), UBSE_SSU_ERROR_MASTER_NOT_READY);
}

// ====== 本地挂载重挂对账定时器（reliable_design.md §12.3收敛增强，UT26） ======
// 触发背景：agent无collector首采回调触发源，重挂重试仅靠MASTER_ONLINE事件驱动可能长期不触发，
// 定时器提供确定性本地对账节奏。覆盖：PENDING跳过（在途挂载主流程收敛）、
// SUSPECT/ATTACHED统一探测缺失即重挂（持续重试直到挂上，失败计数达§12.3阈值标记
// SUSPECT告警但不止损）、按角色上报、master本地对账路径、单轮防重入

namespace {
std::atomic<int> g_localReenterCount{0};
} // namespace

// UT26-1：PENDING在途挂载跳过——定时器不得干扰主流程（不重挂、不作废、不上报，状态原样保留）
TEST_F(TestUbseSsuServiceImpRecover, LocalReconcileSkipsPendingRecord)
{
    const std::string devPath = "/tmp/ubse_ssut_lrec_pend_dev";
    unlink(devPath.c_str()); // 设备缺失（在途挂载的正常中间态）也不触发任何动作
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_lrec_pend", devPath, false, UbseSsuAttachRecordState::PENDING)),
              UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();
    service_.RunLocalMountReconcile();

    EXPECT_EQ(g_attachCallCount.load(), 0);
    EXPECT_TRUE(g_reportedNames.empty());
    auto records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].recordState, UbseSsuAttachRecordState::PENDING);
}

// UT26-2：SUSPECT持续重试重挂（§12.3，不再止损），重挂仍失败保留SUSPECT零上报；
// 设备恢复（探测命中）后收敛回ATTACHED并重报（纯探测零挂载原语）
TEST_F(TestUbseSsuServiceImpRecover, LocalReconcileSuspectKeepsRetryingThenRecovers)
{
    const std::string devPath = "/tmp/ubse_ssut_lrec_sus_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_lrec_sus", devPath, false, UbseSsuAttachRecordState::SUSPECT)),
              UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();
    g_attachFailAfter.store(0); // 重挂原语持续失败（模拟故障硬件）
    g_attachFailRet.store(UBSE_SSU_ERROR_NS_NOT_FOUND);

    // 第一轮：仍缺失 → 持续重挂（失败1/3），保留SUSPECT，零上报
    service_.RunLocalMountReconcile();
    EXPECT_EQ(g_attachCallCount.load(), 1);
    EXPECT_TRUE(g_reportedNames.empty());
    auto records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].recordState, UbseSsuAttachRecordState::SUSPECT);

    // 第二轮：设备恢复 → 探测命中收敛回ATTACHED，经STATUS_UPDATE重报
    TouchFile(devPath);
    g_attachCallCount.store(0);
    service_.RunLocalMountReconcile();
    EXPECT_EQ(g_attachCallCount.load(), 0); // 收敛纯探测，不调挂载原语
    EXPECT_EQ(g_reportedNames, std::vector<std::string>({"space_lrec_sus"}));
    records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].recordState, UbseSsuAttachRecordState::ATTACHED);
    unlink(devPath.c_str());
}

// UT26-3：ATTACHED漂移重挂——探测缺失触发AgentAttachNs重挂，成功后重报账本（agent路径）；
// 设备就绪后重复tick零操作（幂等）
TEST_F(TestUbseSsuServiceImpRecover, LocalReconcileRemountsAttachedDriftAndReports)
{
    const std::string devPath = "/tmp/ubse_ssut_lrec_drift_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_lrec_drift", devPath)), UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();

    // 第一轮：探测缺失 → 重挂一次并重报
    service_.RunLocalMountReconcile();
    EXPECT_EQ(g_attachCallCount.load(), 1);
    EXPECT_EQ(g_reportedNames, std::vector<std::string>({"space_lrec_drift"}));

    // 设备恢复后第二轮：探测命中 → 零原语零上报
    TouchFile(devPath);
    g_attachCallCount.store(0);
    ResetReportCollector();
    service_.RunLocalMountReconcile();
    EXPECT_EQ(g_attachCallCount.load(), 0);
    EXPECT_TRUE(g_reportedNames.empty());
    unlink(devPath.c_str());
}

// UT26-4：重挂失败计数达阈值进SUSPECT（§12.3）——连续3轮失败标记SUSPECT告警，
// 此后tick仍持续重试重挂（不再止损），记录保留SUSPECT零上报
TEST_F(TestUbseSsuServiceImpRecover, LocalReconcileFailCountReachesSuspect)
{
    const std::string devPath = "/tmp/ubse_ssut_lrec_fail_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_lrec_fail", devPath)), UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();
    g_attachFailAfter.store(0); // 所有attach原语调用失败
    g_attachFailRet.store(-1);

    service_.RunLocalMountReconcile(); // 1/3：保留ATTACHED
    service_.RunLocalMountReconcile(); // 2/3：保留ATTACHED
    EXPECT_EQ(g_attachCallCount.load(), 2);
    auto records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].recordState, UbseSsuAttachRecordState::ATTACHED);

    service_.RunLocalMountReconcile(); // 3/3：达阈值 → SUSPECT标记告警
    EXPECT_EQ(g_attachCallCount.load(), 3);
    records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].recordState, UbseSsuAttachRecordState::SUSPECT);
    // 失败轮不上报（仅本地计数，等收敛回ATTACHED后才上报）
    EXPECT_TRUE(g_reportedNames.empty());

    // SUSPECT后：tick仍持续重试重挂（不放弃，直到挂上为止）
    g_attachCallCount.store(0);
    service_.RunLocalMountReconcile();
    EXPECT_EQ(g_attachCallCount.load(), 1);
    records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].recordState, UbseSsuAttachRecordState::SUSPECT);
    EXPECT_TRUE(g_reportedNames.empty());
}

// UT26-8：SUSPECT持续重试直到挂上——探测缺失但重挂原语成功（链路恢复）→
// 收敛回ATTACHED并重报账本（"一直尝试重挂直到挂上为止"的正向收敛路径）
TEST_F(TestUbseSsuServiceImpRecover, LocalReconcileSuspectRemountSucceedsAndConverges)
{
    const std::string devPath = "/tmp/ubse_ssut_lrec_sus_ok_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_lrec_sus_ok", devPath, false, UbseSsuAttachRecordState::SUSPECT)),
              UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();
    // attach原语默认成功：SUSPECT重试即挂上（探测文件虽未出现，mock原语返回OK代表链路恢复）

    service_.RunLocalMountReconcile();
    EXPECT_EQ(g_attachCallCount.load(), 1); // SUSPECT不放弃，本轮仍调重挂原语
    EXPECT_EQ(g_reportedNames, std::vector<std::string>({"space_lrec_sus_ok"})); // 挂上即重报
    auto records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].recordState, UbseSsuAttachRecordState::ATTACHED); // 收敛回ATTACHED
}

// UT26-9：聚合设备组装前NS就绪核验——attach返回OK但NS设备节点未出现（udev竞态窗口）
// 时不发起聚合组装（Probe/Assemble零调用），本轮算重挂失败等下轮重试
TEST_F(TestUbseSsuServiceImpRecover, LocalReconcileSkipsAssemblyWhenNsNodeNotVisible)
{
    const std::string devPath = "/tmp/ubse_ssut_lrec_nsv_dev";
    unlink(devPath.c_str()); // NS设备节点缺失：attach原语mock成功但不创建文件
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_lrec_nsv", devPath, true)), UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::ProbeBlockDevice).stubs().will(invoke(CountProbe));
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::AssembleBlockDevice).stubs().will(invoke(CountAssemble));

    service_.RunLocalMountReconcile();
    EXPECT_EQ(g_attachCallCount.load(), 1); // NS缺失触发重挂（attach原语成功）
    EXPECT_EQ(g_probeCallCount.load(), 0); // NS节点未出现：不进入聚合组装决策
    EXPECT_EQ(g_assembleCallCount.load(), 0); // 不对部分在位硬件发起组装
    EXPECT_TRUE(g_reportedNames.empty()); // 重挂未成功不上报
    auto records = GetRecords();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].recordState, UbseSsuAttachRecordState::ATTACHED); // 1/3次失败未达阈值
}

// UT26-5：master路径——重挂成功后走本地账本对账（ReconcileMasterLedger本地Modify，
// 不经STATUS_UPDATE RPC），attachNodes补入本节点、state置ATTACHED
TEST_F(TestUbseSsuServiceImpRecover, LocalReconcileMasterPathUpdatesLedger)
{
    const std::string devPath = "/tmp/ubse_ssut_lrec_master_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_lrec_master", devPath)), UBSE_OK);
    PutRebuiltLedgerEntry("space_lrec_master", UbseSsuAllocStrategy::NORMAL);

    // fixture默认master角色+账本就绪
    SetupReportCollector(); // master本地Modify不经RPC，收集器应保持空
    service_.RunLocalMountReconcile();

    EXPECT_EQ(g_attachCallCount.load(), 1);
    EXPECT_TRUE(g_reportedNames.empty());
    auto entry = UbseSsuDebtLedger::GetInstance().Get("space_lrec_master");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
    EXPECT_EQ(entry->attachNodes.count("master_node_1"), 1u); // MockGetCurrentNodeInfo_Master的nodeId
}

// UT26-6：单轮防重入——重报钩子内再入RunLocalMountReconcile（单轮执行中重复tick），
// CAS防重入直接跳过，不重入不重挂
TEST_F(TestUbseSsuServiceImpRecover, LocalReconcileReentrancyGuard)
{
    const std::string devPath = "/tmp/ubse_ssut_lrec_reenter_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_lrec_reenter", devPath)), UBSE_OK);

    SetupAgentRoleAndRpcSuccess();
    SetupReportCollector();
    g_localReenterCount.store(0);
    g_onStatusReport = [this](const std::string &name) {
        (void)name;
        g_localReenterCount.fetch_add(1);
        service_.RunLocalMountReconcile(); // 再入：应被CAS防重入直接跳过
    };

    service_.RunLocalMountReconcile();
    EXPECT_EQ(g_localReenterCount.load(), 1); // 钩子触发一次
    EXPECT_EQ(g_attachCallCount.load(), 1);   // 再入未产生第二次重挂
    EXPECT_EQ(g_reportedNames, std::vector<std::string>({"space_lrec_reenter"})); // 外层上报正常完成
}

// UT26-7：定时器启停幂等——重复Start不重复注册、Stop后可重启、未启动时Stop为no-op
TEST_F(TestUbseSsuServiceImpRecover, LocalMountReconcileTimerStartStopIdempotent)
{
    EXPECT_EQ(service_.StartLocalMountReconcileTimer(), UBSE_OK);
    EXPECT_EQ(service_.StartLocalMountReconcileTimer(), UBSE_OK); // CAS幂等
    EXPECT_TRUE(service_.localMountReconcileFlags_.started_.load());
    service_.StopLocalMountReconcileTimer();
    EXPECT_FALSE(service_.localMountReconcileFlags_.started_.load());
    service_.StopLocalMountReconcileTimer(); // 未启动Stop为no-op
    EXPECT_EQ(service_.StartLocalMountReconcileTimer(), UBSE_OK); // 可重启
    service_.StopLocalMountReconcileTimer();
}

// UT27-1（检视S1-01）：master自身对账attacher冲突防护——master重启Rebuild后agent B重报
// 先到（账本attachNodes={B}、state=ATTACHED），master自身恢复对账不得无条件insert自身
// （与HandleStatusReceiver拒收ATTACHER_MISMATCH对称）：attachNodes保留归属方、state与
// devName不被自身记录覆盖、不形成双attacher、不误触"账本查无"自愈清理（记录保留待人工）
TEST_F(TestUbseSsuServiceImpRecover, MasterLedgerReconcileAttacherConflictKeepsHolder)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_conflict_dev";
    TouchFile(devPath); // NS设备在位：确保阶段1重挂成功，阶段2对账真正执行（否则用例空转）
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_conflict", devPath, true)), UBSE_OK);
    // B重报先到后的账本快照：LINEAR、attachNodes={B}、devName为归属方值
    auto entry = MakeAttachedEntry("space_conflict", UbseSsuAllocStrategy::LINEAR, {"node_agent_b"});
    entry.devName = "agg_dev_holder_b";
    UbseSsuDebtLedger::GetInstance().Put("space_conflict",
                                         std::make_shared<const UbseSsuLedgerEntry>(entry));
    UbseSsuDebtLedger::GetInstance().SetReady(true);

    // 聚合设备缺失走重挂路径：mock Probe/Assemble避免真实LVM/mdadm命令
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::ProbeBlockDevice).stubs().will(invoke(CountProbe));
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::AssembleBlockDevice).stubs().will(invoke(CountAssemble));

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    const auto *after = UbseSsuDebtLedger::GetInstance().Get("space_conflict").get();
    ASSERT_NE(after, nullptr);
    EXPECT_EQ(after->attachNodes.size(), 1u); // 未形成双attacher
    EXPECT_EQ(after->attachNodes.count("node_agent_b"), 1u);
    EXPECT_EQ(after->attachNodes.count("master_node_1"), 0u); // 自身未抢占
    EXPECT_EQ(after->state, UbseSsuNsState::ATTACHED); // 归属方状态保留
    EXPECT_EQ(after->devName, "agg_dev_holder_b"); // devName不被自身记录覆盖
    EXPECT_EQ(GetRecords().size(), 1u); // 记录保留，不误触自愈清理
    unlink(devPath.c_str());
}

// UT27-2（检视S1-01顺带项）：master自身对账FAULTED拦截——设备故障空间的状态对账不覆盖
// FAULTED（与HandleStatusReceiver拒收FAULTED上报对称，防故障告警被恢复对账掩盖）；
// attachNodes不insert自身，记录保留，设备恢复由collector对账回正后再收敛
TEST_F(TestUbseSsuServiceImpRecover, MasterLedgerReconcileSkipsFaultedSpace)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_faulted_dev";
    TouchFile(devPath); // NS设备在位：确保阶段1重挂成功，阶段2对账真正执行（否则用例空转）
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_recon_faulted", devPath, true)), UBSE_OK);
    auto entry = MakeAttachedEntry("space_recon_faulted", UbseSsuAllocStrategy::LINEAR, {});
    entry.state = UbseSsuNsState::FAULTED;
    UbseSsuDebtLedger::GetInstance().Put("space_recon_faulted",
                                          std::make_shared<const UbseSsuLedgerEntry>(entry));
    UbseSsuDebtLedger::GetInstance().SetReady(true);

    auto &impl = UbseSsuAdapterImpl::GetInstance();
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::ProbeBlockDevice).stubs().will(invoke(CountProbe));
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::AssembleBlockDevice).stubs().will(invoke(CountAssemble));

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    const auto *after = UbseSsuDebtLedger::GetInstance().Get("space_recon_faulted").get();
    ASSERT_NE(after, nullptr);
    EXPECT_EQ(after->state, UbseSsuNsState::FAULTED); // 故障状态未被覆盖
    EXPECT_TRUE(after->attachNodes.empty()); // 未insert自身
    EXPECT_EQ(GetRecords().size(), 1u); // 记录保留
    unlink(devPath.c_str());
}

// UT27-3（检视S1-01）：master自身对账幂等——attachNodes已含本机（自身先对账/B后到被拒收）
// 时重复对账不重复扩张，devName仍按自身记录回填（冲突守卫只拦"归属他节点"，
// 不影响合法自恢复路径）
TEST_F(TestUbseSsuServiceImpRecover, MasterLedgerReconcileIdempotentWhenSelfHolds)
{
    const std::string devPath = "/tmp/ubse_ssut_recover_selfhold_dev";
    TouchFile(devPath); // NS设备在位：确保阶段1重挂成功，阶段2对账真正执行（否则用例空转）
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_selfhold", devPath, true)), UBSE_OK);
    // 自身先对账的账本快照：LINEAR、attachNodes={master_node_1}、devName待回填
    auto entry = MakeAttachedEntry("space_selfhold", UbseSsuAllocStrategy::LINEAR, {"master_node_1"});
    UbseSsuDebtLedger::GetInstance().Put("space_selfhold",
                                         std::make_shared<const UbseSsuLedgerEntry>(entry));
    UbseSsuDebtLedger::GetInstance().SetReady(true);

    auto &impl = UbseSsuAdapterImpl::GetInstance();
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::ProbeBlockDevice).stubs().will(invoke(CountProbe));
    MOCKER_CPP_VIRTUAL(impl, &UbseSsuAdapterImpl::AssembleBlockDevice).stubs().will(invoke(CountAssemble));

    EXPECT_EQ(service_.RecoverLocalAttachments(), UBSE_OK);
    const auto *after = UbseSsuDebtLedger::GetInstance().Get("space_selfhold").get();
    ASSERT_NE(after, nullptr);
    EXPECT_EQ(after->attachNodes.size(), 1u); // 不重复扩张
    EXPECT_EQ(after->attachNodes.count("master_node_1"), 1u);
    EXPECT_EQ(after->state, UbseSsuNsState::ATTACHED);
    EXPECT_EQ(after->devName, "agg_dev_r"); // 自身记录devName正常回填
    unlink(devPath.c_str());
}

// UT27-4（检视S1-01）：本机直平REPORT分支attacher冲突——账本期望节点不含本机且独占空间
// 已归属他节点（attachNodes非空）→ 直报经ReconcileMasterLedger内置冲突校验跳过，
// 不抢占归属方（原UpdateStateOrNotify→ApplyAggDevLedgerState无条件insert会重新引入
// 双attacher）；零挂载原语、记录保留
TEST_F(TestUbseSsuServiceImpRecover, MountReconcileSelfReportAttacherConflictSkipped)
{
    const std::string devPath = "/tmp/ubse_ssut_reconcile_conflict_dev";
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("sp_report_conflict", devPath)), UBSE_OK);
    auto entry = MakeAttachedEntry("sp_report_conflict", UbseSsuAllocStrategy::LINEAR, {"node_agent_b"});
    entry.devName = "agg_dev_holder_b";
    UbseSsuDebtLedger::GetInstance().Put("sp_report_conflict",
                                          std::make_shared<const UbseSsuLedgerEntry>(entry));

    service_.ReconcileSelfLocalMounts("master_node_1");
    const auto *after = UbseSsuDebtLedger::GetInstance().Get("sp_report_conflict").get();
    ASSERT_NE(after, nullptr);
    EXPECT_EQ(after->attachNodes.size(), 1u); // 未形成双attacher
    EXPECT_EQ(after->attachNodes.count("node_agent_b"), 1u);
    EXPECT_EQ(after->attachNodes.count("master_node_1"), 0u);
    EXPECT_EQ(after->state, UbseSsuNsState::ATTACHED);
    EXPECT_EQ(after->devName, "agg_dev_holder_b"); // 归属方devName不被覆盖
    EXPECT_TRUE(FindRecoverRecord("sp_report_conflict").has_value()); // 记录保留
    EXPECT_EQ(g_attachCallCount.load(), 0); // REPORT分支零挂载原语
    EXPECT_EQ(g_detachCallCount.load(), 0);
}

// UT27-5（检视S1-01）：本地挂载定时器master对账路径冲突防护——重挂成功后经
// ReconcileMasterLedger对账时账本已归属他节点（agent重报先到）→ 跳过对账不抢占；
// 重挂本身仍执行（本地数据面恢复不依赖账本归属），不经STATUS_UPDATE RPC
TEST_F(TestUbseSsuServiceImpRecover, LocalReconcileMasterPathAttacherConflictSkipped)
{
    const std::string devPath = "/tmp/ubse_ssut_lrec_conflict_dev";
    unlink(devPath.c_str());
    ASSERT_EQ(UbseSsuAttachRecordStore::GetInstance().Upsert(
                  MakeRecoverRecord("space_lrec_conflict", devPath)), UBSE_OK);
    auto entry = MakeAttachedEntry("space_lrec_conflict", UbseSsuAllocStrategy::LINEAR, {"node_agent_b"});
    entry.devName = "agg_dev_holder_b";
    UbseSsuDebtLedger::GetInstance().Put("space_lrec_conflict",
                                          std::make_shared<const UbseSsuLedgerEntry>(entry));

    SetupReportCollector(); // master本地Modify不经RPC，收集器应保持空
    service_.RunLocalMountReconcile();

    EXPECT_EQ(g_attachCallCount.load(), 1); // 漂移重挂仍执行（数据面恢复与账本归属解耦）
    EXPECT_TRUE(g_reportedNames.empty());
    const auto *after = UbseSsuDebtLedger::GetInstance().Get("space_lrec_conflict").get();
    ASSERT_NE(after, nullptr);
    EXPECT_EQ(after->attachNodes.size(), 1u);
    EXPECT_EQ(after->attachNodes.count("node_agent_b"), 1u);
    EXPECT_EQ(after->attachNodes.count("master_node_1"), 0u); // 对账未抢占
    EXPECT_EQ(after->devName, "agg_dev_holder_b");
}

// ====== （name, resourceUuid）资源身份对账（ReconcileLedgerResource UT28） ======

namespace {

// 构造带customData身份（name+resourceUuid+nsNum）的NS：devList扫描与账本构造同源，
// uuid字节各NS须不同（默认MakeNsForCache所有NS同uuid，集合去重后会误判NS缺失）
UbseSsuDevNameSpace MakeNsWithIdentity(const std::string &eid, uint32_t nsId, const std::string &spaceName,
                                       const std::string &resourceUuid, uint8_t nsNum, uint8_t uuidByte)
{
    auto ns = UbseSsuServiceImpTestBase::MakeNsForCache(eid, "nqn.identity", nsId, 4096,
                                                        std::string(GUID_SIZE, '\xAB'),
                                                        std::string(UUID_SIZE, static_cast<char>(uuidByte)));
    std::strncpy(ns.customData.name, spaceName.c_str(), sizeof(ns.customData.name) - 1);
    ns.customData.name[sizeof(ns.customData.name) - 1] = '\0';
    // MakeNsForCache默认初始化不清零customData，resourceUuid须先清零再拷贝，
    // 否则strnlen会读到尾部垃圾字节导致复合键错位
    std::memset(ns.customData.resourceUuid, 0, sizeof(ns.customData.resourceUuid));
    std::strncpy(ns.customData.resourceUuid, resourceUuid.c_str(), sizeof(ns.customData.resourceUuid) - 1);
    ns.customData.nsNum = nsNum;
    ns.customData.totalBytes = 4096;
    return ns;
}

// 每NS独立设备（对齐一NS一盘的分配约束）
std::vector<UbseSsuDevInfo> MakeIdentityDevList(const std::vector<UbseSsuDevNameSpace> &nss)
{
    std::vector<UbseSsuDevInfo> devList;
    for (const auto &ns : nss) {
        UbseSsuDevInfo dev{};
        dev.subSystem = ns.subSystem;
        dev.nameSpaces.push_back(ns);
        devList.push_back(dev);
    }
    return devList;
}

// 账本托管条目：与硬件NS同源构造（allocResult.nsUuid经MakeNameSpaceInfo/StrToUuid，
// 与扫描侧集合口径一致）
UbseSsuLedgerEntry MakeIdentityEntry(const std::string &spaceName, const std::string &resourceUuid,
                                     const std::vector<UbseSsuDevNameSpace> &nss)
{
    auto entry = MakeExclusiveEntry(spaceName, false);
    entry.allocReq.nsNum = static_cast<uint32_t>(nss.size());
    entry.allocResult.resourceUuid = resourceUuid;
    for (const auto &ns : nss) {
        entry.allocResult.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns));
    }
    return entry;
}

} // namespace

// UT28-1：2a uuid漂移——同名硬件组换了resourceUuid（资源被替换），连续
// SSU_FAULT_DEBOUNCE_CYCLES(3)周期账本uuid组缺失才置FAULTED；组回归即清零并回正，
// 再次漂移需重新计满
TEST_F(TestUbseSsuServiceImpRecover, UuidDriftDebounceMarksFaulted)
{
    const std::string uuidA = "11111111-1111-1111-1111-111111111111";
    const std::string uuidB = "22222222-2222-2222-2222-222222222222";
    auto nsLedger = MakeNsWithIdentity("eid_drift_a_0001", 1, "space_drift_a", uuidA, 1, 0x11);
    UbseSsuDebtLedger::GetInstance().Put(
        "space_drift_a",
        std::make_shared<const UbseSsuLedgerEntry>(MakeIdentityEntry("space_drift_a", uuidA, {nsLedger})));
    auto &ledger = UbseSsuDebtLedger::GetInstance();

    // 硬件同名但uuid换成B（漂移态）
    auto drifted = MakeIdentityDevList({MakeNsWithIdentity("eid_drift_a_0002", 1, "space_drift_a", uuidB, 1, 0x22)});
    service_.ReconcileLedgerResource(drifted);
    service_.ReconcileLedgerResource(drifted);
    EXPECT_EQ(ledger.Get("space_drift_a")->state, UbseSsuNsState::CREATED); // 防抖不足不置障

    service_.ReconcileLedgerResource(drifted);
    EXPECT_EQ(ledger.Get("space_drift_a")->state, UbseSsuNsState::FAULTED); // 计满置障

    // 组回归（漂移清零 + FAULTED回正CREATED）；再次漂移重新计满，单周期不置障
    service_.ReconcileLedgerResource(MakeIdentityDevList({nsLedger}));
    EXPECT_EQ(ledger.Get("space_drift_a")->state, UbseSsuNsState::CREATED);
    service_.ReconcileLedgerResource(drifted);
    EXPECT_EQ(ledger.Get("space_drift_a")->state, UbseSsuNsState::CREATED);
}

// UT28-2：2b NS缺失与回正——多设备空间单盘故障丢NS，防抖满置FAULTED；
// 设备回归补齐后回正CREATED（无attacher），且计数已清零（再次缺失需重新计满）
TEST_F(TestUbseSsuServiceImpRecover, NsMissingDebounceAndRecover)
{
    const std::string uuid = "33333333-3333-3333-3333-333333333333";
    auto ns1 = MakeNsWithIdentity("eid_miss_a_0001", 1, "space_miss_a", uuid, 2, 0x11);
    auto ns2 = MakeNsWithIdentity("eid_miss_a_0002", 2, "space_miss_a", uuid, 2, 0x22);
    UbseSsuDebtLedger::GetInstance().Put(
        "space_miss_a",
        std::make_shared<const UbseSsuLedgerEntry>(MakeIdentityEntry("space_miss_a", uuid, {ns1, ns2})));
    auto &ledger = UbseSsuDebtLedger::GetInstance();

    auto partial = MakeIdentityDevList({ns1});
    service_.ReconcileLedgerResource(partial);
    service_.ReconcileLedgerResource(partial);
    EXPECT_EQ(ledger.Get("space_miss_a")->state, UbseSsuNsState::CREATED); // 防抖不足
    service_.ReconcileLedgerResource(partial);
    EXPECT_EQ(ledger.Get("space_miss_a")->state, UbseSsuNsState::FAULTED); // 计满置障

    auto full = MakeIdentityDevList({ns1, ns2});
    service_.ReconcileLedgerResource(full);
    EXPECT_EQ(ledger.Get("space_miss_a")->state, UbseSsuNsState::CREATED); // 补齐回正
    service_.ReconcileLedgerResource(partial);
    EXPECT_EQ(ledger.Get("space_miss_a")->state, UbseSsuNsState::CREATED); // 计数已清零
}

// UT28-3：2b回正按attachNodes分支——attacher仍在挂载则回ATTACHED
TEST_F(TestUbseSsuServiceImpRecover, NsMissingRecoverToAttachedWhenAttacherHeld)
{
    const std::string uuid = "44444444-4444-4444-4444-444444444444";
    auto ns1 = MakeNsWithIdentity("eid_miss_b_0001", 1, "space_miss_b", uuid, 2, 0x11);
    auto ns2 = MakeNsWithIdentity("eid_miss_b_0002", 2, "space_miss_b", uuid, 2, 0x22);
    auto entry = MakeIdentityEntry("space_miss_b", uuid, {ns1, ns2});
    entry.state = UbseSsuNsState::FAULTED;
    entry.attachNodes.insert("node_attacher");
    UbseSsuDebtLedger::GetInstance().Put("space_miss_b", std::make_shared<const UbseSsuLedgerEntry>(entry));

    service_.ReconcileLedgerResource(MakeIdentityDevList({ns1, ns2}));
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_miss_b")->state, UbseSsuNsState::ATTACHED);
}

// UT28-4：孤儿组不写账本——3a同名不同uuid残留组（free残留后同名重分配）与3b真孤儿组
// （无同名条目）防抖计满后仅告警：属主条目不受影响，孤儿组不入账
TEST_F(TestUbseSsuServiceImpRecover, OrphanGroupsAlarmButLedgerUntouched)
{
    const std::string uuidHold = "55555555-5555-5555-5555-555555555555";
    const std::string uuidOrphan = "66666666-6666-6666-6666-666666666666";
    auto nsHold = MakeNsWithIdentity("eid_orphan_a_0001", 1, "space_orphan_a", uuidHold, 1, 0x11);
    UbseSsuDebtLedger::GetInstance().Put(
        "space_orphan_a",
        std::make_shared<const UbseSsuLedgerEntry>(MakeIdentityEntry("space_orphan_a", uuidHold, {nsHold})));

    auto devList = MakeIdentityDevList({nsHold,
                                        MakeNsWithIdentity("eid_orphan_a_0002", 1, "space_orphan_a", uuidOrphan, 1,
                                                           0x22),
                                        MakeNsWithIdentity("eid_orphan_b_0001", 1, "space_orphan_b", uuidOrphan, 1,
                                                           0x22)});
    for (int i = 0; i < SSU_FAULT_DEBOUNCE_CYCLES; ++i) {
        service_.ReconcileLedgerResource(devList);
    }
    auto &ledger = UbseSsuDebtLedger::GetInstance();
    // 3a：属主条目保持CREATED、uuid不被孤儿组影响（自身组完整不误判漂移/缺失）
    const auto *holder = ledger.Get("space_orphan_a").get();
    ASSERT_NE(holder, nullptr);
    EXPECT_EQ(holder->state, UbseSsuNsState::CREATED);
    EXPECT_EQ(holder->allocResult.resourceUuid, uuidHold);
    // 3b：真孤儿不入账
    EXPECT_EQ(ledger.Get("space_orphan_b"), nullptr);
}

// UT28-5：CREATING在途跳过——free残留后同名重分配的分配窗口期（账本CREATING、
// 硬件有旧uuid残留组），孤儿判定静默跳过，条目保持CREATING
TEST_F(TestUbseSsuServiceImpRecover, OrphanSkippedWhileCreatingInFlight)
{
    const std::string uuidNew = "77777777-7777-7777-7777-777777777777";
    const std::string uuidResidue = "88888888-8888-8888-8888-888888888888";
    auto nsNew = MakeNsWithIdentity("eid_orphan_c_0001", 1, "space_orphan_c", uuidNew, 1, 0x11);
    auto entry = MakeIdentityEntry("space_orphan_c", uuidNew, {nsNew});
    entry.state = UbseSsuNsState::CREATING;
    UbseSsuDebtLedger::GetInstance().Put("space_orphan_c", std::make_shared<const UbseSsuLedgerEntry>(entry));

    auto devList =
        MakeIdentityDevList({MakeNsWithIdentity("eid_orphan_c_0002", 1, "space_orphan_c", uuidResidue, 1, 0x22)});
    for (int i = 0; i < SSU_FAULT_DEBOUNCE_CYCLES + 1; ++i) {
        service_.ReconcileLedgerResource(devList);
    }
    EXPECT_EQ(UbseSsuDebtLedger::GetInstance().Get("space_orphan_c")->state, UbseSsuNsState::CREATING);
}

// UT28-6：name级让路与未就绪门禁——name整体消失时本对账不判身份（防抖不前进，
// 交给ReconcileSpaceFaultStates）；账本未就绪时no-op
TEST_F(TestUbseSsuServiceImpRecover, IdentityReconcileDefersNameLevelAndNotReady)
{
    const std::string uuid = "99999999-9999-9999-9999-999999999999";
    auto ns1 = MakeNsWithIdentity("eid_defer_a_0001", 1, "space_defer_a", uuid, 1, 0x11);
    UbseSsuDebtLedger::GetInstance().Put(
        "space_defer_a", std::make_shared<const UbseSsuLedgerEntry>(MakeIdentityEntry("space_defer_a", uuid, {ns1})));
    auto &ledger = UbseSsuDebtLedger::GetInstance();

    const std::vector<UbseSsuDevInfo> emptyList{};
    for (int i = 0; i < SSU_FAULT_DEBOUNCE_CYCLES + 1; ++i) {
        service_.ReconcileLedgerResource(emptyList);
    }
    EXPECT_EQ(ledger.Get("space_defer_a")->state, UbseSsuNsState::CREATED);

    ledger.SetReady(false);
    service_.ReconcileLedgerResource(MakeIdentityDevList({ns1}));
    ledger.SetReady(true);
    EXPECT_EQ(ledger.Get("space_defer_a")->state, UbseSsuNsState::CREATED);
}

// UT28-7：NODE_UP差异收编——硬件清单与账本无差异时跳过（已收敛的ATTACHED/attachNodes
// 不被覆盖、不进收敛窗口）；硬件新增资源（脑裂收编模拟）时增量收编新组入账
// （restoredFromHardware、state=CREATED），且既有条目挂载视图保留、收敛窗口重开
TEST_F(TestUbseSsuServiceImpRecover, DiffRebuildSkipsWhenInventoryUnchanged)
{
    const std::string uuidA = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
    auto ns1 = MakeNsWithIdentity("eid_diff_a_0001", 1, "space_diff_a", uuidA, 1, 0x11);
    AddNsToCollectorCache(ns1);
    service_.RebuildLedgerFromDevList();
    auto &ledger = UbseSsuDebtLedger::GetInstance();
    ASSERT_NE(ledger.Get("space_diff_a"), nullptr);

    // 模拟agent重报收敛后的挂载视图
    ledger.Modify("space_diff_a", uuidA, [](UbseSsuLedgerEntry &e) {
        e.state = UbseSsuNsState::ATTACHED;
        e.attachNodes.insert("node_a");
    });

    // 无差异：跳过收编，挂载视图不被覆盖，也不重开收敛窗口
    ledger.ResetRebuildTime();
    service_.AbsorbLedgerDifferences();
    auto after = ledger.Get("space_diff_a");
    ASSERT_NE(after, nullptr);
    EXPECT_EQ(after->state, UbseSsuNsState::ATTACHED);
    EXPECT_EQ(after->attachNodes.size(), 1u);
    EXPECT_FALSE(ledger.IsInReconcileWindow());

    // 硬件新增同名空间的另一uuid组（脑裂期间被收编方分配）→ 增量收编新组入账
    const std::string uuidB = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
    AddNsToCollectorCache(MakeNsWithIdentity("eid_diff_b_0001", 1, "space_diff_b", uuidB, 1, 0x22));
    service_.AbsorbLedgerDifferences();
    auto merged = ledger.Get("space_diff_b");
    ASSERT_NE(merged, nullptr);
    EXPECT_TRUE(merged->restoredFromHardware);
    EXPECT_EQ(merged->state, UbseSsuNsState::CREATED);
    EXPECT_EQ(merged->allocResult.resourceUuid, uuidB);
    // 既有条目挂载视图不被重置（不进全量Rebuild）+ 收编新条目重开收敛窗口
    auto kept = ledger.Get("space_diff_a");
    ASSERT_NE(kept, nullptr);
    EXPECT_EQ(kept->state, UbseSsuNsState::ATTACHED);
    EXPECT_EQ(kept->attachNodes.size(), 1u);
    EXPECT_TRUE(ledger.IsInReconcileWindow());
}

// UT28-8：回正权责分离——身份级FAULTED（uuid漂移）期间name在硬件在线集合中属预期，
// 同周期串行的name级对账（ReconcileSpaceFaultStates先执行）不得回正身份级FAULTED，
// 否则两对账对同一FAULTED周期性置障/回正振荡（告警刷屏、FAULTED拦截保护间歇失效）；
// 身份故障恢复（组匹配清计数）后回正路径恢复正常
TEST_F(TestUbseSsuServiceImpRecover, NameLevelReconcileKeepsIdentityFaulted)
{
    const std::string uuidA = "cccccccc-cccc-cccc-cccc-cccccccccccc";
    auto nsLedger = MakeNsWithIdentity("eid_idt_0001", 1, "space_idt", uuidA, 1, 0x33);
    AddNsToCollectorCache(nsLedger);
    service_.RebuildLedgerFromDevList();
    auto &ledger = UbseSsuDebtLedger::GetInstance();
    ASSERT_NE(ledger.Get("space_idt"), nullptr);

    // 硬件切换为同名不同uuid组（漂移），防抖计满置FAULTED
    const std::string uuidB = "dddddddd-dddd-dddd-dddd-dddddddddddd";
    auto drifted = MakeIdentityDevList({MakeNsWithIdentity("eid_idt_0002", 1, "space_idt", uuidB, 1, 0x34)});
    service_.ReconcileLedgerResource(drifted);
    service_.ReconcileLedgerResource(drifted);
    service_.ReconcileLedgerResource(drifted);
    ASSERT_EQ(ledger.Get("space_idt")->state, UbseSsuNsState::FAULTED);

    // 同周期串行（与采集回调顺序一致：name级先、身份级后）：FAULTED不被name级回正打断
    service_.ReconcileSpaceFaultStates(drifted);
    service_.ReconcileLedgerResource(drifted);
    EXPECT_EQ(ledger.Get("space_idt")->state, UbseSsuNsState::FAULTED);

    // 身份故障恢复（换回原uuid组）：身份对账清计数并回正，随后name级对账不产生异常状态
    auto restored = MakeIdentityDevList({nsLedger});
    service_.ReconcileSpaceFaultStates(restored);
    service_.ReconcileLedgerResource(restored);
    EXPECT_EQ(ledger.Get("space_idt")->state, UbseSsuNsState::CREATED);
    service_.ReconcileSpaceFaultStates(restored);
    EXPECT_EQ(ledger.Get("space_idt")->state, UbseSsuNsState::CREATED);
}

} // namespace ubse::ssu::service::ut
