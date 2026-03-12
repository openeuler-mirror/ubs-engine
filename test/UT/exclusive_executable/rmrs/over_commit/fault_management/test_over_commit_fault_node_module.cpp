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
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string &, std::vector<UbseNumaMemoryDebtInfo> &))

        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage,
               MpResult(*)(OverCommitFaultMemIdModule *, std::string, uint64_t))
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
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string &, std::vector<UbseNumaMemoryDebtInfo> &))

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
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string &, std::vector<UbseNumaMemoryDebtInfo> &))
        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage,
               MpResult(*)(OverCommitFaultMemIdModule *, std::string, uint64_t))
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

    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string &, std::vector<UbseNumaMemoryDebtInfo> &))
        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage,
               MpResult(*)(OverCommitFaultMemIdModule *, std::string, uint64_t))
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
    MOCKER_CPP(&UbseGetNumaMemDebtInfoWithNode, uint32_t(*)(const std::string &, std::vector<UbseNumaMemoryDebtInfo> &))
        .stubs()
        .with(any(), outBound(infos))
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage,
               MpResult(*)(OverCommitFaultMemIdModule *, std::string, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

MpSceneType MockGetSceneVM(MpConfiguration *This)
{
    MpSceneType::VIRTUAL_SCENE;
}
MpResult GetNumaBindTypeSingle(OverCommitStorage *This, const std::string &nodeId, NumaBindType &value)
{
    value = NumaBindType::BIND_SINGLE;
    return MEM_POOLING_OK;
}

MpResult GetNumaBindTypeMulti(OverCommitStorage *This, const std::string &nodeId, NumaBindType &value)
{
    value = NumaBindType::BIND_MULTIPLE;
    return MEM_POOLING_OK;
}

MpResult MockGetNumaMemInfos(const std::string &nodeId, const std::set<int16_t> &numaIds,
                             std::map<int, mempooling::NumaMetaData> &numaMemInfos)
{
    mempooling::NumaMetaData numaMetaData;
    numaMetaData.memFree = 4;
    numaMemInfos[-1] = numaMetaData;
    numaMemInfos[0] = numaMetaData;
    return MEM_POOLING_OK;
}

MpResult MockGetNumaMemInfos2(const std::string &nodeId, const std::set<int16_t> &numaIds,
                              std::map<int, mempooling::NumaMetaData> &numaMemInfos)
{
    mempooling::NumaMetaData numaMetaData;
    numaMetaData.memFree = 4;
    numaMemInfos[0] = numaMetaData;
    return MEM_POOLING_OK;
}

MpResult MockGetNumaMemInfos3(const std::string &nodeId, const std::set<int16_t> &numaIds,
                              std::map<int, mempooling::NumaMetaData> &numaMemInfos)
{
    mempooling::NumaMetaData numaMetaData;
    numaMetaData.memFree = 4;
    numaMemInfos[-1] = numaMetaData;
    return MEM_POOLING_OK;
}

MpResult MockCollectBorrowRecords(const std::string nodeId, std::vector<BorrowRecord> &borrowRecords)
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
               MpResult(*)(BorrowInNodeData &, uint64_t &, uint16_t &, uid_t &, std::string &))
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
               MpResult(*)(BorrowInNodeData &, uint64_t &, uint16_t &, uid_t &, std::string &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam &, uint16_t, uint16_t, std::vector<VmNumaInfo> &,
                           mempooling::WaterMark &))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageFail3)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData &, uint64_t &, uint16_t &, uid_t &, std::string &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam &, uint16_t, uint16_t, std::vector<VmNumaInfo> &,
                           mempooling::WaterMark &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult &, uint64_t, std::vector<VmNumaInfo> &))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageFail4)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData &, uint64_t &, uint16_t &, uid_t &, std::string &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam &, uint16_t, uint16_t, std::vector<VmNumaInfo> &,
                           mempooling::WaterMark &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult &, uint64_t, std::vector<VmNumaInfo> &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(SrcMemoryBorrowParam, uint64_t, WaterMark, MemBorrowExecuteResult &))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageFail5)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData &, uint64_t &, uint16_t &, uid_t &, std::string &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam &, uint16_t, uint16_t, std::vector<VmNumaInfo> &,
                           mempooling::WaterMark &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult &, uint64_t, std::vector<VmNumaInfo> &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(SrcMemoryBorrowParam, uint64_t, WaterMark, MemBorrowExecuteResult &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdExecuteRpc, MpResult(*)(OverCommitFaultMemIdExecuteParam, std::string))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdFaultManage(borrowInNid, memId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultNodeModule, MemIdFaultManageFail6)
{
    std::string borrowInNid = "node1";
    uint64_t memId = 1;
    BorrowInNodeData borNodeData = {.borrowInNid = borrowInNid, .memId = memId};
    MOCKER_CPP(&OverCommitFaultMemIdModule::IsBorrowIdOfCurNidOverCommit,
               MpResult(*)(BorrowInNodeData &, uint64_t &, uint16_t &, uid_t &, std::string &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::PrepareParamForBorrowMem,
               MpResult(*)(outinterface::SrcMemoryBorrowParam &, uint16_t, uint16_t, std::vector<VmNumaInfo> &,
                           mempooling::WaterMark &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetSelectPids,
               MpResult(*)(FMVmInfoResult &, uint64_t, std::vector<VmNumaInfo> &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(SrcMemoryBorrowParam, uint64_t, WaterMark, MemBorrowExecuteResult &))
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
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

} // namespace mempooling::over_commit