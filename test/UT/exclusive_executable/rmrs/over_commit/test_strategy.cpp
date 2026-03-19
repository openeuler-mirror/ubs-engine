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

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "collect_util.h"
#include "mp_error.h"
#include "ubse_logger.h"
#define private public
#include "mp_json_util.h"
#include "mp_smap_helper.h"
#include "mp_smap_module.h"
#include "over_commit_msg.h"
#include "over_commit_msg_handler.h"
#include "page_file_helper.h"
#include "response_info_simpo.h"
#include "vm_mem_migrate_strategy.h"
#undef private
#include <unordered_map>
#include <vector>
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::outinterface {
using namespace ubse::log;
using namespace mempooling::smap;
class MockVMMemMigrateStrategy2 : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

MpResult MockCollectProcessInformation(const std::set<uint16_t> &remoteNuma, const std::vector<pid_t> &pids,
                                       std::map<pid_t, int16_t> &currentVmLocation,
                                       std::unordered_map<pid_t, VMInfo> &vmInfos, uint8_t ratio)
{
    currentVmLocation[1] = -1;
    VMInfo vmInfo1;
    vmInfo1.totalLocalUsedMem = 10;
    vmInfo1.capacity = 10;
    vmInfo1.totalRemoteUsedMem = 0;
    vmInfo1.ratio = 25;
    vmInfo1.localNumaIds = {0, 1};
    vmInfos[1] = vmInfo1;
    return MEM_POOLING_OK;
}

MpResult MockRackMemConvertJsonStr2Map(const JSON_STR &jsonStr, JSON_MAP &strMap)
{
    RmrsOutStream builder;
    ResponseInfo result;
    result.message = "{"
                     "\"MemTotal\": 16384,"
                     "\"MemFree\": 8192,"
                     "\"HugePages_Total\": 128,"
                     "\"HugePages_Free\": 64,"
                     "\"SocketId\": 0"
                     "}";
    strMap["MemTotal"] = "10";
    strMap["MemFree"] = "10";
    strMap["HugePages_Total"] = "10";
    strMap["HugePages_Free"] = "10";
    strMap["SocketId"] = "0";
    return MEM_POOLING_OK;
}

int TestGetSmapQueryProcessConfigFuncMock(int nid, mempooling::smap::ProcessPayload *result, int inLen, int *outLen)
{
    *outLen = 1;
    ProcessPayload payload;
    payload.pid = 1;
    result[0] = payload;
    return 0;
}

MpResult TestCollectBorrowRecords(BorrowRecordHelper *This, const std::string nodeId, const int &numaId,
                                  std::vector<BorrowRecord> &borrowRecords)
{
    BorrowRecord record;
    record.borrowNode = "Node0";
    record.name = "name1";
    record.size = 1024;
    borrowRecords.push_back(record);
    return 0;
}

static uint32_t MockRmrsNumaMemInfoCollectSuccess(const turbo::rmrs::NumaMemInfoCollectParam &numaMemInfoCollectParam,
                                                  turbo::rmrs::ResponseInfoSimpo &responseInfoSimpo)
{
    return 0;
}


TEST_F(MockVMMemMigrateStrategy2, RebalanceTestSuccessWhenStepEmpty)
{
    MOCKER_CPP(PageFileHelper::AllocateHugePages,
               MpResult(*)(const std::vector<MemBorrowInfoWithSrc> &memBorrowInfoWithSrcs))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    message::MempoolingMessage::rmrsNumaMemInfoCollect = &MockRmrsNumaMemInfoCollectSuccess;
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsOnlyBorrowIn,
               MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, const int &numaId,
                           std::vector<BorrowRecord> &borrowRecords))
        .stubs()
        .will(invoke(TestCollectBorrowRecords));
    MOCKER_CPP(&VMMemMigrateStrategy::CollectProcessInformation,
               MpResult(*)(const std::set<uint16_t> &remoteNuma, const std::vector<pid_t> &pids,
                           std::map<pid_t, int16_t> &currentVmLocation, std::unordered_map<pid_t, VMInfo> &vmInfos,
                           uint8_t ratio))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(UpdateContainerInfoInnode,
               MpResult(*)(const std::string &, const std::vector<pid_t> &, std::unordered_map<pid_t, VMInfo> &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR &jsonStr, JSON_MAP &strMap))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&FillNumaInfo, MpResult(*)(mempooling::outinterface::NumaMetaData &, JSON_MAP))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&VMMemMigrateStrategy::processSteps,
               MpResult(*)(int16_t srcNumaId, std::vector<MigrationStep> &, uint8_t, std::map<uint16_t, uint64_t> &,
                           std::vector<pid_t> &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    VMMemMigrateStrategy strategy;
    std::vector<pid_t> pids = {1};
    auto ret = strategy.Rebalance("node0", 0, pids, 25);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

MpResult MockHelpGetContainerPidNumaInfo(const std::string &srcNid, const std::vector<pid_t> &pidList,
                                         std::vector<PidInfo> &pidInfos)
{
    PidInfo pidInfo;
    pidInfo.localUsedMem = 10;
    pidInfo.remoteUsedMem = 0;
    pidInfo.remoteNumaId = -1;
    pidInfo.pid = 1;
    return MEM_POOLING_OK;
}

std::vector<MigrationStep> MockAllocate(std::map<uint16_t, int> &resourceCapacity, std::map<pid_t, int> &vmDemand,
                                        std::map<pid_t, int16_t> &currentVmLocation, std::map<pid_t, int> &vmQuota)
{
    std::vector<MigrationStep> vec;
    MigrationStep step;
    step.from = -1;
    step.to = 4;
    step.vm = 1;
    vec.push_back(step);
    return vec;
}

TEST_F(MockVMMemMigrateStrategy2, TestProcessSteps)
{
    std::vector<MigrationStep> vec;
    MigrationStep step;
    step.from = -1;
    step.to = 4;
    step.vm = 1;
    vec.push_back(step);
    VMMemMigrateStrategy strategy;
    std::map<uint16_t, uint64_t> remoteMap2Size;
    remoteMap2Size[4] = 10;
    std::vector<pid_t> pidsAll;
    pidsAll.push_back(1);
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(const uint16_t &srcNumaId,
                           const std::vector<over_commit::MemBorrowInfoWithSrc> &memBorrowInfosWithSrc))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(MpSmapHelper::MigrateOutInOverCommit,
               MpResult(*)(const std::vector<over_commit::MemMigrateResult> &memMigrateResults, const uint16_t ratio))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    auto ret = strategy.processSteps(0, vec, 25, remoteMap2Size, pidsAll);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(MockVMMemMigrateStrategy2, TestProcessStepsFailWHenSetFailed)
{
    std::vector<MigrationStep> vec;
    MigrationStep step;
    step.from = -1;
    step.to = 4;
    step.vm = 1;
    vec.push_back(step);
    VMMemMigrateStrategy strategy;
    std::map<uint16_t, uint64_t> remoteMap2Size;
    remoteMap2Size[4] = 10;
    std::vector<pid_t> pidsAll;
    pidsAll.push_back(1);
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(const uint16_t &srcNumaId,
                           const std::vector<over_commit::MemBorrowInfoWithSrc> &memBorrowInfosWithSrc))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(MpSmapHelper::MigrateOutInOverCommit,
               MpResult(*)(const std::vector<over_commit::MemMigrateResult> &memMigrateResults, const uint16_t ratio))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    auto ret = strategy.processSteps(0, vec, 25, remoteMap2Size, pidsAll);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(MockVMMemMigrateStrategy2, TestProcessStepsFailWHenMigrateOutFailed)
{
    std::vector<MigrationStep> vec;
    MigrationStep step;
    step.from = -1;
    step.to = 4;
    step.vm = 1;
    vec.push_back(step);
    VMMemMigrateStrategy strategy;
    std::map<uint16_t, uint64_t> remoteMap2Size;
    remoteMap2Size[4] = 10;
    std::vector<pid_t> pidsAll;
    pidsAll.push_back(1);
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(MpSmapHelper::SetSmapRemoteNumaInfo,
               MpResult(*)(const uint16_t &srcNumaId,
                           const std::vector<over_commit::MemBorrowInfoWithSrc> &memBorrowInfosWithSrc))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(MpSmapHelper::MigrateOutInOverCommit,
               MpResult(*)(const std::vector<over_commit::MemMigrateResult> &memMigrateResults, const uint16_t ratio))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto ret = strategy.processSteps(0, vec, 25, remoteMap2Size, pidsAll);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(MockVMMemMigrateStrategy2, RebalanceTest)
{
    std::map<uint16_t, uint64_t> resourceCapacity = {{4, 1048576}};
    std::map<pid_t, uint64_t> Demand = {{3335362, 0}, {3335363, 0}, {3335363, 0}};
    std::map<pid_t, uint64_t> Quota = {{3335362, 5436}, {3335363, 1788}, {3335364, 4195496}};
    std::map<pid_t, int16_t> currentVmLocation = {{3335362, -1}, {3335363, -1}, {3335363, -1}};
    VMMemMigrateStrategy strategy;
    strategy.Allocate(resourceCapacity, Demand, currentVmLocation, Quota);
}

SmapGetRemotePidsFunc GetMockSmapEnableProcessMigrateFunc()
{
    return &TestGetSmapQueryProcessConfigFuncMock;
}

} // namespace mempooling::outinterface
