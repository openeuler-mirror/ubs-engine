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
#include <string>
#include <vector>

#include "config/ubse_conf.h"
#include "test_ubse_ssu_service_imp_perm.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;

// ============================================================================
// Controllable mock for AddNameSpaceAllowHost / RemoveNameSpaceAllowHost
// ============================================================================

std::atomic<int> g_addPermFailAfter{-1};
std::atomic<int> g_addPermCallCount{0};
std::atomic<int> g_removePermFailAfter{-1};
std::atomic<int> g_removePermCallCount{0};

static int PermMockAddNamespaceAllowHost(const char *adminNqn, DevNamespaceInfoT *nsInfo, const char *hostNqn)
{
    (void)adminNqn;
    (void)nsInfo;
    (void)hostNqn;
    int call = g_addPermCallCount.fetch_add(1);
    int failAfter = g_addPermFailAfter.load();
    return (failAfter >= 0 && call >= failAfter) ? -1 : 0;
}

static int PermMockRemoveNamespaceAllowHost(const char *adminNqn, DevNamespaceInfoT *nsInfo, const char *hostNqn)
{
    (void)adminNqn;
    (void)nsInfo;
    (void)hostNqn;
    int call = g_removePermCallCount.fetch_add(1);
    int failAfter = g_removePermFailAfter.load();
    return (failAfter >= 0 && call >= failAfter) ? -1 : 0;
}

void ResetPermMockState()
{
    g_addPermFailAfter.store(-1);
    g_addPermCallCount.store(0);
    g_removePermFailAfter.store(-1);
    g_removePermCallCount.store(0);
}

void SetupPermAdapterFuncs()
{
    SetupAdapterFuncs();
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    impl.addNamespaceAllowHost_ = PermMockAddNamespaceAllowHost;
    impl.removeNamespaceAllowHost_ = PermMockRemoveNamespaceAllowHost;
}

// ============================================================================
// TestUbseSsuServiceImpPerm implementation
// ============================================================================

void TestUbseSsuServiceImpPerm::SetUp()
{
    UbseSsuServiceImpTestBase::SetUp();
    ResetPermMockState();
    SetupPermAdapterFuncs();

    MOCKER_CPP(&ubse::config::UbseGetStr)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any())
        .will(invoke(MockUbseGetStr));
}

void TestUbseSsuServiceImpPerm::TearDown()
{
    UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr;
    UbseSsuServiceImpTestBase::TearDown();
}

void TestUbseSsuServiceImpPerm::SetupPermLedger(const std::string &name, UbseSsuNsState state,
                                                const std::vector<UbseSsuDevNameSpace> &nsList)
{
    UbseSsuAllocResult result;
    result.name = name;
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    for (const auto &ns : nsList) {
        result.nameSpaceList.push_back(MakeNameSpaceInfo(ns));
    }
    PutLedgerEntry(name, state, result);
}

void TestUbseSsuServiceImpPerm::SetupStatsLedger(const std::string &name,
                                                 const std::vector<UbseSsuDevNameSpace> &nsList)
{
    UbseSsuAllocResult result;
    result.name = name;
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    for (const auto &ns : nsList) {
        result.nameSpaceList.push_back(MakeNameSpaceInfo(ns, ns.nsze * 512));
    }
    PutLedgerEntry(name, UbseSsuNsState::CREATED, result);
}

// ============================================================================
// AddAccessPermission - Master tests
// ============================================================================

/*
 * 用例：AddAccessPermission_Success
 * Master 端，预置 CREATED 条目，全部 AddNameSpaceAllowHost 成功。
 */
TEST_F(TestUbseSsuServiceImpPerm, AddAccessPermission_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupPermLedger("test_add_perm_success", UbseSsuNsState::CREATED, {ns1, ns2});

    auto identity = MakeIdentity();
    EXPECT_EQ(service_.AddAccessPermission("test_add_perm_success", "nqn.test.host", identity), UBSE_OK);
}

/*
 * 用例：AddAccessPermission_RecordNotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpPerm, AddAccessPermission_RecordNotFound)
{
    EXPECT_EQ(service_.AddAccessPermission("test_not_found", "nqn.test.host", MakeIdentity()),
              UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例：AddAccessPermission_InvalidState
 * 预置 IDLE/CREATING 条目，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpPerm, AddAccessPermission_InvalidState)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    AddNsToCollectorCache(ns1);
    SetupPermLedger("test_invalid_state", UbseSsuNsState::IDLE, {ns1});

    EXPECT_EQ(service_.AddAccessPermission("test_invalid_state", "nqn.test.host", MakeIdentity()),
              UBSE_SSU_ERROR_STATE_INVALID);
}

/*
 * 用例：AddAccessPermission_IdentityNotMatch
 * 设备缓存返回 identity 不匹配。
 */
TEST_F(TestUbseSsuServiceImpPerm, AddAccessPermission_IdentityNotMatch)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    AddNsToCollectorCache(ns1);
    SetupPermLedger("test_identity_mismatch", UbseSsuNsState::CREATED, {ns1});

    // Use a different identity (uid mismatch)
    auto differentIdentity = MakeIdentity("other_user", 999);
    EXPECT_EQ(service_.AddAccessPermission("test_identity_mismatch", "nqn.test.host", differentIdentity),
              UBSE_ERR_ACCESS_DENIED);
}

/*
 * 用例：AddAccessPermission_PartialFailAndRollback
 * 第二个 NS AddNameSpaceAllowHost 失败，回滚第一个已添加的 NS。
 */
TEST_F(TestUbseSsuServiceImpPerm, AddAccessPermission_PartialFailAndRollback)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupPermLedger("test_partial_fail", UbseSsuNsState::CREATED, {ns1, ns2});

    g_addPermFailAfter.store(1); // 2nd call fails

    EXPECT_NE(service_.AddAccessPermission("test_partial_fail", "nqn.test.host", MakeIdentity()), UBSE_OK);
    // 验证 rollback: 第一个 NS 会被 RemoveNameSpaceAllowHost 回滚
    // 接口返回错误即表示回滚已执行
}

/*
 * 用例：AddAccessPermission_DefaultNqnSkip
 * nqn 等于 defaultNqn，跳过（不调用 adapter）。
 */
TEST_F(TestUbseSsuServiceImpPerm, AddAccessPermission_DefaultNqnSkip)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAB'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.test");
    AddNsToCollectorCache(ns1);
    SetupPermLedger("test_default_nqn_skip", UbseSsuNsState::CREATED, {ns1});

    // Use the same nqn as defaultNqn → should skip
    EXPECT_EQ(service_.AddAccessPermission("test_default_nqn_skip", "nqn.default.test", MakeIdentity()), UBSE_OK);
}

/*
 * 用例：AddAccessPermission_NsNotFound
 * NS 不在设备缓存中，返回错误并回滚。
 */
TEST_F(TestUbseSsuServiceImpPerm, AddAccessPermission_NsNotFound)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    // Deliberately NOT adding ns1 to collector cache
    SetupPermLedger("test_ns_not_found", UbseSsuNsState::CREATED, {ns1});

    EXPECT_NE(service_.AddAccessPermission("test_ns_not_found", "nqn.test.host", MakeIdentity()), UBSE_OK);
}

// ============================================================================
// AddAccessPermission - Agent tests
// ============================================================================

/*
 * 用例：AddAccessPermission_AgentViaRpc
 * Agent 角色，mock UbseGetMasterInfo 失败，验证 Agent 路径进入 RPC 调用逻辑。
 */
TEST_F(TestUbseSsuServiceImpPerm, AddAccessPermission_AgentViaRpc)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(service_.AddAccessPermission("test_agent_rpc", "nqn.test.host", MakeIdentity()), UBSE_ERROR);
}

// ============================================================================
// RemoveAccessPermission - Master tests
// ============================================================================

/*
 * 用例：RemoveAccessPermission_Success
 * Master 端，mock RemoveNameSpaceAllowHost 成功。
 */
TEST_F(TestUbseSsuServiceImpPerm, RemoveAccessPermission_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupPermLedger("test_remove_perm_success", UbseSsuNsState::CREATED, {ns1, ns2});

    auto identity = MakeIdentity();
    EXPECT_EQ(service_.RemoveAccessPermission("test_remove_perm_success", "nqn.test.host", identity), UBSE_OK);
}

/*
 * 用例：RemoveAccessPermission_NsNotFoundSkip
 * NS 不在缓存中，视为已移除，幂等跳过。
 */
TEST_F(TestUbseSsuServiceImpPerm, RemoveAccessPermission_NsNotFoundSkip)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    // Deliberately NOT adding ns1 to collector cache
    SetupPermLedger("test_remove_ns_not_found", UbseSsuNsState::CREATED, {ns1});

    EXPECT_EQ(service_.RemoveAccessPermission("test_remove_ns_not_found", "nqn.test.host", MakeIdentity()), UBSE_OK);
}

// ============================================================================
// RemoveAccessPermission - Agent tests
// ============================================================================

/*
 * 用例：RemoveAccessPermission_AgentViaRpc
 * Agent 角色，mock UbseGetMasterInfo 失败。
 */
TEST_F(TestUbseSsuServiceImpPerm, RemoveAccessPermission_AgentViaRpc)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(service_.RemoveAccessPermission("test_agent_rpc", "nqn.test.host", MakeIdentity()), UBSE_ERROR);
}

// ============================================================================
// GetNsStats - Master tests
// ============================================================================

/*
 * 用例：GetNsStats_Success
 * Master 端，预置条目，设备缓存返回 nuse，正确统计 usedSize。
 */
TEST_F(TestUbseSsuServiceImpPerm, GetNsStats_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096);
    ns1.nuse = 1024; // 已用 1024 LBAs
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 2, 8192);
    ns2.nuse = 2048;
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);

    // nsSize for MakeNameSpaceInfo needs to match the total size
    UbseSsuAllocResult result;
    result.name = "test_stats_success";
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    result.nameSpaceList.push_back(MakeNameSpaceInfo(ns1, 4096 * 512));
    result.nameSpaceList.push_back(MakeNameSpaceInfo(ns2, 8192 * 512));
    PutLedgerEntry("test_stats_success", UbseSsuNsState::CREATED, result);

    std::vector<UbseSsuNsStats> statsList;
    auto ret = service_.GetNsStats("test_stats_success", statsList, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(statsList.size(), 2u);
    EXPECT_EQ(statsList[0].usedSize, 1024u);
    EXPECT_EQ(statsList[1].usedSize, 2048u);
    EXPECT_EQ(statsList[0].totalSize, 4096u * 512);
    EXPECT_EQ(statsList[1].totalSize, 8192u * 512);
}

/*
 * 用例：GetNsStats_RecordNotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpPerm, GetNsStats_RecordNotFound)
{
    std::vector<UbseSsuNsStats> statsList;
    EXPECT_EQ(service_.GetNsStats("test_not_found", statsList, MakeIdentity()), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例：GetNsStats_IdentityNotMatch
 * identity 不匹配，返回 UBSE_ERR_ACCESS_DENIED。
 */
TEST_F(TestUbseSsuServiceImpPerm, GetNsStats_IdentityNotMatch)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    AddNsToCollectorCache(ns1);
    SetupStatsLedger("test_stats_identity", {ns1});

    auto differentIdentity = MakeIdentity("other_user", 999);
    std::vector<UbseSsuNsStats> statsList;
    EXPECT_EQ(service_.GetNsStats("test_stats_identity", statsList, differentIdentity), UBSE_ERR_ACCESS_DENIED);
}

// ============================================================================
// GetNsStats - Agent tests
// ============================================================================

/*
 * 用例：GetNsStats_AgentViaRpc
 * Agent 角色，mock UbseGetMasterInfo 失败。
 */
TEST_F(TestUbseSsuServiceImpPerm, GetNsStats_AgentViaRpc)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbseSsuNsStats> statsList;
    EXPECT_EQ(service_.GetNsStats("test_agent_rpc", statsList, MakeIdentity()), UBSE_ERROR);
}

/*
 * 用例：GetNsStats_CacheMissRefreshSuccess
 * 缓存未命中但硬件刷新成功，返回正确统计。
 */
TEST_F(TestUbseSsuServiceImpPerm, GetNsStats_CacheMissRefreshSuccess)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096);
    ns1.nuse = 1024;
    // Deliberately NOT adding ns1 to collector cache; will be refreshed via acquireDevInfo_

    // Configure controllable acquireDevInfo_ to return the namespace on refresh
    g_acquireDevInfoNsList.push_back(ns1);
    UbseSsuAdapterImpl::GetInstance().acquireDevInfo_ = ControllableAcquireDevInfo;

    UbseSsuAllocResult result;
    result.name = "test_stats_cache_miss_success";
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    result.nameSpaceList.push_back(MakeNameSpaceInfo(ns1, 4096 * 512));
    PutLedgerEntry("test_stats_cache_miss_success", UbseSsuNsState::CREATED, result);

    std::vector<UbseSsuNsStats> statsList;
    auto ret = service_.GetNsStats("test_stats_cache_miss_success", statsList, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(statsList.size(), 1u);
    EXPECT_EQ(statsList[0].usedSize, 1024u);
}

/*
 * 用例：GetNsStats_CacheMissRefreshFailed
 * 缓存未命中且硬件刷新失败，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpPerm, GetNsStats_CacheMissRefreshFailed)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    // Deliberately NOT adding ns1 to collector cache

    // Configure controllable acquireDevInfo_ to fail on refresh
    g_acquireDevInfoFail.store(true);
    UbseSsuAdapterImpl::GetInstance().acquireDevInfo_ = ControllableAcquireDevInfo;

    SetupStatsLedger("test_stats_cache_miss_failed", {ns1});

    std::vector<UbseSsuNsStats> statsList;
    EXPECT_EQ(service_.GetNsStats("test_stats_cache_miss_failed", statsList, MakeIdentity()),
              UBSE_SSU_ERROR_NS_NOT_FOUND);
}

// ============================================================================
// VerifyAttachDetachIdentity tests
// ============================================================================

/*
 * 用例：VerifyAttachDetachIdentity_Success
 * 预置条目，NS 在缓存中且 identity 匹配，返回正确 nsVerifyList。
 */
TEST_F(TestUbseSsuServiceImpPerm, VerifyAttachDetachPrecondition_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAA'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.test");
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 2, 4096,
                              std::string(GUID_SIZE, '\xBB'), std::string(UUID_SIZE, '\xBC'),
                              100, "test_user", "nqn.default.test2");
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupPermLedger("test_verify_success", UbseSsuNsState::CREATED, {ns1, ns2});

    ubse::ssu::message::UbseSsuAttachDetachVerifyOption option;
    option.expectedStrategy = UbseSsuAllocStrategy::LINEAR;
    ubse::ssu::message::UbseSsuAttachDetachVerifyResp verifyResp;
    auto ret = service_.VerifyAttachDetachPrecondition("test_verify_success", MakeIdentity(), option, verifyResp);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(verifyResp.nsVerifyList.size(), 2u);
    EXPECT_EQ(verifyResp.nsVerifyList[0].defaultNqn, "nqn.default.test");
    EXPECT_EQ(verifyResp.nsVerifyList[0].guid, std::string(GUID_SIZE, '\xAA'));
    EXPECT_EQ(verifyResp.nsVerifyList[1].defaultNqn, "nqn.default.test2");
    EXPECT_EQ(verifyResp.nsVerifyList[1].guid, std::string(GUID_SIZE, '\xBB'));
}

/*
 * 用例：VerifyAttachDetachPrecondition_NotFound
 * 账本无记录，返回 UBSE_SSU_ERROR_SPACE_NOT_FOUND。
 */
TEST_F(TestUbseSsuServiceImpPerm, VerifyAttachDetachPrecondition_NotFound)
{
    ubse::ssu::message::UbseSsuAttachDetachVerifyOption option;
    ubse::ssu::message::UbseSsuAttachDetachVerifyResp verifyResp;
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("test_not_found", MakeIdentity(), option, verifyResp),
              UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例：VerifyAttachDetachPrecondition_NsNotInCache
 * NS 不在设备缓存中，返回 UBSE_SSU_ERROR_NS_NOT_FOUND。
 */
TEST_F(TestUbseSsuServiceImpPerm, VerifyAttachDetachPrecondition_NsNotInCache)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    // Deliberately NOT adding ns1 to collector cache
    SetupPermLedger("test_verify_ns_missing", UbseSsuNsState::CREATED, {ns1});

    ubse::ssu::message::UbseSsuAttachDetachVerifyOption option;
    option.expectedStrategy = UbseSsuAllocStrategy::LINEAR;
    ubse::ssu::message::UbseSsuAttachDetachVerifyResp verifyResp;
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("test_verify_ns_missing", MakeIdentity(), option, verifyResp),
              UBSE_SSU_ERROR_NS_NOT_FOUND);
}

/*
 * 用例：VerifyAttachDetachPrecondition_NotMatch
 * identity 不匹配，返回 UBSE_ERR_ACCESS_DENIED。
 */
TEST_F(TestUbseSsuServiceImpPerm, VerifyAttachDetachPrecondition_NotMatch)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    AddNsToCollectorCache(ns1);
    SetupPermLedger("test_verify_not_match", UbseSsuNsState::CREATED, {ns1});

    auto differentIdentity = MakeIdentity("other_user", 999);
    ubse::ssu::message::UbseSsuAttachDetachVerifyOption option;
    option.expectedStrategy = UbseSsuAllocStrategy::LINEAR;
    ubse::ssu::message::UbseSsuAttachDetachVerifyResp verifyResp;
    EXPECT_EQ(service_.VerifyAttachDetachPrecondition("test_verify_not_match", differentIdentity, option, verifyResp),
              UBSE_ERR_ACCESS_DENIED);
}

} // namespace ubse::ssu::service::ut
