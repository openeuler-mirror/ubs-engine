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

#include <atomic>
#include <cstring>
#include <future>
#include <string>
#include <vector>

#include "config/ubse_conf.h"
#include "message/ubse_ssu_free_msg.h"
#include "test_ubse_ssu_service_imp_free.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;

// ============================================================================
// Mock adapter C functions for free tests
// ============================================================================
// We override specific function pointers after calling the common
// SetupAdapterFuncs() to get controllable behavior (e.g. partial failure).

namespace {

// --- Controllable AcquireDevInfo: returns namespace data matching the cache ---
static std::vector<DevNamespaceInfoT> g_freeMockNsList;

static DevNamespaceInfoT BuildMockNsInfo(const UbseSsuDevNameSpace& ns)
{
    DevNamespaceInfoT nsInfo{};
    nsInfo.namespaceId = ns.namespaceId;
    std::memcpy(nsInfo.guid, ns.guid.c_str(), std::min(ns.guid.size(), static_cast<size_t>(GUID_SIZE)));
    std::memcpy(nsInfo.uuid, ns.uuid.c_str(), std::min(ns.uuid.size(), static_cast<size_t>(UUID_SIZE)));
    std::strncpy(nsInfo.devPath, ns.nsDevPath.c_str(), DEV_PATH_SIZE - 1);
    std::memcpy(nsInfo.devAddr.tgtEid.raw, ns.subSystem.eid.c_str(),
                std::min(ns.subSystem.eid.size(), static_cast<size_t>(EID_SIZE)));
    std::strncpy(nsInfo.devAddr.subNqn, ns.subSystem.subNqn.c_str(), SUBNQN_SIZE - 1);
    nsInfo.baseAttr.nsze = ns.nsze;
    nsInfo.baseAttr.ncap = ns.ncap;
    return nsInfo;
}

static int FreeMockAcquireDevInfo(const char* adminNqn, const DevAddrT* devList, int devCnt, DevInfoT* devInfoList)
{
    (void)adminNqn;
    for (int i = 0; i < devCnt; ++i) {
        auto& dev = devInfoList[i];
        std::memset(&dev, 0, sizeof(dev));
        dev.state = DevStatusT::DEV_ONLINE;
        dev.nsCount = static_cast<uint32_t>(g_freeMockNsList.size());
        dev.tnvmcap = 1099511627776ULL;
        dev.unvmcap = 1099511627776ULL;
        dev.cntlId = 1;
        std::strncpy(dev.devPath, "/dev/nvme0", sizeof(dev.devPath) - 1);
        std::strncpy(dev.sn, "MOCKSN00001", sizeof(dev.sn) - 1);
        std::strncpy(dev.mn, "MockSSU", sizeof(dev.mn) - 1);
        std::memcpy(dev.devAddr.tgtEid.raw, devList[i].tgtEid.raw, EID_SIZE);
        std::strncpy(dev.devAddr.subNqn, devList[i].subNqn, SUBNQN_SIZE - 1);
        for (size_t j = 0; j < g_freeMockNsList.size() && j < MAX_NAMESPACES_PER_CTRL; ++j) {
            dev.namespaces[j] = g_freeMockNsList[j];
        }
    }
    return 0;
}

// --- Controllable DeleteNamespace ---
static std::atomic<int> g_freeDeleteFailAfter{-1}; // -1 = never fail
static std::atomic<int> g_freeDeleteCallCount{0};

static int FreeMockDeleteNamespace(const char* adminNqn, DevNamespaceInfoT* nsInfo)
{
    (void)adminNqn;
    (void)nsInfo;
    int call = g_freeDeleteCallCount.fetch_add(1);
    int failAfter = g_freeDeleteFailAfter.load();
    return (failAfter >= 0 && call >= failAfter) ? -1 : 0;
}

// --- Helper: reset controllable mock state ---
static void ResetFreeMockState()
{
    g_freeMockNsList.clear();
    g_freeDeleteCallCount.store(0);
    g_freeDeleteFailAfter.store(-1);
}

// --- Helper: set up adapter with controllable mocks for free test ---
static void SetupFreeAdapterFuncs()
{
    SetupAdapterFuncs();
    auto& impl = UbseSsuAdapterImpl::GetInstance();
    impl.acquireDevInfo_ = FreeMockAcquireDevInfo;
    impl.deleteNamespace_ = FreeMockDeleteNamespace;
}

// --- Helper: add a namespace to both collector cache and mock adapter ---
static void AddNsToBothCaches(const UbseSsuDevNameSpace& ns)
{
    UbseSsuServiceImpTestBase::AddNsToCollectorCache(ns);
    g_freeMockNsList.push_back(BuildMockNsInfo(ns));
}

// --- Helper: create a ledger entry for free test ---
static UbseSsuAllocResult MakeAllocResultForFree(const std::string& name,
                                                 const std::vector<UbseSsuDevNameSpace>& nsList, uint64_t nsSize = 4096)
{
    UbseSsuAllocResult result;
    result.name = name;
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    for (const auto& ns : nsList) {
        result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns, nsSize));
    }
    return result;
}

// --- Mock UbseRpcSend for free RPC success path ---
// 模拟 master 端 FreeRespReceiver：收到请求后通过 UbseFutureMgr::SetResult 回填
// FreeSpaceViaRpc 等待的 future，再返回同步应答（errorCode=UBSE_OK）。
static uint32_t MockFreeRpcSend(UbseRpcEndpoint* self, const std::string& targetNodeId, const UbseRpcMessage& req,
                                UbseRpcMessage& resp)
{
    (void)self;
    (void)targetNodeId;
    const auto* freeReq = dynamic_cast<const UbseSsuFreeReqMsg*>(&req);
    if (freeReq == nullptr) {
        return UBSE_ERROR;
    }

    UbseSsuFreeResp freeResp;
    freeResp.requestId = freeReq->GetSsuFreeRequest().requestId;
    freeResp.errorCode = UBSE_OK;
    UbseFutureMgr::SetResult(freeResp.requestId, freeResp);

    UbseSsuSyncRespMsg syncResp(UBSE_OK);
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t bufferSize = 0;
    if (syncResp.Serialize(buffer, bufferSize) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return resp.Deserialize(buffer.get(), bufferSize);
}

} // anonymous namespace

void TestUbseSsuServiceImpFree::SetUp()
{
    UbseSsuServiceImpTestBase::SetUp();
    ResetFreeMockState();
    SetupFreeAdapterFuncs();

    // Mock UbseGetStr to return a fake admin NQN (needed by adapter's GetAdminNqn)
    MOCKER_CPP(&ubse::config::UbseGetStr)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any())
        .will(invoke(MockUbseGetStr));
}

void TestUbseSsuServiceImpFree::TearDown()
{
    // Reset dlManager handle_ to avoid dlclose(0x1) segfault
    UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr;
    UbseSsuServiceImpTestBase::TearDown();
}

void TestUbseSsuServiceImpFree::SetupFreeEnv(const std::vector<UbseSsuDevNameSpace>& nsList, UbseSsuNsState state,
                                             const std::string& name)
{
    for (const auto& ns : nsList) {
        AddNsToBothCaches(ns);
    }
    auto result = MakeAllocResultForFree(name, nsList);
    PutLedgerEntry(name, state, result);
}

/*
 * 用例描述：
 * FreeSpace 在 Master 角色时直接调用 ExecuteFree。
 * 测试步骤：
 * 1、默认 UbseGetRole 返回 Master（SetUp 默认行为）
 * 2、预置账本 CREATED 条目
 * 3、准备 collector 缓存（2 个设备各一个 NS）
 * 4、mock 适配器（adapter）
 * 5、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、账本条目已移除
 */
TEST_F(TestUbseSsuServiceImpFree, FreeSpace_Master_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    SetupFreeEnv({ns1, ns2}, UbseSsuNsState::CREATED, "free_master_direct");

    auto ret = service_.FreeSpace("free_master_direct", MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(LedgerEntryExists("free_master_direct"));
}

/*
 * 用例描述：
 * FreeSpace 在 Agent 角色时走 RPC 路径，mock UbseGetMasterInfo 失败使 RPC 提前返回。
 * 测试步骤：
 * 1、mock UbseGetRole 返回 Agent
 * 2、mock UbseGetMasterInfo 返回失败
 * 3、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR（RPC 路径早期返回）
 */
TEST_F(TestUbseSsuServiceImpFree, FreeSpace_AgentViaRpc)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    auto ret = service_.FreeSpace("free_agent_rpc", MakeIdentity());
    EXPECT_EQ(ret, UBSE_ERROR);
}

/*
 * 用例描述：
 * FreeSpace 在 Standby 角色时走 RPC 路径。
 * 测试步骤：
 * 1、mock UbseGetRole 返回 Standby
 * 2、mock UbseGetMasterInfo 返回失败
 * 3、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuServiceImpFree, FreeSpace_StandbyViaRpc)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Standby));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(service_.FreeSpace("free_standby_rpc", MakeIdentity()), UBSE_ERROR);
}

/*
 * 用例描述：
 * FreeSpace 在 Standby 角色时走 RPC 路径成功释放，本地账本保留。
 * commit 6b363f0c 移除了 FreeSpaceViaRpc 中的账本删除逻辑：standby 本地账本
 * 不再被释放动作删除（由 master 端 ExecuteFree 统一删除）。
 * 测试步骤：
 * 1、mock UbseGetRole 返回 Standby
 * 2、预置账本 CREATED 条目
 * 3、mock RPC 成功路径（UbseRpcSend 同步回填 future 并返回 UBSE_OK）
 * 4、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、本地账本条目保留
 */
TEST_F(TestUbseSsuServiceImpFree, FreeSpace_StandbyViaRpc_Success_KeepLedger)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Standby));
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).reset();
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    MOCKER_CPP(&UbseRpcEndpoint::UbseRpcSend).reset();
    MOCKER_CPP(&UbseRpcEndpoint::UbseRpcSend).stubs().will(invoke(MockFreeRpcSend));

    auto entry = std::make_shared<UbseSsuLedgerEntry>();
    entry->name = "free_standby_success";
    entry->state = UbseSsuNsState::CREATED;
    UbseSsuDebtLedger::GetInstance().Put(entry->name, entry);

    auto ret = service_.FreeSpace("free_standby_success", MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    // 账本删除由 master 端 ExecuteFree 负责，standby 本地账本应保留
    EXPECT_TRUE(LedgerEntryExists("free_standby_success"));
}

/*
 * 用例描述：
 * FreeSpace 在不存在账本条目时幂等返回成功。
 * 测试步骤：
 * 1、不预置账本条目
 * 2、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_OK（幂等）
 */
TEST_F(TestUbseSsuServiceImpFree, FreeSpace_NotExist)
{
    EXPECT_EQ(service_.FreeSpace("free_not_exist", MakeIdentity()), UBSE_ERR_NO_NEED_FREE);
}

/*
 * 用例描述：
 * FreeSpace 在角色为 "unknown_role" 时返回错误。
 * 注意：需预置账本条目，否则代码会在 role 判断前因幂等性直接返回 OK。
 * 测试步骤：
 * 1、mock UbseGetRole 返回 "unknown_role"
 * 2、预置 CRATED 账本条目
 * 3、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR
 */
TEST_F(TestUbseSsuServiceImpFree, FreeSpace_UnsupportedRole)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Unsupported));
    // Pre-populate ledger entry so the code passes the idempotent early-return
    auto entry = std::make_shared<UbseSsuLedgerEntry>();
    entry->name = "free_unsupported";
    entry->state = UbseSsuNsState::CREATED;
    UbseSsuDebtLedger::GetInstance().Put("free_unsupported", entry);

    EXPECT_EQ(service_.FreeSpace("free_unsupported", MakeIdentity()), UBSE_SSU_ERROR_ROLE_INVALID);
}

/*
 * 用例描述：
 * FreeSpace 在 UbseGetRole 失败时直接返回错误。
 * 测试步骤：
 * 1、mock UbseGetRole 返回非零错误码
 * 2、调用 FreeSpace
 * 预期结果：
 * 1、返回非零错误码
 */
TEST_F(TestUbseSsuServiceImpFree, FreeSpace_GetRoleFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(service_.FreeSpace("free_role_failed", MakeIdentity()), UBSE_ERROR);
}

// --------------------------------------------------------------------------
// ExecuteFree tests
// --------------------------------------------------------------------------

/*
 * 用例描述：
 * ExecuteFree 正常流程：所有 NS 删除成功 → Remove ledger。
 * 测试步骤：
 * 1、预置 CREATED 条目（2 个 NS，分属 2 设备）
 * 2、缓存中有匹配的 NS
 * 3、mock 适配器成功删除
 * 4、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、账本已移除
 */
TEST_F(TestUbseSsuServiceImpFree, ExecuteFree_Normal)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    SetupFreeEnv({ns1, ns2}, UbseSsuNsState::CREATED, "exec_free_normal");

    auto ret = service_.FreeSpace("exec_free_normal", MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(LedgerEntryExists("exec_free_normal"));
}

/*
 * 用例描述：
 * ExecuteFree 当 ledger 状态不为 CREATED 时拒绝释放。
 * 测试步骤：
 * 1、预置 ATTACHED 条目
 * 2、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_ERROR
 * 2、账本条目保留
 */
TEST_F(TestUbseSsuServiceImpFree, ExecuteFree_StateNotCreated)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    SetupFreeEnv({ns1}, UbseSsuNsState::ATTACHED, "exec_free_state_invalid");

    auto ret = service_.FreeSpace("exec_free_state_invalid", MakeIdentity());

    EXPECT_EQ(ret, UBSE_SSU_ERROR_NEED_DETACH_BEFORE_FREE);
    EXPECT_TRUE(LedgerEntryExists("exec_free_state_invalid"));
}

/*
 * 用例描述：
 * ExecuteFree 当 NS 已不在缓存中时，视为已释放，跳过并移除账本。
 * 测试步骤：
 * 1、预置 CREATED 条目（1 个 NS）
 * 2、缓存中没有该 NS（不调用 AddNsToBothCaches）
 * 3、只配置 adapter mock 状态但不添加 NS 到缓存
 * 4、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、账本已移除（幂等）
 */
TEST_F(TestUbseSsuServiceImpFree, ExecuteFree_NsAlreadyDeleted)
{
    // Ledger entry with NS info, but no matching entry in collector cache
    UbseSsuAllocResult result;
    result.name = "exec_free_ns_missing";
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSsuNameSpaceInfo nsInfo;
    nsInfo.tgtEid = std::string(16, 'A');
    nsInfo.tgtNqn = "nqn.test.1";
    nsInfo.namespaceId = 1;
    nsInfo.nsSize = 4096;
    result.nameSpaceList.push_back(nsInfo);
    PutLedgerEntry("exec_free_ns_missing", UbseSsuNsState::CREATED, result);

    auto ret = service_.FreeSpace("exec_free_ns_missing", MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(LedgerEntryExists("exec_free_ns_missing"));
}

/*
 * 用例描述：
 * ExecuteFree 所有 NS 都不在缓存中时，全部跳过并移除账本。
 * 测试步骤：
 * 1、预置 CREATED 条目（2 个 NS，都不在缓存中）
 * 2、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_OK
 * 2、账本已移除
 */
TEST_F(TestUbseSsuServiceImpFree, ExecuteFree_AllNsAlreadyDeleted)
{
    UbseSsuAllocResult result;
    result.name = "exec_free_all_missing";
    result.strategy = UbseSsuAllocStrategy::LINEAR;

    UbseSsuNameSpaceInfo ns1, ns2;
    ns1.tgtEid = std::string(16, 'A');
    ns1.tgtNqn = "nqn.test.1";
    ns1.namespaceId = 1;
    ns1.nsSize = 4096;
    ns2.tgtEid = std::string(16, 'B');
    ns2.tgtNqn = "nqn.test.2";
    ns2.namespaceId = 2;
    ns2.nsSize = 4096;
    result.nameSpaceList.push_back(ns1);
    result.nameSpaceList.push_back(ns2);
    PutLedgerEntry("exec_free_all_missing", UbseSsuNsState::CREATED, result);

    auto ret = service_.FreeSpace("exec_free_all_missing", MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(LedgerEntryExists("exec_free_all_missing"));
}

/*
 * 用例描述：
 * ExecuteFree 当 NS 的身份信息（uid）不匹配时返回 UBSE_ERR_ACCESS_DENIED。
 * 测试步骤：
 * 1、预置 CREATED 条目（1 个 NS，uid=100, userName=test_user）
 * 2、缓存中有该 NS（customData.uid=200 ≠ 100）
 * 3、调用 FreeSpace
 * 预期结果：
 * 1、返回 UBSE_ERR_ACCESS_DENIED
 * 2、账本条目保留
 */
TEST_F(TestUbseSsuServiceImpFree, ExecuteFree_IdentityNotMatch)
{
    // NS in cache has uid=200
    auto ns = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096, std::string(GUID_SIZE, '\xAB'),
                             std::string(UUID_SIZE, '\xAB'), 200, "other_user");
    AddNsToBothCaches(ns);

    UbseSsuAllocResult result = MakeAllocResultForFree("exec_free_id_mismatch", {ns});
    PutLedgerEntry("exec_free_id_mismatch", UbseSsuNsState::CREATED, result);

    // identity has uid=100 (default from MakeIdentity)
    auto ret = service_.FreeSpace("exec_free_id_mismatch", MakeIdentity(/*userName=*/"test_user", /*uid=*/100));

    EXPECT_EQ(ret, UBSE_ERR_ACCESS_DENIED);
    EXPECT_TRUE(LedgerEntryExists("exec_free_id_mismatch"));
}

/*
 * 用例描述：
 * ExecuteFree 部分删除成功的场景：第 1 个 NS 删除成功，第 2 个失败。
 * 测试步骤：
 * 1、预置 CREATED 条目（2 个 NS）
 * 2、缓存中有匹配的 NS
 * 3、mock DeleteNamespace 在 2nd 调用时失败（g_freeDeleteFailAfter=1）
 * 4、调用 FreeSpace
 * 预期结果：
 * 1、返回错误码（非 UBSE_OK）
 * 2、账本保留，只包含第 2 个未删除成功的 NS
 * 3、第 1 个 NS 从账本中移除
 */
TEST_F(TestUbseSsuServiceImpFree, ExecuteFree_PartialSuccess)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    SetupFreeEnv({ns1, ns2}, UbseSsuNsState::CREATED, "exec_free_partial");

    // Make the 2nd delete fail (callIndex >= 1 → fail)
    g_freeDeleteFailAfter.store(1);

    auto ret = service_.FreeSpace("exec_free_partial", MakeIdentity());

    EXPECT_NE(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("exec_free_partial");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->allocResult.nameSpaceList.size(), 1u);
    EXPECT_EQ(entry->allocResult.nameSpaceList[0].tgtEid, std::string(16, 'B'));
}

/*
 * 用例描述：
 * ExecuteFree 部分成功场景：验证账本 Modify 后只包含未删除成功的 NS。
 * 测试步骤：
 * 1、预置 CREATED 条目（3 个 NS，分属 3 设备）
 * 2、缓存中有匹配的 NS
 * 3、mock DeleteNamespace 在 2nd 调用时失败（g_freeDeleteFailAfter=1）
 * 4、调用 FreeSpace
 * 预期结果：
 * 1、返回错误码
 * 2、账本保留 2 个未删除成功的 NS（第 2 和第 3 个，因为第 1 个成功，第 2 个开始失败）
 */
TEST_F(TestUbseSsuServiceImpFree, ExecuteFree_PartialSuccess_VerifyLedgerContent)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    auto ns3 = MakeNsForCache(std::string(16, 'C'), "nqn.test.3", 1);
    SetupFreeEnv({ns1, ns2, ns3}, UbseSsuNsState::CREATED, "exec_free_partial_verify");

    // 1st call succeeds, 2nd and 3rd fail
    g_freeDeleteFailAfter.store(1);

    auto ret = service_.FreeSpace("exec_free_partial_verify", MakeIdentity());

    EXPECT_NE(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("exec_free_partial_verify");
    ASSERT_NE(entry, nullptr);
    // ns1 succeeded, ns2 and ns3 remain
    EXPECT_EQ(entry->allocResult.nameSpaceList.size(), 2u);
    EXPECT_EQ(entry->allocResult.nameSpaceList[0].tgtEid, std::string(16, 'B'));
    EXPECT_EQ(entry->allocResult.nameSpaceList[1].tgtEid, std::string(16, 'C'));
}

// --------------------------------------------------------------------------
// Concurrency test
// --------------------------------------------------------------------------

/*
 * 用例描述：
 * FreeSpace 并发安全验证（内部锁保护）。
 * 测试步骤：
 * 1、预置 CREATED 条目
 * 2、两个线程同时调用 FreeSpace（不同 name）
 * 3、等待两个线程完成
 * 预期结果：
 * 1、两个线程均正常返回（不崩溃、不死锁）
 */
TEST_F(TestUbseSsuServiceImpFree, ExecuteFree_Concurrent)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    SetupFreeEnv({ns1}, UbseSsuNsState::CREATED, "exec_free_concurrent_1");
    // Add a second separate entry
    AddNsToBothCaches(ns2);
    auto result2 = MakeAllocResultForFree("exec_free_concurrent_2", {ns2});
    PutLedgerEntry("exec_free_concurrent_2", UbseSsuNsState::CREATED, result2);

    auto future1 = std::async(std::launch::async,
                              [this]() { return service_.FreeSpace("exec_free_concurrent_1", MakeIdentity()); });
    auto future2 = std::async(std::launch::async,
                              [this]() { return service_.FreeSpace("exec_free_concurrent_2", MakeIdentity()); });

    EXPECT_EQ(future1.get(), UBSE_OK);
    EXPECT_EQ(future2.get(), UBSE_OK);
    EXPECT_FALSE(LedgerEntryExists("exec_free_concurrent_1"));
    EXPECT_FALSE(LedgerEntryExists("exec_free_concurrent_2"));
}

} // namespace ubse::ssu::service::ut
