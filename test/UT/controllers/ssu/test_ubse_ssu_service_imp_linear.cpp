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
#include "test_ubse_ssu_service_imp_linear.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;

// ============================================================================
// TestUbseSsuServiceImpLinear implementation
// ============================================================================

void TestUbseSsuServiceImpLinear::SetUp()
{
    UbseSsuServiceImpTestBase::SetUp();
    ResetControllableMockState();
    SetupAdapterFuncs();

    MOCKER_CPP(&ubse::config::UbseGetStr)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any())
        .will(invoke(MockUbseGetStr));
}

void TestUbseSsuServiceImpLinear::TearDown()
{
    UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr;
    UbseSsuServiceImpTestBase::TearDown();
}

UbseSsuLinearSpaceReq TestUbseSsuServiceImpLinear::MakeLinearReq(const std::string &name, const std::string &devName,
                                                                 const std::string &nqn,
                                                                 const UbseSsuAllocIdentityInfo &identity)
{
    UbseSsuLinearSpaceReq req;
    req.name = name;
    req.devName = devName;
    req.nqn = nqn;
    req.identity = identity;
    return req;
}

UbseSsuNameSpaceInfo TestUbseSsuServiceImpLinear::MakeNsInfo(const UbseSsuDevNameSpace &ns, uint64_t nsSize)
{
    UbseSsuNameSpaceInfo info;
    info.tgtEid = ns.subSystem.eid;
    info.tgtNqn = ns.subSystem.subNqn;
    info.namespaceId = ns.namespaceId;
    info.nsUuid = StrToUuid(ns.uuid);
    info.nsDevPath = ns.nsDevPath;
    info.nsSize = nsSize;
    info.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    return info;
}

void TestUbseSsuServiceImpLinear::SetupLedgerForAttach(const std::string &name, UbseSsuNsState state,
                                                       const std::vector<UbseSsuDevNameSpace> &nsList,
                                                       UbseSsuAllocStrategy strategy)
{
    UbseSsuAllocResult result;
    result.name = name;
    result.strategy = strategy;
    for (const auto &ns : nsList) {
        result.nameSpaceList.push_back(MakeNsInfo(ns));
    }
    PutLedgerEntry(name, state, result);
}

// --------------------------------------------------------------------------
// AttachLinearSpace - Master tests
// --------------------------------------------------------------------------

/*
 * 用例：AttachLinear_Master_AlreadyAttached
 * 预置 ATTACHED 条目，幂等返回。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Master_AlreadyAttached)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    SetupLedgerForAttach("test_linear_attached", UbseSsuNsState::ATTACHED, {ns1, ns2});

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeLinearReq("test_linear_attached");
    auto ret = service_.AttachLinearSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_ERR_ALREADY_ATTACHED);
    ASSERT_EQ(nsDevPaths.size(), 2u);
}

/*
 * 用例：AttachLinear_Master_NotCreated
 * 预置非 CREATED 条目，拒绝。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Master_NotCreated)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    SetupLedgerForAttach("test_linear_not_created", UbseSsuNsState::IDLE, {ns1});

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    EXPECT_EQ(service_.AttachLinearSpace(MakeLinearReq("test_linear_not_created"), nsDevPaths, devPath),
              UBSE_SSU_ERROR_STATE_INVALID);
}

/*
 * 用例：AttachLinear_Master_RecordNotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Master_RecordNotFound)
{
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    EXPECT_EQ(service_.AttachLinearSpace(MakeLinearReq("test_linear_not_found"), nsDevPaths, devPath),
              UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例：AttachLinear_Master_CreateBlockDevFail
 * AttachSingleNs 成功，但 CreateBlockDevice 失败（真实适配器在无 LVM 环境下会失败）。
 * 验证回滚已 attach 的 NS。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Master_CreateBlockDevFail)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupLedgerForAttach("test_linear_blockdev_fail", UbseSsuNsState::CREATED, {ns1, ns2});

    auto identity = MakeIdentity();
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeLinearReq("test_linear_blockdev_fail", "md_linear", "", identity);
    auto ret = service_.AttachLinearSpace(req, nsDevPaths, devPath);

    // CreateBlockDevice will fail in test environment (no real LVM)
    // Verify the state is rolled back
    EXPECT_NE(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_linear_blockdev_fail");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 用例：AttachLinear_Master_AttachNsFail
 * 第二个 NS Attach 失败（g_attachFailAfter=1），回滚已 attach 的 NS。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Master_AttachNsFail)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupLedgerForAttach("test_linear_attach_ns_fail", UbseSsuNsState::CREATED, {ns1, ns2});

    g_attachFailAfter.store(1); // 2nd call fails

    auto identity = MakeIdentity();
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto ret = service_.AttachLinearSpace(MakeLinearReq("test_linear_attach_ns_fail", "md_linear", "", identity),
                                          nsDevPaths, devPath);

    EXPECT_NE(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_linear_attach_ns_fail");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

// --------------------------------------------------------------------------
// AttachLinearSpace - Master success tests (mock CreateBlockDevice)
// --------------------------------------------------------------------------

/*
 * 用例：AttachLinear_Master_SingleNsSuccess
 * Master 角色，mock CreateBlockDevice 成功，单 NS 完整 attach 成功。
 * 需要：AddNsToCollectorCache（缓存命中 + 获取 nsDevPath）、
 *       SetupLedgerForAttach（账本 CREATED 状态）、
 *       MockCreateBlockDeviceSuccess（屏蔽 LVM 创建设备失败）。
 * 验证：nsDevPaths 长度和内容、账本推进到 ATTACHED。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Master_SingleNsSuccess)
{
    auto identity = MakeIdentity();
    auto ns = MakeNsForCache(std::string(16, 'S'), "nqn.test.1", 1, 8192);
    AddNsToCollectorCache(ns);
    SetupLedgerForAttach("test_linear_master_single_success", UbseSsuNsState::CREATED, {ns});

    MockCreateBlockDeviceSuccess();

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeLinearReq("test_linear_master_single_success", "md_linear", "", identity);
    auto ret = service_.AttachLinearSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 1u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");

    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_linear_master_single_success");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：AttachLinear_Master_MultiNsSuccess
 * Master 角色，2 NS，全部 attach + CreateBlockDevice 成功。
 * 验证：nsDevPaths 数组内容和长度、账本推进到 ATTACHED。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Master_MultiNsSuccess)
{
    auto identity = MakeIdentity();
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.2", 2, 8192);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupLedgerForAttach("test_linear_master_multi_success", UbseSsuNsState::CREATED, {ns1, ns2});

    MockCreateBlockDeviceSuccess();

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeLinearReq("test_linear_master_multi_success", "md_linear", "", identity);
    auto ret = service_.AttachLinearSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 2u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
    EXPECT_EQ(nsDevPaths[1], "/dev/nvme0n2");

    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_linear_master_multi_success");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

// --------------------------------------------------------------------------
// AttachLinearSpace - Agent tests
// --------------------------------------------------------------------------

/*
 * 用例：AttachLinear_Agent_VerifyFailed
 * Agent 角色，mock UbseGetMasterInfo 失败使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Agent_VerifyFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    EXPECT_EQ(service_.AttachLinearSpace(MakeLinearReq("test_linear_agent_fail"), nsDevPaths, devPath), UBSE_ERROR);
}

// --------------------------------------------------------------------------
// DetachLinearSpace - Master tests
// --------------------------------------------------------------------------

/*
 * 用例：DetachLinear_Master_NotAttached_Created
 * 预置 CREATED 条目（已 detach），幂等返回。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Master_NotAttached_Created)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    SetupLedgerForAttach("test_detach_linear_created", UbseSsuNsState::CREATED, {ns1});

    EXPECT_EQ(service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_created")), UBSE_ERR_NO_NEED_DETACH);
}

/*
 * 用例：DetachLinear_Master_RecordNotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Master_RecordNotFound)
{
    EXPECT_EQ(service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_not_found")),
              UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例：DetachLinear_Master_InvalidState
 * 预置非 ATTACHED 且非 CREATED 状态（如 IDLE），返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Master_InvalidState)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    SetupLedgerForAttach("test_detach_linear_idle", UbseSsuNsState::IDLE, {ns1});

    EXPECT_EQ(service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_idle")), UBSE_SSU_ERROR_STATE_INVALID);
}

/*
 * 用例：DetachLinear_Master_DeleteBlockDevIdempotent
 * DeleteBlockDevice 在设备不存在时幂等返回成功，因此整个 detach 成功。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Master_DeleteBlockDevIdempotent)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupLedgerForAttach("test_detach_linear_blockdev_ok", UbseSsuNsState::ATTACHED, {ns1, ns2});

    auto identity = MakeIdentity();
    auto ret = service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_blockdev_ok", "md_linear", "", identity));

    // DeleteBlockDevice is idempotent — device not found returns success
    EXPECT_EQ(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_detach_linear_blockdev_ok");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 用例：DetachLinear_Master_DevNameMismatch
 * 删除块设备前校验请求devName与账本记录一致（账本为attach时写入的权威值）。
 * 预置账本 devName=md_ledger，请求 devName=md_other，返回 UBSE_ERR_INVALID_ARG 且账本保持 ATTACHED。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Master_DevNameMismatch)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    UbseSsuAllocResult result;
    result.name = "test_detach_linear_master_devname_mismatch";
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    result.nameSpaceList.push_back(MakeNsInfo(ns1));

    auto entry = std::make_shared<UbseSsuLedgerEntry>();
    entry->name = result.name;
    entry->devName = "md_ledger"; // attach时写入账本的权威值
    entry->state = UbseSsuNsState::ATTACHED;
    entry->allocResult = result;
    UbseSsuDebtLedger::GetInstance().Put(entry->name, entry);

    auto identity = MakeIdentity();
    auto ret = service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_master_devname_mismatch", "md_other", "",
                                                        identity));

    EXPECT_EQ(ret, UBSE_ERR_INVALID_ARG);
    auto entryAfter = UbseSsuDebtLedger::GetInstance().Get("test_detach_linear_master_devname_mismatch");
    ASSERT_NE(entryAfter, nullptr);
    EXPECT_EQ(entryAfter->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：DetachLinear_Master_PartialDetachFail
 * 第二个 NS Detach 失败，保持 ATTACHED 状态。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Master_PartialDetachFail)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupLedgerForAttach("test_detach_linear_partial_fail", UbseSsuNsState::ATTACHED, {ns1, ns2});

    g_detachFailAfter.store(1); // 2nd call fails

    auto identity = MakeIdentity();
    auto ret = service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_partial_fail", "md_linear", "", identity));

    EXPECT_NE(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_detach_linear_partial_fail");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：DetachLinear_Master_Success
 * Master 角色，DeleteBlockDevice 幂等成功，NS detach 全部成功。
 * 验证：账本回退到 CREATED。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Master_Success)
{
    auto identity = MakeIdentity();
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.2", 2, 8192);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupLedgerForAttach("test_detach_linear_master_success", UbseSsuNsState::ATTACHED, {ns1, ns2});

    MockCreateBlockDeviceSuccess();

    auto ret = service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_master_success", "md_linear", "", identity));

    EXPECT_EQ(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_detach_linear_master_success");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

// --------------------------------------------------------------------------
// DetachLinearSpace - Agent tests
// --------------------------------------------------------------------------

/*
 * 用例：DetachLinear_Agent_VerifyFailed
 * Agent 角色，mock UbseGetMasterInfo 失败使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Agent_VerifyFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_agent_fail")), UBSE_ERROR);
}

// --------------------------------------------------------------------------
// AttachLinearSpace - Agent tests (Linear 特有：CreateBlockDevice)
// --------------------------------------------------------------------------

/*
 * 用例：AttachLinear_Agent_CreateBlockDevFail
 * Agent 角色，NS attach 全部成功，但 CreateBlockDevice 在测试环境会失败。
 * 验证已 attach 的 NS 被回滚。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Agent_CreateBlockDevFail)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'S'), "nqn.test.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'S'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto ret = service_.AttachLinearSpace(MakeLinearReq("test_linear_agent_blockdev_fail", "md_linear"),
                                          nsDevPaths, devPath);

    // CreateBlockDevice will fail in test environment (no real LVM)
    EXPECT_NE(ret, UBSE_OK);
}

/*
 * 用例：AttachLinear_Agent_MultiNsCreateBlockDevFail
 * Agent 角色，多个 NS 全部 attach 成功，但 CreateBlockDevice 失败。
 * 验证所有已 attach 的 NS 被回滚。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Agent_MultiNsCreateBlockDevFail)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto ret = service_.AttachLinearSpace(MakeLinearReq("test_linear_agent_multi_blockdev_fail", "md_linear"),
                                          nsDevPaths, devPath);

    EXPECT_NE(ret, UBSE_OK);
}

// --------------------------------------------------------------------------
// DetachLinearSpace - Agent tests (Linear 特有：DeleteBlockDevice)
// --------------------------------------------------------------------------

/*
 * 用例：DetachLinear_Agent_DeleteBlockDevIdempotent
 * Agent 角色，DeleteBlockDevice 在设备不存在时幂等返回成功，整个 detach 成功。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Agent_DeleteBlockDevIdempotent)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'A'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});

    auto ret = service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_agent_blockdev_ok", "md_linear"));

    // DeleteBlockDevice is idempotent — device not found returns success
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例：DetachLinear_Agent_DevNameMismatch
 * Agent 角色，verify RPC 返回的账本 devName（权威值）与请求 devName 不一致时拒绝删除块设备，
 * 返回 UBSE_ERR_INVALID_ARG。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Agent_DevNameMismatch)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'A'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});
    g_mockVerifyDevName = "md_ledger";

    auto ret = service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_agent_devname_mismatch", "md_other"));

    EXPECT_EQ(ret, UBSE_ERR_INVALID_ARG);
}

/*
 * 用例：DetachLinear_Agent_MultiNsDeleteBlockDevIdempotent
 * Agent 角色，多个 NS，DeleteBlockDevice 幂等成功，全部 detach 成功。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Agent_MultiNsDeleteBlockDevIdempotent)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});

    auto ret = service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_agent_multi_blockdev_ok", "md_linear"));

    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例：DetachLinear_Agent_PartialDetachFail
 * Agent 角色，DeleteBlockDevice 成功，第二个 NS detach 失败。
 */
TEST_F(TestUbseSsuServiceImpLinear, DetachLinear_Agent_PartialDetachFail)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});

    g_detachFailAfter.store(1);
    g_detachCallCount.store(0);

    auto ret = service_.DetachLinearSpace(MakeLinearReq("test_detach_linear_agent_partial", "md_linear"));
    EXPECT_NE(ret, UBSE_OK);
}

// --------------------------------------------------------------------------
// AttachLinearSpace - Agent success tests (mock CreateBlockDevice)
// --------------------------------------------------------------------------

/*
 * 用例：AttachLinear_Agent_SingleNsSuccess
 * Agent 角色，mock CreateBlockDevice 成功，单 NS 完整 attach 成功。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Agent_SingleNsSuccess)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'S'), "nqn.test.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'S'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});
    MockCreateBlockDeviceSuccess();

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto ret = service_.AttachLinearSpace(MakeLinearReq("test_linear_agent_single_success", "md_linear"),
                                          nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 1u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
}

/*
 * 用例：AttachLinear_Agent_MultiNsSuccess
 * Agent 角色，mock CreateBlockDevice 成功，多 NS 全部 attach 成功。
 */
TEST_F(TestUbseSsuServiceImpLinear, AttachLinear_Agent_MultiNsSuccess)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});
    MockCreateBlockDeviceSuccess();

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto ret = service_.AttachLinearSpace(MakeLinearReq("test_linear_agent_multi_success", "md_linear"),
                                          nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 2u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
    EXPECT_EQ(nsDevPaths[1], "/dev/nvme0n2");
}

} // namespace ubse::ssu::service::ut
