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
#include "test_ubse_ssu_service_imp_normal.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;

// ============================================================================
// TestUbseSsuServiceImpNormal implementation
// ============================================================================

void TestUbseSsuServiceImpNormal::SetUp()
{
    UbseSsuServiceImpTestBase::SetUp();
    ResetControllableMockState();
    SetupAdapterFuncs();

    MOCKER_CPP(&ubse::config::UbseGetStr)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any())
        .will(invoke(MockUbseGetStr));
}

void TestUbseSsuServiceImpNormal::TearDown()
{
    UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr;
    UbseSsuServiceImpTestBase::TearDown();
}

// --------------------------------------------------------------------------
// AttachSpace - Normal tests
// --------------------------------------------------------------------------

/*
 * 用例：AttachSpace_Master_AttachedIdempotent
 * 预置 ATTACHED 条目，master 幂等返回 nsDevPaths。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Master_AttachedIdempotent)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    UbseSsuAllocResult result;
    result.name = "test_attach_idempotent";
    result.strategy = UbseSsuAllocStrategy::NORMAL;
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns1, 4096));
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns2, 4096));
    PutLedgerEntry("test_attach_idempotent", UbseSsuNsState::ATTACHED, result);

    auto identity = MakeIdentity();
    auto req = MakeSpaceReq("test_attach_idempotent", "", identity);
    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(req, nsDevPaths);

    EXPECT_EQ(ret, UBSE_ERR_ALREADY_ATTACHED);
    ASSERT_EQ(nsDevPaths.size(), 2u);
    EXPECT_EQ(nsDevPaths[0], ns1.nsDevPath);
    EXPECT_EQ(nsDevPaths[1], ns2.nsDevPath);
}

/*
 * 用例：AttachSpace_Master_NotCreated
 * 预置非 CREATED 条目（如 IDLE），Attach 返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Master_NotCreated)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    UbseSsuAllocResult result;
    result.name = "test_attach_not_created";
    result.strategy = UbseSsuAllocStrategy::NORMAL;
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns1));
    PutLedgerEntry("test_attach_not_created", UbseSsuNsState::IDLE, result);

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_attach_not_created"), nsDevPaths),
              UBSE_SSU_ERROR_STATE_INVALID);
}

/*
 * 用例：AttachSpace_Master_Success
 * 预置 CREATED 条目，正常 attach 所有 NS，返回正确的 nsDevPaths。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Master_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);

    UbseSsuAllocResult result;
    result.name = "test_attach_success";
    result.strategy = UbseSsuAllocStrategy::NORMAL;
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns1));
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns2));
    PutLedgerEntry("test_attach_success", UbseSsuNsState::CREATED, result);

    auto identity = MakeIdentity();
    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(MakeSpaceReq("test_attach_success", "", identity), nsDevPaths);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 2u);
    EXPECT_EQ(nsDevPaths[0], ns1.nsDevPath);
    EXPECT_EQ(nsDevPaths[1], ns2.nsDevPath);

    // Verify ledger updated to ATTACHED
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_attach_success");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：AttachSpace_Master_PartialFail
 * 第二个 NS Attach 失败（g_attachFailAfter=1），回滚第一个已 attach 的 NS。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Master_PartialFail)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);

    UbseSsuAllocResult result;
    result.name = "test_attach_partial_fail";
    result.strategy = UbseSsuAllocStrategy::NORMAL;
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns1));
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns2));
    PutLedgerEntry("test_attach_partial_fail", UbseSsuNsState::CREATED, result);

    g_attachFailAfter.store(1); // 2nd call fails

    auto identity = MakeIdentity();
    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(MakeSpaceReq("test_attach_partial_fail", "", identity), nsDevPaths);

    EXPECT_NE(ret, UBSE_OK);
    // Verify ledger state rolled back to CREATED
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_attach_partial_fail");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 用例：AttachSpace_Master_RecordNotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Master_RecordNotFound)
{
    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_attach_not_found"), nsDevPaths),
              UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

// --------------------------------------------------------------------------
// AttachSpace - Agent tests
// --------------------------------------------------------------------------

/*
 * 用例：AttachSpace_Agent_VerifyFailed
 * Agent 角色，mock UbseGetMasterInfo 失败使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_VerifyFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_verify_fail"), nsDevPaths), UBSE_ERROR);
}

/*
 * 用例：AttachSpace_Agent_RpcGetCurrentNodeInfoFailed
 * Agent 角色，UbseGetCurrentNodeInfo 失败。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_RpcGetCurrentNodeInfoFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_role_fail"), nsDevPaths), UBSE_ERROR);
}

// --------------------------------------------------------------------------
// DetachSpace - Master tests
// --------------------------------------------------------------------------

/*
 * 用例：DetachSpace_Master_NotAttached_Created
 * 预置 CREATED 条目（已 detach），幂等返回。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Master_NotAttached_Created)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    UbseSsuAllocResult result;
    result.name = "test_detach_created";
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns1));
    PutLedgerEntry("test_detach_created", UbseSsuNsState::CREATED, result);

    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_created")), UBSE_ERR_NO_NEED_DETACH);
}

/*
 * 用例：DetachSpace_Master_NotAttached_Other
 * 预置 IDLE 条目，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Master_NotAttached_Other)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    UbseSsuAllocResult result;
    result.name = "test_detach_idle";
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns1));
    PutLedgerEntry("test_detach_idle", UbseSsuNsState::IDLE, result);

    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_idle")), UBSE_SSU_ERROR_STATE_INVALID);
}

/*
 * 用例：DetachSpace_Master_Success
 * 预置 ATTACHED 条目，全部 detach 成功，状态回退 CREATED。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Master_Success)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);

    UbseSsuAllocResult result;
    result.name = "test_detach_success";
    result.strategy = UbseSsuAllocStrategy::NORMAL;
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns1));
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns2));
    PutLedgerEntry("test_detach_success", UbseSsuNsState::ATTACHED, result);

    auto identity = MakeIdentity();
    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_success", "", identity)), UBSE_OK);

    // Verify ledger state back to CREATED
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_detach_success");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 用例：DetachSpace_Master_PartialFail
 * 第二个 NS Detach 失败（g_detachFailAfter=1），保持 ATTACHED 状态。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Master_PartialFail)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);

    UbseSsuAllocResult result;
    result.name = "test_detach_partial_fail";
    result.strategy = UbseSsuAllocStrategy::NORMAL;
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns1));
    result.nameSpaceList.push_back(UbseSsuServiceImpTestBase::MakeNameSpaceInfo(ns2));
    PutLedgerEntry("test_detach_partial_fail", UbseSsuNsState::ATTACHED, result);

    g_detachFailAfter.store(1); // 2nd call fails

    auto identity = MakeIdentity();
    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_partial_fail", "", identity)),
              UBSE_SSU_ERROR_DETACH_FAILED);

    // Verify ledger state remains ATTACHED
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_detach_partial_fail");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：DetachSpace_Master_RecordNotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Master_RecordNotFound)
{
    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_not_found")), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

// --------------------------------------------------------------------------
// DetachSpace - Agent tests
// --------------------------------------------------------------------------

/*
 * 用例：DetachSpace_Agent_VerifyFailed
 * Agent 角色，mock UbseGetMasterInfo 失败使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Agent_VerifyFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_agent_fail")), UBSE_ERROR);
}

// --------------------------------------------------------------------------
// AttachSpace - Agent error paths
// --------------------------------------------------------------------------

/*
 * 用例：AttachSpace_Agent_GetRoleFailed
 * Agent 路径，UbseGetRole 本身失败，AttachSpace 入口返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_GetRoleFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_role_error"), nsDevPaths), UBSE_ERROR);
}

/*
 * 用例：AttachSpace_Agent_VerifyEndpointNull
 * Agent 角色，UbseRpcEndpointFactory::GetRpcEndpoint 返回 nullptr 使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_VerifyEndpointNull)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).reset();
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).stubs().will(returnValue(std::shared_ptr<UbseRpcEndpoint>()));

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_endpoint_null"), nsDevPaths),
              UBSE_SSU_ERROR_RPC_SEND_FAILED);
}

/*
 * 用例：AttachSpace_Agent_VerifyRpcSendFailed
 * Agent 角色，UbseRpcEndpoint::UbseRpcSend 返回失败使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_VerifyRpcSendFailed)
{
    SetupAgentRoleAndRpcFail();

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_rpc_send_fail"), nsDevPaths), UBSE_ERROR);
}

/*
 * 用例：AttachSpace_Agent_VerifyRejected
 * Agent 角色，RPC 成功但 master 拒绝（errorCode != 0），返回 master 的 errorCode。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_VerifyRejected)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'R'), "nqn.rej.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'R'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo}, UBSE_ERR_ACCESS_DENIED);

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_verify_rejected"), nsDevPaths), UBSE_ERR_ACCESS_DENIED);
}

/*
 * 用例：AttachSpace_Agent_NsCountMismatch
 * Agent 角色，verify 响应中 nsVerifyList 与 nameSpaceList 数量不匹配。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_NsCountMismatch)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'A'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {verifyInfo});

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_count_mismatch"), nsDevPaths),
              UBSE_SSU_ERROR_NS_COUNT_MISMATCH);
}

// --------------------------------------------------------------------------
// AttachSpace - Agent success & partial fail
// --------------------------------------------------------------------------

/*
 * 用例：AttachSpace_Agent_SingleNsSuccess
 * Agent 角色，单个 NS 的 Attach 完整成功，验证 nsDevPaths 和返回码。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_SingleNsSuccess)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'S'), "nqn.test.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'A'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});

    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(MakeSpaceReq("test_agent_single_success"), nsDevPaths);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 1u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
}

/*
 * 用例：AttachSpace_Agent_MultiNsSuccess
 * Agent 角色，多个 NS 的 Attach 全部成功。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_MultiNsSuccess)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});

    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(MakeSpaceReq("test_agent_multi_success"), nsDevPaths);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 2u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
    EXPECT_EQ(nsDevPaths[1], "/dev/nvme0n2");
}

/*
 * 用例：AttachSpace_Agent_PartialFail
 * Agent 角色，第 2 个 NS attach 失败，回滚第 1 个已 attach 的 NS。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_PartialFail)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});

    g_attachFailAfter.store(1);
    g_attachCallCount.store(0);

    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(MakeSpaceReq("test_agent_attach_partial"), nsDevPaths);
    EXPECT_NE(ret, UBSE_OK);
}

/*
 * 用例：AttachSpace_Agent_SuccessEmptyNs
 * Agent 角色，RPC 成功但 verify 响应为空（默认初始化），attach 整体走通。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_SuccessEmptyNs)
{
    SetupAgentRoleAndRpcSuccess();

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_empty_ns"), nsDevPaths), UBSE_OK);
}

// --------------------------------------------------------------------------
// AttachSpace - Agent status update verification
// --------------------------------------------------------------------------

/*
 * 用例：AttachSpace_Agent_StatusUpdateOnSuccess
 * Agent 角色，Attach 全部成功后验证 SendStatusUpdate 被调用且 state = ATTACHED。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_StatusUpdateOnSuccess)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'S'), "nqn.test.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'A'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});

    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(MakeSpaceReq("test_agent_status_update_success"), nsDevPaths);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(g_statusUpdateCallCount, 1u);
    EXPECT_EQ(g_lastStatusUpdateName, "test_agent_status_update_success");
    EXPECT_EQ(g_lastStatusUpdateState, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：AttachSpace_Agent_StatusUpdateOnVerifyRejected
 * Agent 角色，master 拒绝时验证不发送状态更新（Verify失败直接返回，不再回写CREATED）。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_StatusUpdateOnVerifyRejected)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'R'), "nqn.rej.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'R'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo}, UBSE_ERR_ACCESS_DENIED);

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_status_update_rejected"), nsDevPaths),
              UBSE_ERR_ACCESS_DENIED);
    EXPECT_EQ(g_statusUpdateCallCount, 0u);
}

/*
 * 用例：AttachSpace_Agent_StatusUpdateOnNsCountMismatch
 * Agent 角色，NS 数量不匹配时验证不发送状态更新（直接返回错误，不再回写CREATED）。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_StatusUpdateOnNsCountMismatch)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'A'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {verifyInfo});

    std::vector<std::string> nsDevPaths;
    EXPECT_EQ(service_.AttachSpace(MakeSpaceReq("test_agent_status_update_mismatch"), nsDevPaths),
              UBSE_SSU_ERROR_NS_COUNT_MISMATCH);
    EXPECT_EQ(g_statusUpdateCallCount, 0u);
}

/*
 * 用例：AttachSpace_Agent_StatusUpdateRpcFailed
 * Agent 角色，Attach 成功后 SendStatusUpdate 失败（master 网络异常），
 * 验证 AttachSpace 仍返回 OK（当前代码忽略 UpdateStateOrNotify 的返回值）。
 */
TEST_F(TestUbseSsuServiceImpNormal, AttachSpace_Agent_StatusUpdateRpcFailed)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'S'), "nqn.test.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'A'));
    g_mockStatusUpdateResult = UBSE_ERROR;
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});

    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(MakeSpaceReq("test_agent_status_update_rpc_fail"), nsDevPaths);
    // AgentAttach 忽略 UpdateStateOrNotify 返回值，仍返回 UBSE_OK
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(g_statusUpdateCallCount, 1u);
    EXPECT_EQ(g_lastStatusUpdateState, UbseSsuNsState::ATTACHED);
}

// --------------------------------------------------------------------------
// DetachSpace - Agent success & failure
// --------------------------------------------------------------------------

/*
 * 用例：DetachSpace_Agent_GetRoleFailed
 * Agent 路径，UbseGetRole 本身失败，DetachSpace 入口返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Agent_GetRoleFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_agent_role_error")), UBSE_ERROR);
}

/*
 * 用例：DetachSpace_Agent_RpcGetCurrentNodeInfoFailed
 * Agent 角色，UbseGetCurrentNodeInfo 失败使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Agent_RpcGetCurrentNodeInfoFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_agent_rpc_fail")), UBSE_ERROR);
}

/*
 * 用例：DetachSpace_Agent_VerifyEndpointNull
 * Agent 角色，UbseRpcEndpointFactory::GetRpcEndpoint 返回 nullptr 使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Agent_VerifyEndpointNull)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).reset();
    MOCKER(&UbseRpcEndpointFactory::GetRpcEndpoint).stubs().will(returnValue(std::shared_ptr<UbseRpcEndpoint>()));

    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_agent_endpoint_null")),
              UBSE_SSU_ERROR_RPC_SEND_FAILED);
}

/*
 * 用例：DetachSpace_Agent_VerifyRpcSendFailed
 * Agent 角色，UbseRpcEndpoint::UbseRpcSend 返回失败使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Agent_VerifyRpcSendFailed)
{
    SetupAgentRoleAndRpcFail();

    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_rpc_send_fail")), UBSE_ERROR);
}

/*
 * 用例：DetachSpace_Agent_SuccessEmptyNs
 * Agent 角色，RPC 成功但 verify 响应为空（默认初始化），detach 整体走通。
 */
TEST_F(TestUbseSsuServiceImpNormal, DetachSpace_Agent_SuccessEmptyNs)
{
    SetupAgentRoleAndRpcSuccess();

    EXPECT_EQ(service_.DetachSpace(MakeSpaceReq("test_detach_agent_empty_ns")), UBSE_OK);
}

} // namespace ubse::ssu::service::ut
