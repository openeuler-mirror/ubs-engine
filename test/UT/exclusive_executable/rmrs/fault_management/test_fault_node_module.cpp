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

#include <cstring>
#include <iostream>

#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "ubse_com.h"
#include "fault_memid_helper.h"
#include "fault_memid_module.h"
#include "fault_node_module.h"
#include "mem_borrow_executor.h"
#include "mp_smap_helper.h"
#include "rmrs_serialize.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ubse::com;
using namespace mempooling::smap;
using namespace mempooling::exportV2;
using namespace rmrs::serialize;
using namespace rmrs;

namespace mempooling {

class TestFaultNodeModule : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(TestFaultNodeModule, TestDetermineNodeTypeFail)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(returnValue(1));
    const std::string nodeId = "Node1";
    NodeType nodeType;
    MpResult res = FaultNodeModule::Instance().DetermineNodeType(nodeId, nodeType);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, TestDetermineNodeTypeFail1)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(returnValue(0));
    const std::string nodeId = "Node1";
    NodeType nodeType;
    MpResult res = FaultNodeModule::Instance().DetermineNodeType(nodeId, nodeType);
    ASSERT_EQ(res, 0);
}

MpResult TestCollectBorrowRecordsIn(BorrowRecordHelper* This, const std::string nodeId,
                                    std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.lentNode = "";
    record.name = "";
    record.size = 1;
    record.lentMemId = {1};
    record.lentSocketId = 1;
    record.borrowNode = "Node0";
    record.borrowLocalNuma = 1;
    record.borrowRemoteNuma = 5;
    record.borrowMemId = {1};
    LentNuma lentNuma{1, 1};
    record.lentNuma.push_back(lentNuma);
    std::vector<BorrowRecord> borrowRecordList{record};
    borrowRecords = borrowRecordList;
    return 0;
}

MpResult TestCollectBorrowRecordsOut(BorrowRecordHelper* This, const std::string nodeId,
                                     std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.lentNode = "Node1";
    record.name = "";
    record.size = 1;
    record.lentMemId = {1};
    record.lentSocketId = 1;
    record.borrowNode = "Node0";
    record.borrowLocalNuma = 1;
    record.borrowRemoteNuma = 5;
    record.borrowMemId = {1};
    LentNuma lentNuma{1, 1};
    record.lentNuma.push_back(lentNuma);
    std::vector<BorrowRecord> borrowRecordList{record};
    borrowRecords = borrowRecordList;
    return 0;
}

TEST_F(TestFaultNodeModule, TestDetermineNodeTypeFail2)
{
    MOCKER_CPP(
        &BorrowRecordHelper::CollectBorrowRecords,
        MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(TestCollectBorrowRecordsOut));
    const std::string nodeId = "Node1";
    NodeType nodeType;
    MpResult res = FaultNodeModule::Instance().DetermineNodeType(nodeId, nodeType);
    ASSERT_EQ(res, 0);
    ASSERT_EQ(nodeType, NodeType::BORROW_OUT);
}

TEST_F(TestFaultNodeModule, TestDetermineNodeTypeFail3)
{
    MOCKER_CPP(
        &BorrowRecordHelper::CollectBorrowRecords,
        MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(TestCollectBorrowRecordsIn));
    const std::string nodeId = "Node1";
    NodeType nodeType;
    MpResult res = FaultNodeModule::Instance().DetermineNodeType(nodeId, nodeType);
    ASSERT_EQ(res, 0);
    ASSERT_EQ(nodeType, NodeType::BORROW_IN);
}

MpResult TestCollectBorrowRecords(BorrowRecordHelper* This, const std::string nodeId,
                                  std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.borrowNode = "Node0";
    record.name = "name1";
    borrowRecords.push_back(record);
    return 0;
}

TEST_F(TestFaultNodeModule, QueryNumaAllPidFail1)
{
    uint16_t numaId = 1;
    std::vector<pid_t> pidList;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(1));
    auto res = FaultNodeModule::Instance().QueryNumaAllPid(numaId, pidList);
    ASSERT_EQ(res, false);
}

TEST_F(TestFaultNodeModule, QueryNumaAllPidFail2)
{
    uint16_t numaId = 1;
    std::vector<pid_t> pidList;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(0));
    auto res = FaultNodeModule::Instance().QueryNumaAllPid(numaId, pidList);
    ASSERT_EQ(res, false);
}

MpResult Test111_GetVmInfoImmediately(std::vector<VmDomainInfo>& vmDomainInfos)
{
    VmDomainInfo vmDomainInfo;
    exportV2::VmDomainNumaInfo vmDomainNumaInfo;
    vmDomainNumaInfo.numaId = 1;
    vmDomainNumaInfo.isLocal = 0;
    vmDomainNumaInfo.socketId = -1;
    vmDomainInfo.metaData.pid = 1;
    vmDomainInfos.push_back(vmDomainInfo);
    return 0;
}

TEST_F(TestFaultNodeModule, QueryNumaAllPidSuccess3)
{
    uint16_t numaId = 1;
    std::vector<pid_t> pidList;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(Test111_GetVmInfoImmediately));
    auto res = FaultNodeModule::Instance().QueryNumaAllPid(numaId, pidList);
    ASSERT_EQ(res, false);
}

TEST_F(TestFaultNodeModule, QueryNumaAllPidFail4)
{
    uint16_t numaId = 2;
    std::vector<pid_t> pidList;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(Test111_GetVmInfoImmediately));
    auto res = FaultNodeModule::Instance().QueryNumaAllPid(numaId, pidList);
    ASSERT_EQ(res, false);
}

TEST_F(TestFaultNodeModule, SwitchMigrateForNumaVmFail1)
{
    std::vector<pid_t> pidList;
    int enable;
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(1));
    auto res = FaultNodeModule::Instance().SwitchMigrateForNumaVm(pidList, enable);
    ASSERT_EQ(res, false);
}

TEST_F(TestFaultNodeModule, SwitchMigrateForNumaVmSuccesss2)
{
    std::vector<pid_t> pidList;
    int enable;
    MOCKER_CPP(&MpSmapHelper::SmapEnableProcessMigrateHelper,
               MpResult(*)(pid_t * pidArr, int len, int enable, int flags))
        .stubs()
        .will(returnValue(0));
    auto res = FaultNodeModule::Instance().SwitchMigrateForNumaVm(pidList, enable);
    ASSERT_EQ(res, true);
}

TEST_F(TestFaultNodeModule, GenerateMigrateNumaMsgListSuccess1)
{
    NumaReplaceReturnMsg rpcMsg;
    MigrateBorrowRecord record;
    rpcMsg.migrateBorrowRecordList.push_back(record);
    rpcMsg.srcNid = 1;
    rpcMsg.destNid = 2;
    std::vector<MigrateNumaMsg> msgList;
    auto res = FaultNodeModule::Instance().GenerateMigrateNumaMsgList(rpcMsg, msgList);
    ASSERT_EQ(res, true);
}

TEST_F(TestFaultNodeModule, ExecMigrateRemoteNumaToNumaFail1)
{
    std::vector<MigrateNumaMsg> msgList;
    MigrateNumaMsg msg;
    msg.destNid = 1;
    msgList.push_back(msg);
    NumaReplaceReturnMsg rpcMsg;
    MOCKER_CPP(&MpSmapHelper::AllocateHugePages,
               MpResult(*)(std::vector<uint64_t> & remoteNumaIds, std::vector<uint64_t> & borrowSizes))
        .stubs()
        .will(returnValue(1));
    auto res = FaultNodeModule::Instance().ExecMigrateRemoteNumaToNuma(rpcMsg, msgList);
    ASSERT_EQ(res, false);
}

TEST_F(TestFaultNodeModule, ExecMigrateRemoteNumaToNumaFail2)
{
    std::vector<MigrateNumaMsg> msgList;
    MigrateNumaMsg msg;
    msg.destNid = 1;
    msgList.push_back(msg);
    NumaReplaceReturnMsg rpcMsg;
    MOCKER_CPP(&MpSmapHelper::AllocateHugePages,
               MpResult(*)(std::vector<uint64_t> & remoteNumaIds, std::vector<uint64_t> & borrowSizes))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MpSmapHelper::SmapMigrateRemoteNuma, MpResult(*)(MigrateNumaMsg msg)).stubs().will(returnValue(1));
    auto res = FaultNodeModule::Instance().ExecMigrateRemoteNumaToNuma(rpcMsg, msgList);
    ASSERT_EQ(res, false);
}

TEST_F(TestFaultNodeModule, GetBorrowNodeInfoSuccess1)
{
    std::string nodeId;
    std::vector<BorrowRecord> borrowRecords;
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(returnValue(0));
    auto res = FaultNodeModule::Instance().GetBorrowNodeInfo(nodeId, borrowRecords);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultNodeModule, GetBorrowNodeInfoFail2)
{
    std::string nodeId;
    std::vector<BorrowRecord> borrowRecords;
    MOCKER_CPP(&BorrowRecordHelper::GetFragmentFaultBorrowRecords,
               MpResult(*)(const std::string nodeId, std::vector<BorrowRecord> &borrowRecords))
        .stubs()
        .will(returnValue(1));
    auto res = FaultNodeModule::Instance().GetBorrowNodeInfo(nodeId, borrowRecords);
    ASSERT_EQ(res, 1);
}

std::vector<std::string> TestGetNodeIds()
{
    std::vector<std::string> allNodeIdList = {"Node1"};
    return allNodeIdList;
}

std::vector<std::string> TestGetNodeIdsEmptyMock()
{
    std::vector<std::string> allNodeIdList;
    return allNodeIdList;
}

TEST_F(TestFaultNodeModule, GetBorrowAbleNodeIdListFail1)
{
    std::string curDealNodeId = "Node2";
    std::vector<std::string> borrowAbleNodeIdList;
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string>(*)()).stubs().will(invoke(TestGetNodeIds));
    auto res = FaultNodeModule::Instance().GetBorrowAbleNodeIdList(curDealNodeId, borrowAbleNodeIdList);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultNodeModule, TestGetBorrowAbleNodeIdListFail_EmptyNodeId)
{
    std::string curDealNodeId = "Node2";
    std::vector<std::string> borrowAbleNodeIdList;
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string>(*)())
        .stubs()
        .will(invoke(TestGetNodeIdsEmptyMock));
    auto res = FaultNodeModule::Instance().GetBorrowAbleNodeIdList(curDealNodeId, borrowAbleNodeIdList);
    ASSERT_EQ(res, 1);
}

MpResult DetermineNodeTypeMockBorrowIn(FaultNodeModule& thisPtr, const std::string nodeId, NodeType& nodeType)
{
    nodeType = NodeType::NO_RECORD;
    return MEM_POOLING_OK;
}

TEST_F(TestFaultNodeModule, TestGetBorrowAbleNodeIdListSuccess_NoRecord)
{
    MOCKER_CPP(&FaultNodeModule::DetermineNodeType,
               MpResult(*)(FaultNodeModule & thisPtr, const std::string nodeId, NodeType& nodeType))
        .stubs()
        .will(invoke(DetermineNodeTypeMockBorrowIn));
    std::string curDealNodeId = "Node2";
    std::vector<std::string> borrowAbleNodeIdList;
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string>(*)()).stubs().will(invoke(TestGetNodeIds));
    auto res = FaultNodeModule::Instance().GetBorrowAbleNodeIdList(curDealNodeId, borrowAbleNodeIdList);
    ASSERT_EQ(res, 0);
}

MpResult TestCollectBorrowableInfoList(BorrowRecordHelper* This, const std::vector<std::string>& nodeId,
                                       std::vector<NodeMemoryInfoWithReservedMem>& nodeMemoryInfoList)
{
    NodeMemoryInfoWithReservedMem nodeMemoryInfo;
    nodeMemoryInfo.reservedMem = 1024;
    nodeMemoryInfo.sharedMem = 1;
    nodeMemoryInfo.lentMemory = 0;

    RackNumaMemInfo rackNumaMemInfo;
    rackNumaMemInfo.reservedMem = 1024;
    rackNumaMemInfo.lentMem = 1;
    rackNumaMemInfo.sharedMem = 0;

    NodeMemoryInfoWithReservedMem nodeMemoryInfo1;
    nodeMemoryInfo.reservedMem = 2048;
    nodeMemoryInfo.sharedMem = 128;
    nodeMemoryInfo.lentMemory = 128;

    RackNumaMemInfo rackNumaMemInfo1;
    rackNumaMemInfo.reservedMem = 2048;
    rackNumaMemInfo.lentMem = 128;
    rackNumaMemInfo.sharedMem = 128;

    nodeMemoryInfo.numaMemInfo.push_back(rackNumaMemInfo);
    nodeMemoryInfo.numaMemInfo.push_back(rackNumaMemInfo1);
    nodeMemoryInfoList.push_back(nodeMemoryInfo);
    nodeMemoryInfoList.push_back(nodeMemoryInfo1);
    return 0;
}

TEST_F(TestFaultNodeModule, GetBorrowAbleNodeInfoSortByMemSizeSuccess1)
{
    std::vector<std::string> borrowAbleNodeIdList;
    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList;
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfoList,
               MpResult(*)(BorrowRecordHelper * This, const std::vector<std::string>& nodeId,
                           std::vector<NodeMemoryInfoWithReservedMem>& nodeMemoryInfoList))
        .stubs()
        .will(invoke(TestCollectBorrowableInfoList));
    auto res =
        FaultNodeModule::Instance().GetBorrowAbleNodeInfoSortByMemSize(borrowAbleNodeIdList, ableNodeMemInfoList);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultNodeModule, GetBorrowAbleNodeInfoSortByMemSizeSuccess2)
{
    std::vector<std::string> borrowAbleNodeIdList;
    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList;
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfoList,
               MpResult(*)(BorrowRecordHelper * This, const std::vector<std::string>& nodeId,
                           std::vector<NodeMemoryInfoWithReservedMem>& nodeMemoryInfoList))
        .stubs()
        .will(invoke(TestCollectBorrowableInfoList));
    auto res =
        FaultNodeModule::Instance().GetBorrowAbleNodeInfoSortByMemSize(borrowAbleNodeIdList, ableNodeMemInfoList);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultNodeModule, GetBorrowAbleNodeInfoSortByMemSizeFail2)
{
    std::vector<std::string> borrowAbleNodeIdList;
    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList;
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfoList,
               MpResult(*)(BorrowRecordHelper * This, const std::vector<std::string>& nodeId,
                           std::vector<NodeMemoryInfoWithReservedMem>& nodeMemoryInfoList))
        .stubs()
        .will(returnValue(1));
    auto res =
        FaultNodeModule::Instance().GetBorrowAbleNodeInfoSortByMemSize(borrowAbleNodeIdList, ableNodeMemInfoList);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, MergeBorrowRecordsSuccess1)
{
    std::vector<BorrowRecord> borrowRecordList;
    BorrowRecord record;
    record.borrowNode = "node1";
    record.size = 10;
    record.name = "node";
    record.borrowMemId = {1};
    record.borrowRemoteNuma = 2;
    record.borrowLocalNuma = 1;
    borrowRecordList.push_back(record);
    std::vector<NodeBorrowRecord> nodeBorrowRecordList;
    FaultNodeModule::Instance().MergeBorrowRecords(borrowRecordList, nodeBorrowRecordList);
}

TEST_F(TestFaultNodeModule, MayBorrowFromNumaFail1)
{
    NodeBorrowRecord nodeBorrowRecord;
    NodeMemoryInfoWithReservedMem originNodeMemInfoItem;
    NodeMemoryInfoWithReservedMem nodeMemInfoItem;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;

    OnceBorrowRecord record;
    record.borrowSize = 1024;
    record.numaId = 1;
    record.memIdList = {1};
    record.borrowId = "id1";
    nodeBorrowRecord.onceBorrowRecordList.push_back(record);

    RackNumaMemInfo item;
    item.reservedMem = 1024;
    item.lentMem = 1;
    item.sharedMem = 0;
    item.canBorrowMem = 1;

    nodeMemInfoItem.numaMemInfo.push_back(item);

    auto res = FaultNodeModule::Instance().MayBorrowFromNuma(nodeBorrowRecord, originNodeMemInfoItem, nodeMemInfoItem,
                                                             borrowExecuteParamCollectList);
    ASSERT_EQ(res, false);
}

TEST_F(TestFaultNodeModule, MayBorrowFromNumaSuccess2)
{
    NodeBorrowRecord nodeBorrowRecord;
    NodeMemoryInfoWithReservedMem originNodeMemInfoItem;
    NodeMemoryInfoWithReservedMem nodeMemInfoItem;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;

    OnceBorrowRecord record;
    record.borrowSize = 1;
    record.numaId = 1;
    record.memIdList = {1};
    record.borrowId = "id1";
    nodeBorrowRecord.onceBorrowRecordList.push_back(record);
    nodeBorrowRecord.nodeId = "node1";

    RackNumaMemInfo item;
    item.reservedMem = 1024;
    item.lentMem = 1;
    item.sharedMem = 0;
    item.canBorrowMem = 1024;

    nodeMemInfoItem.numaMemInfo.push_back(item);
    nodeMemInfoItem.nodeId = "0";

    auto res = FaultNodeModule::Instance().MayBorrowFromNuma(nodeBorrowRecord, originNodeMemInfoItem, nodeMemInfoItem,
                                                             borrowExecuteParamCollectList);
    ASSERT_EQ(res, true);
}

TEST_F(TestFaultNodeModule, NodeMayBorrowFromOtherNodeFail1)
{
    NodeBorrowRecord nodeBorrowRecord;
    nodeBorrowRecord.borrowMemSize = 2048;

    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    NodeMemoryInfoWithReservedMem nodeMemInfoItem;
    nodeMemInfoItem.reservedMem = 1024;
    nodeMemInfoItem.sharedMem = 0;
    nodeMemInfoItem.lentMemory = 0;
    nodeMemInfoItem.canBorrowMem = 1024;
    ableNodeMemInfoList.push_back(nodeMemInfoItem);
    auto res = FaultNodeModule::Instance().NodeMayBorrowFromOtherNode(nodeBorrowRecord, ableNodeMemInfoList,
                                                                      borrowExecuteParamCollectList);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, NodeMayBorrowFromOtherNodeSuccess2)
{
    NodeBorrowRecord nodeBorrowRecord;
    nodeBorrowRecord.borrowMemSize = 10;

    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    NodeMemoryInfoWithReservedMem nodeMemInfoItem;
    nodeMemInfoItem.reservedMem = 1024;
    nodeMemInfoItem.sharedMem = 0;
    nodeMemInfoItem.lentMemory = 0;
    nodeMemInfoItem.canBorrowMem = 1024;
    ableNodeMemInfoList.push_back(nodeMemInfoItem);
    MOCKER_CPP(&FaultNodeModule::MayBorrowFromNuma,
               bool (*)(NodeBorrowRecord nodeBorrowRecord, NodeMemoryInfoWithReservedMem& originNodeMemInfoItem,
                        NodeMemoryInfoWithReservedMem nodeMemInfoItem,
                        std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList))
        .stubs()
        .will(returnValue(false))
        .then(returnValue(true));
    auto res = FaultNodeModule::Instance().NodeMayBorrowFromOtherNode(nodeBorrowRecord, ableNodeMemInfoList,
                                                                      borrowExecuteParamCollectList);
    ASSERT_EQ(res, 1);
    res = FaultNodeModule::Instance().NodeMayBorrowFromOtherNode(nodeBorrowRecord, ableNodeMemInfoList,
                                                                 borrowExecuteParamCollectList);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, MayBorrowFromOtherNodeFail1)
{
    std::string curDealNodeId;
    std::vector<BorrowRecord> borrowRecords;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeIdList,
               MpResult(*)(std::string curDealNodeId, std::vector<std::string> & borrowAbleNodeIdList))
        .stubs()
        .will(returnValue(1));
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    auto res = FaultNodeModule::Instance().MayBorrowFromOtherNode(curDealNodeId, borrowRecords,
                                                                  borrowExecuteParamCollectList, forwardMemIdParamList);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, MayBorrowFromOtherNodeFail2)
{
    std::string curDealNodeId;
    std::vector<BorrowRecord> borrowRecords;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeIdList,
               MpResult(*)(std::string curDealNodeId, std::vector<std::string> & borrowAbleNodeIdList))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeInfoSortByMemSize,
               MpResult(*)(std::vector<std::string> borrowAbleNodeIdList,
                           std::vector<NodeMemoryInfoWithReservedMem> & ableNodeMemInfoList))
        .stubs()
        .will(returnValue(1));
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    auto res = FaultNodeModule::Instance().MayBorrowFromOtherNode(curDealNodeId, borrowRecords,
                                                                  borrowExecuteParamCollectList, forwardMemIdParamList);
    ASSERT_EQ(res, 1);
}

void TestMergeBorrowRecords1(FaultNodeModule* This, std::vector<BorrowRecord> borrowRecordList,
                             std::vector<NodeBorrowRecord>& nodeBorrowRecordList)
{
    NodeBorrowRecord nodeBorrowRecordItem;
    nodeBorrowRecordItem.nodeId = "node1";
    nodeBorrowRecordList.push_back(nodeBorrowRecordItem);
}

TEST_F(TestFaultNodeModule, MayBorrowFromOtherNodeSuccess3)
{
    std::string curDealNodeId;
    std::vector<BorrowRecord> borrowRecords;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeIdList,
               MpResult(*)(std::string curDealNodeId, std::vector<std::string> & borrowAbleNodeIdList))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeInfoSortByMemSize,
               MpResult(*)(std::vector<std::string> borrowAbleNodeIdList,
                           std::vector<NodeMemoryInfoWithReservedMem> & ableNodeMemInfoList))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultNodeModule::MergeBorrowRecords,
               void (*)(FaultNodeModule* This, std::vector<BorrowRecord> borrowRecordList,
                        std::vector<NodeBorrowRecord>& nodeBorrowRecordList))
        .stubs()
        .will(invoke(TestMergeBorrowRecords1));
    MOCKER_CPP(
        &FaultNodeModule::NodeMayBorrowFromOtherNode,
        MpResult(*)(NodeBorrowRecord nodeBorrowRecord, std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList,
                    std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1));
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    auto res = FaultNodeModule::Instance().MayBorrowFromOtherNode(curDealNodeId, borrowRecords,
                                                                  borrowExecuteParamCollectList, forwardMemIdParamList);
    ASSERT_EQ(res, 0);
    res = FaultNodeModule::Instance().MayBorrowFromOtherNode(curDealNodeId, borrowRecords,
                                                             borrowExecuteParamCollectList, forwardMemIdParamList);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultNodeModule, FillBorrowExecuteParamFail1)
{
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    BorrowExecuteParam param;
    param.borrowNodeId = "0";
    param.lentNodeId = "1";
    borrowExecuteParamCollectList.push_back(param);
    MOCKER_CPP(&MemManager::GetSocketId, MpResult(*)(const std::string& nodeId, const int& numaId, int& socketId))
        .stubs()
        .will(returnValue(1));
    auto res = FaultNodeModule::Instance().FillBorrowExecuteParam(borrowExecuteParamCollectList);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, FillBorrowExecuteParamSuccess)
{
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    BorrowExecuteParam param;
    param.borrowNodeId = "0";
    param.lentNodeId = "1";
    borrowExecuteParamCollectList.push_back(param);
    MOCKER_CPP(&MemManager::GetSocketId, MpResult(*)(const std::string& nodeId, const int& numaId, int& socketId))
        .stubs()
        .will(returnValue(0));
    auto res = FaultNodeModule::Instance().FillBorrowExecuteParam(borrowExecuteParamCollectList);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultNodeModule, DealBorrowResultSuccess1)
{
    std::vector<BorrowExecuteParam> borrowExecuteParamList;
    std::vector<BorrowExecuteParam> successExecuteParamCollectList;
    std::vector<MemBorrowExecuteResult> resultList;
    MemBorrowExecuteResult result;
    result.presentNumaId = {1};
    result.borrowIds = {"1"};
    resultList.push_back(result);
    BorrowExecuteParam param1;
    borrowExecuteParamList.push_back(param1);
    auto res = FaultNodeModule::Instance().DealBorrowResult(borrowExecuteParamList, successExecuteParamCollectList,
                                                            resultList);
    ASSERT_EQ(res, true);
}

TEST_F(TestFaultNodeModule, DoExecuteBorrowSuccess1)
{
    std::vector<BorrowExecuteParam> successExecuteParamCollectList;
    std::pair<std::string, std::vector<BorrowExecuteParam>> nodeBorrowExecuteParam;
    int failNum = 1;
    BorrowExecuteParam param;
    param.borrowNodeId = "0";
    param.borrowSocketId = 1;
    param.borrowNumaId = 2;
    param.lentNumaId = 1;
    param.memSize = 1024;
    param.lentNodeId = "1";
    param.lentSocketId = 1;
    nodeBorrowExecuteParam = {"str1", {param}};
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams,
                           MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultNodeModule::DealBorrowResult,
               bool (*)(std::vector<BorrowExecuteParam>& borrowExecuteParamList,
                        std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                        std::vector<MemBorrowExecuteResult> resultList))
        .stubs()
        .will(returnValue(false));
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    FaultNodeModule::Instance().DoExecuteBorrow(successExecuteParamCollectList, nodeBorrowExecuteParam,
                                                forwardMemIdParamList);
}

TEST_F(TestFaultNodeModule, DoExecuteBorrowSuccess2)
{
    std::vector<BorrowExecuteParam> successExecuteParamCollectList;
    std::pair<std::string, std::vector<BorrowExecuteParam>> nodeBorrowExecuteParam;
    BorrowExecuteParam param;
    param.borrowNodeId = "0";
    param.borrowSocketId = 1;
    param.borrowNumaId = 2;
    param.lentNumaId = 1;
    param.memSize = 1024;
    param.lentNodeId = "1";
    param.lentSocketId = 1;
    nodeBorrowExecuteParam = {"str1", {param}};
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams,
                           MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(1));
    MOCKER_CPP(&FaultNodeModule::DealBorrowResult,
               bool (*)(std::vector<BorrowExecuteParam>& borrowExecuteParamList,
                        std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                        std::vector<MemBorrowExecuteResult> resultList))
        .stubs()
        .will(returnValue(false));
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    FaultNodeModule::Instance().DoExecuteBorrow(successExecuteParamCollectList, nodeBorrowExecuteParam,
                                                forwardMemIdParamList);
}

TEST_F(TestFaultNodeModule, ExecuteBorrowSuccess1)
{
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    std::vector<BorrowExecuteParam> successExecuteParamCollectList;
    BorrowExecuteParam param;
    param.borrowNodeId = "0";
    param.memIdList = {1};
    borrowExecuteParamCollectList.push_back(param);
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    FaultNodeModule::Instance().ExecuteBorrow(borrowExecuteParamCollectList, successExecuteParamCollectList,
                                              forwardMemIdParamList);
}

TEST_F(TestFaultNodeModule, ExecuteBorrowSuccess2)
{
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    std::vector<BorrowExecuteParam> successExecuteParamCollectList;
    BorrowExecuteParam param;
    param.borrowNodeId = "0";
    param.memIdList = {1};
    borrowExecuteParamCollectList.push_back(param);
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    FaultNodeModule::Instance().ExecuteBorrow(borrowExecuteParamCollectList, successExecuteParamCollectList,
                                              forwardMemIdParamList);
}

TEST_F(TestFaultNodeModule, ExecuteBorrowSuccess3)
{
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    std::vector<BorrowExecuteParam> successExecuteParamCollectList;
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    FaultNodeModule::Instance().ExecuteBorrow(borrowExecuteParamCollectList, successExecuteParamCollectList,
                                              forwardMemIdParamList);
}

TEST_F(TestFaultNodeModule, ExecuteNumaReplaceAndReturnSuccess1)
{
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    bool failFlag;
    BorrowExecuteParam borrowExecuteParam;
    borrowExecuteParam.borrowNodeId = "node0";
    borrowExecuteParam.oldNumaId = 1;
    borrowExecuteParam.newNumaId = 1;
    borrowExecuteParam.memIdList = {0};
    borrowExecuteParam.memSize = 1024;
    borrowExecuteParamCollectList.push_back(borrowExecuteParam);
    auto res = FaultNodeModule::Instance().ExecuteNumaReplaceAndReturn(borrowExecuteParamCollectList, failFlag, true);
    ASSERT_EQ(res, 0);
}

TEST_F(TestFaultNodeModule, ProcessBorrowOutNodeFaultFail1)
{
    std::string nodeId = "node1";
    MOCKER_CPP(&FaultNodeModule::GetBorrowNodeInfo,
               MpResult(*)(std::string nodeId, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(returnValue(1));
    auto res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, ProcessBorrowOutNodeFaultSuccess2)
{
    std::string nodeId = "node1";
    MOCKER_CPP(&FaultNodeModule::GetBorrowNodeInfo,
               MpResult(*)(std::string nodeId, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(returnValue(0));
    auto res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);
    ASSERT_EQ(res, 0);
}

MpResult TestGetBorrowNodeInfo(FaultNodeModule* This, std::string nodeId, std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.lentMemId = {1};
    borrowRecords.push_back(record);
    return MEM_POOLING_OK;
}

TEST_F(TestFaultNodeModule, ProcessBorrowOutNodeFaultSuccess3)
{
    GTEST_SKIP();
    std::string nodeId = "node1";
    MOCKER_CPP(&FaultNodeModule::GetBorrowNodeInfo,
               MpResult(*)(FaultNodeModule * This, std::string nodeId, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(TestGetBorrowNodeInfo));
    MOCKER_CPP(&FaultNodeModule::MayBorrowFromOtherNode,
               MpResult(*)(std::string curDealNodeId, std::vector<BorrowRecord> borrowRecords,
                           std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList,
                           std::vector<ForwardMemIdParam> & forwardMemIdParamList))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    auto res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);
    ASSERT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestFaultNodeModule, ProcessBorrowOutNodeFaultFail3)
{
    std::string nodeId = "node1";
    MOCKER_CPP(&FaultNodeModule::GetBorrowNodeInfo,
               MpResult(*)(FaultNodeModule * This, std::string nodeId, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(TestGetBorrowNodeInfo));
    MOCKER_CPP(&FaultNodeModule::MayBorrowFromOtherNode,
               MpResult(*)(std::string curDealNodeId, std::vector<BorrowRecord> borrowRecords,
                           std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList,
                           std::vector<ForwardMemIdParam> & forwardMemIdParamList))
        .stubs()
        .will(returnValue(1));
    auto res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);
    ASSERT_EQ(res, 1);
}

MpResult TestMayBorrowFromOtherNode(FaultNodeModule* This, std::string curDealNodeId,
                                    std::vector<BorrowRecord> borrowRecords,
                                    std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList,
                                    std::vector<ForwardMemIdParam>& forwardMemIdParamList)
{
    BorrowExecuteParam param;
    param.memIdList = {1};
    borrowExecuteParamCollectList.push_back(param);
    return 0;
}

TEST_F(TestFaultNodeModule, ProcessBorrowOutNodeFaultFail4)
{
    std::string nodeId = "node1";
    MOCKER_CPP(&FaultNodeModule::GetBorrowNodeInfo,
               MpResult(*)(FaultNodeModule * This, std::string nodeId, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(TestGetBorrowNodeInfo));
    MOCKER_CPP(&FaultNodeModule::MayBorrowFromOtherNode,
               MpResult(*)(FaultNodeModule * This, std::string curDealNodeId, std::vector<BorrowRecord> borrowRecords,
                           std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList,
                           std::vector<ForwardMemIdParam> & forwardMemIdParamList))
        .stubs()
        .will(invoke(TestMayBorrowFromOtherNode));
    MOCKER_CPP(&FaultNodeModule::FillBorrowExecuteParam,
               MpResult(*)(std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList))
        .stubs()
        .will(returnValue(1));
    auto res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, ProcessBorrowOutNodeFaultFail5)
{
    std::string nodeId = "node1";
    MOCKER_CPP(&FaultNodeModule::GetBorrowNodeInfo,
               MpResult(*)(FaultNodeModule * This, std::string nodeId, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(TestGetBorrowNodeInfo));
    MOCKER_CPP(&FaultNodeModule::MayBorrowFromOtherNode,
               MpResult(*)(FaultNodeModule * This, std::string curDealNodeId, std::vector<BorrowRecord> borrowRecords,
                           std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList,
                           std::vector<ForwardMemIdParam> & forwardMemIdParamList))
        .stubs()
        .will(invoke(TestMayBorrowFromOtherNode));
    MOCKER_CPP(&FaultNodeModule::FillBorrowExecuteParam,
               MpResult(*)(std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList))
        .stubs()
        .will(returnValue(0));
    auto res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, ProcessBorrowOutNodeFaultFail6)
{
    std::string nodeId = "node1";
    MOCKER_CPP(&FaultNodeModule::GetBorrowNodeInfo,
               MpResult(*)(FaultNodeModule * This, std::string nodeId, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(TestGetBorrowNodeInfo));
    MOCKER_CPP(&FaultNodeModule::MayBorrowFromOtherNode,
               MpResult(*)(FaultNodeModule * This, std::string curDealNodeId, std::vector<BorrowRecord> borrowRecords,
                           std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList,
                           std::vector<ForwardMemIdParam> & forwardMemIdParamList))
        .stubs()
        .will(invoke(TestMayBorrowFromOtherNode));
    MOCKER_CPP(&FaultNodeModule::FillBorrowExecuteParam,
               MpResult(*)(std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList))
        .stubs()
        .will(returnValue(0));
    auto res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);
    ASSERT_EQ(res, 1);
}

void TestExecuteBorrow1(FaultNodeModule* This, std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList,
                        std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                        std::vector<ForwardMemIdParam>& forwardMemIdParamList)
{
    BorrowExecuteParam param;
    param.borrowNodeId = "0";
    borrowExecuteParamCollectList.push_back(param);
}

TEST_F(TestFaultNodeModule, TestForwardMemIdFaultDealFail1)
{
    ForwardMemIdParam param;
    std::vector<ForwardMemIdParam> forwardMemIdParamList{param};
    ASSERT_EQ(FaultNodeModule::Instance().ForwardMemIdFaultDeal(forwardMemIdParamList, true), 1);
}

TEST_F(TestFaultNodeModule, TestForwardMemIdFaultDealFail2)
{
    ForwardMemIdParam param;
    param.memIdList.push_back(1);
    std::vector<ForwardMemIdParam> forwardMemIdParamList{param};
    MOCKER_CPP(&FaultMemIdHelper::FaultMemIdManageHelper, MpResult(*)(std::string importNodeId, uint64_t importMemId))
        .stubs()
        .will(returnValue(1));
    ASSERT_EQ(FaultNodeModule::Instance().ForwardMemIdFaultDeal(forwardMemIdParamList, true), 1);
}

TEST_F(TestFaultNodeModule, TestForwardMemIdFaultDealSuccess)
{
    ForwardMemIdParam param;
    param.memIdList.push_back(1);
    std::vector<ForwardMemIdParam> forwardMemIdParamList{param};
    MOCKER_CPP(&FaultMemIdHelper::FaultMemIdManageHelper, MpResult(*)(std::string importNodeId, uint64_t importMemId))
        .stubs()
        .will(returnValue(0));
    ASSERT_EQ(FaultNodeModule::Instance().ForwardMemIdFaultDeal(forwardMemIdParamList, true), 0);
}

TEST_F(TestFaultNodeModule, ProcessBorrowOutNodeFaultFail7)
{
    std::string nodeId = "node1";
    MOCKER_CPP(&FaultNodeModule::GetBorrowNodeInfo,
               MpResult(*)(FaultNodeModule * This, std::string nodeId, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(TestGetBorrowNodeInfo));
    MOCKER_CPP(&FaultNodeModule::MayBorrowFromOtherNode,
               MpResult(*)(FaultNodeModule * This, std::string curDealNodeId, std::vector<BorrowRecord> borrowRecords,
                           std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList,
                           std::vector<ForwardMemIdParam> & forwardMemIdParamList))
        .stubs()
        .will(invoke(TestMayBorrowFromOtherNode));
    MOCKER_CPP(&FaultNodeModule::FillBorrowExecuteParam,
               MpResult(*)(std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultNodeModule::ExecuteBorrow,
               void (*)(FaultNodeModule* This, std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList,
                        std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                        std::vector<ForwardMemIdParam>& forwardMemIdParamList))
        .stubs()
        .will(invoke(TestExecuteBorrow1));
    MOCKER_CPP(&FaultNodeModule::ExecuteNumaReplaceAndReturn,
               MpResult(*)(std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList,
                           std::set<std::string> & failSet, bool forceDeleteMem))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&FaultNodeModule::ForwardMemIdFaultDeal,
               MpResult(*)(std::vector<ForwardMemIdParam> forwardMemIdParamList, bool forceDeleteMem))
        .stubs()
        .will(returnValue(1));
    auto res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultNodeModule, NodeNumaReplaceReturnHandlerSucceed)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    int srcNid = 0;
    int destNid = 1;
    MigrateBorrowRecord migrateBorrowRecord;
    migrateBorrowRecord.memIdList = {1};
    migrateBorrowRecord.memSize = 1024;
    std::vector<MigrateBorrowRecord> migrateBorrowRecordList;
    migrateBorrowRecordList.push_back(migrateBorrowRecord);

    NumaReplaceReturnMsg param = {srcNid, destNid, migrateBorrowRecordList};
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MOCKER_CPP(&mempooling::FaultNodeModule::QueryNumaAllPid, bool (*)(uint16_t numaId, std::vector<pid_t>& pidList))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&mempooling::FaultNodeModule::SwitchMigrateForNumaVm, bool (*)(std::vector<pid_t> pidList, int enable))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&mempooling::FaultNodeModule::GenerateMigrateNumaMsgList,
               bool (*)(std::vector<pid_t> pidList, int enable))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&mempooling::FaultNodeModule::ExecMigrateRemoteNumaToNuma,
               bool (*)(NumaReplaceReturnMsg rpcMsg, std::vector<MigrateNumaMsg> msgList))
        .stubs()
        .will(returnValue(true));
    NodeNumaReplaceReturnHandler(req, resp);
    delete[] req.data;
}

TEST_F(TestFaultNodeModule, NodeNumaReplaceReturnHandlerFailed1)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;

    int srcNid = 0;
    int destNid = 1;
    MigrateBorrowRecord migrateBorrowRecord;
    migrateBorrowRecord.memIdList = {1};
    migrateBorrowRecord.memSize = 1024;
    std::vector<MigrateBorrowRecord> migrateBorrowRecordList;
    migrateBorrowRecordList.push_back(migrateBorrowRecord);

    NumaReplaceReturnMsg param = {srcNid, destNid, migrateBorrowRecordList};
    RmrsOutStream builder;
    builder << param;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    MOCKER_CPP(&mempooling::FaultNodeModule::QueryNumaAllPid, bool (*)(uint16_t numaId, std::vector<pid_t>& pidList))
        .stubs()
        .will(returnValue(false));
    NodeNumaReplaceReturnHandler(req, resp);
    delete[] req.data;
}

TEST_F(TestFaultNodeModule, NodeNumaReplaceReturnResHandlerFailed)
{
    UbseByteBuffer respData;
    respData.len = 1;
    respData.data = new uint8_t[respData.len];
    // 创建一个uint32_t变量作为result，并初始化为一个默认值
    uint32_t result = 1;
    // 将result的地址作为ctx传递
    void* ctx = &result;
    NodeNumaReplaceReturnResHandler(ctx, respData, 1);
    // 清理
    delete[] respData.data;
}

TEST_F(TestFaultNodeModule, NodeNumaReplaceReturnResHandlerSucceed)
{
    UbseByteBuffer respData;
    respData.len = 1;
    respData.data = new uint8_t[respData.len];
    respData.data[0] = 0;
    // 创建一个uint32_t变量作为result，并初始化为一个默认值
    uint32_t result = 0;
    // 将result的地址作为ctx传递
    void* ctx = &result;
    NodeNumaReplaceReturnResHandler(ctx, respData, 1);
    // 清理
    delete[] respData.data;
}

MpResult SetBorrowableList(MpParseGroupProviderConf* obj, const std::string& curNid,
                           std::unordered_set<std::string>& borrowableNidSet)
{
    borrowableNidSet.insert("1");
    borrowableNidSet.insert("2");
    borrowableNidSet.insert("3");
    return MEM_POOLING_OK;
}

bool CheckPlane(MemManager* obj, const std::string& srcNid, const uint16_t& srcSocketId, const std::string& dstNid,
                const uint16_t& dstSocketId)
{
    return (srcSocketId / 100) == (dstSocketId / 100);
}

TEST_F(TestFaultNodeModule, NullBorrowableListNodeMayBorrowFromOtherNodeFail)
{
    MOCKER_CPP(&mempooling::MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(const std::string&, std::unordered_set<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    NodeBorrowRecord nodeBorrowRecord;
    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    auto ret = FaultNodeModule::Instance().NodeMayBorrowFromOtherNode(nodeBorrowRecord, ableNodeMemInfoList,
                                                                      borrowExecuteParamCollectList);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestFaultNodeModule, NodeMayBorrowFromOtherNodeFailed)
{
    MOCKER_CPP(&mempooling::MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string&, std::unordered_set<std::string>&))
        .stubs()
        .will(invoke(SetBorrowableList));
    MOCKER_CPP(&mempooling::MemManager::JudgeSampPlane,
               bool (*)(MemManager*, const std::string&, const uint16_t&, const std::string&, const uint16_t&))
        .stubs()
        .will(invoke(CheckPlane));
    NodeBorrowRecord nodeBorrowRecord;
    nodeBorrowRecord.nodeId = "node0";
    nodeBorrowRecord.borrowSocketId = 1;
    nodeBorrowRecord.borrowMemSize = 10;
    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList(4);
    ableNodeMemInfoList[1].nodeId = "1";
    ableNodeMemInfoList[1].socketId = 101;
    ableNodeMemInfoList[2].nodeId = "2";
    ableNodeMemInfoList[2].socketId = 3;
    ableNodeMemInfoList[3].nodeId = "3";
    ableNodeMemInfoList[3].socketId = 2;
    ableNodeMemInfoList[3].canBorrowMem = 100;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    auto ret = FaultNodeModule::Instance().NodeMayBorrowFromOtherNode(nodeBorrowRecord, ableNodeMemInfoList,
                                                                      borrowExecuteParamCollectList);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestFaultNodeModule, NodeMayBorrowFromOtherNodeSuccess)
{
    MOCKER_CPP(&mempooling::MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string&, std::unordered_set<std::string>&))
        .stubs()
        .will(invoke(SetBorrowableList));
    MOCKER_CPP(&mempooling::MemManager::JudgeSampPlane,
               bool (*)(MemManager*, const std::string&, const uint16_t&, const std::string&, const uint16_t&))
        .stubs()
        .will(invoke(CheckPlane));
    MOCKER_CPP(&mempooling::FaultNodeModule::MayBorrowFromNuma,
               bool (*)(NodeBorrowRecord, NodeMemoryInfoWithReservedMem&, NodeMemoryInfoWithReservedMem,
                        std::vector<BorrowExecuteParam>&))
        .stubs()
        .will(returnValue(true));
    NodeBorrowRecord nodeBorrowRecord;
    nodeBorrowRecord.nodeId = "0";
    nodeBorrowRecord.borrowSocketId = 1;
    nodeBorrowRecord.borrowMemSize = 10;
    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList(4);
    ableNodeMemInfoList[1].nodeId = "1";
    ableNodeMemInfoList[1].socketId = 101;
    ableNodeMemInfoList[2].nodeId = "2";
    ableNodeMemInfoList[2].socketId = 3;
    ableNodeMemInfoList[3].nodeId = "3";
    ableNodeMemInfoList[3].socketId = 2;
    ableNodeMemInfoList[3].canBorrowMem = 100;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    auto ret = FaultNodeModule::Instance().NodeMayBorrowFromOtherNode(nodeBorrowRecord, ableNodeMemInfoList,
                                                                      borrowExecuteParamCollectList);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestFaultNodeModule, AllocateHugePageSuccess2)
{
    MOCKER_CPP(&mempooling::smap::MpSmapHelper::AllocateHugePages,
               MpResult(*)(std::vector<uint64_t>&, std::vector<uint64_t>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    uint16_t numaId = 0;
    uint64_t hugePageMemSize = 1024;
    auto ret = FaultNodeModule::Instance().AllocateHugePage(numaId, hugePageMemSize);
    ASSERT_EQ(ret, true);
}

void SetBorrowRecords(mempooling::FaultNodeModule* obj, std::vector<BorrowRecord> borrowRecords,
                      std::vector<NodeBorrowRecord>& nodeBorrowRecordList)
{
    nodeBorrowRecordList.emplace_back();
    nodeBorrowRecordList[0].onceBorrowRecordList.emplace_back();
}

TEST_F(TestFaultNodeModule, MayBorrowFromOtherNodeSuccess2)
{
    MOCKER_CPP(&mempooling::FaultNodeModule::GetBorrowAbleNodeIdList,
               MpResult(*)(std::string, std::vector<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::FaultNodeModule::GetBorrowAbleNodeInfoSortByMemSize,
               MpResult(*)(std::vector<std::string>, std::vector<NodeMemoryInfoWithReservedMem>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::FaultNodeModule::MergeBorrowRecords,
               void (*)(mempooling::FaultNodeModule*, std::vector<BorrowRecord>, std::vector<NodeBorrowRecord>&))
        .stubs()
        .will(invoke(SetBorrowRecords));
    MOCKER_CPP(
        &mempooling::FaultNodeModule::NodeMayBorrowFromOtherNode,
        MpResult(*)(NodeBorrowRecord, std::vector<NodeMemoryInfoWithReservedMem>&, std::vector<BorrowExecuteParam>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::string curDealNodeId = "node0";
    std::vector<BorrowRecord> borrowRecords;
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    auto ret = FaultNodeModule::Instance().MayBorrowFromOtherNode(curDealNodeId, borrowRecords,
                                                                  borrowExecuteParamCollectList, forwardMemIdParamList);
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

} // namespace mempooling