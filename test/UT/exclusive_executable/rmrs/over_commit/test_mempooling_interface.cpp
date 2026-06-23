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

#include "test_mempooling_interface.h"

#include <gmock/gmock.h>
#include <mockcpp/mockcpp.hpp>

#include "mempool_borrow_module.h"
#include "mempooling_interface.cpp"
#include "mempooling_interface.h"
#include "mp_error.h"
#include "vm_mem_migrate_strategy.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::ut::over_commit {
using namespace mempooling::over_commit;
using namespace mempooling;

void TestMempoolingInterface::SetUp()
{
    Test::SetUp();
}

void TestMempoolingInterface::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

const outinterface::MemBorrowExecuteResult mockBorrowExecuteResult{.borrowIds = {"aaaa", "bbbb"},
                                                                   .presentNumaIds = {4, 5}};

const BorrowRecord mockBorrowRecordNode0Numa0a{.name = "aaaa",
                                               .size = 1073741824,
                                               .lentNode = "Node1",
                                               .lentMemId = {1},
                                               .lentSocketId = 0,
                                               .lentNuma = {{.numaId = 0, .lentSize = 1073741824}},
                                               .borrowNode = "Node0",
                                               .borrowLocalNuma = 0,
                                               .borrowRemoteNuma = 4,
                                               .borrowMemId = {0}};

const BorrowRecord mockBorrowRecordNode0Numa0b{.name = "bbbb",
                                               .size = 2147483648,
                                               .lentNode = "Node1",
                                               .lentMemId = {1},
                                               .lentSocketId = 0,
                                               .lentNuma = {{.numaId = 0, .lentSize = 2147483648}},
                                               .borrowNode = "Node0",
                                               .borrowLocalNuma = 0,
                                               .borrowRemoteNuma = 5,
                                               .borrowMemId = {0}};

constexpr MemMigrateResult mockMemMigrateResult{.pid = 123, .remoteNumaId = 4, .size = 1073741824, .maxRatio = 25};

const mempooling::exportV2::NumaInfo mockNumaInfoNode0Numa0{
    .metaData = {.nodeId = "Node0",
                 .hostName = "controller",
                 .numaId = 0,
                 .socketId = 0,
                 .isLocal = true,
                 .memTotal = 10485760,
                 .memFree = 5242880,
                 .numaPageInfo = {{2048, // pageSize: 2MB = 2048 KB
                                   {.pageSize = 2048, .hugePageTotal = 5120, .hugePageFree = 2560}}}}};

const mempooling::exportV2::NumaInfo mockNumaInfoNode0Numa4{
    .metaData = {.nodeId = "Node0",
                 .hostName = "controller",
                 .numaId = 4,
                 .socketId = 0, // 如果 NUMA4 属于 socket0，按实际填写
                 .isLocal = true,
                 .memTotal = 1048576,    // KB
                 .memFree = 524288,      // KB
                 .numaPageInfo = {{2048, // pageSize KB (2MB hugepage)
                                   {.pageSize = 2048, .hugePageTotal = 512, .hugePageFree = 256}}}}};

MpResult MemBorrowExecuteInOverCommitSuccess(const mempooling::SrcMemoryBorrowParam& srcParam,
                                             const std::vector<uint64_t>& borrowSizes,
                                             const mempooling::WaterMark& waterMark,
                                             mempooling::MemBorrowExecuteResult& borrowExecuteResult)
{
    borrowExecuteResult.borrowIds = {"aaaa", "bbbb"};
    borrowExecuteResult.presentNumaId = {4, 5};
    return MEM_POOLING_OK;
}

MpResult MemBorrowExecuteInOverCommitEmptySuccess(const mempooling::SrcMemoryBorrowParam& srcParam,
                                                  const std::vector<uint64_t>& borrowSizes,
                                                  const mempooling::WaterMark& waterMark,
                                                  mempooling::MemBorrowExecuteResult& borrowExecuteResult)
{
    borrowExecuteResult.borrowIds.clear();
    borrowExecuteResult.presentNumaId.clear();
    return MEM_POOLING_OK;
}

MpResult CollectBorrowRecordsNode0(BorrowRecordHelper* This, const std::string nodeId,
                                   std::vector<BorrowRecord>& borrowRecords)
{
    borrowRecords.clear();
    borrowRecords.emplace_back(mockBorrowRecordNode0Numa0a);
    borrowRecords.emplace_back(mockBorrowRecordNode0Numa0b);
    return MEM_POOLING_OK;
}

uint32_t ProcessMemMigrateRemoteIdSuccess(const outinterface::SrcMemoryBorrowParam& srcParasssm,
                                          const std::vector<outinterface::VMPresetParam>& vmParams,
                                          const std::vector<MemBorrowInfo> memBorrowInfo,
                                          const std::vector<BorrowRecord>& borrowRecord,
                                          std::vector<MemMigrateResult>& memMigrateResult)
{
    memMigrateResult.clear();
    memMigrateResult.emplace_back(mockMemMigrateResult);
    return MEM_POOLING_OK;
}

uint32_t ProcessMemMigrateRemoteIdEmptySuccess(const outinterface::SrcMemoryBorrowParam& srcParasssm,
                                               const std::vector<outinterface::VMPresetParam>& vmParams,
                                               const std::vector<MemBorrowInfo> memBorrowInfo,
                                               const std::vector<BorrowRecord>& borrowRecord,
                                               std::vector<MemMigrateResult>& memMigrateResult)
{
    memMigrateResult.clear();
    return MEM_POOLING_OK;
}

TEST_F(TestMempoolingInterface, UBSRMRSMemBorrowInvalidSocketIdFailed)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = -1, .srcNumaId = 0};
    const std::vector<uint64_t> borrowSizes{1073741824, 2147483648};
    constexpr outinterface::WaterMark waterMark{.highWaterMark = 85, .lowWaterMark = 80};
    outinterface::MemBorrowExecuteResult result;
    MOCKER_CPP(&OverCommitStorage::UpdateBindTypeDB, MpResult(*)(const std::string&, const NumaBindType))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(CollectUtil::GetNumaMemInfos, MpResult(*)(const std::string&, const std::vector<uint32_t>&,
                                                         std::unordered_map<std::uint16_t, std::vector<pid_t>>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = UBSRMRSMemBorrow(srcParam, borrowSizes, waterMark, result);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

MpResult TestCollectBorrowRecords(BorrowRecordHelper* This, const std::string nodeId, const int& numaId,
                                  std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.borrowNode = "Node0";
    record.name = "name1";
    record.size = 1024;
    borrowRecords.push_back(record);
    return 0;
}

TEST_F(TestMempoolingInterface, UBSRMRSMemMigrateFailed1)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = 0};
    const std::vector<outinterface::VMPresetParam> vmPresetParams{{.pid = 123, .ratio = 25}};
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsOnlyBorrowIn,
               MpResult(*)(BorrowRecordHelper*, const std::string, const int&, std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = UBSRMRSMemMigrate(srcParam, vmPresetParams, mockBorrowExecuteResult);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, UBSRMRSMemMigrateFailed2)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = 0};
    const std::vector<outinterface::VMPresetParam> vmPresetParams{{.pid = 123, .ratio = 25}};
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(CollectBorrowRecordsNode0));
    MOCKER(&outinterface::ProcessMemMigrateRemoteId).stubs().will(returnValue(MEM_POOLING_ERROR));

    const auto ret = UBSRMRSMemMigrate(srcParam, vmPresetParams, mockBorrowExecuteResult);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, UBSRMRSMemMigrateFailed4)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = 0};
    const std::vector<outinterface::VMPresetParam> vmPresetParams{{.pid = 123, .ratio = 25}};
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(CollectBorrowRecordsNode0));
    MOCKER(&outinterface::ProcessMemMigrateRemoteId).stubs().will(invoke(ProcessMemMigrateRemoteIdSuccess));

    const auto ret = UBSRMRSMemMigrate(srcParam, vmPresetParams, mockBorrowExecuteResult);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, UBSRMRSMemMigrateFailed5)
{
    const outinterface::SrcMemoryBorrowParam srcParam{"Node0", 0, 0};
    const std::vector<outinterface::VMPresetParam> vmPresetParams{{.pid = 123, .ratio = 25}};
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(CollectBorrowRecordsNode0));
    MOCKER(&outinterface::ProcessMemMigrateRemoteId).stubs().will(invoke(ProcessMemMigrateRemoteIdSuccess));
    MOCKER_CPP(&mempooling::over_commit::SendUCacheMigrationStrategy, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = UBSRMRSMemMigrate(srcParam, vmPresetParams, mockBorrowExecuteResult);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, UBSRMRSMemMigrateFailedWhenRatioERR)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = 0};
    const std::vector<outinterface::VMPresetParam> vmPresetParams{{.pid = 123, .ratio = 125}};
    const auto ret = UBSRMRSMemMigrate(srcParam, vmPresetParams, mockBorrowExecuteResult);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, UBSRMRSMemReturnFailed1)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = 0};
    const std::vector<std::string> borrowIds{"aaaa", "bbbb"};
    const std::vector migratePids{999};
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = UBSRMRSMemReturn(srcParam, borrowIds, migratePids);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, UBSRMRSMemReturnFailed4)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = 0};
    const std::vector<std::string> borrowIds{"aaaa", "bbbb"};
    const std::vector migratePids{999};
    const std::vector numaInfos{mockNumaInfoNode0Numa0, mockNumaInfoNode0Numa4};
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(CollectBorrowRecordsNode0));
    MOCKER(outinterface::ReturnBorrowId).stubs().will(returnValue(MEM_POOLING_ERROR));

    const auto ret = UBSRMRSMemReturn(srcParam, borrowIds, migratePids);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, ReturnBorrowIdFailed1)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = 0};
    const std::vector pids{123};
    const std::string borrowId;
    constexpr uint16_t remoteNumaId = 4;
    std::unordered_map<std::uint16_t, std::vector<pid_t>> borrowNumaId2Pids;
    ReturnNeedMaps returnNeedMaps{.borrowId2Size = {{"aaaa", 1048576}},
                                  .borrowIdNuma2Size = {{4, 1048576}},
                                  .borrowNumaId2Pids = borrowNumaId2Pids,
                                  .borrowId2NumaId = {}};

    const auto ret = ReturnBorrowId(srcParam, pids, borrowId, remoteNumaId, returnNeedMaps);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMempoolingInterface, ReturnBorrowIdFailed2)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = 0};
    const std::vector pids{123};
    const std::string borrowId = "aaaa";
    constexpr uint16_t remoteNumaId = 4;
    std::unordered_map<std::uint16_t, std::vector<pid_t>> borrowNumaId2Pids;

    ReturnNeedMaps returnNeedMaps{.borrowId2Size = {{"aaaa", 1048576}},
                                  .borrowIdNuma2Size = {{4, 1048576}},
                                  .borrowNumaId2Pids = borrowNumaId2Pids,
                                  .borrowId2NumaId = {}};
    MOCKER_CPP(&SetSmapRemoteNumaInfoSend::SendMsg, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_ERROR));

    const auto ret = ReturnBorrowId(srcParam, pids, borrowId, remoteNumaId, returnNeedMaps);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, ProcessBorrowIdsFailed)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = -1};
    std::vector<pid_t> migratePids;
    std::vector<std::string> borrowIds = {"aaaa", "bbbb"};
    std::unordered_map<std::uint16_t, std::vector<pid_t>> borrowNumaId2Pids;
    ReturnNeedMaps returnNeedMaps{.borrowId2Size = {{"aaaa", 1048576}},
                                  .borrowIdNuma2Size = {{4, 1048576}},
                                  .borrowNumaId2Pids = borrowNumaId2Pids,
                                  .borrowId2NumaId = {}};
    MOCKER_CPP(HasPidInMigratePids, bool (*)(const std::vector<pid_t>&, const std::vector<pid_t>&))
        .stubs()
        .will(returnValue(false));
    MOCKER_CPP(ReturnBorrowId, uint32_t(*)(const outinterface::SrcMemoryBorrowParam&, const std::vector<pid_t>&,
                                           const std::string&, const uint16_t&, ReturnNeedMaps&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    const auto ret = ProcessBorrowIds(srcParam, borrowIds, migratePids, returnNeedMaps);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, ProcessBorrowIdsSuccess)
{
    const outinterface::SrcMemoryBorrowParam srcParam{.srcNid = "Node0", .srcSocketId = 0, .srcNumaId = -1};
    std::vector<pid_t> migratePids;
    std::vector<std::string> borrowIds = {"aaaa"};
    std::unordered_map<std::uint16_t, std::vector<pid_t>> borrowNumaId2Pids;
    ReturnNeedMaps returnNeedMaps{.borrowId2Size = {{"aaaa", 1048576}},
                                  .borrowIdNuma2Size = {{4, 1048576}},
                                  .borrowNumaId2Pids = borrowNumaId2Pids,
                                  .borrowId2NumaId = {}};
    MOCKER_CPP(HasPidInMigratePids, bool (*)(const std::vector<pid_t>&, const std::vector<pid_t>&))
        .stubs()
        .will(returnValue(false));
    MOCKER_CPP(ReturnBorrowId, uint32_t(*)(const outinterface::SrcMemoryBorrowParam&, const std::vector<pid_t>&,
                                           const std::string&, const uint16_t&, ReturnNeedMaps&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    const auto ret = ProcessBorrowIds(srcParam, borrowIds, migratePids, returnNeedMaps);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

uint32_t RackGetAllNodeInfosMock()
{
    ubse::election::UbseRoleInfo info;
    info.nodeId = "0";
    std::vector<UbseRoleInfo> infoList;
    infoList.push_back(info);
    return 0;
}

TEST_F(TestMempoolingInterface, UBSRMRSSetWaterMarkFailed1)
{
    outinterface::WaterMark mark;
    mark.highWaterMark = 60;
    mark.lowWaterMark = 90;

    auto ret = UBSRMRSSetWaterMark(mark);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, UBSRMRSSetWaterMarkFailed2)
{
    outinterface::WaterMark mark;
    mark.highWaterMark = 90;
    mark.lowWaterMark = 60;

    MOCKER_CPP(&OverCommitStorage::UpdateWaterMark, MpResult(*)(const uint16_t, const uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto ret = UBSRMRSSetWaterMark(mark);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, UBSRMRSSetWaterMarkSuccess)
{
    outinterface::WaterMark mark;
    mark.highWaterMark = 90;
    mark.lowWaterMark = 60;

    MOCKER_CPP(&OverCommitStorage::UpdateWaterMark, MpResult(*)(const uint16_t, const uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    auto ret = UBSRMRSSetWaterMark(mark);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMempoolingInterface, GenpidsOnNumaFailed)
{
    int16_t srcNumaId = 1;
    std::vector<pid_t> pidsOnNuma = {123};
    mempooling::RmrsPidInfo info;
    info.pid = 123;
    info.totalLocalUsedMem = 1024;
    info.localNumaIds = {};
    info.totalRemoteUsedMem = 0;
    info.remoteNumaId = 4;

    std::vector<mempooling::RmrsPidInfo> pidInfos = {info};

    auto ret = GenpidsOnNuma(srcNumaId, pidsOnNuma, pidInfos);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, GenpidsOnNumaSuccess)
{
    int16_t srcNumaId = 1;
    std::vector<pid_t> pidsOnNuma = {123};
    mempooling::RmrsPidInfo info;
    info.pid = 123;
    info.totalLocalUsedMem = 1024;
    info.localNumaIds = {0};
    info.totalRemoteUsedMem = 0;
    info.remoteNumaId = 4;

    std::vector<mempooling::RmrsPidInfo> pidInfos = {info};

    auto ret = GenpidsOnNuma(srcNumaId, pidsOnNuma, pidInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMempoolingInterface, PrepareSocketIdFail1)
{
    MOCKER_CPP(&CollectUtil::GetNumaMemInfos, uint32_t(*)(const std::string& nodeId, const std::set<int16_t>& numaIds,
                                                          std::map<int, mempooling::NumaMetaData>& numaMemInfos))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::map<int, mempooling::NumaMetaData> numaMemInfos;
    std::string srcNid = "0";
    auto ret = PrepareSocketId(numaMemInfos, srcNid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, PrepareSocketIdFail2)
{
    std::map<int, mempooling::NumaMetaData> numaMemInfos{};
    mempooling::NumaMetaData data;
    numaMemInfos[1] = data;
    std::string srcNid = "0";
    MOCKER_CPP(&CollectUtil::GetNumaMemInfos, uint32_t(*)(const std::string& nodeId, const std::set<int16_t>& numaIds,
                                                          std::map<int, mempooling::NumaMetaData>& numaMemInfos))
        .stubs()
        .with(outBound(srcNid), any(), outBound(numaMemInfos))
        .will(returnValue(MEM_POOLING_OK));
    auto ret = PrepareSocketId(numaMemInfos, srcNid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolingInterface, PrepareSocketIdSuccess)
{
    std::map<int, mempooling::NumaMetaData> numaMemInfos{};
    mempooling::NumaMetaData data;
    numaMemInfos[0] = data;
    std::string srcNid = "0";
    MOCKER_CPP(&CollectUtil::GetNumaMemInfos, uint32_t(*)(const std::string& nodeId, const std::set<int16_t>& numaIds,
                                                          std::map<int, mempooling::NumaMetaData>& numaMemInfos))
        .stubs()
        .with(outBound(srcNid), any(), outBound(numaMemInfos))
        .will(returnValue(MEM_POOLING_OK));
    auto ret = PrepareSocketId(numaMemInfos, srcNid);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMempoolingInterface, UcacheMigrate_Enabled_OK)
{
    std::vector<outinterface::VMPresetParam> vmPresetParams = {{0, 0}};
    outinterface::MemBorrowExecuteResult result = {{"0"}, {0}};
    outinterface::SrcMemoryBorrowParam srcParam{};

    MOCKER_CPP(&mempooling::MpConfiguration::GetUcacheEnable, bool (*)()).stubs().will(returnValue(true));

    uint32_t ret = UcacheMigrate(vmPresetParams, result, srcParam);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMempoolingInterface, UcacheMigrate_Enabled_GenOK_SendFail)
{
    std::vector<outinterface::VMPresetParam> vmPresetParams = {{0, 0}};
    outinterface::MemBorrowExecuteResult result = {{"0"}, {0}};
    outinterface::SrcMemoryBorrowParam srcParam{};

    UCacheMigrationStrategyParam ucacheStrategy = {0, {0}, {0}, 0.1f};

    MOCKER_CPP(&mempooling::MpConfiguration::GetUcacheEnable, bool (*)()).stubs().will(returnValue(true));

    MOCKER(&GenUCacheMigrationStrategy)
        .stubs()
        .with(any(), any(), any(), any(), outBound(ucacheStrategy))
        .will(returnValue(0));

    MOCKER(&SendUCacheMigrationStrategy).stubs().will(returnValue(MEM_POOLING_ERROR));

    uint32_t ret = UcacheMigrate(vmPresetParams, result, srcParam);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);

    GlobalMockObject::verify();
}

TEST_F(TestMempoolingInterface, UcacheMigrate_Enabled_GenOK_SendOK)
{
    std::vector<outinterface::VMPresetParam> vmPresetParams = {{0, 0}};
    outinterface::MemBorrowExecuteResult result = {{"0"}, {0}};
    outinterface::SrcMemoryBorrowParam srcParam{};

    UCacheMigrationStrategyParam ucacheStrategy = {0, {0}, {0}, 0.1f};

    MOCKER_CPP(&mempooling::MpConfiguration::GetUcacheEnable, bool (*)()).stubs().will(returnValue(true));

    MOCKER(&GenUCacheMigrationStrategy)
        .stubs()
        .with(any(), any(), any(), any(), outBound(ucacheStrategy))
        .will(returnValue(0));

    MOCKER(&SendUCacheMigrationStrategy).stubs().will(returnValue(MEM_POOLING_OK));

    uint32_t ret = UcacheMigrate(vmPresetParams, result, srcParam);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

uint32_t UbseQueryResultNotExist(const std::string& borrowId, UbseMemResult& result)
{
    result.stage = UbseMemStage::UBSE_NOT_EXIST;
    return MEM_POOLING_OK;
}

uint32_t UbseQueryResultOnlyImport(const std::string& borrowId, UbseMemResult& result)
{
    result.stage = UbseMemStage::UBSE_ERR_ONLY_IMPORT;
    return MEM_POOLING_OK;
}

TEST_F(TestMempoolingInterface, CallUpdateUcacheUsageRatioTEST)
{
    std::vector<outinterface::VMPresetParam> vmPresetParams = {{0, 0}};
    std::vector<BorrowRecord> borrowRecord{};
    BorrowRecord record{};
    record.size = 0;
    borrowRecord.push_back(record);
    outinterface::SrcMemoryBorrowParam srcParam{};
    MOCKER_CPP(&mempooling::MpConfiguration::GetUcacheEnable, bool (*)()).stubs().will(returnValue(true));
    CallUpdateUcacheUsageRatio(vmPresetParams, borrowRecord, srcParam);
}

TEST_F(TestMempoolingInterface, MemFreeFail_StageNotExist)
{
    std::unordered_map<std::string, uint64_t> borrowId2Size = {{"name1", 1024}};
    std::unordered_map<uint16_t, uint64_t> borrowIdNuma2Size = {{5, 2048}};
    std::unordered_map<std::uint16_t, std::vector<pid_t>> borrowNumaId2Pids = {{5, {1000}}};
    std::unordered_map<std::string, std::uint16_t> borrowId2NumaId = {{"name1", 5}};
    ReturnNeedMaps returnNeedMaps = {borrowId2Size, borrowIdNuma2Size, borrowNumaId2Pids, borrowId2NumaId};
    uint16_t presentNumaId = 5;
    std::string borrowId = "name1";
    std::vector<pid_t> pids = {1000};
    outinterface::SrcMemoryBorrowParam srcParam{};

    // Mock SetSmapRemoteNumaLocalHandler 成功（第一次调用）
    MOCKER_CPP(&OverCommitMsgHandler::SetSmapRemoteNumaLocalHandler,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<MemBorrowInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    // Mock MemFreeWithOps 失败
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(MemBorrowExecutor::*)(const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    // Mock UbseQueryResult 返回 UBSE_NOT_EXIST
    MOCKER_CPP(UbseQueryResult, int (*)(const std::string&, UbseMemResult&))
        .stubs()
        .will(invoke(UbseQueryResultNotExist));

    // 执行被测函数
    uint32_t ret = ReturnBorrowId(srcParam, pids, borrowId, presentNumaId, returnNeedMaps);

    // 验证返回成功（因为stage为NOT_EXIST时，函数返回MEM_POOLING_OK）
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

// Test case 2: MemFreeWithOps 失败，UbseQueryResult 返回 UBSE_ERR_ONLY_IMPORT
TEST_F(TestMempoolingInterface, MemFreeFail_StageErrOnlyImport)
{
    std::unordered_map<std::string, uint64_t> borrowId2Size = {{"name1", 1024}};
    std::unordered_map<uint16_t, uint64_t> borrowIdNuma2Size = {{5, 2048}};
    std::unordered_map<std::uint16_t, std::vector<pid_t>> borrowNumaId2Pids = {{5, {1000}}};
    std::unordered_map<std::string, std::uint16_t> borrowId2NumaId = {{"name1", 5}};
    ReturnNeedMaps returnNeedMaps = {borrowId2Size, borrowIdNuma2Size, borrowNumaId2Pids, borrowId2NumaId};
    uint16_t presentNumaId = 5;
    std::string borrowId = "name1";
    std::vector<pid_t> pids = {1000};
    outinterface::SrcMemoryBorrowParam srcParam{};

    // Mock SetSmapRemoteNumaLocalHandler 成功（第一次调用）
    MOCKER_CPP(&OverCommitMsgHandler::SetSmapRemoteNumaLocalHandler,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<MemBorrowInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    // Mock MemFreeWithOps 失败
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(MemBorrowExecutor::*)(const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    // Mock UbseQueryResult 返回 UBSE_NOT_EXIST
    MOCKER_CPP(UbseQueryResult, int (*)(const std::string&, UbseMemResult&))
        .stubs()
        .will(invoke(UbseQueryResultOnlyImport));

    // 执行被测函数
    uint32_t ret = ReturnBorrowId(srcParam, pids, borrowId, presentNumaId, returnNeedMaps);

    // 验证返回失败（因为stage为ERR_ONLY_IMPORT时，函数返回MEM_POOLING_ERROR）
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
} // namespace mempooling::ut::over_commit