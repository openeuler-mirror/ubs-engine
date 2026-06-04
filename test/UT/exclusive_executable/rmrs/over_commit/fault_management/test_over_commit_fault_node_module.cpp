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

#include "fault_node_module.h"
#include "mempooling_interface.h"
#include "mp_mem_json_util.h"
#include "over_commit_fault_node_module.cpp"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

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

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
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

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
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
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
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
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
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
    payload[0].ratio = 50; // 50%
    return MEM_POOLING_OK;
}
SmapQueryFuncType GetSmapGetRemoteProcessesFuncMock()
{
    return FakeSmapQueryProcess;
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

// Mock: smap query returns OK with multiple pids
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
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
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
    g_smapRemoveCalled = false;
    g_removedPids.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncNullMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemoveProcessTrackingHelper, int (*)(const std::vector<pid_t>&, int))
        .stubs()
        .will(invoke(SmapRemoveProcessTrackingHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(1);

    EXPECT_FALSE(g_smapRemoveCalled);
}

/*
 * 用例描述：smap查询返回错误时，RemovePidsOnRemoteNuma应安全返回，不调用移除逻辑
 * 测试步骤：
 * 1. Mock GetSmapGetRemoteProcessesFunc 返回查询函数，该函数返回 MEM_POOLING_ERROR
 * 2. 调用 RemovePidsOnRemoteNuma
 * 预期结果：
 * 1. 函数正常返回
 * 2. SmapRemoveProcessTrackingHelper 不被调用
 */
TEST_F(TestOverCommitFaultNodeModule, RemovePidsOnRemoteNuma_SmapQueryFailed_NoAction)
{
    g_smapRemoveCalled = false;
    g_removedPids.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncErrorMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemoveProcessTrackingHelper, int (*)(const std::vector<pid_t>&, int))
        .stubs()
        .will(invoke(SmapRemoveProcessTrackingHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(1);

    EXPECT_FALSE(g_smapRemoveCalled);
}

/*
 * 用例描述：smap查询成功但retLen为0时，不调用移除逻辑
 * 测试步骤：
 * 1. Mock GetSmapGetRemoteProcessesFunc 返回查询函数，该函数返回 MEM_POOLING_OK 且 retLen=0
 * 2. 调用 RemovePidsOnRemoteNuma
 * 预期结果：
 * 1. 函数正常返回
 * 2. SmapRemoveProcessTrackingHelper 不被调用
 */
TEST_F(TestOverCommitFaultNodeModule, RemovePidsOnRemoteNuma_NoPids_NoAction)
{
    g_smapRemoveCalled = false;
    g_removedPids.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncEmptyMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemoveProcessTrackingHelper, int (*)(const std::vector<pid_t>&, int))
        .stubs()
        .will(invoke(SmapRemoveProcessTrackingHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(1);

    EXPECT_FALSE(g_smapRemoveCalled);
}

/*
 * 用例描述：smap查询成功且存在pid时，调用SmapRemoveProcessTrackingHelper移除所有pid
 * 测试步骤：
 * 1. Mock GetSmapGetRemoteProcessesFunc 返回查询函数，该函数返回3个pid (1001, 1002, 1003)
 * 2. 调用 RemovePidsOnRemoteNuma
 * 预期结果：
 * 1. SmapRemoveProcessTrackingHelper 被调用
 * 2. 传入的pid列表包含 1001, 1002, 1003
 */
TEST_F(TestOverCommitFaultNodeModule, RemovePidsOnRemoteNuma_WithPids_RemoveAll)
{
    g_smapRemoveCalled = false;
    g_removedPids.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncMultiMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemoveProcessTrackingHelper, int (*)(const std::vector<pid_t>&, int))
        .stubs()
        .will(invoke(SmapRemoveProcessTrackingHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(2);

    EXPECT_TRUE(g_smapRemoveCalled);
    ASSERT_EQ(g_removedPids.size(), 3u);
    EXPECT_EQ(g_removedPids[0], 1001);
    EXPECT_EQ(g_removedPids[1], 1002);
    EXPECT_EQ(g_removedPids[2], 1003);
}

/*
 * 用例描述：smap查询成功且存在单个pid时，正确移除
 * 测试步骤：
 * 1. Mock GetSmapGetRemoteProcessesFunc 返回查询函数，该函数返回1个pid (1234)
 * 2. 调用 RemovePidsOnRemoteNuma
 * 预期结果：
 * 1. SmapRemoveProcessTrackingHelper 被调用
 * 2. 传入的pid列表仅包含 1234
 */
TEST_F(TestOverCommitFaultNodeModule, RemovePidsOnRemoteNuma_SinglePid_RemoveCorrectly)
{
    g_smapRemoveCalled = false;
    g_removedPids.clear();

    MOCKER_CPP(SmapModule::GetSmapGetRemoteProcessesFunc, SmapGetRemotePidsFunc(*)())
        .stubs()
        .will(invoke(GetSmapGetRemoteProcessesFuncMock));

    MOCKER_CPP(&MpSmapHelper::SmapRemoveProcessTrackingHelper, int (*)(const std::vector<pid_t>&, int))
        .stubs()
        .will(invoke(SmapRemoveProcessTrackingHelperMock));

    OverCommitFaultNodeModule module;
    module.RemovePidsOnRemoteNuma(1);

    EXPECT_TRUE(g_smapRemoveCalled);
    ASSERT_EQ(g_removedPids.size(), 1u);
    EXPECT_EQ(g_removedPids[0], 1234);
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
    state.remoteTotalSizeKB = 1024;
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

} // namespace mempooling::over_commit