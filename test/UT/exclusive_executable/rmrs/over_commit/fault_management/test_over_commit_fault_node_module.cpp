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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>
#include "ubse_def.h"
#include "mockcpp/mokc.h"
#include "over_commit_fault_management_handler.h"
#include "over_commit_fault_memid_helper.h"
#include "over_commit_fault_memid_module.h"

#define private public
#include "over_commit_fault_node_module.h"
#undef private

#include "ubse_error.h"
#include "ubse_storage.h"
#include "OsHelper/OsHelper.h"
#include "collect_util.h"
#include "common_delete_func.h"
#include "fault_node_module.h"
#include "mem_borrow_executor.h"
#include "mempool_borrow_module.h"
#include "mempooling_interface.h"
#include "mp_configuration.h"
#include "mp_mem_json_util.h"
#include "mp_memory_info.h"
#include "over_commit_pid_fault_pipeline.h"
#include "over_commit_storage.h"
#include "process_mem_pid_manager_def.h"
#include "rmrs_resource_query.h"
#include "securec.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {
// 由 over_commit_fault_node_module.cpp 提供，声明以便链接（非 static）
bool CanDirectlyReturnRemoteNumas(const std::vector<uint16_t>& remoteNumaIds);
} // namespace mempooling

namespace mempooling::over_commit {
using std::cout;
using std::endl;
class TestOverCommitFaultNodeModule : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestOverCommitFaultNodeModule, ProcessBorrowOutNodeFault_Succeed)
{
    std::string nodeId = "node1";
    std::vector<UbseNumaMemoryDebtInfo> infos;
    UbseNumaMemoryDebtInfo rec;
    rec.borrowNodeId = "node0";
    rec.lentNodeId = nodeId;
    rec.borrowMemId = {0};
    infos.push_back(rec);
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string&, std::vector<UbseNumaMemoryDebtInfo>&))

        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage,
               MpResult(*)(OverCommitFaultMemIdModule*, std::string, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultNodeModule, ProcessBorrowOutNodeFault_GetNumaMemDebtInfo_Failed)
{
    std::string nodeId = "node1";
    std::vector<UbseNumaMemoryDebtInfo> infos;
    UbseNumaMemoryDebtInfo rec;
    rec.borrowNodeId = "node0";
    rec.lentNodeId = nodeId;
    rec.borrowMemId = {0};
    infos.push_back(rec);
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string&, std::vector<UbseNumaMemoryDebtInfo>&))

        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);

    EXPECT_NE(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultNodeModule, ProcessBorrowOutNodeFault_BorrowRecords_Empty_Succeed)
{
    std::string nodeId = "node1";
    std::vector<UbseNumaMemoryDebtInfo> infos;
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string&, std::vector<UbseNumaMemoryDebtInfo>&))
        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage,
               MpResult(*)(OverCommitFaultMemIdModule*, std::string, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultNodeModule, ProcessBorrowOutNodeFault_LentNode_NotEq_Succeed)
{
    std::string nodeId = "node1";
    std::vector<UbseNumaMemoryDebtInfo> infos;
    UbseNumaMemoryDebtInfo rec;
    rec.borrowNodeId = "node0";
    rec.lentNodeId = "node2";
    rec.borrowMemId = {0};
    infos.push_back(rec);

    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string&, std::vector<UbseNumaMemoryDebtInfo>&))
        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage,
               MpResult(*)(OverCommitFaultMemIdModule*, std::string, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultNodeModule, ProcessBorrowOutNodeFault_MemIdFaultManage_Failed)
{
    std::string nodeId = "node1";
    std::vector<UbseNumaMemoryDebtInfo> infos;
    UbseNumaMemoryDebtInfo rec;
    rec.borrowNodeId = "node0";
    rec.lentNodeId = nodeId;
    rec.borrowMemId = {0};
    infos.push_back(rec);
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string&, std::vector<UbseNumaMemoryDebtInfo>&))
        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage,
               MpResult(*)(OverCommitFaultMemIdModule*, std::string, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);

    EXPECT_NE(ret, MEM_POOLING_OK);
}

MpSceneType MockGetSceneVM(MpConfiguration* This)
{
    MpSceneType::VIRTUAL_SCENE;
}
MpResult GetNumaBindTypeSingle(OverCommitStorage* This, const std::string& nodeId, NumaBindType& value)
{
    value = NumaBindType::BIND_SINGLE;
    return MEM_POOLING_OK;
}

MpResult GetNumaBindTypeMulti(OverCommitStorage* This, const std::string& nodeId, NumaBindType& value)
{
    value = NumaBindType::BIND_MULTIPLE;
    return MEM_POOLING_OK;
}

MpResult MockGetNumaMemInfos(const std::string& nodeId, const std::set<int16_t>& numaIds,
                             std::map<int, mempooling::NumaMetaData>& numaMemInfos)
{
    mempooling::NumaMetaData numaMetaData;
    numaMetaData.memFree = 4;
    numaMemInfos[-1] = numaMetaData;
    numaMemInfos[0] = numaMetaData;
    return MEM_POOLING_OK;
}

MpResult MockGetNumaMemInfos2(const std::string& nodeId, const std::set<int16_t>& numaIds,
                              std::map<int, mempooling::NumaMetaData>& numaMemInfos)
{
    mempooling::NumaMetaData numaMetaData;
    numaMetaData.memFree = 4;
    numaMemInfos[0] = numaMetaData;
    return MEM_POOLING_OK;
}

MpResult MockGetNumaMemInfos3(const std::string& nodeId, const std::set<int16_t>& numaIds,
                              std::map<int, mempooling::NumaMetaData>& numaMemInfos)
{
    mempooling::NumaMetaData numaMetaData;
    numaMetaData.memFree = 4;
    numaMemInfos[-1] = numaMetaData;
    return MEM_POOLING_OK;
}

MpResult MockCollectBorrowRecords(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.name = "abc";
    borrowRecords.emplace_back(record);
    return MEM_POOLING_OK;
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageFail1)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint16_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_NE(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageFail2)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint16_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam&, uint16_t, uint16_t, std::vector<VmNumaInfo>&,
                           mempooling::WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_NE(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageOk3)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint16_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam&, uint16_t, uint16_t, std::vector<VmNumaInfo>&,
                           mempooling::WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult&, uint64_t, std::vector<VmNumaInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageOk4)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint16_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam&, uint16_t, uint16_t, std::vector<VmNumaInfo>&,
                           mempooling::WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult&, uint64_t, std::vector<VmNumaInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(SrcMemoryBorrowParam, uint64_t, WaterMark, MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageOk5)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint16_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam&, uint16_t, uint16_t, std::vector<VmNumaInfo>&,
                           mempooling::WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult&, uint64_t, std::vector<VmNumaInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(SrcMemoryBorrowParam, uint64_t, WaterMark, MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdExecuteRpc, MpResult(*)(OverCommitFaultMemIdExecuteParam, std::string))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageOk6)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData&, uint64_t&, uint16_t&, uid_t&, std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam&, uint16_t, uint16_t, std::vector<VmNumaInfo>&,
                           mempooling::WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult&, uint64_t, std::vector<VmNumaInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(SrcMemoryBorrowParam, uint64_t, WaterMark, MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdExecuteRpc, MpResult(*)(OverCommitFaultMemIdExecuteParam, std::string))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::ReturnFaultMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam, std::string borrowId, uint16_t, uint64_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

// Mock: Get borrow records
MpResult CollectBorrowRecordsWithFaultMock(const std::string& nodeId, std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.borrowNode = "node_1";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = 1;
    record.size = 1048576; // 1GB in KB
    record.name = "borrow_id_001";
    borrowRecords.push_back(record);
    return MEM_POOLING_OK;
}

// Mock: RPC send
uint32_t UbseRpcSendMockSuccess(const ubse::com::UbseComEndpoint& endpoint, const UbseByteBuffer& req, void* ctx,
                                const UbseComRespHandler& handler)
{
    // Simulate successful processing by peer, assign to handler return value
    if (ctx != nullptr) {
        *(static_cast<uint32_t*>(ctx)) = MEM_POOLING_OK;
    }
    return MEM_POOLING_OK;
}

// Mock: Get Smap remote process function pointer
using SmapQueryFuncType = int (*)(int, smap::ProcessPayload*, int, int*);
int FakeSmapQueryProcess(int numaId, smap::ProcessPayload* payload, int maxNum, int* retLen)
{
    *retLen = 1;
    payload[0].pid = 1234;
    payload[0].ratio = 50; // 50% for GetVmRatioOnFaultNumaBySmap test
    payload[0].migrateMode = static_cast<uint8_t>(smap::MIG_RATIO_MODE);
    return MEM_POOLING_OK;
}
SmapQueryFuncType GetSmapGetRemoteProcessesFuncMock()
{
    return FakeSmapQueryProcess;
}

// Mock: GetSmapGetRemoteProcessesFunc returns ratio=0 for RemovePidsOnRemoteNuma tests
int FakeSmapQueryProcessZeroRatio(int numaId, smap::ProcessPayload* payload, int maxNum, int* retLen)
{
    *retLen = 1;
    payload[0].pid = 1234;
    payload[0].ratio = 0; // ratio=0 means can be removed directly
    payload[0].migrateMode = static_cast<uint8_t>(smap::MIG_RATIO_MODE);
    return MEM_POOLING_OK;
}
SmapQueryFuncType GetSmapGetRemoteProcessesFuncZeroRatioMock()
{
    return FakeSmapQueryProcessZeroRatio;
}

// Mock: GetSmapGetRemoteProcessesFunc returns nullptr
SmapQueryFuncType GetSmapGetRemoteProcessesFuncNullMock()
{
    return nullptr;
}

// Mock: smap query returns error
int FakeSmapQueryProcessError(int numaId, smap::ProcessPayload* payload, int maxNum, int* retLen)
{
    *retLen = 0;
    return MEM_POOLING_ERROR;
}
SmapQueryFuncType GetSmapGetRemoteProcessesFuncErrorMock()
{
    return FakeSmapQueryProcessError;
}

// Mock: smap query returns OK with retLen=0
int FakeSmapQueryProcessEmpty(int numaId, smap::ProcessPayload* payload, int maxNum, int* retLen)
{
    *retLen = 0;
    return MEM_POOLING_OK;
}
SmapQueryFuncType GetSmapGetRemoteProcessesFuncEmptyMock()
{
    return FakeSmapQueryProcessEmpty;
}

// Mock: smap query returns OK with multiple pids (ratio=0 for RemovePidsOnRemoteNuma)
int FakeSmapQueryProcessZeroRatioMulti(int numaId, smap::ProcessPayload* payload, int maxNum, int* retLen)
{
    *retLen = 3;
    payload[0].pid = 1001;
    payload[0].ratio = 0;
    payload[0].migrateMode = static_cast<uint8_t>(smap::MIG_RATIO_MODE);
    payload[1].pid = 1002;
    payload[1].ratio = 0;
    payload[1].migrateMode = static_cast<uint8_t>(smap::MIG_RATIO_MODE);
    payload[2].pid = 1003;
    payload[2].ratio = 0;
    payload[2].migrateMode = static_cast<uint8_t>(smap::MIG_RATIO_MODE);
    return MEM_POOLING_OK;
}
SmapQueryFuncType GetSmapGetRemoteProcessesFuncZeroRatioMultiMock()
{
    return FakeSmapQueryProcessZeroRatioMulti;
}

// Mock: smap query returns OK with multiple pids (original, ratio not set)
int FakeSmapQueryProcessMulti(int numaId, smap::ProcessPayload* payload, int maxNum, int* retLen)
{
    *retLen = 3;
    payload[0].pid = 1001;
    payload[1].pid = 1002;
    payload[2].pid = 1003;
    return MEM_POOLING_OK;
}
SmapQueryFuncType GetSmapGetRemoteProcessesFuncMultiMock()
{
    return FakeSmapQueryProcessMulti;
}

// Mock: SmapRemoveProcessTrackingHelper
bool g_smapRemoveCalled = false;
std::vector<pid_t> g_removedPids;
int SmapRemoveProcessTrackingHelperMock(const std::vector<pid_t>& pidVec, int flags)
{
    g_smapRemoveCalled = true;
    g_removedPids = pidVec;
    return MEM_POOLING_OK;
}

bool g_smapRemovePidsCalled = false;
std::vector<pid_t> g_removedPidsFromHelper;
MpResult SmapRemovePidsHelperMock(const std::vector<pid_t>& pids, int16_t remoteNumaId)
{
    g_smapRemovePidsCalled = true;
    g_removedPidsFromHelper = pids;
    return MEM_POOLING_OK;
}

// Mock: Get VmInfo
MpResult GetVmInfoImmediatelyMock(std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfosRaw)
{
    mempooling::exportV2::VmDomainInfo info;
    info.metaData.name = "vm_test_1";
    info.metaData.pid = 1234;
    mempooling::exportV2::VmDomainNumaInfo vmNumaInfo;
    vmNumaInfo.isLocal = false;
    vmNumaInfo.usedMem = 1024 * 1024;
    vmNumaInfo.socketId = 0;
    vmNumaInfo.numaId = 1;
    vmNumaInfo.pageSize = 2048;
    info.numaInfo[1] = vmNumaInfo;
    mempooling::exportV2::VmDomainNumaInfo localNumaInfo;
    localNumaInfo.isLocal = true;
    localNumaInfo.usedMem = 512 * 1024;
    localNumaInfo.socketId = 0;
    localNumaInfo.numaId = 0;
    localNumaInfo.pageSize = 2048;
    info.numaInfo[0] = localNumaInfo;
    vmDomainInfosRaw.push_back(info);
    return MEM_POOLING_OK;
}

// Mock: Smap enable/disable process migration
int SmapEnableProcessMigrateHelperMock(pid_t* pids, size_t size, int enable, int flags)
{
    return MEM_POOLING_OK;
}

// Mock: Smap execute process-level migration
MpResult SmapMigratePidMultiRemoteNumaHelperWithRetryMock(const MigrateEscapeMsg& msg)
{
    return MEM_POOLING_OK;
}

// Mock: GetSocketId
MpResult GetSocketIdMock(const std::string& nodeId, const int& numaId, uint16_t& socketId)
{
    socketId = 0;
    return MEM_POOLING_OK;
}

// Mock: GetWaterMark
MpResult GetWaterMarkMock(WaterMark& waterMark)
{
    waterMark.highWaterMark = 80;
    waterMark.lowWaterMark = 20;
    return MEM_POOLING_OK;
}

// Mock: MemBorrowExecuteInOverCommit
MpResult MemBorrowExecuteInOverCommitMock(const SrcMemoryBorrowParam& srcParam,
                                          const std::vector<uint64_t>& borrowSizes, const WaterMark& waterMark,
                                          MemBorrowExecuteResult& borrowExecuteResult, bool flag)
{
    borrowExecuteResult.borrowIds.push_back("new_borrow_id");
    borrowExecuteResult.presentNumaId.push_back(2);
    return MEM_POOLING_OK;
}

MpResult MockCollectBorrowRecordsNode(BorrowRecordHelper* This, const std::string nodeId,
                                      std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.name = "testName0";
    record.size = 1024;
    record.lentNode = "node1";
    record.lentMemId = {5678};
    record.lentSocketId = 1;
    record.lentNuma = {};
    record.borrowNode = "node0";
    record.borrowLocalNuma = 0;
    record.borrowMemId = {0, 1, 2, 3};
    record.borrowRemoteNuma = 4;

    BorrowRecord record1;
    record1.name = "testName1";
    record1.size = 1024;
    record1.lentNode = "node1";
    record1.lentMemId = {5678};
    record1.lentSocketId = 1;
    record1.lentNuma = {};
    record1.borrowNode = "node0";
    record1.borrowLocalNuma = 1;
    record1.borrowMemId = {4, 5, 6, 7};
    record1.borrowRemoteNuma = 4;
    borrowRecords.clear();
    borrowRecords.emplace_back(record);
    borrowRecords.emplace_back(record1);
    return MEM_POOLING_OK;
}

// =========================================================================
// Test case area
// =========================================================================

// 1. Test ProcessBorrowOutNodeFaultMultiNuma
TEST_F(TestOverCommitFaultNodeModule, ProcessBorrowOutNodeFaultMultiNuma_Succeed)
{
    // Stub: Simulate successful collection of ledger
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsWithFault,
               MpResult(*)(BorrowRecordHelper*, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsNode));

    MOCKER_CPP(&OverCommitFaultNodeModule::HandleFaultRemoteNumasPerBorrowNode,
               uint32_t(*)(const std::string&, const std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    // Stub: Simulate successful RPC send to remote node0
    MOCKER_CPP(&UbseRpcSend,
               uint32_t(*)(const ubse::com::UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(invoke(UbseRpcSendMockSuccess));

    OverCommitFaultNodeModule module;
    auto ret = module.ProcessBorrowOutNodeFaultMultiNuma("node_test");

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

// 2. Test HandleFaultRemoteNumasPerBorrowNode
TEST_F(TestOverCommitFaultNodeModule, HandleFaultRemoteNumasPerBorrowNode_Succeed)
{
    MOCKER_CPP(&UbseRpcSend,
               uint32_t(*)(const ubse::com::UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(invoke(UbseRpcSendMockSuccess));

    std::vector<BorrowRecord> borrowRecords;
    BorrowRecord record;
    record.borrowNode = "borrow_node_1";
    record.borrowRemoteNuma = 2;
    borrowRecords.push_back(record);

    OverCommitFaultNodeModule module;
    auto ret = module.HandleFaultRemoteNumasPerBorrowNode("local_node_1", borrowRecords);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

// 3. Test CalculateRemainingQuotaOnFaultNuma (Pure logic calculation)
TEST_F(TestOverCommitFaultNodeModule, CalculateRemainingQuotaOnFaultNuma_Correct)
{
    mempooling::outinterface::VMInfo vm;
    vm.totalLocalUsedMem = 200;
    vm.totalRemoteUsedMem = 800; // actualUsage = 1000
    vm.ratio = 20;               // 20%

    OverCommitFaultNodeModule module;
    uint64_t maxBorrow = module.CalculateRemainingQuotaOnFaultNuma(vm);

    // 1000 * 0.20 + 1 = 201
    EXPECT_EQ(maxBorrow, 201);
}

// 4. Test GetVmRatioOnFaultNumaBySmap
TEST_F(TestOverCommitFaultNodeModule, GetVmRatioOnFaultNumaBySmap_Succeed)
{
    // Stub: Simulate getting function pointer
    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncMock));
    std::unordered_map<pid_t, mempooling::outinterface::VMInfo> vmInfos;
    OverCommitFaultNodeModule module;

    auto ret = module.GetVmRatioOnFaultNumaBySmap(1, vmInfos);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_TRUE(vmInfos.find(1234) != vmInfos.end());
    EXPECT_EQ(vmInfos[1234].ratio, 50);
}

// 5. Test ConvertVminfoFormat (Pure data conversion)
TEST_F(TestOverCommitFaultNodeModule, ConvertVminfoFormat_Succeed)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    GetVmInfoImmediatelyMock(vmDomainInfos); // Reuse data generation function
    std::unordered_map<pid_t, mempooling::outinterface::VMInfo> vmInfos;
    OverCommitFaultNodeModule module;

    auto ret = module.ConvertVminfoFormat(vmDomainInfos, vmInfos);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_TRUE(vmInfos.find(1234) != vmInfos.end());
    EXPECT_EQ(vmInfos[1234].totalRemoteUsedMem, 1024 * 1024);
}

// 6. Test EvaculateVmsExecute
TEST_F(TestOverCommitFaultNodeModule, EvaculateVmsExecute_Succeed)
{
    // Stub: SmapEnableProcessMigrateHelperMock
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, size_t, int, int))
        .stubs()
        .will(invoke(SmapEnableProcessMigrateHelperMock));

    // Stub: SmapMigratePidMultiRemoteNumaHelperWithRetryMock
    MOCKER_CPP(&MpSmapHelper::SmapMigratePidMultiRemoteNumaHelperWithRetry, MpResult(*)(const MigrateEscapeMsg&))
        .stubs()
        .will(invoke(SmapMigratePidMultiRemoteNumaHelperWithRetryMock));

    // Stub: If SetSmapRemoteNumaInfoExec declaration, need to mock its dependency.
    // Here assume it's a global function, we mock it to avoid real Smap dispatch
    // If it's also a member of some class, adjust MOCKER accordingly
    MOCKER_CPP(&SetSmapRemoteNumaInfoExec, MpResult(*)(int16_t, int16_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    std::unordered_map<pid_t, mempooling::outinterface::VMInfo> vmInfos;
    mempooling::outinterface::VMInfo vm;
    vm.pid = 1234;
    vm.totalLocalUsedMem = 100;
    vm.totalRemoteUsedMem = 100;
    vmInfos[1234] = vm;

    std::vector<RemoteNumaFault> remoteNumas;
    BorrowRecord record;
    record.borrowNode = "local_node";
    RemoteNumaFault fault(0, 1, 1024, record);
    remoteNumas.push_back(fault);

    std::vector<mempooling::outinterface::VMResult> vmResults;
    mempooling::outinterface::VMResult vmRes; // pid=1234, remoteNuma=1, size=512
    vmRes.pid = 1234;
    vmRes.remoteNumaId = 1;
    vmRes.size = 0;
    vmRes.maxRatio = 10;
    vmResults.push_back(vmRes);

    OverCommitFaultNodeModule module;
    auto ret = module.EvaculateVmsExecute(1, vmInfos, remoteNumas, vmResults);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

// 7. Test ReturnFaultRemoteNumaMemory
TEST_F(TestOverCommitFaultNodeModule, ReturnFaultRemoteNumaMemory_Succeed)
{
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
               MpResult(*)(const std::string& name, bool isForceDelete, bool smapBack))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    // Ignore SetSmapRemoteNumaInfoExec real call, use cast Mock (if symbol not found, can comment out this line)
    MOCKER_CPP(SetSmapRemoteNumaInfoExec, MpResult(*)(int16_t, int16_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    std::vector<BorrowRecord> borrowRecords;
    BorrowRecord record;
    record.borrowLocalNuma = 0;
    record.size = 1024;
    record.name = "borrow_id_free";
    borrowRecords.push_back(record);

    std::vector<RemoteNumaFault> remoteNumas;
    RemoteNumaFault fault(0, 1, 1024, record);
    remoteNumas.push_back(fault);

    OverCommitFaultNodeModule module;
    auto ret = module.ReturnFaultRemoteNumaMemory(1, borrowRecords, remoteNumas);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

// 8. Test BorrowInNodeProcess overall process
TEST_F(TestOverCommitFaultNodeModule, BorrowInNodeProcess_Succeed)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(GetVmInfoImmediatelyMock));

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, size_t, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MemManager::GetSocketId, MpResult(*)(const std::string&, const int&, uint16_t&))
        .stubs()
        .will(invoke(GetSocketIdMock));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<uint64_t>&, const WaterMark&,
                           MemBorrowExecuteResult&, bool))
        .stubs()
        .will(invoke(MemBorrowExecuteInOverCommitMock));

    MOCKER_CPP(&MpSmapHelper::AllocateHugePages, MpResult(*)(std::vector<uint64_t>&, std::vector<uint64_t>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncMock));

    MOCKER_CPP(&MpSmapHelper::SmapMigratePidMultiRemoteNumaHelperWithRetry, MpResult(*)(const MigrateEscapeMsg&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsWithFault,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(SetSmapRemoteNumaInfoExec, MpResult(*)(int16_t, int16_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(*)(const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&BorrowIdInFaultProcess::Clear, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));

    FaultRecordsInNode faultRecordsInNode;
    faultRecordsInNode.nodeId = "local_node";

    BorrowRecord record;
    record.borrowNode = "local_node";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = 1;
    record.name = "borrow_1";
    record.size = 1024;
    faultRecordsInNode.faultRecords.push_back(record);

    OverCommitFaultNodeModule module;

    auto ret = module.BorrowInNodeProcess(faultRecordsInNode);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

// =========================================================================
// 9. Test EvaculateVmsFromFaultNuma successful main process// =========================================================================
TEST_F(TestOverCommitFaultNodeModule, EvaculateVmsFromFaultNuma_Succeed)
{
    // 1. Stub: Get Smap remote process list and ratio (reuse global Mock: pid=1234, ratio=50)
    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncMock));

    // 2. Stub: Smap migration switch control
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, size_t, int, int))
        .stubs()
        .will(invoke(SmapEnableProcessMigrateHelperMock));

    // 3. Stub: Smap remote to remote actual migration execution
    MOCKER_CPP(&MpSmapHelper::SmapMigratePidMultiRemoteNumaHelperWithRetry, MpResult(*)(const MigrateEscapeMsg&))
        .stubs()
        .will(invoke(SmapMigratePidMultiRemoteNumaHelperWithRetryMock));

    // 4. Stub: Collect ledger and set remote Numa capacity (to make ReSetRemoteNumaInfo in EvaculateVmsExecute run through)
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsWithFault,
               MpResult(*)(BorrowRecordHelper*, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    // If SetSmapRemoteNumaInfoExec undefined, add at top of file:
    // extern MpResult SetSmapRemoteNumaInfoExec(int16_t, int16_t, uint64_t);
    MOCKER_CPP(SetSmapRemoteNumaInfoExec, MpResult(*)(int16_t, int16_t, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    // === Construct perfect closed-loop input parameters ===

    // Parameter 1: Construct mapping relationship (fault remote NUMA 1 -> local NUMA 0)
    std::unordered_map<int16_t, std::set<int16_t>> remoteNumaId2LocalNumaId;
    remoteNumaId2LocalNumaId[1] = {0};

    // Parameter 2: Fault remote NUMA ID
    int16_t faultNumaId = 1;

    // Parameter 3: Construct VM information (must correspond to Mock function pid=1234)
    std::unordered_map<pid_t, mempooling::outinterface::VMInfo> vmInfos;
    mempooling::outinterface::VMInfo vm;
    vm.pid = 1234;
    vm.totalLocalUsedMem = 1000;
    vm.totalRemoteUsedMem = 1000;
    vm.ratio = 0; // This value will be updated to 50 by GetVmRatioOnFaultNumaBySmap
    // Must indicate this VM is on local NUMA 0, otherwise EvaculateVmsStrategyByLocalNuma won't select it
    MetaNumaInfo metaNuma;
    metaNuma.numaId = 0;
    metaNuma.isLocalNuma = true;
    vm.metaNumaInfos.push_back(metaNuma);
    vmInfos[1234] = vm;

    // Parameter 4: Construct newly borrowed remote memory resources (borrowing node is local 0, newly borrowed remote is 2)
    std::vector<RemoteNumaFault> remoteNumas;
    BorrowRecord record;
    record.borrowNode = "local_node";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = 2;
    record.size = 2048; // Available quota 2048
    RemoteNumaFault fault(0, 2, 2048, record);
    remoteNumas.push_back(fault);

    // === Execute and verify ===
    OverCommitFaultNodeModule module;
    auto ret = module.EvaculateVmsFromFaultNuma(remoteNumaId2LocalNumaId, faultNumaId, vmInfos, remoteNumas);

    // Expect main chain to run smoothly, return OK
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

// =========================================================================
// 10. Test EvaculateVmsFromFaultNuma exception process (mapping table doesn't find fault NumA)
// =========================================================================
TEST_F(TestOverCommitFaultNodeModule, EvaculateVmsFromFaultNuma_NotFound_Fail)
{
    // Stub: Only ensure first step Smap query doesn't error
    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncMock));

    // Construct mapping relationship (empty mapping table)
    std::unordered_map<int16_t, std::set<int16_t>> remoteNumaId2LocalNumaId;

    int16_t faultNumaId = 99; // Deliberately pass an ID not in map
    std::unordered_map<pid_t, mempooling::outinterface::VMInfo> vmInfos;
    std::vector<RemoteNumaFault> remoteNumas;

    OverCommitFaultNodeModule module;
    auto ret = module.EvaculateVmsFromFaultNuma(remoteNumaId2LocalNumaId, faultNumaId, vmInfos, remoteNumas);

    // Expect error return when find(faultNumaId) fails
    EXPECT_NE(ret, MEM_POOLING_OK);
}
// =========================================================================
// RemovePidsOnRemoteNuma
// =========================================================================

/*
 * 用例描述：smap函数指针为nullptr时，RemovePidsOnRemoteNuma应安全返回，不调用移除逻辑
 * 测试步骤：
 * 1. Mock GetSmapGetRemoteProcessesFunc 返回 nullptr
 * 2. 调用 RemovePidsOnRemoteNuma
 * 预期结果：
 * 1. 函数正常返回，不崩溃
 * 2. SmapRemoveProcessTrackingHelper 不被调用
 */
TEST_F(TestOverCommitFaultNodeModule, RemovePidsOnRemoteNuma_SmapFuncNull_NoAction)
{
    g_smapRemovePidsCalled = false;
    g_removedPidsFromHelper.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncNullMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemovePidsHelper, MpResult(*)(const std::vector<pid_t>&, int16_t))
        .stubs()
        .will(invoke(SmapRemovePidsHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(1);

    EXPECT_FALSE(g_smapRemovePidsCalled);
}

TEST_F(TestOverCommitFaultNodeModule, RemovePidsOnRemoteNuma_SmapQueryFailed_NoAction)
{
    g_smapRemovePidsCalled = false;
    g_removedPidsFromHelper.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncErrorMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemovePidsHelper, MpResult(*)(const std::vector<pid_t>&, int16_t))
        .stubs()
        .will(invoke(SmapRemovePidsHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(1);

    EXPECT_FALSE(g_smapRemovePidsCalled);
}

TEST_F(TestOverCommitFaultNodeModule, RemovePidsOnRemoteNuma_NoPids_NoAction)
{
    g_smapRemovePidsCalled = false;
    g_removedPidsFromHelper.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncEmptyMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemovePidsHelper, MpResult(*)(const std::vector<pid_t>&, int16_t))
        .stubs()
        .will(invoke(SmapRemovePidsHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(1);

    EXPECT_FALSE(g_smapRemovePidsCalled);
}

TEST_F(TestOverCommitFaultNodeModule, RemovePidsOnRemoteNuma_WithPids_RemoveAll)
{
    g_smapRemovePidsCalled = false;
    g_removedPidsFromHelper.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncZeroRatioMultiMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemovePidsHelper, MpResult(*)(const std::vector<pid_t>&, int16_t))
        .stubs()
        .will(invoke(SmapRemovePidsHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(2);

    EXPECT_TRUE(g_smapRemovePidsCalled);
    ASSERT_EQ(g_removedPidsFromHelper.size(), 3u);
    EXPECT_EQ(g_removedPidsFromHelper[0], 1001);
    EXPECT_EQ(g_removedPidsFromHelper[1], 1002);
    EXPECT_EQ(g_removedPidsFromHelper[2], 1003);
}

TEST_F(TestOverCommitFaultNodeModule, RemovePidsOnRemoteNuma_SinglePid_RemoveCorrectly)
{
    g_smapRemovePidsCalled = false;
    g_removedPidsFromHelper.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncZeroRatioMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemovePidsHelper, MpResult(*)(const std::vector<pid_t>&, int16_t))
        .stubs()
        .will(invoke(SmapRemovePidsHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(1);

    EXPECT_TRUE(g_smapRemovePidsCalled);
    ASSERT_EQ(g_removedPidsFromHelper.size(), 1u);
    EXPECT_EQ(g_removedPidsFromHelper[0], 1234);
}

// =========================================================================
// Remote NUMA Idle Fallback tests
// =========================================================================

MpResult MemBorrowExecuteForFaultSuccessMock(const SrcMemoryBorrowParam& srcParam,
                                             const std::vector<uint64_t>& borrowSizes, const WaterMark& waterMark,
                                             MemBorrowExecuteResult& borrowExecuteResult,
                                             const ProcessMemUsrInfo& processMemUsrInfo)
{
    borrowExecuteResult.borrowIds.push_back("new_borrow_id");
    borrowExecuteResult.presentNumaId.push_back(2);
    return MEM_POOLING_OK;
}

// Mock: OsHelper::GetMemInfoByNumaId returns idle (memFree == memTotal)
MpResult GetMemInfoByNumaIdIdleMock(const uint16_t& numaId, exportV2::NumaInfo& info)
{
    info.metaData.memFree = 1024;
    info.metaData.memTotal = 1024;
    return MEM_POOLING_OK;
}

// Mock: OsHelper::GetMemInfoByNumaId returns non-idle (memFree != memTotal)
MpResult GetMemInfoByNumaIdNonIdleMock(const uint16_t& numaId, exportV2::NumaInfo& info)
{
    info.metaData.memFree = 512;
    info.metaData.memTotal = 1024;
    return MEM_POOLING_OK;
}

// Mock: OsHelper::GetMemInfoByNumaId fails
MpResult GetMemInfoByNumaIdFailMock(const uint16_t& numaId, exportV2::NumaInfo& info)
{
    return MEM_POOLING_ERROR;
}

// Mock: SmapQueryProcessConfigHelper returns empty list (no processes)
MpResult SmapQueryProcessConfigEmptyMock(int nid, std::vector<smap::ProcessPayload>& processPayloadList)
{
    return MEM_POOLING_OK;
}

// Mock: SmapQueryProcessConfigHelper returns one process
MpResult SmapQueryProcessConfigWithProcessMock(int nid, std::vector<smap::ProcessPayload>& processPayloadList)
{
    smap::ProcessPayload payload;
    payload.pid = 1234;
    processPayloadList.push_back(payload);
    return MEM_POOLING_OK;
}

/*
 * 用例描述：CanDirectlyReturnRemoteNumas传入空remoteNumaIds时，应返回false
 * 测试步骤：
 * 1. 构造空的remoteNumaIds向量
 * 2. 调用CanDirectlyReturnRemoteNumas
 * 预期结果：
 * 1. 返回false
 */
TEST_F(TestOverCommitFaultNodeModule, CanDirectlyReturnRemoteNumas_EmptyNumaIds_ReturnsFalse)
{
    std::vector<uint16_t> emptyNumaIds;
    EXPECT_FALSE(CanDirectlyReturnRemoteNumas(emptyNumaIds));
}

/*
 * 用例描述：CanDirectlyReturnRemoteNumas所有NUMA都空闲且无进程时，应返回true
 * 测试步骤：
 * 1. Mock SmapQueryProcessConfigHelper 返回空列表
 * 2. Mock GetMemInfoByNumaId 返回 memFree==memTotal
 * 3. 构造包含2个numaId的向量
 * 4. 调用CanDirectlyReturnRemoteNumas
 * 预期结果：
 * 1. 返回true
 */
TEST_F(TestOverCommitFaultNodeModule, CanDirectlyReturnRemoteNumas_AllIdleNoProcesses_ReturnsTrue)
{
    MOCKER_CPP(&MpSmapHelper::SmapQueryProcessConfigHelper, MpResult(*)(int, std::vector<smap::ProcessPayload>&))
        .stubs()
        .will(invoke(SmapQueryProcessConfigEmptyMock));

    MOCKER_CPP(&exportV2::OsHelper::GetMemInfoByNumaId, MpResult(*)(const uint16_t&, exportV2::NumaInfo&))
        .stubs()
        .will(invoke(GetMemInfoByNumaIdIdleMock));

    std::vector<uint16_t> numaIds = {1, 2};
    EXPECT_TRUE(CanDirectlyReturnRemoteNumas(numaIds));
}

/*
 * 用例描述：CanDirectlyReturnRemoteNumas有NUMA非空闲时，应返回false
 * 测试步骤：
 * 1. Mock SmapQueryProcessConfigHelper 返回空列表
 * 2. Mock GetMemInfoByNumaId 返回 memFree!=memTotal
 * 3. 构造包含2个numaId的向量
 * 4. 调用CanDirectlyReturnRemoteNumas
 * 预期结果：
 * 1. 返回false
 */
TEST_F(TestOverCommitFaultNodeModule, CanDirectlyReturnRemoteNumas_NoneIdle_ReturnsFalse)
{
    MOCKER_CPP(&MpSmapHelper::SmapQueryProcessConfigHelper, MpResult(*)(int, std::vector<smap::ProcessPayload>&))
        .stubs()
        .will(invoke(SmapQueryProcessConfigEmptyMock));

    MOCKER_CPP(&exportV2::OsHelper::GetMemInfoByNumaId, MpResult(*)(const uint16_t&, exportV2::NumaInfo&))
        .stubs()
        .will(invoke(GetMemInfoByNumaIdNonIdleMock));

    std::vector<uint16_t> numaIds = {1, 2};
    EXPECT_FALSE(CanDirectlyReturnRemoteNumas(numaIds));
}

/*
 * 用例描述：CanDirectlyReturnRemoteNumas中GetMemInfoByNumaId失败时，应返回false
 * 测试步骤：
 * 1. Mock SmapQueryProcessConfigHelper 返回空列表
 * 2. Mock GetMemInfoByNumaId 返回 MEM_POOLING_ERROR
 * 3. 构造包含1个numaId的向量
 * 4. 调用CanDirectlyReturnRemoteNumas
 * 预期结果：
 * 1. 返回false
 */
TEST_F(TestOverCommitFaultNodeModule, CanDirectlyReturnRemoteNumas_GetMemInfoFailed_ReturnsFalse)
{
    MOCKER_CPP(&MpSmapHelper::SmapQueryProcessConfigHelper, MpResult(*)(int, std::vector<smap::ProcessPayload>&))
        .stubs()
        .will(invoke(SmapQueryProcessConfigEmptyMock));

    MOCKER_CPP(&exportV2::OsHelper::GetMemInfoByNumaId, MpResult(*)(const uint16_t&, exportV2::NumaInfo&))
        .stubs()
        .will(invoke(GetMemInfoByNumaIdFailMock));

    std::vector<uint16_t> numaIds = {1};
    EXPECT_FALSE(CanDirectlyReturnRemoteNumas(numaIds));
}

/*
 * 用例描述：ProcessNewBorrowFlow中前置检查所有远端NUMA空闲且无进程时，应直接释放oldBorrowIds并返回OK
 * 测试步骤：
 * 1. Mock SmapQueryProcessConfigHelper 返回空列表
 * 2. Mock GetMemInfoByNumaId 返回idle
 * 3. Mock MemFreeWithOps 返回成功
 * 4. 构造records并调用ProcessNewBorrowFlow
 * 预期结果：
 * 1. 返回 MEM_POOLING_OK
 * 2. MemFreeWithOps 被调用（对应oldBorrowIds数量）
 */
TEST_F(TestOverCommitFaultNodeModule, ProcessNewBorrowFlow_PreCheck_AllNumasIdle_FreesOldBorrowIds)
{
    MOCKER_CPP(&MpSmapHelper::SmapQueryProcessConfigHelper, MpResult(*)(int, std::vector<smap::ProcessPayload>&))
        .stubs()
        .will(invoke(SmapQueryProcessConfigEmptyMock));

    MOCKER_CPP(&exportV2::OsHelper::GetMemInfoByNumaId, MpResult(*)(const uint16_t&, exportV2::NumaInfo&))
        .stubs()
        .will(invoke(GetMemInfoByNumaIdIdleMock));

    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(*)(const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    std::vector<BorrowRecord> records;
    BorrowRecord record;
    record.name = "old_borrow_1";
    record.size = 1024;
    record.borrowNode = "node0";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = 1;
    record.borrowSocketId = 0;
    record.uid = 0;
    record.username = "root";
    records.push_back(record);

    pid_t pid = 1234;
    int64_t startTime = 1000;
    MpResult ret = ProcessNewBorrowFlow(pid, startTime, records);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

/*
 * 用例描述：ProcessPendingMigration中前置检查所有远端NUMA空闲且无进程时，应跳过迁移并释放oldBorrowIds
 * 测试步骤：
 * 1. 设置pendingMigrations中有一个未迁移的state（含numaToBorrowIds映射）
 * 2. Mock SmapQueryProcessConfigHelper 返回空列表
 * 3. Mock GetMemInfoByNumaId 返回idle
 * 4. Mock MemFreeWithOps 返回成功
 * 5. Mock BorrowIdRedirection::Update 返回成功
 * 6. 调用ProcessSinglePidFault触发ProcessPendingMigration
 * 预期结果：
 * 1. 返回 MEM_POOLING_OK
 * 2. pendingMigrations中该pid被清除
 */
TEST_F(TestOverCommitFaultNodeModule, ProcessPendingMigration_PreCheck_AllNumasIdle_SkipsMigrate)
{
    auto& pendingMigrations = OverCommitFaultNodeModule::Instance().GetPendingMigrations();
    pendingMigrations.clear();

    PendingMigrationState state;
    state.newBorrowId = "new_borrow_1";
    state.newRemoteNumaId = 2;
    state.oldBorrowIds = {"old_borrow_1"};
    state.borrowNodeId = "node0";
    state.pid = 9999;
    state.remoteNumaIds = {1};
    state.remoteNumaSizeMap[1] = 1024;
    state.numaToBorrowIds[1] = {"old_borrow_1"};
    state.migrated = false;
    pendingMigrations[9999] = state;

    MOCKER_CPP(&MpSmapHelper::SmapQueryProcessConfigHelper, MpResult(*)(int, std::vector<smap::ProcessPayload>&))
        .stubs()
        .will(invoke(SmapQueryProcessConfigEmptyMock));

    MOCKER_CPP(&exportV2::OsHelper::GetMemInfoByNumaId, MpResult(*)(const uint16_t&, exportV2::NumaInfo&))
        .stubs()
        .will(invoke(GetMemInfoByNumaIdIdleMock));

    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(*)(const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&BorrowIdRedirection::Update, MpResult(*)(const std::string&, const std::string&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    std::vector<BorrowRecord> records;
    BorrowRecord record;
    record.name = "old_borrow_1";
    record.size = 1024;
    record.borrowNode = "node0";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = 1;
    record.borrowSocketId = 0;
    record.uid = 0;
    record.username = "root";
    records.push_back(record);

    pid_t pid = 9999;
    int64_t startTime = 3000;
    MpResult ret = ProcessSinglePidFault(pid, startTime, records);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_TRUE(pendingMigrations.find(9999) == pendingMigrations.end());
}

MpResult FilterValidPidListByLocalNodeNoOpMock(std::vector<pid_t>& pidList)
{
    return MEM_POOLING_OK;
}

uint32_t UbseStoragePutDataMock(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data)
{
    return MEM_POOLING_OK;
}

uint32_t UbseStorageQueryDataMock(const std::string& keyPrefix, const std::string& key, void* ctx,
                                  UbseStorageDealDataFunc func)
{
    return MEM_POOLING_OK;
}

MpResult ExecuteFaultMemoryBorrowFailMock(OverCommitFaultNodeModule*, const std::vector<BorrowRecord>& borrowRecords,
                                          std::vector<RemoteNumaFault>& remoteNumas)
{
    return MEM_POOLING_ERROR;
}

MpResult ExecuteFaultMemoryBorrowOkMock(OverCommitFaultNodeModule*, const std::vector<BorrowRecord>& borrowRecords,
                                        std::vector<RemoteNumaFault>& remoteNumas)
{
    BorrowRecord record;
    record.borrowNode = "local_node";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = 2;
    record.size = 2048;
    RemoteNumaFault fault(0, 2, 2048, record);
    remoteNumas.push_back(fault);
    return MEM_POOLING_OK;
}

MpResult EvaculateVmsFromFaultNumaFailMock(
    OverCommitFaultNodeModule*, const std::unordered_map<int16_t, std::set<int16_t>>& remoteNumaId2LocalNumaId,
    const int16_t faultNumaId, std::unordered_map<pid_t, mempooling::outinterface::VMInfo>& vmInfos,
    std::vector<RemoteNumaFault>& remoteNumas)
{
    return MEM_POOLING_ERROR;
}

void ClearPidSmapEnableCompleted()
{
    PidSmapEnableCompleted::Instance().pidSmapEnableCompleted.clear();
}

/*
 * 用例描述：BorrowIdGroupProcess中ExecuteFaultMemoryBorrow失败时，应调用RollBackSmapEnablePids回滚，
 *           PidSmapEnableCompleted中不应存在被回滚的oldPids
 * 测试步骤：
 * 1. 先清空PidSmapEnableCompleted
 * 2. Mock SmapEnableProcessMigrateHelper(disable)返回OK
 * 3. Mock UbseStoragePutData/UbseStorageQueryData使PidSmapEnableCompleted::Update成功
 * 4. Mock ExecuteFaultMemoryBorrow返回MEM_POOLING_ERROR
 * 5. Mock FilterValidPidListByLocalNode使RollBackSmapEnablePids中FilterValidPidsByLocalNode成功
 * 6. Mock SmapEnableProcessMigrateHelper(enable)返回OK使RollBackSmapEnablePids中SmapEnable成功
 * 7. Mock UbseStoragePutData/UbseStorageQueryData使PidSmapEnableCompleted::Remove成功
 * 8. 调用BorrowIdGroupProcess
 * 预期结果：
 * 1. 返回MEM_POOLING_ERROR
 * 2. PidSmapEnableCompleted的pidSmapEnableCompleted中不存在vmInfos中的pid
 */
TEST_F(TestOverCommitFaultNodeModule, BorrowIdGroupProcess_ExecuteFaultMemoryBorrowFailed_RollBackSmapEnablePids)
{
    ClearPidSmapEnableCompleted();

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, size_t, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(UbseStoragePutData, uint32_t(*)(const std::string&, const std::string&, UbseByteBuffer*))
        .stubs()
        .will(invoke(UbseStoragePutDataMock));

    MOCKER_CPP(UbseStorageQueryData,
               uint32_t(*)(const std::string&, const std::string&, void*, UbseStorageDealDataFunc))
        .stubs()
        .will(invoke(UbseStorageQueryDataMock));

    MOCKER_CPP(&OverCommitFaultNodeModule::ExecuteFaultMemoryBorrow,
               MpResult(*)(OverCommitFaultNodeModule*, const std::vector<BorrowRecord>&, std::vector<RemoteNumaFault>&))
        .stubs()
        .will(invoke(ExecuteFaultMemoryBorrowFailMock));

    MOCKER_CPP(&ResourceQuery::FilterValidPidListByLocalNode, MpResult(*)(std::vector<pid_t>&))
        .stubs()
        .will(invoke(FilterValidPidListByLocalNodeNoOpMock));

    std::unordered_map<int16_t, std::set<int16_t>> remoteNumaId2LocalNumaId;
    remoteNumaId2LocalNumaId[1] = {0};
    int16_t faultNumaId = 1;
    std::vector<BorrowRecord> borrowRecords;
    BorrowRecord record;
    record.borrowNode = "local_node";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = 1;
    record.name = "borrow_1";
    record.size = 1024;
    borrowRecords.push_back(record);

    std::unordered_map<pid_t, mempooling::outinterface::VMInfo> vmInfos;
    mempooling::outinterface::VMInfo vm;
    vm.pid = 1234;
    vm.totalLocalUsedMem = 1000;
    vm.totalRemoteUsedMem = 1000;
    vmInfos[1234] = vm;

    OverCommitFaultNodeModule module;
    auto ret = module.BorrowIdGroupProcess(remoteNumaId2LocalNumaId, faultNumaId, borrowRecords, vmInfos);

    EXPECT_NE(ret, MEM_POOLING_OK);
    EXPECT_TRUE(PidSmapEnableCompleted::Instance().pidSmapEnableCompleted.find(1234) ==
                PidSmapEnableCompleted::Instance().pidSmapEnableCompleted.end());
}

/*
 * 用例描述：BorrowIdGroupProcess中EvaculateVmsFromFaultNuma失败时，应调用RollBackSmapEnablePids回滚，
 *           PidSmapEnableCompleted中不应存在被回滚的oldPids
 * 测试步骤：
 * 1. 先清空PidSmapEnableCompleted
 * 2. Mock SmapEnableProcessMigrateHelper(disable)返回OK
 * 3. Mock UbseStoragePutData/UbseStorageQueryData使PidSmapEnableCompleted::Update成功
 * 4. Mock ExecuteFaultMemoryBorrow返回MEM_POOLING_OK
 * 5. Mock EvaculateVmsFromFaultNuma返回MEM_POOLING_ERROR
 * 6. Mock FilterValidPidListByLocalNode使RollBackSmapEnablePids中FilterValidPidsByLocalNode成功
 * 7. Mock SmapEnableProcessMigrateHelper(enable)返回OK使RollBackSmapEnablePids中SmapEnable成功
 * 8. Mock UbseStoragePutData/UbseStorageQueryData使PidSmapEnableCompleted::Remove成功
 * 9. 调用BorrowIdGroupProcess
 * 预期结果：
 * 1. 返回MEM_POOLING_ERROR
 * 2. PidSmapEnableCompleted的pidSmapEnableCompleted中不存在vmInfos中的pid
 */
TEST_F(TestOverCommitFaultNodeModule, BorrowIdGroupProcess_EvaculateVmsFromFaultNumaFailed_RollBackSmapEnablePids)
{
    ClearPidSmapEnableCompleted();

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, size_t, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(UbseStoragePutData, uint32_t(*)(const std::string&, const std::string&, UbseByteBuffer*))
        .stubs()
        .will(invoke(UbseStoragePutDataMock));

    MOCKER_CPP(UbseStorageQueryData,
               uint32_t(*)(const std::string&, const std::string&, void*, UbseStorageDealDataFunc))
        .stubs()
        .will(invoke(UbseStorageQueryDataMock));

    MOCKER_CPP(&OverCommitFaultNodeModule::ExecuteFaultMemoryBorrow,
               MpResult(*)(OverCommitFaultNodeModule*, const std::vector<BorrowRecord>&, std::vector<RemoteNumaFault>&))
        .stubs()
        .will(invoke(ExecuteFaultMemoryBorrowOkMock));

    MOCKER_CPP(
        &OverCommitFaultNodeModule::EvaculateVmsFromFaultNuma,
        MpResult(*)(OverCommitFaultNodeModule*, const std::unordered_map<int16_t, std::set<int16_t>>&, const int16_t,
                    std::unordered_map<pid_t, mempooling::outinterface::VMInfo>&, std::vector<RemoteNumaFault>&))
        .stubs()
        .will(invoke(EvaculateVmsFromFaultNumaFailMock));

    MOCKER_CPP(&ResourceQuery::FilterValidPidListByLocalNode, MpResult(*)(std::vector<pid_t>&))
        .stubs()
        .will(invoke(FilterValidPidListByLocalNodeNoOpMock));

    std::unordered_map<int16_t, std::set<int16_t>> remoteNumaId2LocalNumaId;
    remoteNumaId2LocalNumaId[1] = {0};
    int16_t faultNumaId = 1;
    std::vector<BorrowRecord> borrowRecords;
    BorrowRecord record;
    record.borrowNode = "local_node";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = 1;
    record.name = "borrow_1";
    record.size = 1024;
    borrowRecords.push_back(record);

    std::unordered_map<pid_t, mempooling::outinterface::VMInfo> vmInfos;
    mempooling::outinterface::VMInfo vm;
    vm.pid = 1234;
    vm.totalLocalUsedMem = 1000;
    vm.totalRemoteUsedMem = 1000;
    vmInfos[1234] = vm;

    OverCommitFaultNodeModule module;
    auto ret = module.BorrowIdGroupProcess(remoteNumaId2LocalNumaId, faultNumaId, borrowRecords, vmInfos);

    EXPECT_NE(ret, MEM_POOLING_OK);
    EXPECT_TRUE(PidSmapEnableCompleted::Instance().pidSmapEnableCompleted.find(1234) ==
                PidSmapEnableCompleted::Instance().pidSmapEnableCompleted.end());
}

// ===== 简化故障路径：CollectClusterSocketQueue / AllocatePidsToSockets =====

static std::vector<std::string> MockGetNodeIdsFour()
{
    return {"node1", "node2", "node3", "node4"};
}

static std::vector<std::string> MockGetNodeIdsSingle()
{
    return {"node1"};
}

UbseResult MockUbseGetNodeNumaInfoByNodeIdSuccess(const std::string& nodeId,
                                                  std::vector<UbseNodeNumaInfo>& numaNodeInfoList)
{
    UbseNodeNumaInfo numa;
    numa.nodeId = nodeId;
    numa.socketId = 0;
    numa.numaId = 0;
    numa.mReservedMemRatio = 100;
    if (nodeId == "node3") {
        numa.memTotal = 8 * 1024 * 1024;
        numa.memFree = 4 * 1024 * 1024;
    } else if (nodeId == "node4") {
        numa.memTotal = 16 * 1024 * 1024;
        numa.memFree = 8 * 1024 * 1024;
    } else {
        numa.memTotal = 4 * 1024 * 1024;
        numa.memFree = 2 * 1024 * 1024;
    }
    numaNodeInfoList.push_back(numa);
    return UBSE_OK;
}

TEST_F(TestOverCommitFaultNodeModule, CollectClusterSocketQueue_ExcludesFaultAndBorrowerNodes)
{
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string>(*)()).stubs().will(invoke(MockGetNodeIdsFour));
    MOCKER_CPP(&UbseGetNodeNumaInfoByNodeId, UbseResult(*)(const std::string&, std::vector<UbseNodeNumaInfo>&))
        .stubs()
        .will(invoke(MockUbseGetNodeNumaInfoByNodeIdSuccess));

    std::unordered_set<std::string> borrowerNodes{"node2"};
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>> socketQueueBySocketId;
    MpResult ret = CollectClusterSocketQueue("node1", borrowerNodes, socketQueueBySocketId);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(socketQueueBySocketId.size(), 1u);
    ASSERT_EQ(socketQueueBySocketId[0].size(), 2u);
    EXPECT_EQ(socketQueueBySocketId[0][0].nodeId, "node3");
    EXPECT_EQ(socketQueueBySocketId[0][0].canBorrowMem, 4096u);
    EXPECT_EQ(socketQueueBySocketId[0][1].nodeId, "node4");
    EXPECT_EQ(socketQueueBySocketId[0][1].canBorrowMem, 8192u);
}

TEST_F(TestOverCommitFaultNodeModule, CollectClusterSocketQueue_AllNodesEmpty_FailsResourceCollect)
{
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string>(*))
        .stubs()
        .will(returnValue(std::vector<std::string>{}));
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>> socketQueueBySocketId;
    MpResult ret = CollectClusterSocketQueue("node1", {}, socketQueueBySocketId);
    EXPECT_EQ(ret, MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
}

TEST_F(TestOverCommitFaultNodeModule, CollectClusterSocketQueue_NoCandidate_FailsLackRemoteMem)
{
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string>(*)).stubs().will(invoke(MockGetNodeIdsSingle));
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>> socketQueueBySocketId;
    MpResult ret = CollectClusterSocketQueue("node1", {"node1"}, socketQueueBySocketId);
    EXPECT_EQ(ret, MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR);
}

TEST_F(TestOverCommitFaultNodeModule, CollectClusterSocketQueue_CollectFailed_FailsResourceCollect)
{
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string>(*)).stubs().will(invoke(MockGetNodeIdsFour));
    MOCKER_CPP(&UbseGetNodeNumaInfoByNodeId, UbseResult(*)(const std::string&, std::vector<UbseNodeNumaInfo>&))
        .stubs()
        .will(returnValue(UBSE_ERROR));
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>> socketQueueBySocketId;
    MpResult ret = CollectClusterSocketQueue("node1", {"node2"}, socketQueueBySocketId);
    EXPECT_EQ(ret, MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
}

TEST_F(TestOverCommitFaultNodeModule, AllocatePidsToSockets_AllFit)
{
    // 分块初始化：{sizeKB, preferredSocketId, faultNumaId}
    std::unordered_map<pid_t, std::vector<PidChunkInfo>> pidChunks{{1, {{1024, 0, 2}}}, {2, {{4096, 0, 2}}}};
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>> socketQueueBySocketId;
    socketQueueBySocketId[0].push_back({"node4", 8192, 1024});
    std::unordered_map<pid_t, std::vector<SimplifiedFaultPidAllocTarget>> pidAllocMap;
    std::vector<pid_t> unallocatedPids;

    MpResult ret = AllocatePidsToSockets(pidChunks, socketQueueBySocketId, pidAllocMap, unallocatedPids);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(pidAllocMap.size(), 2u);
    ASSERT_EQ(pidAllocMap[1].size(), 1u);
    EXPECT_EQ(pidAllocMap[1][0].lendNodeId, "node4");
    EXPECT_EQ(pidAllocMap[1][0].lendSizeKB, 1024u);
    ASSERT_EQ(pidAllocMap[2].size(), 1u);
    EXPECT_EQ(pidAllocMap[2][0].lendNodeId, "node4");
    EXPECT_EQ(pidAllocMap[2][0].lendSizeKB, 4096u);
    EXPECT_TRUE(unallocatedPids.empty());
}

TEST_F(TestOverCommitFaultNodeModule, AllocatePidsToSockets_BigProcessNotFit_Unallocated)
{
    std::unordered_map<pid_t, std::vector<PidChunkInfo>> pidChunks{{1, {{1024, 0, 2}}}, {2, {{8192, 0, 2}}}};
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>> socketQueueBySocketId;
    socketQueueBySocketId[0].push_back({"node4", 4096, 1024});
    std::unordered_map<pid_t, std::vector<SimplifiedFaultPidAllocTarget>> pidAllocMap;
    std::vector<pid_t> unallocatedPids;

    MpResult ret = AllocatePidsToSockets(pidChunks, socketQueueBySocketId, pidAllocMap, unallocatedPids);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(pidAllocMap.size(), 1u);
    ASSERT_EQ(pidAllocMap[1].size(), 1u);
    EXPECT_EQ(pidAllocMap[1][0].lendNodeId, "node4");
    EXPECT_EQ(pidAllocMap[1][0].lendSizeKB, 1024u);
    ASSERT_EQ(unallocatedPids.size(), 1u);
    EXPECT_EQ(unallocatedPids[0], 2);
}

TEST_F(TestOverCommitFaultNodeModule, AllocatePidsToSockets_SocketAffinity_MultiSocketChunks)
{
    // pid 在 socket0 占 1024KB（故障NUMA2）、在 socket1 占 2048KB（故障NUMA3）：优先从各自 socket 分配
    std::unordered_map<pid_t, std::vector<PidChunkInfo>> pidChunks{{1, {{1024, 0, 2}, {2048, 1, 3}}}};
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>> socketQueueBySocketId;
    socketQueueBySocketId[0].push_back({"node3", 4096, 1024});
    socketQueueBySocketId[1].push_back({"node4", 4096, 1024});
    std::unordered_map<pid_t, std::vector<SimplifiedFaultPidAllocTarget>> pidAllocMap;
    std::vector<pid_t> unallocatedPids;

    MpResult ret = AllocatePidsToSockets(pidChunks, socketQueueBySocketId, pidAllocMap, unallocatedPids);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(pidAllocMap.size(), 1u);
    ASSERT_EQ(pidAllocMap[1].size(), 2u);
    EXPECT_EQ(pidAllocMap[1][0].lendNodeId, "node3");
    EXPECT_EQ(pidAllocMap[1][0].lendSocketId, 0);
    EXPECT_EQ(pidAllocMap[1][0].srcFaultNumaId, 2);
    EXPECT_EQ(pidAllocMap[1][0].lendSizeKB, 1024u);
    EXPECT_EQ(pidAllocMap[1][1].lendNodeId, "node4");
    EXPECT_EQ(pidAllocMap[1][1].lendSocketId, 1);
    EXPECT_EQ(pidAllocMap[1][1].srcFaultNumaId, 3);
    EXPECT_EQ(pidAllocMap[1][1].lendSizeKB, 2048u);
    EXPECT_TRUE(unallocatedPids.empty());
}

TEST_F(TestOverCommitFaultNodeModule, AllocatePidsToSockets_BlockSizeRounding_ZeroTakeUnallocated)
{
    // 模拟不变量被破坏的异常输入：chunk 仅 8192KB，目标节点 blockSize=131072KB(128MB)，
    // take 按 blockSize 向下取整后为 0 -> LOG_ERROR + break -> pid 进 unallocatedPids
    std::unordered_map<pid_t, std::vector<PidChunkInfo>> pidChunks{{1, {{8192, 0, 2}}}};
    std::unordered_map<int, std::vector<SimplifiedSocketCapacity>> socketQueueBySocketId;
    socketQueueBySocketId[0].push_back({"node4", 131072, 131072});
    std::unordered_map<pid_t, std::vector<SimplifiedFaultPidAllocTarget>> pidAllocMap;
    std::vector<pid_t> unallocatedPids;

    MpResult ret = AllocatePidsToSockets(pidChunks, socketQueueBySocketId, pidAllocMap, unallocatedPids);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_TRUE(pidAllocMap.empty());
    ASSERT_EQ(unallocatedPids.size(), 1u);
    EXPECT_EQ(unallocatedPids[0], 1);
}

TEST_F(TestOverCommitFaultNodeModule, ProcessBorrowOutNodeFaultSimplified_GetDebtFailed_ResourceCollectError)
{
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string&, std::vector<UbseNumaMemoryDebtInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFaultSimplified("node1");
    EXPECT_EQ(ret, MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
}

TEST_F(TestOverCommitFaultNodeModule, ProcessBorrowOutNodeFaultSimplified_NoCandidate_LackRemoteMemError)
{
    std::vector<UbseNumaMemoryDebtInfo> infos;
    UbseNumaMemoryDebtInfo debt;
    debt.borrowNodeId = "node2";
    debt.lentNodeId = "node1";
    debt.remoteNumaId = 1;
    debt.name = "1-borrow";
    infos.push_back(debt);
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string&, std::vector<UbseNumaMemoryDebtInfo>&))
        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    // 集群只有故障节点 node1 本身：无候选借出方
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string>(*)).stubs().will(invoke(MockGetNodeIdsSingle));
    MOCKER_CPP(&MemBorrowExecutor::IsValidBorrowIdFormat, bool (*)(const UbseNumaMemoryDebtInfo&))
        .stubs()
        .will(returnValue(true));
    MpResult ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFaultSimplified("node1");
    EXPECT_EQ(ret, MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR);
}

TEST_F(TestOverCommitFaultNodeModule, ProcessBorrowOutNodeFaultSimplified_AllUnallocated_LackRemoteMemError)
{
    std::vector<UbseNumaMemoryDebtInfo> infos;
    UbseNumaMemoryDebtInfo debt;
    debt.borrowNodeId = "node2";
    debt.lentNodeId = "node1";
    debt.remoteNumaId = 1;
    debt.name = "1-borrow";
    // 16384*1024 字节 = 16384KB > 合计可借 12288KB（node3=4096 + node4=8192）
    debt.size = 16384 * 1024;
    infos.push_back(debt);
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string&, std::vector<UbseNumaMemoryDebtInfo>&))
        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string>(*)).stubs().will(invoke(MockGetNodeIdsFour));
    MOCKER_CPP(&UbseGetNodeNumaInfoByNodeId, UbseResult(*)(const std::string&, std::vector<UbseNodeNumaInfo>&))
        .stubs()
        .will(invoke(MockUbseGetNodeNumaInfoByNodeIdSuccess));
    MOCKER_CPP(&MemBorrowExecutor::IsValidBorrowIdFormat, bool (*)(const UbseNumaMemoryDebtInfo&))
        .stubs()
        .will(returnValue(true));
    MpResult ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFaultSimplified("node1");
    EXPECT_EQ(ret, MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR);
}

// ===== ExecuteBorrowForPid / FinalizePidProcessing / BuildBorrowerData =====

// 捕获 ExecuteBorrowForPid 组装的 splits，验证 (oldNumaId, lendNodeId, lendSocketId, sizeBytes) 组装与 KB→字节换算
static std::vector<FaultBorrowSplit> gCapturedSplits;
static MpResult MockMemBorrowExecuteSplitsOk(const SrcMemoryBorrowParam& srcParam,
                                             const std::vector<FaultBorrowSplit>& splits, const WaterMark& waterMark,
                                             MemBorrowExecuteResult& borrowExecuteResult,
                                             const ProcessMemUsrInfo& processMemUsrInfo)
{
    (void)srcParam;
    (void)waterMark;
    (void)processMemUsrInfo;
    gCapturedSplits = splits;
    for (size_t i = 0; i < splits.size(); ++i) {
        borrowExecuteResult.borrowIds.push_back("split_bid_" + std::to_string(i));
        borrowExecuteResult.presentNumaId.push_back(static_cast<uint16_t>(200 + i));
    }
    return MEM_POOLING_OK;
}

// 部分成功语义mock：无论几个split只回填1笔结果，覆盖ExecuteBorrowForPid部分结果硬失败分支
static MpResult MockMemBorrowExecuteSplitsPartial(const SrcMemoryBorrowParam& srcParam,
                                                  const std::vector<FaultBorrowSplit>& splits,
                                                  const WaterMark& waterMark,
                                                  MemBorrowExecuteResult& borrowExecuteResult,
                                                  const ProcessMemUsrInfo& processMemUsrInfo)
{
    (void)srcParam;
    (void)splits;
    (void)waterMark;
    (void)processMemUsrInfo;
    borrowExecuteResult.borrowIds.push_back("partial_bid");
    borrowExecuteResult.presentNumaId.push_back(100);
    return MEM_POOLING_OK;
}

/*
 * 用例描述：量纲决策（含lendSizeKB）走split路径：按target组装FaultBorrowSplit（KB→字节），
 *           perNuma逐split与borrowIds/presentNumaId下标配对
 * 测试步骤：
 * 1. ctx.remoteNumaSizeMap按字节口径设置（2号NUMA 4096KB、3号NUMA 2048KB，与split合计一致）
 * 2. allocLendTargets：2号NUMA一笔（node4/socket0, lendSizeKB=4096）、3号NUMA一笔（node4/socket0, lendSizeKB=2048）
 * 3. Mock MemBorrowExecuteSplitsForFaultInOverCommit捕获splits并回填成功结果
 * 预期结果：
 * 1. 返回MEM_POOLING_OK，splits按决策顺序组装且sizeBytes=lendSizeKB*1024
 * 2. perNuma两个条目与splits顺序配对，borrowSizeKB=sizeBytes/1024
 */
TEST_F(TestOverCommitFaultNodeModule, ExecuteBorrowForPid_PerNumaPairing_Correct)
{
    PidBorrowContext ctx;
    ctx.pid = 1234;
    ctx.borrowNodeId = "node2";
    ctx.borrowSocketId = 0;
    // remoteNumaSizeMap为字节口径（record.size直传）
    ctx.remoteNumaSizeMap[2] = 4096ULL * 1024;
    ctx.remoteNumaSizeMap[3] = 2048ULL * 1024;
    ctx.allocLendTargets = {{"node4", 0, 2, 4096}, {"node4", 0, 3, 2048}};

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteSplitsForFaultInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<FaultBorrowSplit>&, const WaterMark&,
                           MemBorrowExecuteResult&, const ProcessMemUsrInfo&))
        .stubs()
        .will(invoke(MockMemBorrowExecuteSplitsOk));

    gCapturedSplits.clear();
    BorrowForPidResult result = ExecuteBorrowForPid(ctx);

    EXPECT_EQ(result.status, MEM_POOLING_OK);
    ASSERT_EQ(gCapturedSplits.size(), 2U);
    EXPECT_EQ(gCapturedSplits[0].oldNumaId, 2);
    EXPECT_EQ(gCapturedSplits[0].lendNodeId, "node4");
    EXPECT_EQ(gCapturedSplits[0].lendSocketId, 0);
    EXPECT_EQ(gCapturedSplits[0].sizeBytes, 4096ULL * 1024); // KB→字节换算
    EXPECT_EQ(gCapturedSplits[1].oldNumaId, 3);
    EXPECT_EQ(gCapturedSplits[1].lendNodeId, "node4");
    EXPECT_EQ(gCapturedSplits[1].lendSocketId, 0);
    EXPECT_EQ(gCapturedSplits[1].sizeBytes, 2048ULL * 1024);

    ASSERT_EQ(result.perNuma.size(), 2U);
    EXPECT_EQ(result.perNuma[0].oldNumaId, 2);
    EXPECT_EQ(result.perNuma[0].newNumaId, 200);
    EXPECT_EQ(result.perNuma[0].borrowSizeKB, 4096ULL);
    EXPECT_EQ(result.perNuma[0].newBorrowId, "split_bid_0");
    EXPECT_EQ(result.perNuma[1].oldNumaId, 3);
    EXPECT_EQ(result.perNuma[1].newNumaId, 201);
    EXPECT_EQ(result.perNuma[1].borrowSizeKB, 2048ULL);
    EXPECT_EQ(result.perNuma[1].newBorrowId, "split_bid_1");
}

/*
 * 用例描述：split执行返回部分成功（2笔split只回1笔结果）时按硬失败处理，不返回部分结果
 * 测试步骤：
 * 1. 两个量纲target（oldNuma 2/3），remoteNumaSizeMap字节口径与split合计一致
 * 2. Mock MemBorrowExecuteSplitsForFaultInOverCommit只回填1笔成功结果
 * 预期结果：
 * 1. 返回MEM_POOLING_FAULT_BORROW_MEM_ERROR
 * 2. perNuma为空（缺失split的新NUMA与旧NUMA无关，部分结果无法用于迁移调度）
 */
TEST_F(TestOverCommitFaultNodeModule, ExecuteBorrowForPid_PartialResult_HardFailure)
{
    PidBorrowContext ctx;
    ctx.pid = 1234;
    ctx.borrowNodeId = "node2";
    ctx.remoteNumaSizeMap[2] = 1024ULL * 1024;
    ctx.remoteNumaSizeMap[3] = 2048ULL * 1024;
    ctx.allocLendTargets = {{"node4", 0, 2, 1024}, {"node4", 0, 3, 2048}};

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteSplitsForFaultInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<FaultBorrowSplit>&, const WaterMark&,
                           MemBorrowExecuteResult&, const ProcessMemUsrInfo&))
        .stubs()
        .will(invoke(MockMemBorrowExecuteSplitsPartial));

    BorrowForPidResult result = ExecuteBorrowForPid(ctx);

    // 部分成功（1/2 split有结果）按硬失败处理，不返回部分结果
    EXPECT_EQ(result.status, MEM_POOLING_FAULT_BORROW_MEM_ERROR);
    EXPECT_TRUE(result.perNuma.empty());
}

TEST_F(TestOverCommitFaultNodeModule, FinalizePidProcessing_ReleasesOnlyMigratedNumas)
{
    PidBorrowContext ctx;
    ctx.pid = 1234;
    ctx.numaToBorrowIds[2] = {"old_bid_1"};
    ctx.numaToBorrowIds[3] = {"old_bid_2"};

    std::vector<PerRemoteNumaBorrowResult> perNumaBorrows;
    PerRemoteNumaBorrowResult e1;
    e1.oldNumaId = 2;
    e1.newBorrowId = "new_bid_1";
    perNumaBorrows.push_back(e1);

    std::vector<uint16_t> migratedNumaIds{2};
    std::unordered_set<std::string> freedOldBorrowIds;

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, size_t, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps, MpResult(*)(const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&UbseStoragePutData, uint32_t(*)(const std::string&, const std::string&, UbseByteBuffer*))
        .stubs()
        .will(invoke(UbseStoragePutDataMock));

    MpResult ret = FinalizePidProcessing(ctx, perNumaBorrows, migratedNumaIds, freedOldBorrowIds);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    // 只释放已迁移 NUMA2 的 old_bid_1；NUMA3 的 old_bid_2 保留
    ASSERT_EQ(freedOldBorrowIds.size(), 1u);
    EXPECT_TRUE(freedOldBorrowIds.count("old_bid_1") > 0);
    EXPECT_TRUE(freedOldBorrowIds.count("old_bid_2") == 0);
}

// ===== split 路径（master量纲决策）=====

// 捕获 FinalizePidProcessing 经 MemFreeWithOpsForProcessMem 释放的旧借用id
static std::vector<std::string> gFreedOldBorrowIds;
static MpResult MockMemFreeWithOpsForProcessMemOk(MemBorrowExecutor* This, const std::string& name, bool smapBack,
                                                  bool isFault)
{
    (void)This;
    (void)smapBack;
    (void)isFault;
    gFreedOldBorrowIds.push_back(name);
    return MEM_POOLING_OK;
}

// 捕获 BorrowIdRedirection::Update 的 (oldBorrowId, newBorrowId) 重定向映射
static std::vector<std::pair<std::string, std::string>> gCapturedRedirections;
static MpResult MockBorrowIdRedirectionUpdate(BorrowIdRedirection* This, const std::string key, const std::string value)
{
    (void)This;
    gCapturedRedirections.push_back({key, value});
    return MEM_POOLING_OK;
}

/*
 * 用例描述：量纲决策非空且全带lendSizeKB时走split路径：按target组装FaultBorrowSplit（KB→字节），
 *           perNuma逐split与borrowIds/presentNumaId下标配对
 * 测试步骤：
 * 1. ctx.remoteNumaSizeMap按字节口径设置（2号NUMA 4096KB、3号NUMA 2048KB，与split合计一致）
 * 2. allocLendTargets：2号NUMA拆两笔（node5/socket1、node7/socket2）、3号NUMA一笔（node5/socket1）
 * 3. Mock MemBorrowExecuteSplitsForFaultInOverCommit捕获splits并回填成功结果
 * 预期结果：
 * 1. 返回MEM_POOLING_OK，splits按决策顺序组装且sizeBytes=lendSizeKB*1024
 * 2. perNuma三个条目与splits顺序配对，borrowSizeKB=sizeBytes/1024
 */
TEST_F(TestOverCommitFaultNodeModule, ExecuteBorrowForPid_SplitWiring_Correct)
{
    PidBorrowContext ctx;
    ctx.pid = 1;
    ctx.borrowNodeId = "node2";
    ctx.borrowSocketId = 0;
    // remoteNumaSizeMap为字节口径（record.size直传）：4096KB / 2048KB
    ctx.remoteNumaSizeMap[2] = 4096ULL * 1024;
    ctx.remoteNumaSizeMap[3] = 2048ULL * 1024;
    ctx.allocLendTargets = {{"node5", 1, 2, 2048}, {"node7", 2, 2, 2048}, {"node5", 1, 3, 2048}};

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteSplitsForFaultInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<FaultBorrowSplit>&, const WaterMark&,
                           MemBorrowExecuteResult&, const ProcessMemUsrInfo&))
        .stubs()
        .will(invoke(MockMemBorrowExecuteSplitsOk));

    gCapturedSplits.clear();
    BorrowForPidResult result = ExecuteBorrowForPid(ctx);

    EXPECT_EQ(result.status, MEM_POOLING_OK);
    ASSERT_EQ(gCapturedSplits.size(), 3U);
    EXPECT_EQ(gCapturedSplits[0].oldNumaId, 2);
    EXPECT_EQ(gCapturedSplits[0].lendNodeId, "node5");
    EXPECT_EQ(gCapturedSplits[0].lendSocketId, 1);
    EXPECT_EQ(gCapturedSplits[0].sizeBytes, 2048ULL * 1024); // KB→字节换算
    EXPECT_EQ(gCapturedSplits[1].oldNumaId, 2);
    EXPECT_EQ(gCapturedSplits[1].lendNodeId, "node7");
    EXPECT_EQ(gCapturedSplits[1].lendSocketId, 2);
    EXPECT_EQ(gCapturedSplits[1].sizeBytes, 2048ULL * 1024);
    EXPECT_EQ(gCapturedSplits[2].oldNumaId, 3);
    EXPECT_EQ(gCapturedSplits[2].lendNodeId, "node5");
    EXPECT_EQ(gCapturedSplits[2].lendSocketId, 1);
    EXPECT_EQ(gCapturedSplits[2].sizeBytes, 2048ULL * 1024);

    ASSERT_EQ(result.perNuma.size(), 3U);
    EXPECT_EQ(result.perNuma[0].oldNumaId, 2);
    EXPECT_EQ(result.perNuma[1].oldNumaId, 2);
    EXPECT_EQ(result.perNuma[2].oldNumaId, 3);
    EXPECT_EQ(result.perNuma[0].newNumaId, 200);
    EXPECT_EQ(result.perNuma[1].newNumaId, 201);
    EXPECT_EQ(result.perNuma[2].newNumaId, 202);
    EXPECT_EQ(result.perNuma[0].borrowSizeKB, 2048ULL);
    EXPECT_EQ(result.perNuma[1].borrowSizeKB, 2048ULL);
    EXPECT_EQ(result.perNuma[2].borrowSizeKB, 2048ULL);
    EXPECT_EQ(result.perNuma[0].newBorrowId, "split_bid_0");
    EXPECT_EQ(result.perNuma[1].newBorrowId, "split_bid_1");
    EXPECT_EQ(result.perNuma[2].newBorrowId, "split_bid_2");
}

/*
 * 用例描述：split每组合计与remoteNumaSizeMap（KB口径）不一致时仅告警，仍以master split为准继续执行
 * 测试步骤：
 * 1. remoteNumaSizeMap[2]=4096字节（4KB），master决策仅一笔2048KB → 合计与预期不一致
 * 2. Mock MemBorrowExecuteSplitsForFaultInOverCommit返回成功
 * 预期结果：
 * 1. 不回退legacy，仍走split路径：gCapturedSplits.size()==1，返回OK
 * 2. perNuma以master split为准（1笔，oldNumaId=2）
 */
TEST_F(TestOverCommitFaultNodeModule, ExecuteBorrowForPid_SplitSumMismatch_WarnsAndProceeds)
{
    PidBorrowContext ctx;
    ctx.pid = 1;
    ctx.borrowNodeId = "node2";
    ctx.remoteNumaSizeMap[2] = 4096; // 字节口径=4KB，与master拆分合计2048KB不一致（WARN-only）
    ctx.allocLendTargets = {{"node5", 1, 2, 2048}};

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecuteSplitsForFaultInOverCommit,
               MpResult(*)(const SrcMemoryBorrowParam&, const std::vector<FaultBorrowSplit>&, const WaterMark&,
                           MemBorrowExecuteResult&, const ProcessMemUsrInfo&))
        .stubs()
        .will(invoke(MockMemBorrowExecuteSplitsOk));

    gCapturedSplits.clear();
    BorrowForPidResult result = ExecuteBorrowForPid(ctx);

    EXPECT_EQ(result.status, MEM_POOLING_OK);
    ASSERT_EQ(gCapturedSplits.size(), 1U);
    EXPECT_EQ(gCapturedSplits[0].oldNumaId, 2);
    EXPECT_EQ(gCapturedSplits[0].lendNodeId, "node5");
    EXPECT_EQ(gCapturedSplits[0].sizeBytes, 2048ULL * 1024);
    ASSERT_EQ(result.perNuma.size(), 1U);
    EXPECT_EQ(result.perNuma[0].oldNumaId, 2);
    EXPECT_EQ(result.perNuma[0].newBorrowId, "split_bid_0");
}

/*
 * 用例描述：allocLendTargets为空（主节点未下发决策）时直接返回失败，不再回退legacy借用
 * 测试步骤：
 * 1. ctx.allocLendTargets保持为空，remoteNumaSizeMap[2]=4096KB（字节口径）
 * 2. 仅Mock GetWaterMark成功，不mock任何借用执行器
 * 预期结果：
 * 1. 返回MEM_POOLING_FAULT_BORROW_MEM_ERROR且perNuma为空
 */
TEST_F(TestOverCommitFaultNodeModule, ExecuteBorrowForPid_EmptyDecision_ReturnsError)
{
    PidBorrowContext ctx;
    ctx.pid = 1234;
    ctx.borrowNodeId = "node2";
    ctx.remoteNumaSizeMap[2] = 4096ULL * 1024;

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    BorrowForPidResult result = ExecuteBorrowForPid(ctx);

    EXPECT_EQ(result.status, MEM_POOLING_FAULT_BORROW_MEM_ERROR);
    EXPECT_TRUE(result.perNuma.empty());
}

/*
 * 用例描述：targets存在但存在lendSizeKB==0（未传量纲）时同样直接返回失败
 * 测试步骤：
 * 1. ctx.allocLendTargets含一个lendSizeKB=0的target
 * 2. 仅Mock GetWaterMark成功，不mock任何借用执行器
 * 预期结果：
 * 1. 返回MEM_POOLING_FAULT_BORROW_MEM_ERROR且perNuma为空
 */
TEST_F(TestOverCommitFaultNodeModule, ExecuteBorrowForPid_ZeroLendSize_ReturnsError)
{
    PidBorrowContext ctx;
    ctx.pid = 1235;
    ctx.borrowNodeId = "node2";
    ctx.remoteNumaSizeMap[2] = 4096ULL * 1024;
    ctx.allocLendTargets = {{"node4", 0, 2, 0}};

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetWaterMark, MpResult(*)(WaterMark&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    BorrowForPidResult result = ExecuteBorrowForPid(ctx);

    EXPECT_EQ(result.status, MEM_POOLING_FAULT_BORROW_MEM_ERROR);
    EXPECT_TRUE(result.perNuma.empty());
}

/*
 * 用例描述：一个旧NUMA拆多笔新借用时，Finalize选borrowSizeKB最大者做BorrowIdRedirection（部分归还语义，
 *           其余较小split由水线归还）
 * 测试步骤：
 * 1. ctx.numaToBorrowIds[2]={"old_bid_1"}，migratedNumaIds={2}
 * 2. perNumaBorrows两笔：small(1024KB)与large(3072KB)
 * 3. Mock MemFreeWithOpsForProcessMem与BorrowIdRedirection::Update捕获入参
 * 预期结果：
 * 1. 返回MEM_POOLING_OK，old_bid_1被释放
 * 2. 重定向old_bid_1 → large（最大split），而非small
 */
TEST_F(TestOverCommitFaultNodeModule, FinalizePidProcessing_Redirection_LargestSplit)
{
    PidBorrowContext ctx;
    ctx.pid = 1234;
    ctx.numaToBorrowIds[2] = {"old_bid_1"};

    std::vector<PerRemoteNumaBorrowResult> perNumaBorrows;
    PerRemoteNumaBorrowResult small;
    small.oldNumaId = 2;
    small.newNumaId = 10;
    small.borrowSizeKB = 1024;
    small.newBorrowId = "small";
    PerRemoteNumaBorrowResult large;
    large.oldNumaId = 2;
    large.newNumaId = 11;
    large.borrowSizeKB = 3072;
    large.newBorrowId = "large";
    perNumaBorrows.push_back(small);
    perNumaBorrows.push_back(large);

    std::vector<uint16_t> migratedNumaIds{2};
    std::unordered_set<std::string> freedOldBorrowIds;

    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper, int (*)(pid_t*, size_t, int, int))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOpsForProcessMem,
               MpResult(*)(MemBorrowExecutor*, const std::string&, bool, bool))
        .stubs()
        .will(invoke(MockMemFreeWithOpsForProcessMemOk));
    MOCKER_CPP(&BorrowIdRedirection::Update, MpResult(*)(BorrowIdRedirection*, const std::string, const std::string))
        .stubs()
        .will(invoke(MockBorrowIdRedirectionUpdate));

    gFreedOldBorrowIds.clear();
    gCapturedRedirections.clear();
    MpResult ret = FinalizePidProcessing(ctx, perNumaBorrows, migratedNumaIds, freedOldBorrowIds);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(freedOldBorrowIds.size(), 1U);
    EXPECT_TRUE(freedOldBorrowIds.count("old_bid_1") > 0);
    ASSERT_EQ(gFreedOldBorrowIds.size(), 1U);
    EXPECT_EQ(gFreedOldBorrowIds[0], "old_bid_1");
    ASSERT_EQ(gCapturedRedirections.size(), 1U);
    EXPECT_EQ(gCapturedRedirections[0].first, "old_bid_1");
    EXPECT_EQ(gCapturedRedirections[0].second, "large");
}

} // namespace mempooling::over_commit