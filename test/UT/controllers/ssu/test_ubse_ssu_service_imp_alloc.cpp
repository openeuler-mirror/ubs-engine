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

#include <dlfcn.h>
#include <atomic>
#include <cstring>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/ubse_conf.h"
#include "test_ubse_ssu_service_imp_alloc.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;

// ExecuteScheduler 强制"分配总大小必须为1GiB整数倍"（SSU服务API容量粒度契约）
constexpr uint64_t ONE_GIB = 1024ULL * 1024ULL * 1024ULL;

// ============================================================================
// TestUbseSsuServiceImpAlloc implementation
// ============================================================================

void TestUbseSsuServiceImpAlloc::SetUp()
{
    UbseSsuServiceImpTestBase::SetUp();

    // Populate adapter function pointers directly (bypasses DlOpenLib)
    SetupAdapterFuncs();

    // Mock UbseGetStr to return a fake admin NQN
    MOCKER_CPP(&ubse::config::UbseGetStr)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any())
        .will(invoke(MockUbseGetStr));

    g_mockNextNsId.store(1);
    ResetDeviceNsCounters();
}

void TestUbseSsuServiceImpAlloc::TearDown()
{
    // Reset dlManager handle_ to avoid ~UbseDlManager calling dlclose(0x1) and causing segfault
    UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr;
    UbseSsuServiceImpTestBase::TearDown();
}

void TestUbseSsuServiceImpAlloc::SetupAllocMocks()
{
    MOCKER_CPP(&GenerateHostNqn)
        .stubs()
        .will(returnValue(std::string("nqn.2024-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab")));
}

const std::string TestUbseSsuServiceImpAlloc::kTestHostNqn =
    "nqn.2024-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab";

// --------------------------------------------------------------------------
// AllocSpace - Role dispatch tests
// --------------------------------------------------------------------------

/*
 * 测试用例：AllocSpace 在 Master 角色时直接调用 ExecuteAlloc，不走 RPC。
 * 测试步骤：
 * 1、mock UbseGetRole 返回 Master（默认已设置）
 * 2、准备 collector 缓存（eid 需 16 字节）
 * 3、mock GenerateHostNqn
 * 4、调用 AllocSpace
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、账本 CREATED 条目已创建
 * 3、result 包含分配的命名空间信息
 */
TEST_F(TestUbseSsuServiceImpAlloc, AllocSpace_Master_Success)
{
    std::vector<UbseSsuDevInfo> devices = {
        MakeDev(std::string(16, 'A'), "nqn.test.1"),
        MakeDev(std::string(16, 'B'), "nqn.test.2"),
    };
    PopulateCollectorCache(devices);
    SetupAllocMocks();

    auto req = MakeAllocReq("alloc_master_direct", 2 * ONE_GIB, 2);
    auto identity = MakeIdentity();
    UbseSsuAllocResult result;

    auto ret = service_.AllocSpace(req, identity, result);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(result.name, "alloc_master_direct");
    EXPECT_EQ(result.nameSpaceList.size(), 2u);
    EXPECT_TRUE(LedgerEntryExists("alloc_master_direct"));

    auto entry = UbseSsuDebtLedger::GetInstance().Get("alloc_master_direct");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
    EXPECT_EQ(entry->allocResult.nameSpaceList.size(), 2u);
}

/*
 * 测试用例：AllocSpace 在 Agent 角色时走 RPC 路径。
 * 测试步骤：
 * 1、mock UbseGetRole 返回 Agent
 * 2、mock UbseGetMasterInfo 返回失败
 * 3、调用 AllocSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR（RPC 路径早期返回）
 * 2、账本无条目
 */
TEST_F(TestUbseSsuServiceImpAlloc, AllocSpace_AgentRpcPath)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("alloc_agent_rpc"), MakeIdentity(), result), UBSE_ERROR);
    EXPECT_FALSE(LedgerEntryExists("alloc_agent_rpc"));
}

/*
 * 测试用例：AllocSpace 在 Standby 角色时走 RPC 路径（同 Agent）。
 * 测试步骤：
 * 1、mock UbseGetRole 返回 Standby
 * 2、mock UbseGetMasterInfo 返回失败
 * 3、调用 AllocSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuServiceImpAlloc, AllocSpace_StandbyViaRpc)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Standby));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("alloc_standby_rpc"), MakeIdentity(), result), UBSE_ERROR);
}

/*
 * 测试用例：AllocSpace 在角色为非 master/agent/standby 时返回错误。
 * 测试步骤：
 * 1、mock UbseGetRole 返回 "unknown_role"
 * 2、调用 AllocSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuServiceImpAlloc, AllocSpace_UnsupportedRole)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Unsupported));
    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("alloc_unsupported"), MakeIdentity(), result),
              UBSE_SSU_ERROR_ROLE_INVALID);
}

/*
 * 测试用例：AllocSpace 在 UbseGetRole 失败时直接返回错误。
 * 测试步骤：
 * 1、mock UbseGetRole 返回非零错误码
 * 2、调用 AllocSpace
 * 预期结果：
 * 1、返回非零错误码
 */
TEST_F(TestUbseSsuServiceImpAlloc, AllocSpace_GetRoleFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(returnValue(UBSE_ERROR));
    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("alloc_role_failed"), MakeIdentity(), result), UBSE_ERROR);
}

// --------------------------------------------------------------------------
// ExecuteAlloc tests
// --------------------------------------------------------------------------

/*
 * 测试用例：ExecuteAlloc 正常流程：scheduler 成功 -> CreateDevNameSpace 成功 -> ledger Put(CREATED)。
 * 测试步骤：
 * 1、准备 collector 缓存（2 个设备，eid 16 字节）
 * 2、mock GenerateHostNqn
 * 3、调用 AllocSpace
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、账本 CREATED，包含 2 个命名空间
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteAlloc_Normal)
{
    std::vector<UbseSsuDevInfo> devices = {
        MakeDev(std::string(16, 'A'), "nqn.test.1"),
        MakeDev(std::string(16, 'B'), "nqn.test.2"),
    };
    PopulateCollectorCache(devices);
    SetupAllocMocks();

    auto req = MakeAllocReq("exec_alloc_normal", 2 * ONE_GIB, 2);
    auto identity = MakeIdentity();
    UbseSsuAllocResult result;

    auto ret = service_.AllocSpace(req, identity, result);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(result.nameSpaceList.size(), 2u);

    auto entry = UbseSsuDebtLedger::GetInstance().Get("exec_alloc_normal");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
    EXPECT_EQ(entry->allocResult.nameSpaceList.size(), 2u);
    for (const auto& ns : entry->allocResult.nameSpaceList) {
        EXPECT_FALSE(ns.tgtEid.empty());
        EXPECT_FALSE(ns.tgtNqn.empty());
        EXPECT_FALSE(ns.nsUuid.empty());
        EXPECT_GT(ns.namespaceId, 0u);
        EXPECT_FALSE(ns.nsDevPath.empty());
    }
}

/*
 * 测试用例：ExecuteAlloc 条带化分配成功（nsSize 可整除 nsNum 和 lbaSize）。
 * 测试步骤：
 * 1、准备 collector 缓存（3 个设备，eid 16 字节）
 * 2、mock GenerateHostNqn
 * 3、调用 AllocSpace（STRIPED, nsSize=12288, nsNum=3, lbaFormat=512）
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、3 个命名空间各 4096 字节
 * 3、账本 CREATED
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteAlloc_Striped_Success)
{
    PopulateCollectorCache({
        MakeDev(std::string(16, 'A'), "nqn.test.1"),
        MakeDev(std::string(16, 'B'), "nqn.test.2"),
        MakeDev(std::string(16, 'C'), "nqn.test.3"),
    });
    SetupAllocMocks();

    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("exec_alloc_striped", 3 * ONE_GIB, 3, UbseSsuLBAFormat::LBA_FORMAT_512,
                                               UbseSsuAllocStrategy::STRIPED),
                                  MakeIdentity(), result),
              UBSE_OK);
    EXPECT_EQ(result.nameSpaceList.size(), 3u);
    for (const auto& ns : result.nameSpaceList) {
        EXPECT_EQ(ns.nsSize, ONE_GIB);
    }

    auto entry = UbseSsuDebtLedger::GetInstance().Get("exec_alloc_striped");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 测试用例：ExecuteAlloc 单设备单 NS 分配（LINEAR 策略）。
 * 测试步骤：
 * 1、准备 collector 缓存（1 个设备，eid 16 字节）
 * 2、mock GenerateHostNqn
 * 3、调用 AllocSpace（nsNum=1）
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、1 个命名空间
 * 3、账本 CREATED
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteAlloc_SingleNs)
{
    PopulateCollectorCache({MakeDev(std::string(16, 'A'), "nqn.test.1")});
    SetupAllocMocks();

    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("exec_alloc_single", ONE_GIB, 1, UbseSsuLBAFormat::LBA_FORMAT_512,
                                               UbseSsuAllocStrategy::LINEAR),
                                  MakeIdentity(), result),
              UBSE_OK);
    EXPECT_EQ(result.nameSpaceList.size(), 1u);

    auto entry = UbseSsuDebtLedger::GetInstance().Get("exec_alloc_single");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 测试用例：ExecuteAlloc NORMAL 策略单设备多 NS 分配成功（LINEAR/STRIPED 会因设备数不足拒绝）。
 * 测试步骤：
 * 1、准备 collector 缓存（1 个设备，eid 16 字节）
 * 2、mock GenerateHostNqn
 * 3、调用 AllocSpace（NORMAL, nsNum=2, 1 设备）
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、2 个命名空间（同设备）
 * 3、账本 CREATED
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteAlloc_NormalMultiNsSingleDev)
{
    PopulateCollectorCache({MakeDev(std::string(16, 'A'), "nqn.test.1")});
    SetupAllocMocks();

    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("exec_alloc_normal_multi", ONE_GIB, 2, UbseSsuLBAFormat::LBA_FORMAT_512,
                                               UbseSsuAllocStrategy::NORMAL),
                                  MakeIdentity(), result),
              UBSE_OK);
    EXPECT_EQ(result.nameSpaceList.size(), 2u);

    auto entry = UbseSsuDebtLedger::GetInstance().Get("exec_alloc_normal_multi");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 测试用例：ExecuteAlloc 存在 CREATED 条目时幂等返回。
 * 测试步骤：
 * 1、预置账本 CREATED 条目
 * 2、调用 AllocSpace（同名 name）
 * 预期结果：
 * 1、返回 UBSE_OK（不重新创建）
 * 2、账本条目不变
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteAlloc_DuplicateCreated)
{
    UbseSsuAllocResult existingResult;
    existingResult.name = "exec_alloc_dup";
    existingResult.strategy = UbseSsuAllocStrategy::LINEAR;
    PutLedgerEntry("exec_alloc_dup", UbseSsuNsState::CREATED, existingResult);
    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("exec_alloc_dup"), MakeIdentity(), result), UBSE_ERR_ALREADY_ALLOCATED);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("exec_alloc_dup");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 测试用例：ExecuteAlloc 当 scheduler 失败时回滚预留并返回错误。
 * 测试步骤：
 * 1、准备空 collector 缓存（导致 scheduler 失败）
 * 2、调用 AllocSpace
 * 预期结果：
 * 1、返回错误码
 * 2、账本无条目
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteAlloc_SchedulerFailed)
{
    PopulateCollectorCache({});
    UbseSsuAllocResult result;
    EXPECT_NE(service_.AllocSpace(MakeAllocReq("exec_alloc_sched_fail"), MakeIdentity(), result), UBSE_OK);
    EXPECT_FALSE(LedgerEntryExists("exec_alloc_sched_fail"));
}

/*
 * 测试用例：ExecuteAlloc 当 ledger 已有同名非 CREATING 条目（如 ATTACHED）时，拒绝重复分配。
 * 测试步骤：
 * 1、预置 ATTACHED 条目（空间已挂载，name 已被占用）
 * 2、调用 AllocSpace（同名 name）
 * 预期结果：
 * 1、返回 UBSE_ERR_ALREADY_ALLOCATED（重复分配一律拒绝）
 * 2、账本原有 ATTACHED 条目保留（不覆盖）
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteAlloc_LedgerPutNonCreatedEntry)
{
    PutLedgerEntry("exec_alloc_existing", UbseSsuNsState::ATTACHED);

    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("exec_alloc_existing", ONE_GIB, 1), MakeIdentity(), result),
              UBSE_ERR_ALREADY_ALLOCATED);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("exec_alloc_existing");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

// --------------------------------------------------------------------------
// ExecuteScheduler - Parameter validation tests
// --------------------------------------------------------------------------

/*
 * 测试用例：ExecuteScheduler 条带化策略时，size % nsNum != 0 时拒绝。
 * 测试步骤：
 * 1、准备 collector 缓存（3 个设备）
 * 2、请求 strategy=STRIPED, nsSize=100, nsNum=3（100%3 != 0）
 * 3、调用 AllocSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR
 * 2、账本无条目
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteScheduler_StripedAlloc_ParamInvalid)
{
    PopulateCollectorCache({MakeDev(std::string(16, 'A'), "nqn.test.1"), MakeDev(std::string(16, 'B'), "nqn.test.2"),
                            MakeDev(std::string(16, 'C'), "nqn.test.3")});
    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("striped_param_invalid", 2 * ONE_GIB, 3,
                                               UbseSsuLBAFormat::LBA_FORMAT_512, UbseSsuAllocStrategy::STRIPED),
                                  MakeIdentity(), result),
              UBSE_ERR_INVALID_ARG);
    EXPECT_FALSE(LedgerEntryExists("striped_param_invalid"));
}

/*
 * 测试用例：ExecuteScheduler 条带化且单 NS 大小不是 lbaSize 整数倍时拒绝。
 * 测试步骤：
 * 1、准备 collector 缓存（2 个设备）
 * 2、请求 strategy=STRIPED, nsSize=100, nsNum=2（100%2=0, singleNsSize=50, 50%512 != 0）
 * 3、调用 AllocSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR
 * 2、账本无条目
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteScheduler_Striped_SizeNotDivisibleByLba)
{
    PopulateCollectorCache({MakeDev(std::string(16, 'A'), "nqn.test.1"), MakeDev(std::string(16, 'B'), "nqn.test.2")});
    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("striped_lba_invalid", 100, 2, UbseSsuLBAFormat::LBA_FORMAT_512,
                                               UbseSsuAllocStrategy::STRIPED),
                                  MakeIdentity(), result),
              UBSE_ERR_INVALID_ARG);
    EXPECT_FALSE(LedgerEntryExists("striped_lba_invalid"));
}

/*
 * 测试用例：ExecuteScheduler 设备列表为空时拒绝。
 * 测试步骤：
 * 1、空 collector 缓存
 * 2、调用 AllocSpace
 * 预期结果：
 * 1、返回错误码
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteScheduler_NoDevices)
{
    PopulateCollectorCache({});
    UbseSsuAllocResult result;
    EXPECT_NE(service_.AllocSpace(MakeAllocReq("sched_no_devices"), MakeIdentity(), result), UBSE_OK);
}

/*
 * 测试用例：ExecuteScheduler nsNum 为 0 时拒绝。
 * 测试步骤：
 * 1、准备 collector 缓存（1 个设备）
 * 2、请求 nsNum=0
 * 3、调用 AllocSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR
 * 2、账本无条目
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteScheduler_NsNumZero)
{
    PopulateCollectorCache({MakeDev(std::string(16, 'A'), "nqn.test.1")});
    UbseSsuAllocResult result;
    EXPECT_EQ(service_.AllocSpace(MakeAllocReq("sched_nsnum_zero", ONE_GIB, 0), MakeIdentity(), result),
              UBSE_ERR_INVALID_ARG);
    EXPECT_FALSE(LedgerEntryExists("sched_nsnum_zero"));
}

// --------------------------------------------------------------------------
// Concurrency test
// --------------------------------------------------------------------------

/*
 * 测试用例：ExecuteScheduler 并发安全验证（mutex 保护）。
 * 测试步骤：
 * 1、准备 collector 缓存（2 个设备）
 * 2、两个线程同时调用 AllocSpace（不同 name）
 * 3、等待两个线程完成
 * 预期结果：
 * 1、两个线程均正常返回（不崩溃、不死锁）
 */
TEST_F(TestUbseSsuServiceImpAlloc, ExecuteScheduler_LockHeld)
{
    std::vector<UbseSsuDevInfo> devices = {
        MakeDev(std::string(16, 'A'), "nqn.test.1"),
        MakeDev(std::string(16, 'B'), "nqn.test.2"),
    };
    PopulateCollectorCache(devices);
    SetupAllocMocks();

    UbseSsuAllocResult result1, result2;
    auto future1 = std::async(std::launch::async, [this, &result1]() {
        return service_.AllocSpace(MakeAllocReq("lock_held_req1", ONE_GIB, 1), MakeIdentity(), result1);
    });
    auto future2 = std::async(std::launch::async, [this, &result2]() {
        return service_.AllocSpace(MakeAllocReq("lock_held_req2", ONE_GIB, 1), MakeIdentity(), result2);
    });

    EXPECT_EQ(future1.get(), UBSE_OK);
    EXPECT_EQ(future2.get(), UBSE_OK);
    EXPECT_TRUE(LedgerEntryExists("lock_held_req1"));
    EXPECT_TRUE(LedgerEntryExists("lock_held_req2"));
}

} // namespace ubse::ssu::service::ut
