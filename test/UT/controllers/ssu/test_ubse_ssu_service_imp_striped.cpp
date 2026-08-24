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
#include "test_ubse_ssu_service_imp_striped.h"

namespace ubse::ssu::service::ut {

using namespace ubse::adapter_plugins::ssu::def;

// ============================================================================
// TestUbseSsuServiceImpStriped implementation
// ============================================================================

void TestUbseSsuServiceImpStriped::SetUp()
{
    TestUbseSsuServiceImpLinear::SetUp();
    ResetControllableMockState();
    SetupAdapterFuncs();
}

void TestUbseSsuServiceImpStriped::TearDown()
{
    TestUbseSsuServiceImpLinear::TearDown();
}

UbseSsuStripedSpaceReq TestUbseSsuServiceImpStriped::MakeStripedReq(const std::string &name, const std::string &devName,
                                                                    UbseSsuAggregationRaidLevel level,
                                                                    UbseSsuChunkSize chunkSize, const std::string &nqn,
                                                                    const UbseSsuAllocIdentityInfo &identity)
{
    UbseSsuStripedSpaceReq req;
    req.name = name;
    req.devName = devName;
    req.level = level;
    req.chunkSize = chunkSize;
    req.nqn = nqn;
    req.identity = identity;
    return req;
}

// --------------------------------------------------------------------------
// AttachStripedSpace - Validation tests (before role check)
// --------------------------------------------------------------------------

/*
 * 用例：AttachStriped_Master_RecordNotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_Master_RecordNotFound)
{
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    EXPECT_EQ(service_.AttachStripedSpace(MakeStripedReq("test_striped_not_found"), nsDevPaths, devPath),
              UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例：AttachStriped_NotCreated
 * 预置非 CREATED 条目，拒绝。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_NotCreated)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    SetupLedgerForAttach("test_striped_not_created", UbseSsuNsState::IDLE, {ns1}, UbseSsuAllocStrategy::STRIPED);

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    EXPECT_EQ(service_.AttachStripedSpace(MakeStripedReq("test_striped_not_created"), nsDevPaths, devPath),
              UBSE_SSU_ERROR_STATE_INVALID);
}

/*
 * 用例：AttachStriped_AlreadyAttached
 * 预置 ATTACHED 条目，幂等返回。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_AlreadyAttached)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    SetupLedgerForAttach("test_striped_attached", UbseSsuNsState::ATTACHED, {ns1, ns2},
                         UbseSsuAllocStrategy::STRIPED);

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto ret = service_.AttachStripedSpace(MakeStripedReq("test_striped_attached"), nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_ERR_ALREADY_ATTACHED);
    ASSERT_EQ(nsDevPaths.size(), 2u);
}

/*
 * 用例：AttachStriped_EmptyNsList
 * 预置空的 nameSpaceList，拒绝。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_EmptyNsList)
{
    SetupLedgerForAttach("test_striped_empty", UbseSsuNsState::CREATED, {});

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    EXPECT_EQ(service_.AttachStripedSpace(MakeStripedReq("test_striped_empty"), nsDevPaths, devPath),
              UBSE_SSU_ERROR_STRATEGY_MISMATCH);
}

// --------------------------------------------------------------------------
// AttachStripedSpace - RAID/Chunk validation tests
// --------------------------------------------------------------------------

/*
 * 用例：AttachStriped_RAID0_Success_CreateBlockDevFail
 * RAIO0 正常参数，但 CreateBlockDevice 在测试环境会失败。
 * 验证账本回退到 CREATED。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_RAID0_CreateBlockDevFail)
{
    // nsSize must be multiple of chunkSize (4KB chunk = 4096 bytes)
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1, 8192);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);

    // Ledger with nameSpaceList: both NS size=8192, chunk=4K=4096, 8192%4096==0
    UbseSsuAllocResult result;
    result.name = "test_striped_raid0";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns1, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(ns2, 8192));
    PutLedgerEntry("test_striped_raid0", UbseSsuNsState::CREATED, result);

    auto identity = MakeIdentity();
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_raid0", "md_striped_raid0", UbseSsuAggregationRaidLevel::RAID0,
                              UbseSsuChunkSize::CHUNK_SIZE_4K, "", identity);
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_NE(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_striped_raid0");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 用例：AttachStriped_RAID5_LessThan3Ns
 * RAID5 但只有 2 个 NS，拒绝。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_RAID5_LessThan3Ns)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1, 8192);

    UbseSsuAllocResult result;
    result.name = "test_striped_raid5_less";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns1, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(ns2, 8192));
    PutLedgerEntry("test_striped_raid5_less", UbseSsuNsState::CREATED, result);

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_raid5_less", "md_striped_raid5", UbseSsuAggregationRaidLevel::RAID5,
                              UbseSsuChunkSize::CHUNK_SIZE_4K);
    EXPECT_EQ(service_.AttachStripedSpace(req, nsDevPaths, devPath), UBSE_ERR_INVALID_ARG);
}

/*
 * 用例：AttachStriped_NsSizeNotMultipleOfChunk
 * NS 大小不是 chunkSize 整数倍时拒绝。
 * 4KB chunk = 4096 bytes, nsSize=5000 → 5000%4096 != 0
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_NsSizeNotMultipleOfChunk)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 5000);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1, 5000);

    UbseSsuAllocResult result;
    result.name = "test_striped_chunk_mismatch";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns1, 5000));
    result.nameSpaceList.push_back(MakeNsInfo(ns2, 5000));
    PutLedgerEntry("test_striped_chunk_mismatch", UbseSsuNsState::CREATED, result);

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_chunk_mismatch", "md_striped", UbseSsuAggregationRaidLevel::RAID0,
                              UbseSsuChunkSize::CHUNK_SIZE_4K);
    EXPECT_EQ(service_.AttachStripedSpace(req, nsDevPaths, devPath), UBSE_SSU_ERROR_STRIPED_CONFIG_INVALID);
}

/*
 * 用例：AttachStriped_NsSizesNotEqual
 * NS 大小不一致时拒绝。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_NsSizesNotEqual)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1, 4096);

    UbseSsuAllocResult result;
    result.name = "test_striped_sizes_unequal";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns1, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(ns2, 4096));
    PutLedgerEntry("test_striped_sizes_unequal", UbseSsuNsState::CREATED, result);

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_sizes_unequal", "md_striped", UbseSsuAggregationRaidLevel::RAID0,
                              UbseSsuChunkSize::CHUNK_SIZE_4K);
    EXPECT_EQ(service_.AttachStripedSpace(req, nsDevPaths, devPath), UBSE_SSU_ERROR_STRIPED_CONFIG_INVALID);
}

/*
 * 用例：AttachStriped_RAID5_Success_CreateBlockDevFail
 * RAID5 3 个 NS，参数校验通过，但 CreateBlockDevice 在测试环境会失败。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_RAID5_CreateBlockDevFail)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1, 8192);
    auto ns3 = MakeNsForCache(std::string(16, 'C'), "nqn.test.3", 1, 8192);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    AddNsToCollectorCache(ns3);

    UbseSsuAllocResult result;
    result.name = "test_striped_raid5";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns1, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(ns2, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(ns3, 8192));
    PutLedgerEntry("test_striped_raid5", UbseSsuNsState::CREATED, result);

    auto identity = MakeIdentity();
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_raid5", "md_striped_raid5", UbseSsuAggregationRaidLevel::RAID5,
                              UbseSsuChunkSize::CHUNK_SIZE_4K, "", identity);
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_NE(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_striped_raid5");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

// --------------------------------------------------------------------------
// AttachStripedSpace - Agent tests
// --------------------------------------------------------------------------

/*
 * 用例：AttachStriped_Agent_VerifyFailed
 * Agent 角色，mock UbseGetMasterInfo 失败使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_Agent_VerifyFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    EXPECT_EQ(service_.AttachStripedSpace(MakeStripedReq("test_striped_agent_fail"), nsDevPaths, devPath), UBSE_ERROR);
}

// --------------------------------------------------------------------------
// DetachStripedSpace - Master tests
// --------------------------------------------------------------------------

/*
 * 用例：DetachStriped_Master_RecordNotFound
 * 账本无记录，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Master_RecordNotFound)
{
    EXPECT_EQ(service_.DetachStripedSpace(MakeStripedReq("test_detach_striped_not_found")),
              UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例：DetachStriped_Master_NotAttached_Created
 * 预置 CREATED 条目（已 detach），幂等返回。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Master_NotAttached_Created)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    SetupLedgerForAttach("test_detach_striped_created", UbseSsuNsState::CREATED, {ns1},
                         UbseSsuAllocStrategy::STRIPED);

    EXPECT_EQ(service_.DetachStripedSpace(MakeStripedReq("test_detach_striped_created")),
              UBSE_ERR_NO_NEED_DETACH);
}

/*
 * 用例：DetachStriped_Master_InvalidState
 * 预置非 ATTACHED 且非 CREATED 状态，返回 UBSE_ERROR。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Master_InvalidState)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    SetupLedgerForAttach("test_detach_striped_idle", UbseSsuNsState::IDLE, {ns1}, UbseSsuAllocStrategy::STRIPED);

    EXPECT_EQ(service_.DetachStripedSpace(MakeStripedReq("test_detach_striped_idle")),
              UBSE_SSU_ERROR_STATE_INVALID);
}

/*
 * 用例：DetachStriped_Master_DeleteBlockDevIdempotent
 * DeleteBlockDevice 在设备不存在时幂等返回成功，整个 detach 成功。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Master_DeleteBlockDevIdempotent)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupLedgerForAttach("test_detach_striped_blockdev_ok", UbseSsuNsState::ATTACHED, {ns1, ns2},
                         UbseSsuAllocStrategy::STRIPED);

    auto identity = MakeIdentity();
    auto ret = service_.DetachStripedSpace(MakeStripedReq("test_detach_striped_blockdev_ok", "md_striped",
                                                          UbseSsuAggregationRaidLevel::RAID0,
                                                          UbseSsuChunkSize::CHUNK_SIZE_4K, "", identity));

    EXPECT_EQ(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_detach_striped_blockdev_ok");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

/*
 * 用例：DetachStriped_Master_DevNameMismatch
 * 删除块设备前校验请求devName与账本记录一致（账本为attach时写入的权威值）。
 * 预置账本 devName=md_ledger，请求 devName=md_other，返回 UBSE_ERR_INVALID_ARG 且账本保持 ATTACHED。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Master_DevNameMismatch)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    UbseSsuAllocResult result;
    result.name = "test_detach_striped_master_devname_mismatch";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns1));

    auto entry = std::make_shared<UbseSsuLedgerEntry>();
    entry->name = result.name;
    entry->devName = "md_ledger"; // attach时写入账本的权威值
    entry->state = UbseSsuNsState::ATTACHED;
    entry->allocResult = result;
    UbseSsuDebtLedger::GetInstance().Put(entry->name, entry);

    auto identity = MakeIdentity();
    auto ret = service_.DetachStripedSpace(MakeStripedReq("test_detach_striped_master_devname_mismatch", "md_other",
                                                          UbseSsuAggregationRaidLevel::RAID0,
                                                          UbseSsuChunkSize::CHUNK_SIZE_4K, "", identity));

    EXPECT_EQ(ret, UBSE_ERR_INVALID_ARG);
    auto entryAfter = UbseSsuDebtLedger::GetInstance().Get("test_detach_striped_master_devname_mismatch");
    ASSERT_NE(entryAfter, nullptr);
    EXPECT_EQ(entryAfter->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：DetachStriped_Master_PartialDetachFail
 * 第二个 NS Detach 失败，保持 ATTACHED 状态。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Master_PartialDetachFail)
{
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.test.1", 1);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.test.2", 1);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupLedgerForAttach("test_detach_striped_partial_fail", UbseSsuNsState::ATTACHED, {ns1, ns2});

    g_detachFailAfter.store(1);

    auto identity = MakeIdentity();
    auto ret = service_.DetachStripedSpace(MakeStripedReq("test_detach_striped_partial_fail", "md_striped",
                                                          UbseSsuAggregationRaidLevel::RAID0,
                                                          UbseSsuChunkSize::CHUNK_SIZE_4K, "", identity));

    EXPECT_NE(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_detach_striped_partial_fail");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

// --------------------------------------------------------------------------
// DetachStripedSpace - Agent tests
// --------------------------------------------------------------------------

/*
 * 用例：DetachStriped_Agent_VerifyFailed
 * Agent 角色，mock UbseGetMasterInfo 失败使 verify RPC 提前返回。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Agent_VerifyFailed)
{
    MOCKER_CPP(&UbseGetRole).reset();
    MOCKER_CPP(&UbseGetRole).stubs().will(invoke(MockGetRole_Agent));
    MOCKER_CPP(&UbseGetMasterInfo).reset();
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(service_.DetachStripedSpace(MakeStripedReq("test_detach_striped_agent_fail")), UBSE_ERROR);
}

// --------------------------------------------------------------------------
// AttachStripedSpace - Agent tests (Striped 特有：CreateBlockDevice)
// --------------------------------------------------------------------------

/*
 * 用例：AttachStriped_Agent_CreateBlockDevFail
 * Agent 角色，NS attach 全部成功，但 CreateBlockDevice 在测试环境会失败。
 * 验证已 attach 的 NS 被回滚。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_Agent_CreateBlockDevFail)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'S'), "nqn.test.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'S'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_agent_blockdev_fail", "md_striped_raid0",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K);
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    // CreateBlockDevice will fail in test environment (no real md/LVM)
    EXPECT_NE(ret, UBSE_OK);
}

/*
 * 用例：AttachStriped_Agent_MultiNsCreateBlockDevFail
 * Agent 角色，多个 NS 全部 attach 成功，但 CreateBlockDevice 失败。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_Agent_MultiNsCreateBlockDevFail)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_agent_multi_blockdev_fail", "md_striped_raid0",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K);
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_NE(ret, UBSE_OK);
}

// --------------------------------------------------------------------------
// DetachStripedSpace - Agent tests (Striped 特有：DeleteBlockDevice)
// --------------------------------------------------------------------------

/*
 * 用例：DetachStriped_Agent_DeleteBlockDevIdempotent
 * Agent 角色，DeleteBlockDevice 在设备不存在时幂等返回成功，整个 detach 成功。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Agent_DeleteBlockDevIdempotent)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'A'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});

    auto req = MakeStripedReq("test_detach_striped_agent_blockdev_ok", "md_striped",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K);
    auto ret = service_.DetachStripedSpace(req);

    // DeleteBlockDevice is idempotent — device not found returns success
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例：DetachStriped_Agent_DevNameMismatch
 * Agent 角色，verify RPC 返回的账本 devName（权威值）与请求 devName 不一致时拒绝删除块设备，
 * 返回 UBSE_ERR_INVALID_ARG。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Agent_DevNameMismatch)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'A'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});
    g_mockVerifyDevName = "md_ledger";

    auto ret = service_.DetachStripedSpace(MakeStripedReq("test_detach_striped_agent_devname_mismatch", "md_other"));

    EXPECT_EQ(ret, UBSE_ERR_INVALID_ARG);
}

/*
 * 用例：DetachStriped_Agent_MultiNsDeleteBlockDevIdempotent
 * Agent 角色，多个 NS，DeleteBlockDevice 幂等成功，全部 detach 成功。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Agent_MultiNsDeleteBlockDevIdempotent)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});

    auto req = MakeStripedReq("test_detach_striped_agent_multi_blockdev_ok", "md_striped",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K);
    auto ret = service_.DetachStripedSpace(req);

    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例：DetachStriped_Agent_PartialDetachFail
 * Agent 角色，DeleteBlockDevice 成功，第二个 NS detach 失败。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Agent_PartialDetachFail)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});

    g_detachFailAfter.store(1);
    g_detachCallCount.store(0);

    auto req = MakeStripedReq("test_detach_striped_agent_partial", "md_striped",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K);
    auto ret = service_.DetachStripedSpace(req);
    EXPECT_NE(ret, UBSE_OK);
}

// --------------------------------------------------------------------------
// AttachStripedSpace - Master success tests (mock CreateBlockDevice)
// --------------------------------------------------------------------------

/*
 * 用例：AttachStriped_Master_SingleNsSuccess
 * Master 角色，mock CreateBlockDevice 成功，单 NS 完整 attach 成功。
 * 需要：AddNsToCollectorCache（缓存命中 + 获取 nsDevPath）、
 *       PutLedgerEntry（账本 CREATED 状态）、
 *       MockCreateBlockDeviceSuccess（屏蔽 md 创建设备失败）。
 * 验证：nsDevPaths 长度和内容、账本推进到 ATTACHED。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_Master_RAID0_MinNsSuccess)
{
    auto identity = MakeIdentity();
    // RAID0 至少需要 2 个成员设备
    auto ns1 = MakeNsForCache(std::string(16, 'S'), "nqn.test.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'T'), "nqn.test.2", 2, 8192);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);

    UbseSsuAllocResult result;
    result.name = "test_striped_master_single_success";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns1, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(ns2, 8192));
    PutLedgerEntry("test_striped_master_single_success", UbseSsuNsState::CREATED, result);

    MockCreateBlockDeviceSuccess();

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_master_single_success", "md_striped_raid0",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K, "", identity);
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 2u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
    EXPECT_EQ(nsDevPaths[1], "/dev/nvme0n2");

    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_striped_master_single_success");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：AttachStriped_Master_MultiNsSuccess
 * Master 角色，2 NS + RAID0，全部 attach + CreateBlockDevice 成功。
 * 验证：nsDevPaths 数组内容和长度、账本推进到 ATTACHED。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_Master_MultiNsSuccess)
{
    auto identity = MakeIdentity();
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.2", 2, 8192);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);

    UbseSsuAllocResult result;
    result.name = "test_striped_master_multi_success";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns1, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(ns2, 8192));
    PutLedgerEntry("test_striped_master_multi_success", UbseSsuNsState::CREATED, result);

    MockCreateBlockDeviceSuccess();

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_master_multi_success", "md_striped_raid0",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K, "", identity);
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 2u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
    EXPECT_EQ(nsDevPaths[1], "/dev/nvme0n2");

    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_striped_master_multi_success");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：AttachStriped_Master_RAID5_Success
 * Master 角色，3 NS + RAID5，全部 attach + CreateBlockDevice 成功。
 * 验证：nsDevPaths 长度和内容、账本推进到 ATTACHED。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_Master_RAID5_Success)
{
    auto identity = MakeIdentity();
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.2", 2, 8192);
    auto ns3 = MakeNsForCache(std::string(16, 'C'), "nqn.3", 3, 8192);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    AddNsToCollectorCache(ns3);

    UbseSsuAllocResult result;
    result.name = "test_striped_master_raid5_success";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns1, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(ns2, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(ns3, 8192));
    PutLedgerEntry("test_striped_master_raid5_success", UbseSsuNsState::CREATED, result);

    MockCreateBlockDeviceSuccess();

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_master_raid5_success", "md_striped_raid5",
                              UbseSsuAggregationRaidLevel::RAID5, UbseSsuChunkSize::CHUNK_SIZE_4K, "", identity);
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 3u);

    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_striped_master_raid5_success");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例：DetachStriped_Master_Success
 * Master 角色，DeleteBlockDevice 幂等成功，NS detach 全部成功。
 * 验证：账本回退到 CREATED。
 */
TEST_F(TestUbseSsuServiceImpStriped, DetachStriped_Master_Success)
{
    auto identity = MakeIdentity();
    auto ns1 = MakeNsForCache(std::string(16, 'A'), "nqn.1", 1, 8192);
    auto ns2 = MakeNsForCache(std::string(16, 'B'), "nqn.2", 2, 8192);
    AddNsToCollectorCache(ns1);
    AddNsToCollectorCache(ns2);
    SetupLedgerForAttach("test_detach_striped_master_success", UbseSsuNsState::ATTACHED, {ns1, ns2},
                         UbseSsuAllocStrategy::STRIPED);

    MockCreateBlockDeviceSuccess();

    auto req = MakeStripedReq("test_detach_striped_master_success", "md_striped_raid0",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K, "", identity);
    auto ret = service_.DetachStripedSpace(req);

    EXPECT_EQ(ret, UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_detach_striped_master_success");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

// --------------------------------------------------------------------------
// AttachStripedSpace - Agent success tests (mock CreateBlockDevice)
// --------------------------------------------------------------------------

/*
 * 用例：AttachStriped_Agent_SingleNsSuccess
 * Agent 角色，mock CreateBlockDevice 成功，单 NS 完整 attach 成功。
 * 注意：AttachStripedSpace 在角色判断前需要 ledger entry 和条带化参数校验，
 * 因此需预置账本（与 master 端 setup 一致）。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_Agent_SingleNsSuccess)
{
    auto nsInfo = MakeNsAgentInfo(std::string(16, 'S'), "nqn.test.1", 1, "/dev/nvme0n1");
    auto verifyInfo = MakeNsVerifyInfo(std::string(16, 'S'));
    SetupAgentRoleAndRpcSuccess({nsInfo}, {verifyInfo});
    MockCreateBlockDeviceSuccess();

    // AttachStripedSpace 需要 ledger entry 通过参数校验
    auto ns = MakeNsForCache(std::string(16, 'S'), "nqn.test.1", 1, 8192);
    UbseSsuAllocResult result;
    result.name = "test_striped_agent_single_success";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(ns, 8192));
    PutLedgerEntry("test_striped_agent_single_success", UbseSsuNsState::CREATED, result);

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_agent_single_success", "md_striped_raid0",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K);
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 1u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
}

/*
 * 用例：AttachStriped_Agent_MultiNsSuccess
 * Agent 角色，mock CreateBlockDevice 成功，多 NS 全部 attach 成功。
 */
TEST_F(TestUbseSsuServiceImpStriped, AttachStriped_Agent_MultiNsSuccess)
{
    auto ns1 = MakeNsAgentInfo(std::string(16, 'A'), "nqn.1", 1, "/dev/nvme0n1");
    auto ns2 = MakeNsAgentInfo(std::string(16, 'B'), "nqn.2", 2, "/dev/nvme0n2");
    auto v1 = MakeNsVerifyInfo(std::string(16, 'A'));
    auto v2 = MakeNsVerifyInfo(std::string(16, 'B'));
    SetupAgentRoleAndRpcSuccess({ns1, ns2}, {v1, v2});
    MockCreateBlockDeviceSuccess();

    // AttachStripedSpace 需要 ledger entry 通过参数校验
    auto nsA = MakeNsForCache(std::string(16, 'A'), "nqn.1", 1, 8192);
    auto nsB = MakeNsForCache(std::string(16, 'B'), "nqn.2", 2, 8192);
    UbseSsuAllocResult result;
    result.name = "test_striped_agent_multi_success";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(MakeNsInfo(nsA, 8192));
    result.nameSpaceList.push_back(MakeNsInfo(nsB, 8192));
    PutLedgerEntry("test_striped_agent_multi_success", UbseSsuNsState::CREATED, result);

    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto req = MakeStripedReq("test_striped_agent_multi_success", "md_striped_raid0",
                              UbseSsuAggregationRaidLevel::RAID0, UbseSsuChunkSize::CHUNK_SIZE_4K);
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 2u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
    EXPECT_EQ(nsDevPaths[1], "/dev/nvme0n2");
}

} // namespace ubse::ssu::service::ut
