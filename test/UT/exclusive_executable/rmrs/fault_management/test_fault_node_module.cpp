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
#include "ubse_storage.h"
#include "fault_memid_helper.h"
#include "fault_memid_module.h"
#include "fault_node_module.h"
#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mp_borrow_conf_util.h"
#include "mp_smap_helper.h"
#include "over_commit_fault_memid_module.h"
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
               MpResult(*)(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
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
               bool (*)(NodeBorrowRecord nodeBorrowRecord, NodeMemoryInfoWithReservedMem & originNodeMemInfoItem,
                        NodeMemoryInfoWithReservedMem nodeMemInfoItem,
                        std::vector<BorrowExecuteParam> & borrowExecuteParamCollectList))
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
               void (*)(FaultNodeModule * This, std::vector<BorrowRecord> borrowRecordList,
                        std::vector<NodeBorrowRecord> & nodeBorrowRecordList))
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
               bool (*)(std::vector<BorrowExecuteParam> & borrowExecuteParamList,
                        std::vector<BorrowExecuteParam> & successExecuteParamCollectList,
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
               bool (*)(std::vector<BorrowExecuteParam> & borrowExecuteParamList,
                        std::vector<BorrowExecuteParam> & successExecuteParamCollectList,
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

MpResult TestGetBorrowNodeInfo(FaultNodeModule* This, std::string nodeId, std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.lentMemId = {1};
    borrowRecords.push_back(record);
    return MEM_POOLING_OK;
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

TEST_F(TestFaultNodeModule, BorrowIdLevelBorrowedExecute_FaultHandleMigrateFailed_StoreBorrowedDecision)
{
    BorrowGroupResult group;
    group.borrowNodeId = "node0";
    group.remoteNumaId = 3;

    BorrowIdLevelBorrowedDecision borrowedDecision;
    borrowedDecision.presentNumaId = 1;
    borrowedDecision.oldNumaId = 3;
    borrowedDecision.pids = {100, 200};
    borrowedDecision.borrowSize = 4096;
    borrowedDecision.oldName = "oldBorrowId";
    borrowedDecision.newName = "newBorrowId";

    MOCKER_CPP(&FaultNodeModule::FaultHandleMigrate,
               MpResult(FaultNodeModule::*)(uint16_t, uint16_t, std::vector<pid_t>&, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MOCKER_CPP(UbseStoragePutData, uint32_t(*)(const std::string&, const std::string&, UbseByteBuffer*))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    auto res = FaultNodeModule::Instance().BorrowIdLevelBorrowedExecute(group, borrowedDecision);
    EXPECT_EQ(res, MEM_POOLING_ERROR);

    BorrowedDecision storedDecision;
    MpResult queryRet = FaultHandleBorrowedDecision::Instance().Query(storedDecision, borrowedDecision.oldNumaId);
    EXPECT_EQ(queryRet, MEM_POOLING_OK);
    EXPECT_EQ(storedDecision.borrowNodeId, group.borrowNodeId);
    EXPECT_EQ(storedDecision.remoteNumaId, group.remoteNumaId);
    EXPECT_FALSE(storedDecision.isNumaLevel);
    EXPECT_EQ(storedDecision.borrowIdBorrowedDecisions.size(), 1u);
    EXPECT_EQ(storedDecision.borrowIdBorrowedDecisions[0].oldName, borrowedDecision.oldName);
    EXPECT_EQ(storedDecision.borrowIdBorrowedDecisions[0].newName, borrowedDecision.newName);
    EXPECT_EQ(storedDecision.borrowIdBorrowedDecisions[0].presentNumaId, borrowedDecision.presentNumaId);
    EXPECT_EQ(storedDecision.borrowIdBorrowedDecisions[0].oldNumaId, borrowedDecision.oldNumaId);
    EXPECT_EQ(storedDecision.borrowIdBorrowedDecisions[0].borrowSize, borrowedDecision.borrowSize);
}

TEST_F(TestFaultNodeModule, NumaLevelBorrowedExecute_FaultHandleMigrateFailed_StoreBorrowedDecision)
{
    BorrowGroupResult group;
    group.borrowNodeId = "node1";
    group.remoteNumaId = 5;

    NumaLevelBorrowedDecision decision;
    decision.presentNumaId = 2;
    decision.oldNumaId = 5;
    decision.pids = {300, 400};
    decision.totalBorrowSize = 8192;
    decision.borrowResultMap = {{"oldName1", "newName1"}};

    MOCKER_CPP(&FaultNodeModule::FaultHandleMigrate,
               MpResult(FaultNodeModule::*)(uint16_t, uint16_t, std::vector<pid_t>&, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MOCKER_CPP(UbseStoragePutData, uint32_t(*)(const std::string&, const std::string&, UbseByteBuffer*))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    auto res = FaultNodeModule::Instance().NumaLevelBorrowedExecute(group, decision);
    EXPECT_EQ(res, MEM_POOLING_ERROR);

    BorrowedDecision storedDecision;
    MpResult queryRet = FaultHandleBorrowedDecision::Instance().Query(storedDecision, decision.oldNumaId);
    EXPECT_EQ(queryRet, MEM_POOLING_OK);
    EXPECT_EQ(storedDecision.borrowNodeId, group.borrowNodeId);
    EXPECT_EQ(storedDecision.remoteNumaId, group.remoteNumaId);
    EXPECT_TRUE(storedDecision.isNumaLevel);
    EXPECT_EQ(storedDecision.numaBorrowedDecision.presentNumaId, decision.presentNumaId);
    EXPECT_EQ(storedDecision.numaBorrowedDecision.oldNumaId, decision.oldNumaId);
    EXPECT_EQ(storedDecision.numaBorrowedDecision.totalBorrowSize, decision.totalBorrowSize);
    EXPECT_EQ(storedDecision.numaBorrowedDecision.borrowResultMap.size(), decision.borrowResultMap.size());
}

// ==================== GroupBorrowRecordsByNuma ====================

TEST_F(TestFaultNodeModule, GroupBorrowRecordsByNuma_EmptyRecords_ReturnsEmpty)
{
    std::vector<BorrowRecord> records;
    auto groups = FaultNodeModule::Instance().GroupBorrowRecordsByNuma(records);
    ASSERT_TRUE(groups.empty());
}

TEST_F(TestFaultNodeModule, GroupBorrowRecordsByNuma_SingleRecord_ReturnsOneGroup)
{
    std::vector<BorrowRecord> records;
    BorrowRecord rec;
    rec.borrowNode = "Node1";
    rec.borrowRemoteNuma = 3;
    rec.size = 1024;
    rec.borrowSocketId = 1;
    records.push_back(rec);
    auto groups = FaultNodeModule::Instance().GroupBorrowRecordsByNuma(records);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].borrowNodeId, "Node1");
    EXPECT_EQ(groups[0].remoteNumaId, 3);
    EXPECT_EQ(groups[0].totalSize, 1024u);
    EXPECT_EQ(groups[0].records.size(), 1u);
}

TEST_F(TestFaultNodeModule, GroupBorrowRecordsByNuma_MultipleRecordsSameGroup_SortedBySizeDesc)
{
    std::vector<BorrowRecord> records;
    BorrowRecord rec;
    rec.borrowNode = "Node1";
    rec.borrowRemoteNuma = 3;
    rec.size = 512;
    records.push_back(rec);
    rec.size = 2048;
    records.push_back(rec);
    rec.size = 1024;
    records.push_back(rec);

    auto groups = FaultNodeModule::Instance().GroupBorrowRecordsByNuma(records);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].totalSize, 512u + 2048u + 1024u);
    ASSERT_EQ(groups[0].records.size(), 3u);
    EXPECT_EQ(groups[0].records[0].size, 2048u);
    EXPECT_EQ(groups[0].records[1].size, 1024u);
    EXPECT_EQ(groups[0].records[2].size, 512u);
}

TEST_F(TestFaultNodeModule, GroupBorrowRecordsByNuma_DifferentBorrowNode_SortedByTotalSizeDesc)
{
    std::vector<BorrowRecord> records;
    BorrowRecord rec;
    rec.borrowNode = "Node1";
    rec.borrowRemoteNuma = 1;
    rec.size = 100;
    records.push_back(rec);
    rec.borrowNode = "Node2";
    rec.borrowRemoteNuma = 1;
    rec.size = 500;
    records.push_back(rec);

    auto groups = FaultNodeModule::Instance().GroupBorrowRecordsByNuma(records);
    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0].borrowNodeId, "Node2");
    EXPECT_EQ(groups[0].totalSize, 500u);
    EXPECT_EQ(groups[1].borrowNodeId, "Node1");
    EXPECT_EQ(groups[1].totalSize, 100u);
}

TEST_F(TestFaultNodeModule, GroupBorrowRecordsByNuma_SameNodeDifferentNuma_DifferentGroups)
{
    std::vector<BorrowRecord> records;
    BorrowRecord rec;
    rec.borrowNode = "Node1";
    rec.borrowRemoteNuma = 1;
    rec.size = 200;
    records.push_back(rec);
    rec.borrowNode = "Node1";
    rec.borrowRemoteNuma = 5;
    rec.size = 300;
    records.push_back(rec);

    auto groups = FaultNodeModule::Instance().GroupBorrowRecordsByNuma(records);
    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0].borrowNodeId, "Node1");
    EXPECT_EQ(groups[1].borrowNodeId, "Node1");
    EXPECT_NE(groups[0].remoteNumaId, groups[1].remoteNumaId);
}

TEST_F(TestFaultNodeModule, GroupBorrowRecordsByNuma_MixedScenario_CorrectGroupsAndSort)
{
    std::vector<BorrowRecord> records;
    BorrowRecord rec;
    rec.borrowNode = "Node1";
    rec.borrowRemoteNuma = 1;
    rec.size = 100;
    records.push_back(rec);
    rec.size = 200;
    records.push_back(rec);
    rec.borrowNode = "Node1";
    rec.borrowRemoteNuma = 2;
    rec.size = 50;
    records.push_back(rec);
    rec.size = 150;
    records.push_back(rec);
    rec.borrowNode = "Node2";
    rec.borrowRemoteNuma = 1;
    rec.size = 80;
    records.push_back(rec);
    rec.size = 120;
    records.push_back(rec);
    rec.borrowNode = "Node2";
    rec.borrowRemoteNuma = 3;
    rec.size = 400;
    records.push_back(rec);

    auto groups = FaultNodeModule::Instance().GroupBorrowRecordsByNuma(records);
    ASSERT_EQ(groups.size(), 4u);
    // 按totalSize降序: Node2-numa3(400) 排第一
    EXPECT_EQ(groups[0].totalSize, 400u);
    // 组内records按size降序
    for (const auto& g : groups) {
        for (size_t i = 1; i < g.records.size(); ++i) {
            EXPECT_GE(g.records[i - 1].size, g.records[i].size);
        }
    }
}

// ==================== RemoveMigratedPidsFromVmInfos ====================

TEST_F(TestFaultNodeModule, RemoveMigratedPidsFromVmInfos_EmptyVmInfos_NoChange)
{
    std::vector<FaultNumaVmInfo> vmInfos;
    std::vector<pid_t> pids = {101};
    FaultNodeModule::Instance().RemoveMigratedPidsFromVmInfos(vmInfos, pids);
    ASSERT_TRUE(vmInfos.empty());
}

TEST_F(TestFaultNodeModule, RemoveMigratedPidsFromVmInfos_EmptyPids_NoChange)
{
    FaultNumaVmInfo vm1{101, 0, 5, 1024};
    FaultNumaVmInfo vm2{102, 0, 5, 2048};
    FaultNumaVmInfo vm3{103, 1, 5, 512};
    std::vector<FaultNumaVmInfo> vmInfos = {vm1, vm2, vm3};
    std::vector<pid_t> pids;
    FaultNodeModule::Instance().RemoveMigratedPidsFromVmInfos(vmInfos, pids);
    ASSERT_EQ(vmInfos.size(), 3u);
}

TEST_F(TestFaultNodeModule, RemoveMigratedPidsFromVmInfos_ExistingPid_Removed)
{
    FaultNumaVmInfo vm1{101, 0, 5, 1024};
    FaultNumaVmInfo vm2{102, 0, 5, 2048};
    FaultNumaVmInfo vm3{103, 1, 5, 512};
    std::vector<FaultNumaVmInfo> vmInfos = {vm1, vm2, vm3};
    std::vector<pid_t> pids = {102};
    FaultNodeModule::Instance().RemoveMigratedPidsFromVmInfos(vmInfos, pids);
    ASSERT_EQ(vmInfos.size(), 2u);
    EXPECT_EQ(vmInfos[0].pid, 101);
    EXPECT_EQ(vmInfos[1].pid, 103);
}

TEST_F(TestFaultNodeModule, RemoveMigratedPidsFromVmInfos_NonExistingPid_NoChange)
{
    FaultNumaVmInfo vm1{101, 0, 5, 1024};
    FaultNumaVmInfo vm2{103, 1, 5, 512};
    std::vector<FaultNumaVmInfo> vmInfos = {vm1, vm2};
    std::vector<pid_t> pids = {999};
    FaultNodeModule::Instance().RemoveMigratedPidsFromVmInfos(vmInfos, pids);
    ASSERT_EQ(vmInfos.size(), 2u);
    EXPECT_EQ(vmInfos[0].pid, 101);
    EXPECT_EQ(vmInfos[1].pid, 103);
}

TEST_F(TestFaultNodeModule, RemoveMigratedPidsFromVmInfos_MultiplePids_AllRemoved)
{
    FaultNumaVmInfo vm1{101, 0, 5, 1024};
    FaultNumaVmInfo vm2{102, 0, 5, 2048};
    FaultNumaVmInfo vm3{103, 1, 5, 512};
    FaultNumaVmInfo vm4{104, 2, 5, 768};
    std::vector<FaultNumaVmInfo> vmInfos = {vm1, vm2, vm3, vm4};
    std::vector<pid_t> pids = {101, 103};
    FaultNodeModule::Instance().RemoveMigratedPidsFromVmInfos(vmInfos, pids);
    ASSERT_EQ(vmInfos.size(), 2u);
    EXPECT_EQ(vmInfos[0].pid, 102);
    EXPECT_EQ(vmInfos[1].pid, 104);
}

// ==================== GetVmOccupancyForGroup ====================

TEST_F(TestFaultNodeModule, GetVmOccupancyForGroup_RpcFailed_ReturnsError)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::GetVmNumaInfoMapRpc,
               MpResult(*)(OverCommitFaultMemIdModule*, std::string, std::vector<VmNumaInfoWithSocket>&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::vector<FaultNumaVmInfo> vmInfos;
    MpResult res = FaultNodeModule::Instance().GetVmOccupancyForGroup("Node1", 3, vmInfos);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
    EXPECT_TRUE(vmInfos.empty());
}

TEST_F(TestFaultNodeModule, GetVmOccupancyForGroup_RpcSuccessButNoMatchingVm_ReturnsOkEmpty)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::GetVmNumaInfoMapRpc,
               MpResult(*)(OverCommitFaultMemIdModule*, std::string, std::vector<VmNumaInfoWithSocket>&, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    // allVms stays empty, so no matching VMs
    std::vector<FaultNumaVmInfo> vmInfos;
    MpResult res = FaultNodeModule::Instance().GetVmOccupancyForGroup("Node1", 3, vmInfos);
    EXPECT_EQ(res, MEM_POOLING_OK);
    EXPECT_TRUE(vmInfos.empty());
}

MpResult TestGetVmNumaInfoMapRpc_WithData(OverCommitFaultMemIdModule* This, std::string importNodeId,
                                          std::vector<VmNumaInfoWithSocket>& vmNumaInfoWithSocketList,
                                          uint16_t remoteNumaId)
{
    VmNumaInfoWithSocket vm1;
    vm1.pid = 101;
    vm1.localNumaId = 0;
    vm1.remoteNumaId = 3;
    vm1.remoteUsedMem = 1024;
    VmNumaInfoWithSocket vm2;
    vm2.pid = 102;
    vm2.localNumaId = 1;
    vm2.remoteNumaId = 3;
    vm2.remoteUsedMem = 2048;
    // vm3 has remoteUsedMem=0, should be filtered out
    VmNumaInfoWithSocket vm3;
    vm3.pid = 103;
    vm3.localNumaId = 0;
    vm3.remoteNumaId = 3;
    vm3.remoteUsedMem = 0;
    // vm4 has different remoteNumaId, should be filtered out
    VmNumaInfoWithSocket vm4;
    vm4.pid = 104;
    vm4.localNumaId = 2;
    vm4.remoteNumaId = 5;
    vm4.remoteUsedMem = 512;
    vmNumaInfoWithSocketList = {vm1, vm2, vm3, vm4};
    return MEM_POOLING_OK;
}

TEST_F(TestFaultNodeModule, GetVmOccupancyForGroup_WithMatchingVms_ReturnsSortedByUsedMemDesc)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::GetVmNumaInfoMapRpc,
               MpResult(*)(OverCommitFaultMemIdModule*, std::string, std::vector<VmNumaInfoWithSocket>&, uint16_t))
        .stubs()
        .will(invoke(TestGetVmNumaInfoMapRpc_WithData));
    std::vector<FaultNumaVmInfo> vmInfos;
    MpResult res = FaultNodeModule::Instance().GetVmOccupancyForGroup("Node1", 3, vmInfos);
    EXPECT_EQ(res, MEM_POOLING_OK);
    // Only vm1(pid=101) and vm2(pid=102) match; vm3 filtered(usedMem=0), vm4 filtered(diff numa)
    ASSERT_EQ(vmInfos.size(), 2u);
    // Sorted by remoteNumaUsedMem desc: vm2(2048) then vm1(1024)
    EXPECT_EQ(vmInfos[0].pid, 102);
    EXPECT_EQ(vmInfos[0].remoteNumaUsedMem, 2048u);
    EXPECT_EQ(vmInfos[1].pid, 101);
    EXPECT_EQ(vmInfos[1].remoteNumaUsedMem, 1024u);
}

TEST_F(TestFaultNodeModule, GetVmOccupancyForGroup_RpcSuccessWrongNuma_ReturnsEmptyOk)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::GetVmNumaInfoMapRpc,
               MpResult(*)(OverCommitFaultMemIdModule*, std::string, std::vector<VmNumaInfoWithSocket>&, uint16_t))
        .stubs()
        .will(invoke(TestGetVmNumaInfoMapRpc_WithData));
    // Query numa 99, but stub only has data for numa 3 and 5
    std::vector<FaultNumaVmInfo> vmInfos;
    MpResult res = FaultNodeModule::Instance().GetVmOccupancyForGroup("Node1", 99, vmInfos);
    EXPECT_EQ(res, MEM_POOLING_OK);
    EXPECT_TRUE(vmInfos.empty());
}

// ==================== TryBestFitAllocate ====================

TEST_F(TestFaultNodeModule, TryBestFitAllocate_EmptyCandidates_ReturnsFalse)
{
    FaultNodeModule::sBlockSizeKb = 128;
    BorrowGroupResult group;
    std::vector<ClusterSnapshotItem*> candidates;
    AllocResult result;
    bool ret = FaultNodeModule::Instance().TryBestFitAllocate(group, candidates, 4, false, result);
    EXPECT_FALSE(ret);
    EXPECT_EQ(result.selected, nullptr);
}

TEST_F(TestFaultNodeModule, TryBestFitAllocate_SingleNumaInsufficient_ReturnsFalse)
{
    FaultNodeModule::sBlockSizeKb = 128;
    BorrowGroupResult group;
    ClusterSnapshotItem item;
    item.nodeId = "Node1";
    item.socketId = 1;
    item.totalBlocks = 2;
    item.canLentMemSize = 256;
    item.numaCanLentMap[10] = 2; // only 2 blocks, need 4
    std::vector<ClusterSnapshotItem*> candidates = {&item};
    AllocResult result;
    bool ret = FaultNodeModule::Instance().TryBestFitAllocate(group, candidates, 4, false, result);
    EXPECT_FALSE(ret);
}

TEST_F(TestFaultNodeModule, TryBestFitAllocate_SingleNumaExactFit_ReturnsTrue)
{
    FaultNodeModule::sBlockSizeKb = 128;
    BorrowGroupResult group;
    ClusterSnapshotItem item;
    item.nodeId = "Node1";
    item.socketId = 1;
    item.totalBlocks = 4;
    item.canLentMemSize = 512;
    item.numaCanLentMap[10] = 4; // exactly 4 blocks
    std::vector<ClusterSnapshotItem*> candidates = {&item};
    AllocResult result;
    bool ret = FaultNodeModule::Instance().TryBestFitAllocate(group, candidates, 4, false, result);
    EXPECT_TRUE(ret);
    EXPECT_EQ(result.selected, &item);
    ASSERT_EQ(result.allocatedNumas.size(), 1u);
    EXPECT_EQ(result.allocatedNumas[0].first, 10);
    EXPECT_EQ(result.allocatedNumas[0].second, 4u);
    // verify deduction
    EXPECT_EQ(item.numaCanLentMap[10], 0u);
    EXPECT_EQ(item.totalBlocks, 0u);
    EXPECT_EQ(item.canLentMemSize, 0u);
}

TEST_F(TestFaultNodeModule, TryBestFitAllocate_SingleNumaMoreThanNeeded_ReturnsTrue)
{
    FaultNodeModule::sBlockSizeKb = 128;
    BorrowGroupResult group;
    ClusterSnapshotItem item;
    item.nodeId = "Node1";
    item.socketId = 1;
    item.totalBlocks = 10;
    item.canLentMemSize = 1280;
    item.numaCanLentMap[10] = 10; // 10 blocks, need only 3
    std::vector<ClusterSnapshotItem*> candidates = {&item};
    AllocResult result;
    bool ret = FaultNodeModule::Instance().TryBestFitAllocate(group, candidates, 3, false, result);
    EXPECT_TRUE(ret);
    ASSERT_EQ(result.allocatedNumas.size(), 1u);
    EXPECT_EQ(result.allocatedNumas[0].first, 10);
    EXPECT_EQ(result.allocatedNumas[0].second, 3u);
    EXPECT_EQ(item.numaCanLentMap[10], 7u); // 10 - 3
    EXPECT_EQ(item.totalBlocks, 7u);
    EXPECT_EQ(item.canLentMemSize, 7u * 128);
}

TEST_F(TestFaultNodeModule, TryBestFitAllocate_MultipleCandidates_BestFitSelected)
{
    FaultNodeModule::sBlockSizeKb = 128;
    BorrowGroupResult group;
    // Candidate 1: small fit (best)
    ClusterSnapshotItem best;
    best.nodeId = "NodeBest";
    best.socketId = 1;
    best.totalBlocks = 8;
    best.canLentMemSize = 1024;
    best.numaCanLentMap[10] = 8;
    // Candidate 2: larger
    ClusterSnapshotItem larger;
    larger.nodeId = "NodeLarge";
    larger.socketId = 2;
    larger.totalBlocks = 20;
    larger.canLentMemSize = 2560;
    larger.numaCanLentMap[20] = 20;
    // Candidate 3: insufficient
    ClusterSnapshotItem insufficient;
    insufficient.nodeId = "NodeInsuff";
    insufficient.socketId = 3;
    insufficient.totalBlocks = 2;
    insufficient.canLentMemSize = 256;
    insufficient.numaCanLentMap[30] = 2;
    // BestFit sorts by canLentMemSize asc — best(1024) is smallest sufficient
    std::vector<ClusterSnapshotItem*> candidates = {&larger, &best, &insufficient};
    AllocResult result;
    bool ret = FaultNodeModule::Instance().TryBestFitAllocate(group, candidates, 6, false, result);
    EXPECT_TRUE(ret);
    EXPECT_EQ(result.selected, &best);
    EXPECT_EQ(result.selected->nodeId, "NodeBest");
}

TEST_F(TestFaultNodeModule, TryBestFitAllocate_SamePlaneFilter_SkipsNonSamePlane)
{
    FaultNodeModule::sBlockSizeKb = 128;
    BorrowGroupResult group;
    group.borrowNodeId = "NodeBorrow";
    group.borrowSocketId = 1;
    // non-same-plane candidate
    ClusterSnapshotItem nonSamePlane;
    nonSamePlane.nodeId = "NodeDiff";
    nonSamePlane.socketId = 200;
    nonSamePlane.totalBlocks = 10;
    nonSamePlane.canLentMemSize = 1280;
    nonSamePlane.numaCanLentMap[10] = 10;
    // Mock JudgeSampPlane to return false (not same plane)
    MOCKER_CPP(&MemManager::JudgeSampPlane,
               bool (*)(MemManager*, const std::string&, const uint16_t&, const std::string&, const uint16_t&))
        .stubs()
        .will(returnValue(false));
    std::vector<ClusterSnapshotItem*> candidates = {&nonSamePlane};
    AllocResult result;
    bool ret = FaultNodeModule::Instance().TryBestFitAllocate(group, candidates, 4, true, result);
    EXPECT_FALSE(ret);
}

TEST_F(TestFaultNodeModule, TryBestFitAllocate_SamePlaneMatch_AllocatesSuccessfully)
{
    FaultNodeModule::sBlockSizeKb = 128;
    BorrowGroupResult group;
    group.borrowNodeId = "NodeBorrow";
    group.borrowSocketId = 1;
    ClusterSnapshotItem samePlane;
    samePlane.nodeId = "NodeSame";
    samePlane.socketId = 101;
    samePlane.totalBlocks = 10;
    samePlane.canLentMemSize = 1280;
    samePlane.numaCanLentMap[10] = 10;
    // Mock JudgeSampPlane to return true (same plane)
    MOCKER_CPP(&MemManager::JudgeSampPlane,
               bool (*)(MemManager*, const std::string&, const uint16_t&, const std::string&, const uint16_t&))
        .stubs()
        .will(returnValue(true));
    std::vector<ClusterSnapshotItem*> candidates = {&samePlane};
    AllocResult result;
    bool ret = FaultNodeModule::Instance().TryBestFitAllocate(group, candidates, 4, true, result);
    EXPECT_TRUE(ret);
    EXPECT_EQ(result.selected, &samePlane);
}

// ==================== NumaLevelDecisionFill ====================

TEST_F(TestFaultNodeModule, NumaLevelDecisionFill_Normal_DecisionFilled)
{
    FaultNodeModule::sBlockSizeKb = 128;
    BorrowGroupResult group;
    group.borrowNodeId = "Node1";
    BorrowRecord rec1;
    rec1.name = "borrowId1";
    rec1.borrowLocalNuma = 3;
    rec1.borrowSocketId = 1;
    rec1.uid = 1000;
    rec1.username = "user1";
    group.records.push_back(rec1);
    BorrowRecord rec2;
    rec2.name = "borrowId2";
    rec2.borrowLocalNuma = 3;
    rec2.borrowSocketId = 1;
    rec2.uid = 1000;
    rec2.username = "user1";
    group.records.push_back(rec2);
    FaultNumaVmInfo vm1{101, 0, 5, 1024};
    group.vmInfos.push_back(vm1);

    ClusterSnapshotItem best;
    best.nodeId = "LentNode";
    best.socketId = 2;

    std::vector<std::pair<uint16_t, uint64_t>> allocatedNumas = {{10, 4}}; // numa 10, 4 blocks
    uint64_t needBlocks = 4;

    FaultNodeModule::Instance().NumaLevelDecisionFill(group, best, needBlocks, allocatedNumas);

    EXPECT_EQ(group.strategyType, BorrowStrategyType::STRATEGY_FAILED); // unchanged by Fill
    const auto& d = group.numaDecision;
    ASSERT_EQ(d.oldNames.size(), 2u);
    EXPECT_EQ(d.oldNames[0], "borrowId1");
    EXPECT_EQ(d.oldNames[1], "borrowId2");
    EXPECT_EQ(d.borrowNumaId, 3);
    EXPECT_EQ(d.borrowSocketId, 1);
    EXPECT_EQ(d.lentNodeId, "LentNode");
    EXPECT_EQ(d.lentSocketId, 2);
    EXPECT_EQ(d.borrowSize, 4u * 128);
    EXPECT_EQ(d.lentNumaId, 10);
    EXPECT_EQ(d.lentMemSize, 4u * 128);
    auto it10 = d.lentNumaIdAndSizeMap.find(10);
    ASSERT_NE(it10, d.lentNumaIdAndSizeMap.end());
    EXPECT_EQ(it10->second, 4u * 128);
    ASSERT_EQ(d.pids.size(), 1u);
    EXPECT_EQ(d.pids[0], 101);
    EXPECT_EQ(d.uid, 1000u);
    EXPECT_EQ(d.username, "user1");
    // vmInfos should have pid removed after RemoveMigratedPidsFromVmInfos
    EXPECT_TRUE(group.vmInfos.empty());
}

TEST_F(TestFaultNodeModule, NumaLevelDecisionFill_MultipleAllocatedNumas_MapCorrect)
{
    FaultNodeModule::sBlockSizeKb = 128;
    BorrowGroupResult group;
    BorrowRecord rec;
    rec.name = "bid1";
    rec.borrowLocalNuma = 5;
    rec.borrowSocketId = 3;
    group.records.push_back(rec);

    ClusterSnapshotItem best;
    best.nodeId = "LN1";
    best.socketId = 7;

    std::vector<std::pair<uint16_t, uint64_t>> allocatedNumas = {{20, 2}, {30, 3}};
    uint64_t needBlocks = 5;

    FaultNodeModule::Instance().NumaLevelDecisionFill(group, best, needBlocks, allocatedNumas);

    const auto& d = group.numaDecision;
    EXPECT_EQ(d.lentNumaIdAndSizeMap.size(), 2u);
    auto it20 = d.lentNumaIdAndSizeMap.find(20);
    ASSERT_NE(it20, d.lentNumaIdAndSizeMap.end());
    EXPECT_EQ(it20->second, 2u * 128);
    auto it30 = d.lentNumaIdAndSizeMap.find(30);
    ASSERT_NE(it30, d.lentNumaIdAndSizeMap.end());
    EXPECT_EQ(it30->second, 3u * 128);
}

// ==================== GetBaseClusterSnapshot ====================

TEST_F(TestFaultNodeModule, GetBaseClusterSnapshot_GetBorrowAbleNodeIdListFailed_ReturnsError)
{
    FaultNodeModule::sBlockSizeKb = 128;
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeIdList,
               MpResult(*)(FaultNodeModule*, std::string, std::vector<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::vector<ClusterSnapshotItem> clusterInfos;
    MpResult res = FaultNodeModule::Instance().GetBaseClusterSnapshot("FaultNode", clusterInfos);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

MpResult TestGetBorrowAbleNodeIdList_Empty(FaultNodeModule* This, std::string curDealNodeId,
                                           std::vector<std::string>& borrowAbleNodeIdList)
{
    borrowAbleNodeIdList.clear();
    return MEM_POOLING_OK;
}

TEST_F(TestFaultNodeModule, GetBaseClusterSnapshot_EmptyNodeIdList_ReturnsOkEmpty)
{
    FaultNodeModule::sBlockSizeKb = 128;
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeIdList,
               MpResult(*)(FaultNodeModule*, std::string, std::vector<std::string>&))
        .stubs()
        .will(invoke(TestGetBorrowAbleNodeIdList_Empty));
    std::vector<ClusterSnapshotItem> clusterInfos;
    MpResult res = FaultNodeModule::Instance().GetBaseClusterSnapshot("FaultNode", clusterInfos);
    EXPECT_EQ(res, MEM_POOLING_OK);
    EXPECT_TRUE(clusterInfos.empty());
}

MpResult TestGetBorrowAbleNodeIdList_WithData(FaultNodeModule* This, std::string curDealNodeId,
                                              std::vector<std::string>& borrowAbleNodeIdList)
{
    borrowAbleNodeIdList = {"Node1", "Node2"};
    return MEM_POOLING_OK;
}

MpResult TestGetBorrowAbleNodeInfoSortByMemSize_Failed(FaultNodeModule* This,
                                                       std::vector<std::string> borrowAbleNodeIdList,
                                                       std::vector<NodeMemoryInfoWithReservedMem>& ableNodeMemInfoList)
{
    return MEM_POOLING_ERROR;
}

TEST_F(TestFaultNodeModule, GetBaseClusterSnapshot_GetMemInfoFailed_ReturnsError)
{
    FaultNodeModule::sBlockSizeKb = 128;
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeIdList,
               MpResult(*)(FaultNodeModule*, std::string, std::vector<std::string>&))
        .stubs()
        .will(invoke(TestGetBorrowAbleNodeIdList_WithData));
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeInfoSortByMemSize,
               MpResult(*)(FaultNodeModule*, std::vector<std::string>, std::vector<NodeMemoryInfoWithReservedMem>&))
        .stubs()
        .will(invoke(TestGetBorrowAbleNodeInfoSortByMemSize_Failed));
    std::vector<ClusterSnapshotItem> clusterInfos;
    MpResult res = FaultNodeModule::Instance().GetBaseClusterSnapshot("FaultNode", clusterInfos);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

MpResult TestGetBorrowAbleNodeInfoSortByMemSize_WithData(
    FaultNodeModule* This, std::vector<std::string> borrowAbleNodeIdList,
    std::vector<NodeMemoryInfoWithReservedMem>& ableNodeMemInfoList)
{
    // Node1: 1024KB available = 8 blocks of 128KB, ascending sort by canBorrowMem
    NodeMemoryInfoWithReservedMem node1;
    node1.nodeId = "Node1";
    node1.socketId = 1;
    node1.canBorrowMem = 1024;
    RackNumaMemInfo numa1;
    numa1.numaId = 10;
    numa1.canBorrowMem = 1024; // 8 blocks
    node1.numaMemInfo.push_back(numa1);

    // Node2: 2048KB available = 16 blocks
    NodeMemoryInfoWithReservedMem node2;
    node2.nodeId = "Node2";
    node2.socketId = 2;
    node2.canBorrowMem = 2048;
    RackNumaMemInfo numa2;
    numa2.numaId = 20;
    numa2.canBorrowMem = 2048; // 16 blocks
    node2.numaMemInfo.push_back(numa2);

    // Sorted by canBorrowMem ascending: node1(1024), node2(2048)
    ableNodeMemInfoList = {node1, node2};
    return MEM_POOLING_OK;
}

TEST_F(TestFaultNodeModule, GetBaseClusterSnapshot_NormalFlow_ReturnsSortedClusterSnapshot)
{
    FaultNodeModule::sBlockSizeKb = 128;
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeIdList,
               MpResult(*)(FaultNodeModule*, std::string, std::vector<std::string>&))
        .stubs()
        .will(invoke(TestGetBorrowAbleNodeIdList_WithData));
    MOCKER_CPP(&FaultNodeModule::GetBorrowAbleNodeInfoSortByMemSize,
               MpResult(*)(FaultNodeModule*, std::vector<std::string>, std::vector<NodeMemoryInfoWithReservedMem>&))
        .stubs()
        .will(invoke(TestGetBorrowAbleNodeInfoSortByMemSize_WithData));
    std::vector<ClusterSnapshotItem> clusterInfos;
    MpResult res = FaultNodeModule::Instance().GetBaseClusterSnapshot("FaultNode", clusterInfos);
    EXPECT_EQ(res, MEM_POOLING_OK);
    // 2 nodes with totalBlocks > 0
    ASSERT_EQ(clusterInfos.size(), 2u);
    // Sorted by canLentMemSize ascending
    EXPECT_LE(clusterInfos[0].canLentMemSize, clusterInfos[1].canLentMemSize);
    EXPECT_EQ(clusterInfos[0].nodeId, "Node1");
    EXPECT_EQ(clusterInfos[0].totalBlocks, 8u);
    EXPECT_EQ(clusterInfos[0].canLentMemSize, 1024u);
    EXPECT_EQ(clusterInfos[1].nodeId, "Node2");
    EXPECT_EQ(clusterInfos[1].totalBlocks, 16u);
    EXPECT_EQ(clusterInfos[1].canLentMemSize, 2048u);
}

// ==================== GetPidListAndHugePageMemSize ====================

TEST_F(TestFaultNodeModule, GetPidListAndHugePageMemSize_QueryNumaAllPidFailed_PidListEmpty)
{
    MOCKER_CPP(&FaultNodeModule::QueryNumaAllPid, bool (*)(FaultNodeModule*, uint16_t, std::vector<pid_t>&))
        .stubs()
        .will(returnValue(false));
    NumaReplaceReturnMsg rpcMsg;
    rpcMsg.destNid = 3;
    std::vector<pid_t> destPidList;
    uint64_t hugePageMemSize = 0;
    GetPidListAndHugePageMemSize(rpcMsg, destPidList, hugePageMemSize);
    EXPECT_TRUE(destPidList.empty());
    EXPECT_EQ(hugePageMemSize, 0u);
}

TEST_F(TestFaultNodeModule, GetPidListAndHugePageMemSize_EmptyBorrowParams_HugePageMemZero)
{
    MOCKER_CPP(&FaultNodeModule::QueryNumaAllPid, bool (*)(FaultNodeModule*, uint16_t, std::vector<pid_t>&))
        .stubs()
        .will(returnValue(true));
    NumaReplaceReturnMsg rpcMsg;
    rpcMsg.destNid = 3;
    // BorrowExecuteParamVec is empty
    std::vector<pid_t> destPidList;
    uint64_t hugePageMemSize = 0;
    GetPidListAndHugePageMemSize(rpcMsg, destPidList, hugePageMemSize);
    EXPECT_EQ(hugePageMemSize, 0u);
}

TEST_F(TestFaultNodeModule, GetPidListAndHugePageMemSize_SingleParam_CalculatesCorrectly)
{
    MOCKER_CPP(&FaultNodeModule::QueryNumaAllPid, bool (*)(FaultNodeModule*, uint16_t, std::vector<pid_t>&))
        .stubs()
        .will(returnValue(true));
    NumaReplaceReturnMsg rpcMsg;
    rpcMsg.destNid = 5;
    BorrowExecuteParam param;
    param.memSize = 128; // KB
    rpcMsg.BorrowExecuteParamVec.push_back(param);
    std::vector<pid_t> destPidList;
    uint64_t hugePageMemSize = 0;
    GetPidListAndHugePageMemSize(rpcMsg, destPidList, hugePageMemSize);
    EXPECT_EQ(hugePageMemSize, 128u * 1024);
}

TEST_F(TestFaultNodeModule, GetPidListAndHugePageMemSize_MultipleParams_SumsCorrectly)
{
    MOCKER_CPP(&FaultNodeModule::QueryNumaAllPid, bool (*)(FaultNodeModule*, uint16_t, std::vector<pid_t>&))
        .stubs()
        .will(returnValue(true));
    NumaReplaceReturnMsg rpcMsg;
    rpcMsg.destNid = 5;
    BorrowExecuteParam p1;
    p1.memSize = 100;
    rpcMsg.BorrowExecuteParamVec.push_back(p1);
    BorrowExecuteParam p2;
    p2.memSize = 200;
    rpcMsg.BorrowExecuteParamVec.push_back(p2);
    std::vector<pid_t> destPidList;
    uint64_t hugePageMemSize = 0;
    GetPidListAndHugePageMemSize(rpcMsg, destPidList, hugePageMemSize);
    EXPECT_EQ(hugePageMemSize, (100u + 200u) * 1024);
}

// ==================== FilterSnapshotByBorrowNode ====================

TEST_F(TestFaultNodeModule, FilterSnapshotByBorrowNode_GetBorrowableListFailed_ReturnsEmpty)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string&, std::unordered_set<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::vector<ClusterSnapshotItem> clusterInfos;
    auto candidates = FaultNodeModule::Instance().FilterSnapshotByBorrowNode("Node1", 1, clusterInfos);
    EXPECT_TRUE(candidates.empty());
}

MpResult TestGetBorrowableList_Empty(MpParseGroupProviderConf* This, const std::string& curNid,
                                     std::unordered_set<std::string>& borrowableNidSet)
{
    borrowableNidSet.clear();
    return MEM_POOLING_OK;
}

TEST_F(TestFaultNodeModule, FilterSnapshotByBorrowNode_EmptyWhitelist_ReturnsEmpty)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string&, std::unordered_set<std::string>&))
        .stubs()
        .will(invoke(TestGetBorrowableList_Empty));
    std::vector<ClusterSnapshotItem> clusterInfos;
    auto candidates = FaultNodeModule::Instance().FilterSnapshotByBorrowNode("Node1", 1, clusterInfos);
    EXPECT_TRUE(candidates.empty());
}

MpResult TestGetBorrowableList_WithNodes(MpParseGroupProviderConf* This, const std::string& curNid,
                                         std::unordered_set<std::string>& borrowableNidSet)
{
    borrowableNidSet = {"Node1", "Node2", "Node3"};
    return MEM_POOLING_OK;
}

TEST_F(TestFaultNodeModule, FilterSnapshotByBorrowNode_NormalFlow_ReturnsMatchingCandidates)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string&, std::unordered_set<std::string>&))
        .stubs()
        .will(invoke(TestGetBorrowableList_WithNodes));
    // Mock AntiNode::Query to return empty (no anti-affinity)
    MOCKER_CPP(&AntiNode::Query, MpResult(*)(AntiNode*, const std::string&, std::vector<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    // Mock CollectBorrowRecordsAll to return error (ring filter skipped)
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper*, std::vector<BorrowRecord>&, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    // Mock MemReturnManager::QueryAll to return empty (timeout filter skipped)
    MOCKER_CPP(&MemReturnManager::QueryAll,
               MpResult(*)(MemReturnManager*, std::unordered_map<std::string, BorrowItem>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    std::vector<ClusterSnapshotItem> clusterInfos(3);
    clusterInfos[0].nodeId = "Node1";
    clusterInfos[1].nodeId = "Node2";
    clusterInfos[2].nodeId = "Node4"; // not in whitelist

    auto candidates = FaultNodeModule::Instance().FilterSnapshotByBorrowNode("BorrowNode", 1, clusterInfos);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0]->nodeId, "Node1");
    EXPECT_EQ(candidates[1]->nodeId, "Node2");
}

MpResult TestAntiNodeQuery_ExcludeNode2(AntiNode* This, const std::string& nodeId, std::vector<std::string>& outVec)
{
    outVec = {"Node2"};
    return MEM_POOLING_OK;
}

MpResult TestCollectBorrowRecordsAll_RingNode1(BorrowRecordHelper* This, std::vector<BorrowRecord>& records, bool b1,
                                               bool b2)
{
    BorrowRecord rec;
    rec.borrowNode = "Node1";
    records.push_back(rec);
    return MEM_POOLING_OK;
}

MpResult TestMemReturnManagerQueryAll_TimeoutNode2(MemReturnManager* This,
                                                   std::unordered_map<std::string, BorrowItem>& cache)
{
    BorrowItem item;
    item.srcNid = "BorrowNode";
    item.dstNid = "Node2";
    item.dstSocketId = 101;
    cache["key1"] = item;
    return MEM_POOLING_OK;
}

TEST_F(TestFaultNodeModule, FilterSnapshotByBorrowNode_AntiNodeExcluded)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string&, std::unordered_set<std::string>&))
        .stubs()
        .will(invoke(TestGetBorrowableList_WithNodes));
    MOCKER_CPP(&AntiNode::Query, MpResult(*)(AntiNode*, const std::string&, std::vector<std::string>&))
        .stubs()
        .will(invoke(TestAntiNodeQuery_ExcludeNode2));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper*, std::vector<BorrowRecord>&, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&MemReturnManager::QueryAll,
               MpResult(*)(MemReturnManager*, std::unordered_map<std::string, BorrowItem>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    std::vector<ClusterSnapshotItem> clusterInfos(3);
    clusterInfos[0].nodeId = "Node1";
    clusterInfos[1].nodeId = "Node2"; // anti-affinity, should be excluded
    clusterInfos[2].nodeId = "Node3";

    auto candidates = FaultNodeModule::Instance().FilterSnapshotByBorrowNode("BorrowNode", 1, clusterInfos);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0]->nodeId, "Node1");
    EXPECT_EQ(candidates[1]->nodeId, "Node3");
}

TEST_F(TestFaultNodeModule, FilterSnapshotByBorrowNode_RingFilterExcludesBorrowNodes)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string&, std::unordered_set<std::string>&))
        .stubs()
        .will(invoke(TestGetBorrowableList_WithNodes));
    MOCKER_CPP(&AntiNode::Query, MpResult(*)(AntiNode*, const std::string&, std::vector<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper*, std::vector<BorrowRecord>&, bool, bool))
        .stubs()
        .will(invoke(TestCollectBorrowRecordsAll_RingNode1));
    MOCKER_CPP(&MemReturnManager::QueryAll,
               MpResult(*)(MemReturnManager*, std::unordered_map<std::string, BorrowItem>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    std::vector<ClusterSnapshotItem> clusterInfos(3);
    clusterInfos[0].nodeId = "Node1"; // is a borrower, should be excluded
    clusterInfos[1].nodeId = "Node2";
    clusterInfos[2].nodeId = "Node3";

    auto candidates = FaultNodeModule::Instance().FilterSnapshotByBorrowNode("BorrowNode", 1, clusterInfos);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0]->nodeId, "Node2");
    EXPECT_EQ(candidates[1]->nodeId, "Node3");
}

TEST_F(TestFaultNodeModule, FilterSnapshotByBorrowNode_TimeoutFilterExcludesDstNid)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string&, std::unordered_set<std::string>&))
        .stubs()
        .will(invoke(TestGetBorrowableList_WithNodes));
    MOCKER_CPP(&AntiNode::Query, MpResult(*)(AntiNode*, const std::string&, std::vector<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper*, std::vector<BorrowRecord>&, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&MemReturnManager::QueryAll, void (*)(MemReturnManager*, std::unordered_map<std::string, BorrowItem>&))
        .stubs()
        .will(invoke(TestMemReturnManagerQueryAll_TimeoutNode2));
    // Mock JudgeSampPlane: Node2 is same plane as borrowSocketId=1
    MOCKER_CPP(&MemManager::JudgeSampPlane,
               bool (*)(MemManager*, const std::string&, const uint16_t&, const std::string&, const uint16_t&))
        .stubs()
        .will(returnValue(true)); // same plane => should be excluded

    std::vector<ClusterSnapshotItem> clusterInfos(3);
    clusterInfos[0].nodeId = "Node1";
    clusterInfos[1].nodeId = "Node2"; // timeout dst, same plane => excluded
    clusterInfos[2].nodeId = "Node3";

    auto candidates = FaultNodeModule::Instance().FilterSnapshotByBorrowNode("BorrowNode", 1, clusterInfos);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0]->nodeId, "Node1");
    EXPECT_EQ(candidates[1]->nodeId, "Node3");
}

// ==================== FaultHandleBorrowStrategy ====================

TEST_F(TestFaultNodeModule, FaultHandleBorrowStrategy_AllGroupsFailed_ReturnsError)
{
    // Mock GetMustSamePlane
    MOCKER_CPP(&MpConfiguration::GetMustSamePlane, bool (*)(MpConfiguration*)).stubs().will(returnValue(false));
    // Mock GenerateNumaLevelDecision and GenerateBorrowIdLevelDecision as no-ops
    MOCKER_CPP(&FaultNodeModule::GenerateNumaLevelDecision,
               void (*)(FaultNodeModule*, std::vector<BorrowGroupResult>&, std::vector<ClusterSnapshotItem>&, bool))
        .stubs();
    MOCKER_CPP(&FaultNodeModule::GenerateBorrowIdLevelDecision,
               void (*)(FaultNodeModule*, std::vector<BorrowGroupResult>&, std::vector<ClusterSnapshotItem>&, bool))
        .stubs();

    std::vector<BorrowGroupResult> borrowGroups(2);
    borrowGroups[0].strategyType = BorrowStrategyType::STRATEGY_FAILED;
    borrowGroups[1].strategyType = BorrowStrategyType::STRATEGY_FAILED;
    std::vector<ClusterSnapshotItem> baseSnapshot;

    MpResult res = FaultNodeModule::Instance().FaultHandleBorrowStrategy(borrowGroups, baseSnapshot);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestFaultNodeModule, FaultHandleBorrowStrategy_AllGroupsSucceed_ReturnsOk)
{
    MOCKER_CPP(&MpConfiguration::GetMustSamePlane, bool (*)(MpConfiguration*)).stubs().will(returnValue(false));
    MOCKER_CPP(&FaultNodeModule::GenerateNumaLevelDecision,
               void (*)(FaultNodeModule*, std::vector<BorrowGroupResult>&, std::vector<ClusterSnapshotItem>&, bool))
        .stubs();
    MOCKER_CPP(&FaultNodeModule::GenerateBorrowIdLevelDecision,
               void (*)(FaultNodeModule*, std::vector<BorrowGroupResult>&, std::vector<ClusterSnapshotItem>&, bool))
        .stubs();

    std::vector<BorrowGroupResult> borrowGroups(2);
    borrowGroups[0].strategyType = BorrowStrategyType::NUMA_LEVEL_STRATEGY;
    borrowGroups[1].strategyType = BorrowStrategyType::BORROW_ID_LEVEL_STRATEGY;
    std::vector<ClusterSnapshotItem> baseSnapshot;

    MpResult res = FaultNodeModule::Instance().FaultHandleBorrowStrategy(borrowGroups, baseSnapshot);
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestFaultNodeModule, FaultHandleBorrowStrategy_PartialFailed_ReturnsPartialOk)
{
    MOCKER_CPP(&MpConfiguration::GetMustSamePlane, bool (*)(MpConfiguration*)).stubs().will(returnValue(false));
    MOCKER_CPP(&FaultNodeModule::GenerateNumaLevelDecision,
               void (*)(FaultNodeModule*, std::vector<BorrowGroupResult>&, std::vector<ClusterSnapshotItem>&, bool))
        .stubs();
    MOCKER_CPP(&FaultNodeModule::GenerateBorrowIdLevelDecision,
               void (*)(FaultNodeModule*, std::vector<BorrowGroupResult>&, std::vector<ClusterSnapshotItem>&, bool))
        .stubs();

    std::vector<BorrowGroupResult> borrowGroups(3);
    borrowGroups[0].strategyType = BorrowStrategyType::NUMA_LEVEL_STRATEGY;
    borrowGroups[1].strategyType = BorrowStrategyType::STRATEGY_FAILED;
    borrowGroups[2].strategyType = BorrowStrategyType::BORROW_ID_LEVEL_STRATEGY;
    std::vector<ClusterSnapshotItem> baseSnapshot;

    MpResult res = FaultNodeModule::Instance().FaultHandleBorrowStrategy(borrowGroups, baseSnapshot);
    EXPECT_EQ(res, MEM_POOLING_PARTIAL_OK);
}

} // namespace mempooling