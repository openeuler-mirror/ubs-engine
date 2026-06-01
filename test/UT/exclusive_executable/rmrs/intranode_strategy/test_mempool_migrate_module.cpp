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
#include <cstring>

#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "ubse_topology_interface.h"
#include "LibvirtHelper.h"
#include "exporter.h"
#include "gtest/gtest.h"
#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mempool_migrate_module.h"
#include "mempooling_interface.h"
#include "mempooling_message.h"
#include "mockcpp/mokc.h"
#include "mp_smap_helper.h"
#include "rmrs_serialize.h"

#include <iostream>

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ubse::com;
using namespace mempooling::smap;
using namespace mempooling::exportV2;
using namespace mempooling::message;
using namespace rmrs::serialize;

namespace mempooling::migrate {
using Str2NodeInfo = std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>;
// 测试类
class TestMempoolMigrateModule : public ::testing::Test {
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

void GetRemoteNumaList(std::vector<NumaHugePageInfo>& numaHugePageInfoSumList, std::vector<uint16_t>& remoteNumaIdList);

void GetLocalVmInfo(std::vector<VmNumaInfo>& allVmNumaInfoInfoList, std::map<pid_t, VmNumaInfo>& VmNumaInfoMap,
                    std::vector<VmDomainInfo>& vmDomainInfos);

void AdjustMigrationRatio(uint64_t difference, uint64_t& accumulatedMigrateMem,
                          std::map<pid_t, VmNumaInfo>& vmNumaInfoMap,
                          const std::vector<mempooling::VMPresetParam>& vmPresetParam,
                          std::vector<std::tuple<pid_t, uint64_t, uint16_t>>& potentialMigration,
                          std::map<pid_t, uint16_t>& vmFreqPidRatioMap);

uint32_t TestDeserializeNumaInfos(std::vector<mempooling::exportV2::NumaInfo>& numaInfos,
                                  const std::vector<std::string>& data)
{
    mempooling::exportV2::NumaInfo numaInfo;
    mempooling::exportV2::NumaMetaData numaMetaInfo;
    numaMetaInfo.nodeId = "Node1";
    numaMetaInfo.numaId = 1;
    numaInfo.metaData = numaMetaInfo;
    numaInfos.push_back(numaInfo);
    return MEM_POOLING_OK;
}

uint32_t TestDeserializeVmDomainInfos(std::vector<VmDomainInfo>& domainInfos, const std::vector<std::string>& data)
{
    VmDomainInfo vmDomainInfo;
    VmMetaData metaData;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 0, 0, 1};
    vmDomainInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {4, 2048, 0, -1, 0};
    vmDomainInfo.numaInfo[1] = vmNumaInfo2;
    metaData.nodeId = "Node1";
    vmDomainInfo.metaData = metaData;
    domainInfos.push_back(vmDomainInfo);
    return MEM_POOLING_OK;
}

uint32_t TestDeserializeVmDomainInfosImplFailed(std::vector<VmDomainInfo>& domainInfos,
                                                const std::vector<std::string>& data)
{
    VmDomainInfo vmDomainInfo;
    VmMetaData metaData;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 0, 0, 1};
    vmDomainInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {4, 2048, 0, -1, 0};
    vmDomainInfo.numaInfo[1] = vmNumaInfo2;
    metaData.pid = 12345;
    metaData.nodeId = "Node1";
    vmDomainInfo.metaData = metaData;
    domainInfos.push_back(vmDomainInfo);
    return MEM_POOLING_OK;
}

uint32_t TestDeserializeVmDomainInfosImplSuccess(std::vector<VmDomainInfo>& domainInfos,
                                                 const std::vector<std::string>& data)
{
    VmDomainInfo vmDomainInfo;
    VmMetaData metaData;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 0, 0, 1};
    vmDomainInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {4, 2048, 10 * 1024 * 1024, -1, 0};
    vmDomainInfo.numaInfo[1] = vmNumaInfo2;
    metaData.pid = 12345;
    metaData.nodeId = "Node1";
    vmDomainInfo.metaData = metaData;
    domainInfos.push_back(vmDomainInfo);
    return MEM_POOLING_OK;
}

uint32_t TestGetNumaInfoImmediately(std::vector<mempooling::exportV2::NumaInfo>& numaInfos)
{
    mempooling::exportV2::NumaInfo numaInfo;
    mempooling::exportV2::NumaMetaData numaMetaInfo;
    numaMetaInfo.nodeId = "Node1";
    numaInfo.metaData = numaMetaInfo;
    numaInfos.push_back(numaInfo);
    return 0;
}

TEST_F(TestMempoolMigrateModule, FillDestNumaFreeHugePageMapFailed)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::map<uint16_t, uint64_t> destNumaFreeHugePageMap{};
    std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap{};
    bool res =
        MempoolMigrateModule::FillDestNumaFreeHugePageMap(destNumaFreeHugePageMap, vmMigrateOutParamGroupByNumaIdMap);
    ASSERT_FALSE(res);
}

TEST_F(TestMempoolMigrateModule, FillDestNumaFreeHugePageMap1)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(0));
    std::map<uint16_t, uint64_t> destNumaFreeHugePageMap{};
    std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap{};
    bool res =
        MempoolMigrateModule::FillDestNumaFreeHugePageMap(destNumaFreeHugePageMap, vmMigrateOutParamGroupByNumaIdMap);
    ASSERT_FALSE(res);
}

MpResult TestGetNumaInfoImmediately_1(std::vector<mempooling::exportV2::NumaInfo>& numaInfos)
{
    mempooling::exportV2::NumaInfo numaInfo;
    numaInfo.metaData.numaId = 1;
    numaInfos.push_back(numaInfo);
    return 0;
}

TEST_F(TestMempoolMigrateModule, FillDestNumaFreeHugePageMap2)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(TestGetNumaInfoImmediately_1));
    std::map<uint16_t, uint64_t> destNumaFreeHugePageMap{};
    std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap{};
    bool res =
        MempoolMigrateModule::FillDestNumaFreeHugePageMap(destNumaFreeHugePageMap, vmMigrateOutParamGroupByNumaIdMap);
    ASSERT_EQ(res, true);
}

TEST_F(TestMempoolMigrateModule, GetVmInfoMapFailed)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(0));
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::vector<pid_t> pidList;
    bool res = MempoolMigrateModule::GetVmInfoMap(vmInfoMap, pidList);
    ASSERT_FALSE(res);
}

MpResult TestGetVmInfoImmediately_1(std::vector<VmDomainInfo>& vmDomainInfos)
{
    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = 1;
    vmDomainInfos.push_back(vmDomainInfo);
    return 0;
}

TEST_F(TestMempoolMigrateModule, GetVmInfoMap_1)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(0));
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::vector<pid_t> pidList;
    bool res = MempoolMigrateModule::GetVmInfoMap(vmInfoMap, pidList);
    ASSERT_FALSE(res);
}

TEST_F(TestMempoolMigrateModule, GetVmInfoMap_2)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately_1));
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::vector<pid_t> pidList;
    bool res = MempoolMigrateModule::GetVmInfoMap(vmInfoMap, pidList);
    ASSERT_EQ(res, true);
}

TEST_F(TestMempoolMigrateModule, TestGetVmInfoMapSuccess)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(TestGetVmInfoImmediately_1));
    std::map<pid_t, VmDomainInfo> vmInfoMap;
    std::vector<pid_t> pidList{1};
    bool res = MempoolMigrateModule::GetVmInfoMap(vmInfoMap, pidList);
    ASSERT_EQ(res, true);
}

bool TestFillDestNumaFreeHugePageMap(
    std::map<uint16_t, uint64_t>& destNumaFreeHugePageMap,
    std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap)
{
    destNumaFreeHugePageMap[4] = 1024;
    std::vector<VMMigrateOutParam> paramList;
    VMMigrateOutParam param;
    param.desNumaId = 4;
    param.memSize = 25;
    param.pid = 12345;
    paramList.push_back(param);
    vmMigrateOutParamGroupByNumaIdMap[4] = paramList;
    return true;
}

bool TestGetVmInfoMap(std::map<pid_t, VmDomainInfo>& vmInfoMap)
{
    VmDomainInfo vmDomainInfo;
    VmMetaData metaData;
    VmDomainNumaInfo vmNumaInfo1 = {0, 1, 2 * 1024 * 1024, 0, 1};
    vmDomainInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {4, 1, 0, -1, 0};
    vmDomainInfo.numaInfo[1] = vmNumaInfo2;
    metaData.nodeId = "Node1";
    metaData.pid = 12345;
    vmDomainInfo.metaData = metaData;
    vmInfoMap[12345] = vmDomainInfo;
    return true;
}

bool TestGetVmInfoMapDoubleParam(std::map<pid_t, VmDomainInfo>& vmInfoMap)
{
    VmDomainInfo vmDomainInfo;
    VmMetaData metaData;
    VmDomainNumaInfo vmNumaInfo1 = {0, 1, 2 * 1024 * 1024, 0, 1};
    vmDomainInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {4, 1, 0, -1, 0};
    vmDomainInfo.numaInfo[1] = vmNumaInfo2;
    metaData.nodeId = "Node1";
    metaData.pid = 12345;
    vmDomainInfo.metaData = metaData;
    vmInfoMap[12345] = vmDomainInfo;
    vmInfoMap[2] = vmDomainInfo;
    return true;
}

bool TestGetVmInfoMapFailed(std::map<pid_t, VmDomainInfo>& vmInfoMap)
{
    VmDomainInfo vmDomainInfo;
    VmMetaData metaData;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 20 * 1024 * 1024, 0, 1};
    vmDomainInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {4, 2048, 0, -1, 0};
    vmDomainInfo.numaInfo[1] = vmNumaInfo2;
    metaData.nodeId = "Node1";
    metaData.pid = 12345;
    vmDomainInfo.metaData = metaData;
    vmInfoMap[12345] = vmDomainInfo;
    return true;
}

TEST_F(TestMempoolMigrateModule, ValidateRemoteFreeSpaceFailed)
{
    MOCKER_CPP(&MempoolMigrateModule::GetVmInfoMap,
               bool (*)(std::map<pid_t, VmDomainInfo> & vmInfoMap, std::vector<pid_t> pidList))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&MempoolMigrateModule::FillDestNumaFreeHugePageMap,
               bool (*)(std::map<uint16_t, uint64_t> & destNumaFreeHugePageMap,
                        std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap))
        .stubs()
        .will(returnValue(false));
    VMMigrateOutParam param;
    const std::vector<VMMigrateOutParam> vmMigrateOutParam{param};
    bool res = MempoolMigrateModule::ValidateRemoteFreeSpace(vmMigrateOutParam);
    ASSERT_FALSE(res);
}

TEST_F(TestMempoolMigrateModule, ValidateRemoteFreeSpaceFailed2)
{
    MOCKER_CPP(&MempoolMigrateModule::FillDestNumaFreeHugePageMap,
               bool (*)(std::map<uint16_t, uint64_t> & destNumaFreeHugePageMap,
                        std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap))
        .stubs()
        .will(invoke(TestFillDestNumaFreeHugePageMap));
    MOCKER_CPP(&MempoolMigrateModule::GetVmInfoMap,
               bool (*)(std::map<pid_t, VmDomainInfo> & vmInfoMap, std::vector<pid_t> pidList))
        .stubs()
        .will(returnValue(false));
    VMMigrateOutParam param;
    const std::vector<VMMigrateOutParam> vmMigrateOutParam{param};
    bool res = MempoolMigrateModule::ValidateRemoteFreeSpace(vmMigrateOutParam);
    ASSERT_FALSE(res);
}

TEST_F(TestMempoolMigrateModule, ValidateRemoteFreeSpaceFailed3)
{
    MOCKER_CPP(&MempoolMigrateModule::FillDestNumaFreeHugePageMap,
               bool (*)(std::map<uint16_t, uint64_t> & destNumaFreeHugePageMap,
                        std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap))
        .stubs()
        .will(invoke(TestFillDestNumaFreeHugePageMap));
    MOCKER_CPP(&MempoolMigrateModule::GetVmInfoMap,
               bool (*)(std::map<pid_t, VmDomainInfo> & vmInfoMap, std::vector<pid_t> pidList))
        .stubs()
        .will(invoke(TestGetVmInfoMapFailed));
    VMMigrateOutParam param;
    param.desNumaId = 4;
    param.memSize = 25;
    param.pid = 12345;
    const std::vector<VMMigrateOutParam> vmMigrateOutParam{param};
    bool res = MempoolMigrateModule::ValidateRemoteFreeSpace(vmMigrateOutParam);
    ASSERT_FALSE(res);
}

TEST_F(TestMempoolMigrateModule, ValidateRemoteFreeSpaceSuccess)
{
    MOCKER_CPP(&MempoolMigrateModule::FillDestNumaFreeHugePageMap,
               bool (*)(std::map<uint16_t, uint64_t> & destNumaFreeHugePageMap,
                        std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap))
        .stubs()
        .will(invoke(TestFillDestNumaFreeHugePageMap));
    MOCKER_CPP(&MempoolMigrateModule::GetVmInfoMap,
               bool (*)(std::map<pid_t, VmDomainInfo> & vmInfoMap, std::vector<pid_t> pidList))
        .stubs()
        .will(invoke(TestGetVmInfoMap));
    VMMigrateOutParam param;
    param.desNumaId = 4;
    param.memSize = 1;
    param.pid = 12345;
    const std::vector<VMMigrateOutParam> vmMigrateOutParam{param};
    bool res = MempoolMigrateModule::ValidateRemoteFreeSpace(vmMigrateOutParam);
    ASSERT_EQ(res, false);
}

TEST_F(TestMempoolMigrateModule, TestValidateRemoteFreeSpaceFailed_NotValidPid)
{
    MOCKER_CPP(&MempoolMigrateModule::GetVmInfoMap,
               bool (*)(std::map<pid_t, VmDomainInfo> & vmInfoMap, std::vector<pid_t> pidList))
        .stubs()
        .will(invoke(TestGetVmInfoMapDoubleParam));
    VMMigrateOutParam param;
    param.desNumaId = 4;
    param.memSize = 1;
    param.pid = 12345;
    const std::vector<VMMigrateOutParam> vmMigrateOutParam{param};
    bool res = MempoolMigrateModule::ValidateRemoteFreeSpace(vmMigrateOutParam);
    ASSERT_FALSE(res);
}

TEST_F(TestMempoolMigrateModule, TestValidateRemoteFreeSpaceFailed_FillNumaInfoError)
{
    MOCKER_CPP(&MempoolMigrateModule::FillDestNumaFreeHugePageMap,
               bool (*)(std::map<uint16_t, uint64_t> & destNumaFreeHugePageMap,
                        std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap))
        .stubs()
        .will(returnValue(false));
    MOCKER_CPP(&MempoolMigrateModule::GetVmInfoMap,
               bool (*)(std::map<pid_t, VmDomainInfo> & vmInfoMap, std::vector<pid_t> pidList))
        .stubs()
        .will(invoke(TestGetVmInfoMap));
    VMMigrateOutParam param;
    param.desNumaId = 4;
    param.memSize = 1;
    param.pid = 12345;
    const std::vector<VMMigrateOutParam> vmMigrateOutParam{param};
    bool res = MempoolMigrateModule::ValidateRemoteFreeSpace(vmMigrateOutParam);
    ASSERT_FALSE(res);
}

TEST_F(TestMempoolMigrateModule, TestValidateRemoteFreeSpaceFailed_NotEnoughFreeMem)
{
    MOCKER_CPP(&MempoolMigrateModule::FillDestNumaFreeHugePageMap,
               bool (*)(std::map<uint16_t, uint64_t> & destNumaFreeHugePageMap,
                        std::map<uint16_t, std::vector<VMMigrateOutParam>> vmMigrateOutParamGroupByNumaIdMap))
        .stubs()
        .will(invoke(TestFillDestNumaFreeHugePageMap));
    MOCKER_CPP(&MempoolMigrateModule::GetVmInfoMap,
               bool (*)(std::map<pid_t, VmDomainInfo> & vmInfoMap, std::vector<pid_t> pidList))
        .stubs()
        .will(invoke(TestGetVmInfoMap));
    VMMigrateOutParam param;
    param.desNumaId = 4;
    param.memSize = 102400;
    param.pid = 12345;
    const std::vector<VMMigrateOutParam> vmMigrateOutParam{param};
    bool res = MempoolMigrateModule::ValidateRemoteFreeSpace(vmMigrateOutParam);
    ASSERT_FALSE(res);
}

MpResult CollectBorrowRecordsMock(BorrowRecordHelper* This, const std::string nodeId,
                                  std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord borrowRecord;
    borrowRecord.borrowNode = "NodeA";
    borrowRecord.borrowRemoteNuma = 1;
    borrowRecord.lentNode = "NodeB";
    borrowRecord.lentSocketId = 2;
    borrowRecords.push_back(borrowRecord);
    return MEM_POOLING_OK;
}

TEST_F(TestMempoolMigrateModule, ValidateSamePlane_success)
{
    VMMigrateOutParam perVmParam = {.pid = 12345, .memSize = 1024, .desNumaId = 1};

    VmDomainInfo vmInfo;
    vmInfo.metaData.pid = 12345;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 0, 0, 1};
    vmInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 0, -1, 0};
    vmInfo.numaInfo[1] = vmNumaInfo2;
    std::vector<VmDomainInfo> vmDomainInfos;
    vmDomainInfos.push_back(vmInfo);

    std::string curNodeId = "NodeA";

    // 模拟 nodeTopology["NodeA-0"] 下有 NodeB-2 的信息
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    MemNodeData memNodeData;
    memNodeData.nodeId = "NodeB";
    memNodeData.socket.socketId = "2"; // 注意 socketId 是 string
    nodeTopology["NodeA-0"] = {memNodeData};
    MOCKER_CPP(
        &BorrowRecordHelper::CollectBorrowRecords,
        MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsMock));
    MpResult ret = MempoolMigrateModule::ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMempoolMigrateModule, ValidateSamePlane_failed1)
{
    VMMigrateOutParam perVmParam = {.pid = 9999};

    VmDomainInfo vmInfo;
    vmInfo.metaData.pid = 12345;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 0, 0, 1};
    vmInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 0, -1, 0};
    vmInfo.numaInfo[1] = vmNumaInfo2;
    std::vector<VmDomainInfo> vmDomainInfos;
    vmDomainInfos.push_back(vmInfo);

    std::string curNodeId = "NodeA";

    // 模拟 nodeTopology["NodeA-0"] 下有 NodeB-2 的信息
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;

    MpResult ret = MempoolMigrateModule::ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolMigrateModule, ValidateSamePlane_failed2)
{
    VMMigrateOutParam perVmParam = {.pid = 12345, .desNumaId = 1};

    VmDomainInfo vmInfo;
    vmInfo.metaData.pid = 12345;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 4096, 0, 1};
    vmInfo.numaInfo[0] = vmNumaInfo1;
    std::vector<VmDomainInfo> vmDomainInfos;
    vmDomainInfos.push_back(vmInfo);

    std::string curNodeId = "NodeA";

    // 模拟 nodeTopology["NodeA-0"] 下有 NodeB-2 的信息
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology = {
        {"NodeX-0", {}} // 错误 key
    };
    MpResult ret = MempoolMigrateModule::ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

MpResult CollectBorrowRecordsMock1(BorrowRecordHelper* This, const std::string nodeId,
                                   std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord borrowRecord;
    borrowRecord.borrowNode = "NodeA";
    borrowRecord.borrowRemoteNuma = 99;
    borrowRecord.lentNode = "NodeB";
    borrowRecord.lentSocketId = 2;
    borrowRecords.push_back(borrowRecord);
    return MEM_POOLING_OK;
}

TEST_F(TestMempoolMigrateModule, ValidateSamePlane_failed3)
{
    VMMigrateOutParam perVmParam = {.pid = 12345, .memSize = 1024, .desNumaId = 1};

    VmDomainInfo vmInfo;
    vmInfo.metaData.pid = 12345;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 0, 0, 1};
    vmInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 0, -1, 0};
    vmInfo.numaInfo[1] = vmNumaInfo2;
    std::vector<VmDomainInfo> vmDomainInfos;
    vmDomainInfos.push_back(vmInfo);

    std::string curNodeId = "NodeA";

    // 模拟 nodeTopology["NodeA-0"] 下有 NodeB-2 的信息
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    MemNodeData data;
    data.nodeId = "NodeB";
    data.socket.socketId = "2";
    nodeTopology["NodeA-0"] = {data};
    MOCKER_CPP(
        &BorrowRecordHelper::CollectBorrowRecords,
        MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsMock1));
    MpResult ret = MempoolMigrateModule::ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolMigrateModule, ValidateSamePlane_failed4)
{
    VMMigrateOutParam perVmParam = {.pid = 12345, .memSize = 1024, .desNumaId = 1};

    VmDomainInfo vmInfo;
    vmInfo.metaData.pid = 12345;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 0, 0, 1};
    vmInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 0, -1, 0};
    vmInfo.numaInfo[1] = vmNumaInfo2;
    std::vector<VmDomainInfo> vmDomainInfos;
    vmDomainInfos.push_back(vmInfo);

    std::string curNodeId = "NodeA";

    // 模拟 nodeTopology["NodeA-0"] 下有 NodeB-2 的信息
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    MemNodeData data;
    data.nodeId = "NodeC";
    data.socket.socketId = "3";
    nodeTopology["NodeA-0"] = {data};
    MOCKER_CPP(
        &BorrowRecordHelper::CollectBorrowRecords,
        MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsMock));
    MpResult ret = MempoolMigrateModule::ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolMigrateModule, ValidateSamePlane_failed5)
{
    VMMigrateOutParam perVmParam = {.pid = 12345, .memSize = 1024, .desNumaId = 1};

    VmDomainInfo vmInfo;
    vmInfo.metaData.pid = 12345;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 0, 0, 1};
    vmInfo.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 0, -1, 0};
    vmInfo.numaInfo[1] = vmNumaInfo2;
    std::vector<VmDomainInfo> vmDomainInfos;
    vmDomainInfos.push_back(vmInfo);

    std::string curNodeId = "NodeA";

    // 模拟 nodeTopology["NodeA-0"] 下有 NodeB-2 的信息
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    MemNodeData memNodeData;
    memNodeData.nodeId = "NodeB";
    memNodeData.socket.socketId = "2"; // 注意 socketId 是 string
    nodeTopology["NodeA-0"] = {memNodeData};
    MOCKER_CPP(
        &BorrowRecordHelper::CollectBorrowRecords,
        MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = MempoolMigrateModule::ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolMigrateModule, ValidateAllPidSamePlane_failed1)
{
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(returnValue(1));
    MpResult ret = MempoolMigrateModule::ValidateAllPidSamePlane(vmMigrateOutParam);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolMigrateModule, ValidateAllPidSamePlane_failed2)
{
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&ubse::mti::UbseGetLocalNodeInfo, uint32_t(*)(NodeInfo & rackNodeInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = MempoolMigrateModule::ValidateAllPidSamePlane(vmMigrateOutParam);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolMigrateModule, ValidateAllPidSamePlane_failed3)
{
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&ubse::mti::UbseGetLocalNodeInfo, uint32_t(*)(NodeInfo & rackNodeInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = MempoolMigrateModule::ValidateAllPidSamePlane(vmMigrateOutParam);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolMigrateModule, ValidateAllPidSamePlane_success1)
{
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&ubse::mti::UbseGetLocalNodeInfo, uint32_t(*)(NodeInfo & rackNodeInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult ret = MempoolMigrateModule::ValidateAllPidSamePlane(vmMigrateOutParam);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

MpResult GetVmInfoImmediatelyMock(std::vector<VmDomainInfo>& vmDomainInfos)
{
    VmDomainInfo vm;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 4 * 1024 * 1024 * 1024, 0, 1};
    vm.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 2 * 1024 * 1024 * 1024, -1, 0};
    vm.numaInfo[1] = vmNumaInfo2;
    vm.metaData.state = "running";
    vm.timestamp = std::time(nullptr);

    vmDomainInfos.push_back(vm);

    return MEM_POOLING_OK;
}

TEST_F(TestMempoolMigrateModule, ValidateAllPidSamePlane_failed4)
{
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&ubse::mti::UbseGetLocalNodeInfo, uint32_t(*)(NodeInfo & rackNodeInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(GetVmInfoImmediatelyMock));

    MOCKER_CPP(&MempoolMigrateModule::ValidateSamePlane,
               MpResult(*)(const VMMigrateOutParam& perVmParam, const std::vector<VmDomainInfo>& vmDomainInfos,
                           const std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology,
                           const std::string& curNodeId))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = MempoolMigrateModule::ValidateAllPidSamePlane(vmMigrateOutParam);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMempoolMigrateModule, ValidateAllPidSamePlane_success2)
{
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&ubse::mti::UbseGetLocalNodeInfo, uint32_t(*)(NodeInfo & rackNodeInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(GetVmInfoImmediatelyMock));

    MOCKER_CPP(&MempoolMigrateModule::ValidateSamePlane,
               MpResult(*)(const VMMigrateOutParam& perVmParam, const std::vector<VmDomainInfo>& vmDomainInfos,
                           const std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology,
                           const std::string& curNodeId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult ret = MempoolMigrateModule::ValidateAllPidSamePlane(vmMigrateOutParam);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

MpResult Test_GetNumaInfoImmediately(std::vector<mempooling::exportV2::NumaInfo>& numaInfos)
{
    mempooling::exportV2::NumaInfo numaInfo;
    numaInfo.metaData.isLocal = false;
    mempooling::exportV2::NumaPageData numaPageData1 = {2048, 1024, 1024};
    numaInfo.metaData.numaPageInfo[2048] = numaPageData1;
    numaInfo.metaData.memTotal = 2048;
    numaInfo.metaData.numaId = 1;
    numaInfo.metaData.memFree = 512;
    numaInfos.push_back(numaInfo);
    return 0;
}

MpResult Test_GetNumaInfoImmediately2(std::vector<mempooling::exportV2::NumaInfo>& numaInfos)
{
    mempooling::exportV2::NumaInfo numaInfo;
    numaInfo.metaData.isLocal = true;
    mempooling::exportV2::NumaPageData numaPageData1 = {2048, 1024, 1024};
    numaInfo.metaData.numaPageInfo[2048] = numaPageData1;
    numaInfo.metaData.memTotal = 2048;
    numaInfo.metaData.numaId = 1;
    numaInfo.metaData.memFree = 512;
    numaInfos.push_back(numaInfo);
    return 0;
}

MpResult Test_GetVmInfoImmediately(std::vector<VmDomainInfo>& vmDomainInfos)
{
    VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = 1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 1024, 1024, -1, 0};
    vmDomainInfo.numaInfo[1] = vmNumaInfo2;
    vmDomainInfos.push_back(vmDomainInfo);
    return 0;
}

TEST_F(TestMempoolMigrateModule, MigrateExecuteImplFailed)
{
    MOCKER_CPP(&MempoolMigrateModule::ValidateAllPidSamePlane,
               MpResult(*)(const std::vector<VMMigrateOutParam>& vmMigrateOutParam))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolMigrateModule::ValidateRemoteFreeSpace,
               bool (*)(const std::vector<VMMigrateOutParam>& vmMigrateOutParam))
        .stubs()
        .will(returnValue(false));
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    uint64_t waitingTime;
    std::vector<std::string> borrowIdList;
    MpResult res = MempoolMigrateExecute::MigrateExecuteImpl(vmMigrateOutParam, waitingTime, borrowIdList);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

static uint32_t MockOsturboFunctionCallerReturn0(const std::string& function, const TurboByteBuffer& params,
                                                 TurboByteBuffer& result)
{
    return 0;
}

static uint32_t MockOsturboFunctionCallerReturn1(const std::string& function, const TurboByteBuffer& params,
                                                 TurboByteBuffer& result)
{
    return 1;
}

static uint32_t MockOsturboFunctionCallerReturn3(const std::string& function, const TurboByteBuffer& params,
                                                 TurboByteBuffer& result)
{
    return 3;
}

static uint32_t MockRmrsMigrateExecuteReturn0(const MigrateStrategyResult& migrateStrategyResult)
{
    return 0;
}

static uint32_t MockRmrsMigrateExecuteReturn1(const MigrateStrategyResult& migrateStrategyResult)
{
    return 1;
}

static uint32_t MockRmrsMigrateExecuteReturn3(const MigrateStrategyResult& migrateStrategyResult)
{
    return 3;
}

static uint32_t RmrsMigrateBackReturn0(MigrateBackResult& migrateBackResult)
{
    return 0;
}

static uint32_t RmrsMigrateBackReturn1(MigrateBackResult& migrateBackResult)
{
    return 1;
}

static uint32_t RmrsMigrateBackReturn3(MigrateBackResult& migrateBackResult)
{
    return 3;
}

TEST_F(TestMempoolMigrateModule, MigrateExecuteImplFailed1)
{
    MOCKER_CPP(&MempoolMigrateModule::ValidateAllPidSamePlane,
               MpResult(*)(const std::vector<VMMigrateOutParam>& vmMigrateOutParam))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolMigrateModule::ValidateRemoteFreeSpace,
               bool (*)(const std::vector<VMMigrateOutParam>& vmMigrateOutParam))
        .stubs()
        .will(returnValue(true));
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    VMMigrateOutParam vm1{10086, 131072, 4};
    vmMigrateOutParam.push_back(vm1);
    uint64_t waitingTime = 50000;
    std::vector<std::string> borrowIdList;
    MempoolingMessage::rmrsMigrateExecute = &MockRmrsMigrateExecuteReturn3;
    MpResult res = MempoolMigrateExecute::MigrateExecuteImpl(vmMigrateOutParam, waitingTime, borrowIdList);
    EXPECT_EQ(res, 17);
}

TEST_F(TestMempoolMigrateModule, MigrateExecuteImplFailed2)
{
    MOCKER_CPP(&MempoolMigrateModule::ValidateAllPidSamePlane,
               MpResult(*)(const std::vector<VMMigrateOutParam>& vmMigrateOutParam))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolMigrateModule::ValidateRemoteFreeSpace,
               bool (*)(const std::vector<VMMigrateOutParam>& vmMigrateOutParam))
        .stubs()
        .will(returnValue(true));
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    VMMigrateOutParam vm1{10086, 131072, 4};
    vmMigrateOutParam.push_back(vm1);
    uint64_t waitingTime = 50000;
    std::vector<std::string> borrowIdList;
    MempoolingMessage::rmrsMigrateExecute = &MockRmrsMigrateExecuteReturn1;
    MpResult res = MempoolMigrateExecute::MigrateExecuteImpl(vmMigrateOutParam, waitingTime, borrowIdList);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMempoolMigrateModule, MigrateExecuteImplOK)
{
    MOCKER_CPP(&MempoolMigrateModule::ValidateAllPidSamePlane,
               MpResult(*)(const std::vector<VMMigrateOutParam>& vmMigrateOutParam))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolMigrateModule::ValidateRemoteFreeSpace,
               bool (*)(const std::vector<VMMigrateOutParam>& vmMigrateOutParam))
        .stubs()
        .will(returnValue(true));
    std::vector<VMMigrateOutParam> vmMigrateOutParam;
    VMMigrateOutParam vm1{10086, 131072, 4};
    vmMigrateOutParam.push_back(vm1);
    uint64_t waitingTime = 50000;
    std::vector<std::string> borrowIdList;
    MempoolingMessage::rmrsMigrateExecute = &MockRmrsMigrateExecuteReturn0;
    MpResult res = MempoolMigrateExecute::MigrateExecuteImpl(vmMigrateOutParam, waitingTime, borrowIdList);
    EXPECT_EQ(res, 0);
}

uint32_t MockOsturboFunctionCallerResult(const std::string& function, const TurboByteBuffer& params,
                                         TurboByteBuffer& result)
{
    result.len = 0;
    return 0;
}

uint32_t MockOsturboFunctionCallerResultOk(const std::string& function, const TurboByteBuffer& params,
                                           TurboByteBuffer& result)
{
    result.len = 1;
    result.data = new uint8_t[1];
    result.data[0] = 0;
    result.freeFunc = [](uint8_t* p) {
        delete[] p;
    };
    return 0;
}

uint32_t MockRmrsBorrowRollBackReturn0(std::map<std::string, std::set<BorrowIdInfo>>& borrowIdsPidsMap)
{
    return 0;
}

uint32_t MockRmrsBorrowRollBackReturn1(std::map<std::string, std::set<BorrowIdInfo>>& borrowIdsPidsMap)
{
    return 1;
}

uint32_t MockRmrsBorrowRollBackReturn3(std::map<std::string, std::set<BorrowIdInfo>>& borrowIdsPidsMap)
{
    return 3;
}

TEST_F(TestMempoolMigrateModule, FreeMemAndPersistentFailed1)
{
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
               MpResult(*)(const std::string& name, bool isForceDelete, bool smapBack))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::set<std::string> validBorrowIdSet = {"borrowId1", "borrowId2"};
    std::map<std::string, std::set<BorrowIdInfo>> curBorrowIdsPidsMap;
    BorrowIdInfo borrowIdInfo = {10086, 0};
    std::set<BorrowIdInfo> borrowIdSet1;
    borrowIdSet1.insert(borrowIdInfo);

    BorrowIdInfo borrowIdInfo1 = {10000, 0};
    std::set<BorrowIdInfo> borrowIdSet2;
    borrowIdSet2.insert(borrowIdInfo1);

    curBorrowIdsPidsMap["borrowId1"] = borrowIdSet1;
    curBorrowIdsPidsMap["borrowId2"] = borrowIdSet2;
    RollBackBorrowIdPid outEntry;
    bool res = MempoolMigrateModule::FreeMemAndPersistent(validBorrowIdSet, curBorrowIdsPidsMap, outEntry);
    EXPECT_EQ(res, false);
}

TEST_F(TestMempoolMigrateModule, FreeMemAndPersistentSuccess)
{
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
               MpResult(*)(const std::string& name, bool isForceDelete, bool smapBack))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    std::set<std::string> validBorrowIdSet = {"borrowId1", "borrowId2"};
    std::map<std::string, std::set<BorrowIdInfo>> curBorrowIdsPidsMap;

    BorrowIdInfo borrowIdInfo = {10086, 0};
    std::set<BorrowIdInfo> borrowIdSet1;
    borrowIdSet1.insert(borrowIdInfo);

    BorrowIdInfo borrowIdInfo1 = {10000, 0};
    std::set<BorrowIdInfo> borrowIdSet2;
    borrowIdSet2.insert(borrowIdInfo1);

    curBorrowIdsPidsMap["borrowId1"] = borrowIdSet1;
    curBorrowIdsPidsMap["borrowId2"] = borrowIdSet2;
    RollBackBorrowIdPid outEntry;
    bool res = MempoolMigrateModule::FreeMemAndPersistent(validBorrowIdSet, curBorrowIdsPidsMap, outEntry);
    EXPECT_EQ(res, true);
}

TEST_F(TestMempoolMigrateModule, FreeMemAndPersistentSuccess1)
{
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
               MpResult(*)(const std::string& name, bool isForceDelete, bool smapBack))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    std::set<std::string> validBorrowIdSet = {"borrowId1", "borrowId2"};
    std::map<std::string, std::set<BorrowIdInfo>> curBorrowIdsPidsMap;

    BorrowIdInfo borrowIdInfo = {10086, 0};
    std::set<BorrowIdInfo> borrowIdSet1;
    borrowIdSet1.insert(borrowIdInfo);

    BorrowIdInfo borrowIdInfo1 = {10000, 0};
    std::set<BorrowIdInfo> borrowIdSet2;
    borrowIdSet2.insert(borrowIdInfo1);

    BorrowIdInfo borrowIdInfo2 = {10010, 0};
    std::set<BorrowIdInfo> borrowIdSet3;
    borrowIdSet3.insert(borrowIdInfo2);

    curBorrowIdsPidsMap["borrowId1"] = borrowIdSet1;
    curBorrowIdsPidsMap["borrowId2"] = borrowIdSet2;
    curBorrowIdsPidsMap["borrowId3"] = borrowIdSet3;
    RollBackBorrowIdPid outEntry;
    bool res = MempoolMigrateModule::FreeMemAndPersistent(validBorrowIdSet, curBorrowIdsPidsMap, outEntry);
    EXPECT_EQ(res, true);
}

TEST_F(TestMempoolMigrateModule, MemBorrowRollbackImplFailed)
{
    std::vector<std::string> borrowIds;
    RollBackBorrowIdPid inEntry;
    RollBackForOutEntry outEntry;
    MpResult res = MempoolMigrateExecute::MemBorrowRollbackImpl(borrowIds, inEntry, outEntry);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolMigrateModule, MemBorrowRollbackImplFailed1)
{
    std::vector<std::string> borrowIds = {"borrowId1", "borrowId2", "borrowId3"};
    RollBackBorrowIdPid inEntry;
    inEntry.borrowIdList = {"borrowId1", "borrowId2", "borrowId3"};
    inEntry.pidList = {{10086, 0}, {10000, 0}};
    RollBackForOutEntry outEntry;
    MempoolingMessage::rmrsBorrowRollBack = &MockRmrsBorrowRollBackReturn3;
    MpResult res = MempoolMigrateExecute::MemBorrowRollbackImpl(borrowIds, inEntry, outEntry);
    EXPECT_EQ(res, 17);
}

TEST_F(TestMempoolMigrateModule, MemBorrowRollbackImplFailed2)
{
    std::vector<std::string> borrowIds = {"borrowId1", "borrowId2", "borrowId3"};
    RollBackBorrowIdPid inEntry;
    inEntry.borrowIdList = {"borrowId1", "borrowId2", "borrowId3"};
    inEntry.pidList = {{10086, 0}, {10000, 0}};
    RollBackForOutEntry outEntry;
    MempoolingMessage::rmrsBorrowRollBack = &MockRmrsBorrowRollBackReturn1;
    MpResult res = MempoolMigrateExecute::MemBorrowRollbackImpl(borrowIds, inEntry, outEntry);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMempoolMigrateModule, MemBorrowRollbackImplFailed5)
{
    std::vector<std::string> borrowIds = {"borrowId1", "borrowId2", "borrowId3"};
    RollBackBorrowIdPid inEntry;
    inEntry.borrowIdList = {"borrowId1", "borrowId2", "borrowId3"};
    inEntry.pidList = {{10086, 0}, {10000, 0}};
    RollBackForOutEntry outEntry;
    MempoolingMessage::rmrsBorrowRollBack = &MockRmrsBorrowRollBackReturn0;

    MpResult res = MempoolMigrateExecute::MemBorrowRollbackImpl(borrowIds, inEntry, outEntry);
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestMempoolMigrateModule, MemBorrowRollbackImplSuccess)
{
    std::vector<std::string> borrowIds = {"borrowId1", "borrowId2", "borrowId3"};
    RollBackBorrowIdPid inEntry;
    inEntry.borrowIdList = {"borrowId1", "borrowId2", "borrowId3"};
    inEntry.pidList = {{10086, 0}, {10000, 0}};
    RollBackForOutEntry outEntry;
    MempoolingMessage::rmrsBorrowRollBack = &MockRmrsBorrowRollBackReturn0;
    MOCKER_CPP(
        &MempoolMigrateModule::FreeMemAndPersistent,
        bool (*)(std::set<std::string> & validBorrowIdSet,
                 std::map<std::string, std::set<BorrowIdInfo>> & curBorrowIdsPidsMap, RollBackBorrowIdPid & outEntry))
        .stubs()
        .will(returnValue(true));

    MpResult res = MempoolMigrateExecute::MemBorrowRollbackImpl(borrowIds, inEntry, outEntry);

    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestMempoolMigrateModule, ValidateSamePlane_Fail01)
{
    VMMigrateOutParam perVmParam;
    perVmParam.pid = 112233;
    perVmParam.memSize = 1024;
    perVmParam.desNumaId = 4;
    std::vector<VmDomainInfo> vmDomainInfos;
    VmDomainInfo VmA;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 1024, 0, 1};
    VmA.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {4, 2048, 1024, -1, 0};
    VmA.numaInfo[1] = vmNumaInfo2;
    VmA.metaData.pid = 11;
    vmDomainInfos.push_back(VmA);

    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    std::string curNodeId;

    MpResult res = MempoolMigrateModule::ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
    ASSERT_EQ(res, 1);
}

TEST_F(TestMempoolMigrateModule, ValidateSamePlane_Fail02)
{
    VMMigrateOutParam perVmParam;
    perVmParam.pid = 112233;
    perVmParam.memSize = 1024;
    perVmParam.desNumaId = 4;
    std::vector<VmDomainInfo> vmDomainInfos;
    VmDomainInfo VmA;

    VmA.metaData.pid = 112233;
    vmDomainInfos.push_back(VmA);

    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    std::vector<ubse::nodeController::NumaData> numas{};
    std::vector<ubse::nodeController::CpuData> cpus{};
    TelemetrySocketData t1{"node-1", {"0", numas, cpus}, "hostA"};
    TelemetrySocketData t3{"node-3", {"0", numas, cpus}, "hostC"};

    MemNodeData m1(std::move(t1));
    m1.isRegisterRm = true;
    MemNodeData m3(std::move(t3));
    m3.isRegisterRm = true;

    // 分组，比如 key 是 "rack-1"
    nodeTopology["Node0-0"] = {m1};
    nodeTopology["Node1-0"] = {m3};

    std::string curNodeId = "Node0";

    MpResult res = MempoolMigrateModule::ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
    ASSERT_EQ(res, 1);
}

MpResult CollectBorrowRecords_moc(BorrowRecordHelper* This, const std::string nodeId,
                                  std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord record;
    record.borrowNode = "Node0";
    record.lentNode = "Node1";
    record.lentSocketId = 0;
    record.borrowRemoteNuma = 4;
    record.borrowMemId = {1};
    record.name = "name1";
    record.size = 1;
    record.borrowRemoteNuma = 1;
    borrowRecords.push_back(record);
    return 0;
}

TEST_F(TestMempoolMigrateModule, ValidateSamePlane_Fail03)
{
    VMMigrateOutParam perVmParam;
    perVmParam.pid = 112233;
    perVmParam.memSize = 1024;
    perVmParam.desNumaId = 4;
    std::vector<VmDomainInfo> vmDomainInfos;
    VmDomainInfo VmA;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 1024, 0, 1};
    VmA.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {4, 2048, 1024, -1, 0};
    VmA.numaInfo[1] = vmNumaInfo2;
    VmA.metaData.pid = 112233;
    vmDomainInfos.push_back(VmA);

    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    std::vector<ubse::nodeController::NumaData> numas{};
    std::vector<ubse::nodeController::CpuData> cpus{};
    TelemetrySocketData t1{"node-1", {"0", numas, cpus}, "hostA"};
    TelemetrySocketData t3{"node-3", {"0", numas, cpus}, "hostC"};

    MemNodeData m1(std::move(t1));
    m1.isRegisterRm = true;
    MemNodeData m3(std::move(t3));
    m3.isRegisterRm = true;

    // 分组，比如 key 是 "rack-1"
    nodeTopology["Node0-0"] = {m1};
    nodeTopology["Node1-0"] = {m3};

    std::string curNodeId = "Node0";

    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecords_moc));

    MpResult res = MempoolMigrateModule::ValidateSamePlane(perVmParam, vmDomainInfos, nodeTopology, curNodeId);
    ASSERT_EQ(res, 1);
}

uint32_t MockRackRpcSendReturnInMigrateExecuteRpc(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData,
                                                  void* ctx, const UbseComRespHandler& handler)
{
    // 解析
    uint32_t* ret = static_cast<uint32_t*>(ctx);
    *ret = MEM_POOLING_ERROR;
    return MEM_POOLING_OK;
}

TEST_F(TestMempoolMigrateModule, MigrateExecuteRpc_TestWithFailed0)
{
    std::string borrowInNode = "Node0";
    VMMigrateOutParam vm_out = {10086, 131072, 4};
    std::vector<VMMigrateOutParam> vmInfoList;
    vmInfoList.push_back(vm_out);
    uint64_t waitingTime = 60000;
    std::vector<std::string> borrowIdList{"fafdsfasdfd"};
    MOCKER_CPP(&VmInfosCompleted::Update,
               MpResult(*)(const pid_t pid, std::string remoteNumaId, std::string borrowInNode))
        .stubs()
        .will(returnValue(1));
    MOCKER_CPP(&UbseRpcSend, uint32_t(*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                         const UbseComRespHandler& handler))
        .stubs()
        .will(invoke(MockRackRpcSendReturnInMigrateExecuteRpc));
    MOCKER_CPP(&LibvirtHelper::Connect, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MpResult ret = MempoolMigrateExecute::MigrateExecuteRpc(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, 1);
}

uint32_t MockRackRpcSendReturnInMigrateExecuteRpcSuccess(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData,
                                                         void* ctx, const UbseComRespHandler& handler)
{
    // 解析
    uint32_t* ret = static_cast<uint32_t*>(ctx);
    *ret = 0;
    return MEM_POOLING_OK;
}

TEST_F(TestMempoolMigrateModule, MigrateExecuteRpc_TestWithFailed1)
{
    std::string borrowInNode = "Node0";
    VMMigrateOutParam vm_out = {10086, 131072, 4};
    std::vector<VMMigrateOutParam> vmInfoList;
    vmInfoList.push_back(vm_out);
    uint64_t waitingTime = 60000;
    std::vector<std::string> borrowIdList{"fafdsfasdfd"};
    MOCKER_CPP(&VmInfosCompleted::Update,
               MpResult(*)(const pid_t pid, std::string remoteNumaId, std::string borrowInNode))
        .stubs()
        .will(returnValue(1));
    MOCKER_CPP(&UbseRpcSend, uint32_t(*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                         const UbseComRespHandler& handler))
        .stubs()
        .will(invoke(MockRackRpcSendReturnInMigrateExecuteRpcSuccess));
    MOCKER_CPP(&LibvirtHelper::Connect, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&VmInfosCompleted::Query, MpResult(*)(std::unordered_map<pid_t, std::string> & vmInfosCompletedMap))
        .stubs()
        .will(returnValue(1));
    MpResult ret = MempoolMigrateExecute::MigrateExecuteRpc(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, 1);
}

MpResult MockGetVmInfosCompletedMap(VmInfosCompleted* This, std::unordered_map<pid_t, std::string>& vmInfosCompletedMap)
{
    vmInfosCompletedMap.clear();
    vmInfosCompletedMap.insert({{10086, "vm_a"}, {10087, "vm_b"}});
    return 0;
}

TEST_F(TestMempoolMigrateModule, MigrateExecuteRpc_TestWithFailed2)
{
    std::string borrowInNode = "Node0";
    VMMigrateOutParam vm_out = {10086, 131072, 4};
    std::vector<VMMigrateOutParam> vmInfoList;
    vmInfoList.push_back(vm_out);
    uint64_t waitingTime = 60000;
    std::vector<std::string> borrowIdList{"fafdsfasdfd"};
    MOCKER_CPP(&VmInfosCompleted::Update,
               MpResult(*)(const pid_t pid, std::string remoteNumaId, std::string borrowInNode))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&UbseRpcSend, uint32_t(*)(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                         const UbseComRespHandler& handler))
        .stubs()
        .will(invoke(MockRackRpcSendReturnInMigrateExecuteRpcSuccess));
    MOCKER_CPP(&VmInfosCompleted::Query,
               MpResult(*)(VmInfosCompleted*, std::unordered_map<pid_t, std::string> & vmInfosCompletedMap))
        .stubs()
        .will(invoke(MockGetVmInfosCompletedMap));
    MOCKER_CPP(&LibvirtHelper::Connect, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&VmInfosCompleted::Remove, MpResult(*)(const pid_t pid)).stubs().will(returnValue(1));

    MpResult ret = MempoolMigrateExecute::MigrateExecuteRpc(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMempoolMigrateModule, GetVmInfoImmediatelyRecvHandlerFailed)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately, MpResult(*)(std::vector<VmDomainInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    UbseByteBuffer req;
    UbseByteBuffer resp;
    auto ret = GetVmInfoImmediatelyRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

MpResult MockGetVmInfoImmediately(std::vector<VmDomainInfo>& vmDomainInfos)
{
    VmDomainInfo vm1;
    vm1.metaData.nodeId = "node-001";
    vm1.metaData.hostName = "host1";
    vm1.metaData.uuid = "uuid-1234";
    vm1.metaData.name = "vm-1";
    vm1.metaData.vmCreateTime = time(nullptr) - 3600;
    vm1.metaData.maxMem = 8192; // 8GB
    vm1.metaData.pid = 10086;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 4096, 0, 1};
    vm1.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 2048, -1, 0};
    vm1.numaInfo[1] = vmNumaInfo2;
    vm1.metaData.state = "running";
    vm1.timestamp = time(nullptr);

    // 可按需添加多个 vm
    VmDomainInfo vm2 = vm1;
    vm2.metaData.name = "vm-2";
    vm2.metaData.uuid = "uuid-5678";
    vm2.metaData.pid = 10087;
    VmDomainNumaInfo vmNumaInfo3 = {0, 2048, 2048, 0, 1};
    vm2.numaInfo[0] = vmNumaInfo3;
    VmDomainNumaInfo vmNumaInfo4 = {1, 2048, 0, -1, 0};
    vm2.numaInfo[1] = vmNumaInfo4;
    vm2.metaData.state = "shutoff";
    vm2.timestamp = time(nullptr) - 300;

    vmDomainInfos.push_back(vm1);
    vmDomainInfos.push_back(vm2);

    return MEM_POOLING_OK;
}

TEST_F(TestMempoolMigrateModule, GetVmInfoImmediatelyRecvHandlerSucceed)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(MockGetVmInfoImmediately));
    UbseByteBuffer req;
    UbseByteBuffer resp;
    auto ret = GetVmInfoImmediatelyRecvHandler(req, resp);
    delete[] resp.data;
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMempoolMigrateModule, GetVmInfoImmediatelyResHandlerSucceed)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    vmDomainInfos.push_back(mempooling::exportV2::VmDomainInfo{});
    RmrsOutStream builder;
    builder << vmDomainInfos;
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos2;
    UbseByteBuffer resp{.data = builder.GetBufferPointer(), .len = builder.GetSize()};
    GetVmInfoImmediatelyResHandler(&vmDomainInfos2, resp, 0);
    EXPECT_EQ(vmDomainInfos2.size(), 1);
}

TEST_F(TestMempoolMigrateModule, ConvertNodeTopologySuccess)
{
    ubse::nodeController::NumaData numaData = {"1"};
    ubse::nodeController::CpuData cpuData = {"8"};
    std::string socketId = "216";

    ubse::nodeController::SocketData socketData;
    socketData.socketId = socketId;
    socketData.cpus.push_back(cpuData);
    socketData.numas.push_back(numaData);

    std::string nodeId = "2-36";
    std::string hostName = "computer01";

    ubse::nodeController::TelemetrySocketData telemetrySocketData;
    telemetrySocketData.socket = socketData;
    telemetrySocketData.nodeId = nodeId;
    telemetrySocketData.hostname = hostName;

    // 注意：ConvertNodeTopology 需要的是 ubse::nodeController::MemNodeData
    ubse::nodeController::MemNodeData memNodeData(std::move(telemetrySocketData));

    memNodeData.isRegisterRm = true;

    // 参数类型必须匹配 ConvertNodeTopology
    std::unordered_map<std::string, std::vector<ubse::nodeController::MemNodeData>> nodeTopology;
    std::unordered_map<std::string, std::vector<turbo::rmrs::MemNodeDataNew>> nodeTopologyNew;

    nodeTopology["test"].push_back(memNodeData);

    mempooling::migrate::ConvertNodeTopology(nodeTopology, nodeTopologyNew);

    ASSERT_NE(0, nodeTopologyNew.size());
}

// ==================== BatchBorrowStrategyImpl Test Helpers ====================

static uint32_t MockTopologyTwoPlanes(std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology)
{
    MemNodeData nodeA;
    nodeA.nodeId = "NodeA";
    nodeA.socket.socketId = "0";
    MemNodeData nodeB;
    nodeB.nodeId = "NodeB";
    nodeB.socket.socketId = "0";
    MemNodeData nodeC;
    nodeC.nodeId = "NodeC";
    nodeC.socket.socketId = "1";
    MemNodeData nodeD;
    nodeD.nodeId = "NodeD";
    nodeD.socket.socketId = "1";

    nodeTopology["NodeA-0"] = {nodeA, nodeB};
    nodeTopology["NodeB-0"] = {nodeA, nodeB};
    nodeTopology["NodeC-1"] = {nodeC, nodeD};
    nodeTopology["NodeD-1"] = {nodeC, nodeD};
    return 0;
}

static std::unordered_map<std::string, UbseNodeInfo> BuildNodeMapBatchBorrow(uint32_t nodeBFree1G, uint32_t nodeCFree1G,
                                                                             uint32_t nodeDFree1G)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeMap;

    // NodeA (src): blockSize=1024 MB, NUMA 0 on socket 0
    UbseNodeInfo nodeA;
    nodeA.nodeId = "NodeA";
    nodeA.blockSize = 1024;
    UbseNumaLocation locA = {"NodeA", 0};
    UbseNumaInfo numaA{};
    numaA.socketId = 0;
    numaA.nr_hugepages_1G = 10;
    numaA.free_hugepages_1G = 5;
    nodeA.numaInfos[locA] = numaA;
    nodeMap["NodeA"] = nodeA;

    // NodeB (same plane): NUMA 0 on socket 0
    UbseNodeInfo nodeBInfo;
    nodeBInfo.nodeId = "NodeB";
    nodeBInfo.blockSize = 1024;
    UbseNumaLocation locB = {"NodeB", 0};
    UbseNumaInfo numaB{};
    numaB.socketId = 0;
    numaB.nr_hugepages_1G = 10;
    numaB.free_hugepages_1G = nodeBFree1G;
    nodeBInfo.numaInfos[locB] = numaB;
    nodeMap["NodeB"] = nodeBInfo;

    // NodeC (cross plane): NUMA 0 on socket 1
    UbseNodeInfo nodeCInfo;
    nodeCInfo.nodeId = "NodeC";
    nodeCInfo.blockSize = 1024;
    UbseNumaLocation locC = {"NodeC", 0};
    UbseNumaInfo numaC{};
    numaC.socketId = 1;
    numaC.nr_hugepages_1G = 20;
    numaC.free_hugepages_1G = nodeCFree1G;
    nodeCInfo.numaInfos[locC] = numaC;
    nodeMap["NodeC"] = nodeCInfo;

    // NodeD (cross plane): NUMA 0 on socket 1
    UbseNodeInfo nodeDInfo;
    nodeDInfo.nodeId = "NodeD";
    nodeDInfo.blockSize = 1024;
    UbseNumaLocation locD = {"NodeD", 0};
    UbseNumaInfo numaD{};
    numaD.socketId = 1;
    numaD.nr_hugepages_1G = 15;
    numaD.free_hugepages_1G = nodeDFree1G;
    nodeDInfo.numaInfos[locD] = numaD;
    nodeMap["NodeD"] = nodeDInfo;

    return nodeMap;
}

static MpResult MockGetNumaInfoBatchBorrow(std::vector<mempooling::exportV2::NumaInfo>& numaInfos)
{
    mempooling::exportV2::NumaInfo numaInfo;
    numaInfo.metaData.numaId = 0;
    numaInfo.metaData.socketId = 0;
    numaInfo.metaData.isLocal = true;
    numaInfos.push_back(numaInfo);
    return MEM_POOLING_OK;
}

static MpResult MockCollectBorrowRecordsEmpty(BorrowRecordHelper* This, const std::string nodeId,
                                              std::vector<BorrowRecord>& borrowRecords)
{
    return MEM_POOLING_OK;
}

static MpResult MockCollectBorrowRecordsWithHistory(BorrowRecordHelper* This, const std::string nodeId,
                                                    std::vector<BorrowRecord>& borrowRecords)
{
    for (int i = 0; i < 3; ++i) {
        BorrowRecord record;
        record.borrowNode = "NodeA";
        record.lentNode = "NodeC";
        borrowRecords.push_back(record);
    }
    for (int i = 0; i < 5; ++i) {
        BorrowRecord record;
        record.borrowNode = "NodeA";
        record.lentNode = "NodeD";
        borrowRecords.push_back(record);
    }
    return MEM_POOLING_OK;
}

static void SetupBatchBorrowCommonMocks(uint32_t nodeBFree1G, uint32_t nodeCFree1G, uint32_t nodeDFree1G)
{
    auto nodeMap = BuildNodeMapBatchBorrow(nodeBFree1G, nodeCFree1G, nodeDFree1G);

    MOCKER_CPP(&UbseMemGetTopologyInfo, uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>>&))
        .stubs()
        .will(invoke(MockTopologyTwoPlanes));

    MOCKER_CPP(&UbseNodeController::GetAllNodes,
               (std::unordered_map<std::string, UbseNodeInfo>(*)(UbseNodeController*)))
        .stubs()
        .will(returnValue(nodeMap));

    MOCKER_CPP(&UbseGetAllNodeNumaInfo, uint32_t(*)(std::vector<UbseNodeNumaInfo>&)).stubs().will(returnValue(UBSE_OK));

    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo>&))
        .stubs()
        .will(invoke(MockGetNumaInfoBatchBorrow));
}

// ==================== BatchBorrowStrategyImpl Test Cases ====================

/*
 * UT-1: BatchBorrow_CrossPlaneBasic
 * 用例描述：验证跨平面基本借用场景，同平面不足时从跨平面补充
 * 测试步骤：
 * 1. 设置2平面拓扑，NodeB(同平面5GB), NodeC(跨平面10GB), NodeD(跨平面8GB)
 * 2. 请求借用12GB
 * 3. 同平面NodeB分配5GB，剩余7GB从跨平面NodeC分配
 * 预期结果：
 * 1. 返回MEM_POOLING_OK
 * 2. results.size() == 1
 * 3. 总分配量 == 12GB
 * 4. destParam包含NodeB和NodeC的分配条目
 */
TEST_F(TestMempoolMigrateModule, BatchBorrow_CrossPlaneBasic)
{
    constexpr uint64_t HUGEPAGE_1G_KB = 1048576ULL;
    constexpr uint64_t GB = HUGEPAGE_1G_KB;

    SetupBatchBorrowCommonMocks(5, 10, 8);
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsEmpty));

    std::string srcNid = "NodeA";
    std::vector<int16_t> srcNumaIds = {0};
    uint64_t borrowSize = 12 * GB;
    std::vector<MemBorrowStrategyResult> results;

    MpResult ret = MempoolMigrateModule::BatchBorrowStrategyImpl(
        srcNid, srcNumaIds, borrowSize, mempooling::outinterface::BorrowStrategy::AVERAGE, results);

    ASSERT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(results.size(), 1U);

    uint64_t totalAllocated = 0;
    std::map<std::string, uint64_t> perNodeAllocated;
    for (const auto& dest : results[0].destParam) {
        for (size_t i = 0; i < dest.memSize.size(); ++i) {
            totalAllocated += dest.memSize[i];
            perNodeAllocated[dest.destNid] += dest.memSize[i];
        }
    }

    EXPECT_EQ(totalAllocated, 12 * GB);
    EXPECT_TRUE(perNodeAllocated.count("NodeB") > 0);
    EXPECT_TRUE(perNodeAllocated.count("NodeC") > 0);
    EXPECT_EQ(perNodeAllocated["NodeB"], 5 * GB);
    EXPECT_EQ(perNodeAllocated["NodeC"], 7 * GB);
}

/*
 * UT-2: BatchBorrow_MultisetStateConsistency
 * 用例描述：验证multiset状态一致性，确保分配后节点数据正确
 * 测试步骤：
 * 1. 设置NodeB(5GB), NodeC(10GB), NodeD(8GB)
 * 2. 请求借用15GB
 * 3. 同平面NodeB分配5GB，跨平面NodeC分配10GB
 * 预期结果：
 * 1. 返回MEM_POOLING_OK
 * 2. NodeC分配恰好10GB，NodeB分配恰好5GB
 * 3. destParam条目的destNid值正确
 */
TEST_F(TestMempoolMigrateModule, BatchBorrow_MultisetStateConsistency)
{
    constexpr uint64_t HUGEPAGE_1G_KB = 1048576ULL;
    constexpr uint64_t GB = HUGEPAGE_1G_KB;

    SetupBatchBorrowCommonMocks(5, 10, 8);
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsEmpty));

    std::string srcNid = "NodeA";
    std::vector<int16_t> srcNumaIds = {0};
    uint64_t borrowSize = 15 * GB;
    std::vector<MemBorrowStrategyResult> results;

    MpResult ret = MempoolMigrateModule::BatchBorrowStrategyImpl(
        srcNid, srcNumaIds, borrowSize, mempooling::outinterface::BorrowStrategy::AVERAGE, results);

    ASSERT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(results.size(), 1U);

    uint64_t totalAllocated = 0;
    std::map<std::string, uint64_t> perNodeAllocated;
    for (const auto& dest : results[0].destParam) {
        for (size_t i = 0; i < dest.memSize.size(); ++i) {
            totalAllocated += dest.memSize[i];
            perNodeAllocated[dest.destNid] += dest.memSize[i];
        }
    }

    EXPECT_EQ(totalAllocated, 15 * GB);
    EXPECT_EQ(perNodeAllocated["NodeB"], 5 * GB);
    EXPECT_EQ(perNodeAllocated["NodeC"], 10 * GB);
    EXPECT_EQ(perNodeAllocated.count("NodeD"), 0U);
}

/*
 * UT-3: BatchBorrow_NodeQuotaExhausted
 * 用例描述：验证多节点按优先级依次分配的场景
 * 测试步骤：
 * 1. 设置NodeB(1GB), NodeC(3GB), NodeD(8GB)
 * 2. 请求借用10GB
 * 3. 同平面NodeB分配1GB，跨平面NodeD(8GB totalMem > NodeC 3GB totalMem)先分配8GB，再NodeC分配1GB
 * 预期结果：
 * 1. 返回MEM_POOLING_OK
 * 2. 总分配量 == 10GB
 */
TEST_F(TestMempoolMigrateModule, BatchBorrow_NodeQuotaExhausted)
{
    constexpr uint64_t HUGEPAGE_1G_KB = 1048576ULL;
    constexpr uint64_t GB = HUGEPAGE_1G_KB;

    SetupBatchBorrowCommonMocks(1, 3, 8);
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsEmpty));

    std::string srcNid = "NodeA";
    std::vector<int16_t> srcNumaIds = {0};
    uint64_t borrowSize = 10 * GB;
    std::vector<MemBorrowStrategyResult> results;

    MpResult ret = MempoolMigrateModule::BatchBorrowStrategyImpl(
        srcNid, srcNumaIds, borrowSize, mempooling::outinterface::BorrowStrategy::AVERAGE, results);

    ASSERT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(results.size(), 1U);

    uint64_t totalAllocated = 0;
    std::map<std::string, uint64_t> perNodeAllocated;
    for (const auto& dest : results[0].destParam) {
        for (size_t i = 0; i < dest.memSize.size(); ++i) {
            totalAllocated += dest.memSize[i];
            perNodeAllocated[dest.destNid] += dest.memSize[i];
        }
    }

    EXPECT_EQ(totalAllocated, 10 * GB);
    EXPECT_EQ(perNodeAllocated["NodeB"], 1 * GB);
    EXPECT_EQ(perNodeAllocated["NodeD"], 8 * GB);
    EXPECT_EQ(perNodeAllocated["NodeC"], 1 * GB);
}

/*
 * UT-4: BatchBorrow_MultiRoundSortStability
 * 用例描述：验证borrowCount影响优先级排序，高borrowCount节点优先被选择
 * 测试步骤：
 * 1. 设置NodeB(0GB), NodeC(10GB, borrowCount=3), NodeD(8GB, borrowCount=5)
 * 2. 请求借用10GB
 * 3. 同平面无候选，跨平面NodeD(borrowCount=5)优先分配8GB，NodeC(borrowCount=3)分配2GB
 * 预期结果：
 * 1. 返回MEM_POOLING_OK
 * 2. 总分配量 == 10GB
 * 3. NodeD分配8GB，NodeC分配2GB
 */
TEST_F(TestMempoolMigrateModule, BatchBorrow_MultiRoundSortStability)
{
    constexpr uint64_t HUGEPAGE_1G_KB = 1048576ULL;
    constexpr uint64_t GB = HUGEPAGE_1G_KB;

    SetupBatchBorrowCommonMocks(0, 10, 8);
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsWithHistory));

    std::string srcNid = "NodeA";
    std::vector<int16_t> srcNumaIds = {0};
    uint64_t borrowSize = 10 * GB;
    std::vector<MemBorrowStrategyResult> results;

    MpResult ret = MempoolMigrateModule::BatchBorrowStrategyImpl(
        srcNid, srcNumaIds, borrowSize, mempooling::outinterface::BorrowStrategy::AVERAGE, results);

    ASSERT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(results.size(), 1U);

    uint64_t totalAllocated = 0;
    std::map<std::string, uint64_t> perNodeAllocated;
    for (const auto& dest : results[0].destParam) {
        for (size_t i = 0; i < dest.memSize.size(); ++i) {
            totalAllocated += dest.memSize[i];
            perNodeAllocated[dest.destNid] += dest.memSize[i];
        }
    }

    EXPECT_EQ(totalAllocated, 10 * GB);
    EXPECT_EQ(perNodeAllocated["NodeD"], 8 * GB);
    EXPECT_EQ(perNodeAllocated["NodeC"], 2 * GB);
}

/*
 * UT-5: BatchBorrow_EmptyCrossPlane
 * 用例描述：验证同平面即可满足需求时不触发跨平面分配
 * 测试步骤：
 * 1. 设置NodeB(5GB), NodeC(10GB), NodeD(8GB)
 * 2. 请求借用3GB
 * 3. 同平面NodeB(5GB)足够，分配3GB
 * 预期结果：
 * 1. 返回MEM_POOLING_OK
 * 2. results.size() == 1
 * 3. 所有destParam条目的destNid均为NodeB
 */
TEST_F(TestMempoolMigrateModule, BatchBorrow_EmptyCrossPlane)
{
    constexpr uint64_t HUGEPAGE_1G_KB = 1048576ULL;
    constexpr uint64_t GB = HUGEPAGE_1G_KB;

    SetupBatchBorrowCommonMocks(5, 10, 8);
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsEmpty));

    std::string srcNid = "NodeA";
    std::vector<int16_t> srcNumaIds = {0};
    uint64_t borrowSize = 3 * GB;
    std::vector<MemBorrowStrategyResult> results;

    MpResult ret = MempoolMigrateModule::BatchBorrowStrategyImpl(
        srcNid, srcNumaIds, borrowSize, mempooling::outinterface::BorrowStrategy::AVERAGE, results);

    ASSERT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(results.size(), 1U);

    uint64_t totalAllocated = 0;
    for (const auto& dest : results[0].destParam) {
        EXPECT_EQ(dest.destNid, "NodeB");
        for (size_t i = 0; i < dest.memSize.size(); ++i) {
            totalAllocated += dest.memSize[i];
        }
    }

    EXPECT_EQ(totalAllocated, 3 * GB);
}

/*
 * UT-6: BatchBorrow_SingleCandidateMultipleAllocations
 * 用例描述：验证仅有一个跨平面候选节点时的分配行为
 * 测试步骤：
 * 1. 设置NodeB(0GB), NodeC(5GB), NodeD(0GB)
 * 2. 请求借用3GB
 * 3. 同平面无候选，跨平面仅NodeC可用，分配3GB
 * 预期结果：
 * 1. 返回MEM_POOLING_OK
 * 2. 总分配量 == 3GB
 * 3. 所有分配均来自NodeC
 */
TEST_F(TestMempoolMigrateModule, BatchBorrow_SingleCandidateMultipleAllocations)
{
    constexpr uint64_t HUGEPAGE_1G_KB = 1048576ULL;
    constexpr uint64_t GB = HUGEPAGE_1G_KB;

    SetupBatchBorrowCommonMocks(0, 5, 0);
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(invoke(MockCollectBorrowRecordsEmpty));

    std::string srcNid = "NodeA";
    std::vector<int16_t> srcNumaIds = {0};
    uint64_t borrowSize = 3 * GB;
    std::vector<MemBorrowStrategyResult> results;

    MpResult ret = MempoolMigrateModule::BatchBorrowStrategyImpl(
        srcNid, srcNumaIds, borrowSize, mempooling::outinterface::BorrowStrategy::AVERAGE, results);

    ASSERT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(results.size(), 1U);

    uint64_t totalAllocated = 0;
    for (const auto& dest : results[0].destParam) {
        EXPECT_EQ(dest.destNid, "NodeC");
        for (size_t i = 0; i < dest.memSize.size(); ++i) {
            totalAllocated += dest.memSize[i];
        }
    }

    EXPECT_EQ(totalAllocated, 3 * GB);
}

} // namespace mempooling::migrate