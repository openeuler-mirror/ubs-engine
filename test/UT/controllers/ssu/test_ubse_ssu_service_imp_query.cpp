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

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include "config/ubse_conf.h"
#include "test_ubse_ssu_service_imp_query.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;

// ============================================================================
// TestUbseSsuServiceImpQuery implementation
// ============================================================================

void TestUbseSsuServiceImpQuery::SetUp()
{
    UbseSsuServiceImpTestBase::SetUp();
    SetupAdapterFuncs();
    ResetAllowHostsMockState();
    // 默认装配可注入的 GetNamespaceAllowHosts，便于 ListAllocInfo/GetAllocInfoByName 填充测试
    UbseSsuAdapterImpl::GetInstance().getNamespaceAllowHosts_ = ControllableGetNamespaceAllowHosts;

    MOCKER_CPP(&ubse::config::UbseGetStr)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any())
        .will(invoke(MockUbseGetStr));
}

void TestUbseSsuServiceImpQuery::TearDown()
{
    UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr;
    UbseSsuServiceImpTestBase::TearDown();
}

void TestUbseSsuServiceImpQuery::SetupQueryLedger(const std::string &name,
                                                  const std::vector<UbseSsuDevNameSpace> &nsList,
                                                  UbseSsuNsState state)
{
    UbseSsuAllocResult result;
    result.name = name;
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    for (const auto &ns : nsList) {
        result.nameSpaceList.push_back(MakeNameSpaceInfo(ns, ns.nsze * 512));
    }
    PutLedgerEntry(name, state, result);
}

// ============================================================================
// ListAllocInfo - Master tests
// ============================================================================

/*
 * 用例：ListAllocInfo_Master_Success
 * 预置多条账本条目，返回所有匹配 identity 的 allocResult 列表。
 */
TEST_F(TestUbseSsuServiceImpQuery, ListAllocInfo_Master_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAA'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.1");
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 2, 4096,
                              std::string(GUID_SIZE, '\xBB'), std::string(UUID_SIZE, '\xBC'),
                              100, "test_user", "nqn.default.2");
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);

    // 预置两条账本条目，属于同一 identity
    SetupQueryLedger("test_list_1", {ns1});
    SetupQueryLedger("test_list_2", {ns2});

    std::vector<UbseSsuAllocResult> result;
    auto ret = service_.ListAllocInfo(result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(result.size(), 2u);
    // 验证返回的 name 包含两个条目
    std::vector<std::string> names;
    for (const auto &item : result) {
        names.push_back(item.name);
    }
    EXPECT_NE(std::find(names.begin(), names.end(), "test_list_1"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "test_list_2"), names.end());
}

/*
 * 用例：ListAllocInfo_Master_IdentityFilter
 * 预置分属不同 identity 的条目，只返回匹配 identity 的条目。
 */
TEST_F(TestUbseSsuServiceImpQuery, ListAllocInfo_Master_IdentityFilter)
{
    // 用户 test_user 的条目
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAA'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.1");
    AddNsToCollectorCache(ns1);
    SetupQueryLedger("test_filter_match", {ns1});

    // 用户 other_user 的条目
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 2, 4096,
                              std::string(GUID_SIZE, '\xBB'), std::string(UUID_SIZE, '\xBC'),
                              999, "other_user", "nqn.default.2");
    AddNsToCollectorCache(ns2);
    SetupQueryLedger("test_filter_other", {ns2});

    std::vector<UbseSsuAllocResult> result;
    auto ret = service_.ListAllocInfo(result, MakeIdentity("test_user", 100));

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].name, "test_filter_match");
}

// ============================================================================
// ListAllocInfo - Agent tests
// ============================================================================

/*
 * 用例：ListAllocInfo_AgentViaRpc
 * Agent 角色，mock UbseGetMasterInfo 失败。
 */
TEST_F(TestUbseSsuServiceImpQuery, ListAllocInfo_AgentViaRpc)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbseSsuAllocResult> result;
    EXPECT_EQ(service_.ListAllocInfo(result, MakeIdentity()), UBSE_ERROR);
}

// ============================================================================
// GetAllocInfoByName - Master tests
// ============================================================================

/*
 * 用例：GetAllocInfoByName_Master_Success
 * 预置条目，验证通过后返回指定条目。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetAllocInfoByName_Master_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    AddNsToCollectorCache(ns1);
    SetupQueryLedger("test_get_by_name", {ns1});

    UbseSsuAllocResult result;
    auto ret = service_.GetAllocInfoByName("test_get_by_name", result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(result.name, "test_get_by_name");
    ASSERT_EQ(result.nameSpaceList.size(), 1u);
}

/*
 * 用例：GetAllocInfoByName_Master_NotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetAllocInfoByName_Master_NotFound)
{
    UbseSsuAllocResult result;
    EXPECT_EQ(service_.GetAllocInfoByName("test_not_found", result, MakeIdentity()),
              UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

// ============================================================================
// GetAllocInfoByName - Agent tests
// ============================================================================

/*
 * 用例：GetAllocInfoByName_AgentViaRpc
 * Agent 角色，mock UbseGetMasterInfo 失败。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetAllocInfoByName_AgentViaRpc)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    UbseSsuAllocResult result;
    EXPECT_EQ(service_.GetAllocInfoByName("test_agent_rpc", result, MakeIdentity()), UBSE_ERROR);
}

// ============================================================================
// GetConnectInfo - Master tests
// ============================================================================

/*
 * 用例：GetConnectInfo_Master_Success
 * 预置条目，设备缓存命中，identity 匹配，返回正确填充的 connectInfoList。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetConnectInfo_Master_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAA'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.1");
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 2, 4096,
                              std::string(GUID_SIZE, '\xBB'), std::string(UUID_SIZE, '\xBC'),
                              100, "test_user", "nqn.default.2");
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupQueryLedger("test_connect_info", {ns1, ns2});

    std::vector<UbseSsuConnectInfo> connectInfoList;
    auto ret = service_.GetConnectInfo("test_connect_info", nullptr, connectInfoList, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(connectInfoList.size(), 2u);
    EXPECT_EQ(connectInfoList[0].tgtEid, std::string(16, 'A'));
    EXPECT_EQ(connectInfoList[0].hostNqn, "nqn.default.1");
    EXPECT_EQ(connectInfoList[0].nsId, 1u);
    EXPECT_EQ(connectInfoList[1].tgtEid, std::string(16, 'B'));
    EXPECT_EQ(connectInfoList[1].hostNqn, "nqn.default.2");
    EXPECT_EQ(connectInfoList[1].nsId, 2u);
}

/*
 * 用例：GetConnectInfo_Master_RecordNotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetConnectInfo_Master_RecordNotFound)
{
    std::vector<UbseSsuConnectInfo> connectInfoList;
    EXPECT_EQ(service_.GetConnectInfo("test_not_found", nullptr, connectInfoList, MakeIdentity()),
              UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例：GetConnectInfo_Master_IdentityNotMatch
 * identity 不匹配，返回 UBSE_ERR_ACCESS_DENIED。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetConnectInfo_Master_IdentityNotMatch)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    AddNsToCollectorCache(ns1);
    SetupQueryLedger("test_connect_identity", {ns1});

    auto differentIdentity = MakeIdentity("other_user", 999);
    std::vector<UbseSsuConnectInfo> connectInfoList;
    EXPECT_EQ(service_.GetConnectInfo("test_connect_identity", nullptr, connectInfoList, differentIdentity),
              UBSE_ERR_ACCESS_DENIED);
}

/*
 * 用例：GetConnectInfo_Master_CacheMissRefreshSuccess
 * 缓存未命中→硬件刷新成功，返回正确连接信息。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetConnectInfo_Master_CacheMissRefreshSuccess)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAA'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.1");
    // Deliberately NOT adding ns1 to collector cache; will be refreshed via acquireDevInfo_

    // Configure controllable acquireDevInfo_ to return the namespace on refresh
    g_acquireDevInfoNsList.push_back(ns1);
    UbseSsuAdapterImpl::GetInstance().acquireDevInfo_ = ControllableAcquireDevInfo;

    SetupQueryLedger("test_connect_cache_miss_success", {ns1});

    std::vector<UbseSsuConnectInfo> connectInfoList;
    auto ret = service_.GetConnectInfo("test_connect_cache_miss_success", nullptr, connectInfoList, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(connectInfoList.size(), 1u);
    EXPECT_EQ(connectInfoList[0].tgtEid, std::string(16, 'A'));
    EXPECT_EQ(connectInfoList[0].hostNqn, "nqn.default.1");
    EXPECT_EQ(connectInfoList[0].nsId, 1u);
}

/*
 * 用例：GetConnectInfo_Master_CacheMissRefreshFailed
 * 缓存未命中→硬件刷新失败，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetConnectInfo_Master_CacheMissRefreshFailed)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    // Deliberately NOT adding ns1 to collector cache

    // Configure controllable acquireDevInfo_ to fail on refresh
    g_acquireDevInfoFail.store(true);
    UbseSsuAdapterImpl::GetInstance().acquireDevInfo_ = ControllableAcquireDevInfo;

    SetupQueryLedger("test_connect_cache_miss_failed", {ns1});

    std::vector<UbseSsuConnectInfo> connectInfoList;
    EXPECT_EQ(service_.GetConnectInfo("test_connect_cache_miss_failed", nullptr, connectInfoList, MakeIdentity()),
              UBSE_SSU_ERROR_NS_NOT_FOUND);
}

// ============================================================================
// GetConnectInfo - Agent tests
// ============================================================================

/*
 * 用例：GetConnectInfo_AgentViaRpc
 * Agent 角色，mock UbseGetMasterInfo 失败。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetConnectInfo_AgentViaRpc)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    std::vector<UbseSsuConnectInfo> connectInfoList;
    EXPECT_EQ(service_.GetConnectInfo("test_agent_rpc", nullptr, connectInfoList, MakeIdentity()), UBSE_ERROR);
}

// ============================================================================
// FillAllowHostNqnList tests (via ListAllocInfo / GetAllocInfoByName)
// 验证 master 端查询返回前对每个 ns 实时填充 allowHostNqnList 的行为。
// ============================================================================

/*
 * 用例：GetAllocInfoByName_Master_FillAllowHostNqnList
 * 验证：defaultNqn 置于首位，硬件返回的 NQN 追加其后；defaultNqn 在硬件列表中时去重。
 */
TEST_F(TestUbseSsuServiceImpQuery, GetAllocInfoByName_Master_FillAllowHostNqnList)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAA'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.1");
    AddNsToCollectorCache(ns1);
    SetupQueryLedger("test_fill_allow", {ns1});

    // nsId=1 的硬件 allow 列表：包含一个与 defaultNqn 相同的 NQN（应被去重），以及一个额外 NQN
    g_allowHostsMap[1] = {"nqn.default.1", "nqn.host.extra"};

    UbseSsuAllocResult result;
    auto ret = service_.GetAllocInfoByName("test_fill_allow", result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(result.nameSpaceList.size(), 1u);
    const auto &allowList = result.nameSpaceList[0].allowHostNqnList;
    ASSERT_EQ(allowList.size(), 2u);
    EXPECT_EQ(allowList[0], "nqn.default.1");
    EXPECT_EQ(allowList[1], "nqn.host.extra");
}

/*
 * 用例：ListAllocInfo_Master_FillAllowHostNqnList_MultiNs
 * 验证：多个 allocResult、多个 ns 时，每个 ns 独立填充。
 */
TEST_F(TestUbseSsuServiceImpQuery, ListAllocInfo_Master_FillAllowHostNqnList_MultiNs)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAA'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.1");
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 2, 4096,
                              std::string(GUID_SIZE, '\xBB'), std::string(UUID_SIZE, '\xBC'),
                              100, "test_user", "nqn.default.2");
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupQueryLedger("test_fill_multi_1", {ns1});
    SetupQueryLedger("test_fill_multi_2", {ns2});

    g_allowHostsMap[1] = {"nqn.host.a1"};
    g_allowHostsMap[2] = {"nqn.host.b1", "nqn.host.b2"};

    std::vector<UbseSsuAllocResult> result;
    auto ret = service_.ListAllocInfo(result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(result.size(), 2u);
    // 按 name 排序便于稳定断言
    std::sort(result.begin(), result.end(),
              [](const UbseSsuAllocResult &a, const UbseSsuAllocResult &b) { return a.name < b.name; });
    ASSERT_EQ(result[0].nameSpaceList.size(), 1u);
    ASSERT_EQ(result[1].nameSpaceList.size(), 1u);
    const auto &list1 = result[0].nameSpaceList[0].allowHostNqnList;
    const auto &list2 = result[1].nameSpaceList[0].allowHostNqnList;
    ASSERT_EQ(list1.size(), 2u);
    EXPECT_EQ(list1[0], "nqn.default.1");
    EXPECT_EQ(list1[1], "nqn.host.a1");
    ASSERT_EQ(list2.size(), 3u);
    EXPECT_EQ(list2[0], "nqn.default.2");
    EXPECT_EQ(list2[1], "nqn.host.b1");
    EXPECT_EQ(list2[2], "nqn.host.b2");
}

/*
 * 用例：FillAllowHostNqnList_GetFailed_SkipNs
 * 验证：GetNameSpaceAllowHostList 失败时该 ns 的 allowHostNqnList 为空，不影响整体返回与其他 ns。
 */
TEST_F(TestUbseSsuServiceImpQuery, FillAllowHostNqnList_GetFailed_SkipNs)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAA'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.1");
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 2, 4096,
                              std::string(GUID_SIZE, '\xBB'), std::string(UUID_SIZE, '\xBC'),
                              100, "test_user", "nqn.default.2");
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupQueryLedger("test_fill_partial_fail", {ns1, ns2});

    // nsId=1 查询失败，nsId=2 正常
    g_allowHostsGetFail.store(false);
    g_allowHostsMap.clear();
    g_allowHostsMap[2] = {"nqn.host.b1"};
    // 仅对 nsId=1 注入失败：使用自定义 mock
    static thread_local std::unordered_set<uint32_t> failNsIds{1};
    UbseSsuAdapterImpl::GetInstance().getNamespaceAllowHosts_ =
        [](const char *adminNqn, DevNamespaceInfoT *nsInfo, char ***list, uint32_t *count) -> int {
        if (nsInfo != nullptr && failNsIds.count(nsInfo->namespaceId) > 0) {
            return -1;
        }
        return ControllableGetNamespaceAllowHosts(adminNqn, nsInfo, list, count);
    };

    UbseSsuAllocResult result;
    auto ret = service_.GetAllocInfoByName("test_fill_partial_fail", result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(result.nameSpaceList.size(), 2u);
    // 找到 nsId=1 和 nsId=2
    const UbseSsuNameSpaceInfo *nsInfo1 = nullptr;
    const UbseSsuNameSpaceInfo *nsInfo2 = nullptr;
    for (const auto &ns : result.nameSpaceList) {
        if (ns.namespaceId == 1) {
            nsInfo1 = &ns;
        } else if (ns.namespaceId == 2) {
            nsInfo2 = &ns;
        }
    }
    ASSERT_NE(nsInfo1, nullptr);
    ASSERT_NE(nsInfo2, nullptr);
    // 失败的 ns 仅含 defaultNqn（来自设备缓存，不依赖硬件查询），不含硬件 allow 列表
    ASSERT_EQ(nsInfo1->allowHostNqnList.size(), 1u);
    EXPECT_EQ(nsInfo1->allowHostNqnList[0], "nqn.default.1");
    ASSERT_EQ(nsInfo2->allowHostNqnList.size(), 2u);
    EXPECT_EQ(nsInfo2->allowHostNqnList[0], "nqn.default.2");
    EXPECT_EQ(nsInfo2->allowHostNqnList[1], "nqn.host.b1");
}

/*
 * 用例：FillAllowHostNqnList_CacheMissRefreshSuccess
 * 验证：devMap 缓存未命中时触发 RefreshDevCache，刷新成功后仍能填充 allowHostNqnList。
 */
TEST_F(TestUbseSsuServiceImpQuery, FillAllowHostNqnList_CacheMissRefreshSuccess)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 4096,
                              std::string(GUID_SIZE, '\xAA'), std::string(UUID_SIZE, '\xAB'),
                              100, "test_user", "nqn.default.1");
    // 故意不加入 collector 缓存，触发 RefreshDevCache
    g_acquireDevInfoNsList.push_back(ns1);
    UbseSsuAdapterImpl::GetInstance().acquireDevInfo_ = ControllableAcquireDevInfo;
    SetupQueryLedger("test_fill_cache_miss", {ns1});

    g_allowHostsMap[1] = {"nqn.host.x"};

    UbseSsuAllocResult result;
    auto ret = service_.GetAllocInfoByName("test_fill_cache_miss", result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(result.nameSpaceList.size(), 1u);
    const auto &allowList = result.nameSpaceList[0].allowHostNqnList;
    ASSERT_EQ(allowList.size(), 2u);
    EXPECT_EQ(allowList[0], "nqn.default.1");
    EXPECT_EQ(allowList[1], "nqn.host.x");
}

} // namespace ubse::ssu::service::ut
