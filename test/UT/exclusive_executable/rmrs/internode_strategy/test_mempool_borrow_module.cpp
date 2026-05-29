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

#include <gtest/gtest.h>
#include "mockcpp/mockcpp.hpp"

#include "ubse_com.h"
#include "ubse_common_def.h"
#define private public
#include "mem_manager.h"
#include "mempool_borrow_module.h"
#include "mp_configuration.h"
#undef private
#include <iostream>
#include "exporter.h"
#include "mem_borrow_executor.h"
#include "mem_json_def.h"
#include "mempool_migrate_module.h"
#include "mempooling_message.h"
#include "rmrs_serialize.h"
#include "turbo_rmrs_interface.h"
#include "unordered_set"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace mempooling {
using namespace std;
using namespace ubse::common::def;
using namespace ubse::com;
using namespace mempooling::message;
using namespace mempooling::exportV2;
using namespace rmrs::serialize;

class TestMemPoolBorrowModule : public ::testing::Test {
protected:
    TestMemPoolBorrowModule() {}
    // virtual ~TestMemBorrowHelper() {}
    virtual void SetUp() {}
    virtual void TearDown()
    {
        GlobalMockObject::reset();
    }
};

uint32_t MockRackRpcSend(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                         const UbseComRespHandler& handler)
{
    // 解析
    turbo::rmrs::MigrateBackResult* migrateStrategyResult = static_cast<turbo::rmrs::MigrateBackResult*>(ctx);
    turbo::rmrs::MigrateBackResult result;
    result.result = MEM_POOLING_OK;
    result.NumaIds = {0, 1};
    *migrateStrategyResult = result;
    return MEM_POOLING_OK;
}

uint32_t MockRackRpcSendReturnError(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                                    const UbseComRespHandler& handler)
{
    // 解析
    uint32_t* migrateStrategyResult = static_cast<uint32_t*>(ctx);
    uint32_t result = MEM_POOLING_ERROR;
    *migrateStrategyResult = result;
    return MEM_POOLING_OK;
}

TEST_F(TestMemPoolBorrowModule, Init_TestSuccess)
{
    MpResult ret = MempoolBorrowModule::Instance().Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolBorrowModule, compareNodeMemInfo_TestSuccess)
{
    std::pair<std::string, NodeMemInfo> a = {"nodeA", NodeMemInfo{.totalBorrowableMem = 100}};
    std::pair<std::string, NodeMemInfo> b = {"nodeB", NodeMemInfo{.totalBorrowableMem = 200}};

    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(const std::string& nodeId, NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem))
        .stubs()
        .will(returnValue(1));
    bool flag = compareNodeMemInfo("main-node", a, b);
    GlobalMockObject::verify();
    EXPECT_EQ(flag, false);
}

MpResult CollectBorrowableInfoMock1(BorrowRecordHelper* This, const std::string& nodeId,
                                    NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem)
{
    // 填充基础字段（可选）
    nodeMemoryInfoWithReservedMem.timestamp = std::time(nullptr);
    nodeMemoryInfoWithReservedMem.nodeId = "NodeA";
    nodeMemoryInfoWithReservedMem.totalMemory = 65536; // 64 GB
    nodeMemoryInfoWithReservedMem.usedMemory = 32768;  // 32 GB
    nodeMemoryInfoWithReservedMem.freeMemory = 16384;  // 16 GB
    nodeMemoryInfoWithReservedMem.lentMemory = 4096;   // 4 GB
    nodeMemoryInfoWithReservedMem.reservedMem = 2048;  // 2 GB
    nodeMemoryInfoWithReservedMem.sharedMem = 1024;    // 1 GB
    nodeMemoryInfoWithReservedMem.socketId = 0;
    nodeMemoryInfoWithReservedMem.canBorrowMem = 4096;
    return MEM_POOLING_OK;
}

TEST_F(TestMemPoolBorrowModule, compareNodeMemInfo_TestFailed)
{
    std::pair<std::string, NodeMemInfo> a = {"NodeB", NodeMemInfo{.totalBorrowableMem = 100}};
    std::pair<std::string, NodeMemInfo> b = {"NodeD", NodeMemInfo{.totalBorrowableMem = 200}};

    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(BorrowRecordHelper*, const std::string& nodeId,
                           NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem))
        .stubs()
        .will(invoke(CollectBorrowableInfoMock1));
    bool flag = compareNodeMemInfo("main-node", a, b);
    GlobalMockObject::verify();
    EXPECT_EQ(flag, false);
}

TEST_F(TestMemPoolBorrowModule, compareNodeMemInfo_TestFailed1)
{
    std::pair<std::string, NodeMemInfo> a = {"NodeX", NodeMemInfo{.totalBorrowableMem = 100}};
    std::pair<std::string, NodeMemInfo> b = {"NodeC", NodeMemInfo{.totalBorrowableMem = 200}};
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(BorrowRecordHelper*, const std::string& nodeId,
                           NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem))
        .stubs()
        .will(invoke(CollectBorrowableInfoMock1));
    bool flag = compareNodeMemInfo("main-node", a, b);
    GlobalMockObject::verify();
    EXPECT_EQ(flag, false);
}

TEST_F(TestMemPoolBorrowModule, compareNodeMemInfo_TestFailed2)
{
    std::pair<std::string, NodeMemInfo> a = {"NodeX", NodeMemInfo{.totalBorrowableMem = 100}};
    std::pair<std::string, NodeMemInfo> b = {"NodeY", NodeMemInfo{.totalBorrowableMem = 200}};
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(BorrowRecordHelper*, const std::string& nodeId,
                           NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem))
        .stubs()
        .will(invoke(CollectBorrowableInfoMock1));
    bool flag = compareNodeMemInfo("main-node", a, b);
    GlobalMockObject::verify();
    EXPECT_EQ(flag, false);
}

TEST_F(TestMemPoolBorrowModule, compareNodeMemInfo_TestFailed3)
{
    std::pair<std::string, NodeMemInfo> a = {"NodeX", NodeMemInfo{.totalBorrowableMem = 300}};
    std::pair<std::string, NodeMemInfo> b = {"NodeY", NodeMemInfo{.totalBorrowableMem = 200}};
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(BorrowRecordHelper*, const std::string& nodeId,
                           NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem))
        .stubs()
        .will(invoke(CollectBorrowableInfoMock1));
    bool flag = compareNodeMemInfo("main-node", a, b);
    GlobalMockObject::verify();
    EXPECT_EQ(flag, true);
}

TEST_F(TestMemPoolBorrowModule, compareSocketMemInfo_TestFailed1)
{
    std::pair<std::string, NodeMemInfo> a = {"nodeA", NodeMemInfo{.totalBorrowableMem = 100}};
    std::pair<std::string, NodeMemInfo> b = {"nodeB", NodeMemInfo{.totalBorrowableMem = 200}};
    std::unordered_map<std::string, std::unordered_set<uint16_t>> nodeIdToNumaIdSetMap;
    std::vector<RackMemNumaPair> borrowedItemVec;
    MOCKER_CPP(&mempooling::MemManager::JudgeSampPlane,
               bool (*)(MemManager*, const std::string&, const uint16_t&, const std::string&, const uint16_t&))
        .stubs()
        .will(returnValue(true));
    SrcMemoryBorrowParam srcParam;
    bool flag = compareSocketMemInfo(nodeIdToNumaIdSetMap, borrowedItemVec, srcParam, a, b);
    GlobalMockObject::verify();
    EXPECT_EQ(flag, false);
}

TEST_F(TestMemPoolBorrowModule, compareSocketMemInfo_TestSuccess)
{
    std::pair<std::string, NodeMemInfo> a = {"NodeB", NodeMemInfo{.totalBorrowableMem = 100}};
    std::pair<std::string, NodeMemInfo> b = {"NodeA", NodeMemInfo{.totalBorrowableMem = 200}};
    std::unordered_map<std::string, std::unordered_set<uint16_t>> nodeIdToNumaIdSetMap;
    nodeIdToNumaIdSetMap["NodeA"] = {0, 1, 2};
    std::vector<RackMemNumaPair> borrowedItemVec;
    RackMemNumaPair rackMemNumaPair;
    rackMemNumaPair.nodeId = "NodeA";
    rackMemNumaPair.numaId = 2;
    borrowedItemVec.push_back(rackMemNumaPair);
    MOCKER_CPP(&mempooling::MemManager::JudgeSampPlane,
               bool (*)(MemManager*, const std::string&, const uint16_t&, const std::string&, const uint16_t&))
        .stubs()
        .will(returnValue(true));
    SrcMemoryBorrowParam srcParam;
    bool flag = compareSocketMemInfo(nodeIdToNumaIdSetMap, borrowedItemVec, srcParam, a, b);
    GlobalMockObject::verify();
    EXPECT_EQ(flag, false);
}

TEST_F(TestMemPoolBorrowModule, compareSocketMemInfo_TestFailed2)
{
    std::pair<std::string, NodeMemInfo> a = {"NodeA", NodeMemInfo{.totalBorrowableMem = 100}};
    std::pair<std::string, NodeMemInfo> b = {"NodeB", NodeMemInfo{.totalBorrowableMem = 200}};
    std::unordered_map<std::string, std::unordered_set<uint16_t>> nodeIdToNumaIdSetMap;
    nodeIdToNumaIdSetMap["NodeA"] = {0, 1, 2};
    std::vector<RackMemNumaPair> borrowedItemVec;
    RackMemNumaPair rackMemNumaPair;
    rackMemNumaPair.nodeId = "NodeA";
    rackMemNumaPair.numaId = 2;
    borrowedItemVec.push_back(rackMemNumaPair);
    MOCKER_CPP(&mempooling::MemManager::JudgeSampPlane,
               bool (*)(MemManager*, const std::string&, const uint16_t&, const std::string&, const uint16_t&))
        .stubs()
        .will(returnValue(true));
    SrcMemoryBorrowParam srcParam;
    bool flag = compareSocketMemInfo(nodeIdToNumaIdSetMap, borrowedItemVec, srcParam, a, b);
    GlobalMockObject::verify();
    EXPECT_EQ(flag, true);
}

TEST_F(TestMemPoolBorrowModule, ValidateSrcparam_TestFailed1)
{
    MOCKER_CPP(&MemManager::GenerateNumaSocketMap,
               MpResult(*)(std::map<std::string, std::map<int, int>> & numaSocketMap))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    SrcMemoryBorrowParam srcParam = {"Node3", 0, 0};
    MpResult ret = MempoolBorrowModule::Instance().ValidateSrcparam(srcParam);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

MpResult MockGenerateNumaSocketMap(std::map<std::string, std::map<int, uint16_t>>& numaSocketMap)
{
    numaSocketMap.clear();
    numaSocketMap["Node0"][0] = 0;
    return MEM_POOLING_OK;
}

TEST_F(TestMemPoolBorrowModule, ValidateSrcparam_TestFailed2)
{
    MOCKER_CPP(&MemManager::GenerateNumaSocketMap,
               MpResult(*)(std::map<std::string, std::map<int, int>> & numaSocketMap))
        .stubs()
        .will(invoke(MockGenerateNumaSocketMap));
    SrcMemoryBorrowParam srcParam = {"Node3", 0, 0};
    MpResult ret = MempoolBorrowModule::Instance().ValidateSrcparam(srcParam);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ValidateSrcparam_TestFailed3)
{
    MOCKER_CPP(&MemManager::GenerateNumaSocketMap,
               MpResult(*)(std::map<std::string, std::map<int, int>> & numaSocketMap))
        .stubs()
        .will(invoke(MockGenerateNumaSocketMap));
    SrcMemoryBorrowParam srcParam = {"Node0", 1, 0};
    MpResult ret = MempoolBorrowModule::Instance().ValidateSrcparam(srcParam);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ValidateSrcparam_TestFailed4)
{
    MOCKER_CPP(&MemManager::GenerateNumaSocketMap,
               MpResult(*)(std::map<std::string, std::map<int, int>> & numaSocketMap))
        .stubs()
        .will(invoke(MockGenerateNumaSocketMap));
    SrcMemoryBorrowParam srcParam = {"Node0", 0, 1};
    MpResult ret = MempoolBorrowModule::Instance().ValidateSrcparam(srcParam);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ValidateSrcparam_TestSuccess)
{
    MOCKER_CPP(&MemManager::GenerateNumaSocketMap,
               MpResult(*)(std::map<std::string, std::map<int, uint16_t>> & numaSocketMap))
        .stubs()
        .will(invoke(MockGenerateNumaSocketMap));

    SrcMemoryBorrowParam srcParam = {"Node0", 0, 0};
    MpResult ret = MempoolBorrowModule::Instance().ValidateSrcparam(srcParam);

    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolBorrowModule, UpdateNodeMemInfoWithNuma_Test)
{
    mempooling::exportV2::NumaInfo numaInfo1;
    numaInfo1.metaData.numaId = 1;
    numaInfo1.metaData.memFree = 5000;

    mempooling::exportV2::NumaInfo numaInfo2;
    numaInfo2.metaData.numaId = 2;
    numaInfo2.metaData.memFree = 3000;

    // 2. Constructing NodeMemInfo objects
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.localnumaMemInfo.push_back({1, 100, 200, 1000, 0});
    nodeMemInfo1.localnumaMemInfo.push_back({2, 200, 100, 1500, 0});

    std::unordered_map<std::string, NodeMemInfo> nodeMemMap = {{"Node1", nodeMemInfo1}};

    // 3. Testing UpdateNodeMemInfoWithNuma function
    MempoolBorrowModule::Instance().UpdateNodeMemInfoWithNuma(nodeMemMap);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategy_TestWithBorrowFailed)
{
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;
    uint64_t borrowSize = 1048576;
    MemBorrowStrategyResult borrowStrategyResult;

    std::unordered_map<std::string, NodeMemInfo> nodeMemMapSave = MemManager::Instance().nodeMemMap;
    // 初始化一个 NodeMemInfo 实例
    NodeMemInfo node1;
    node1.totalReservedMem = 1000;
    node1.totalBorrowableMem = 500;
    node1.totalLentMem = 200;
    node1.totalBorrowedMem = 300;

    // 添加 NumaMemInfo 实例到 localnumaMemInfo
    NumaMemInfo numa1;
    numa1.numaId = 1;
    numa1.reservedMem = 100;
    numa1.lentMem = 50;
    numa1.borrowableMem = 30;
    numa1.borrowedMem = 20;
    node1.localnumaMemInfo.push_back(numa1);

    // 将 node1 添加到 nodeMemMap
    MemManager::Instance().nodeMemMap["node1"] = node1;
    // GetMemoryInfo
    MOCKER_CPP(&MempoolBorrowModule::FilterAndSortSockets,
               MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology,
                           std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::UpdateNodeMemInfoWithNuma,
               void (*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap))
        .stubs()
        .will(returnValue(nullptr));
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowStrategy(srcParam, borrowSize, borrowStrategyResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategy_TestWithBadBorrowSuccess)
{
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;
    uint64_t borrowSize = 131072;
    MemBorrowStrategyResult borrowStrategyResult;

    MemManager::Instance().nodeMemMap.clear();
    // 初始化一个 NodeMemInfo 实例
    NodeMemInfo node1;
    node1.totalReservedMem = 1048576;
    node1.totalBorrowableMem = 1048576;
    node1.totalLentMem = 0;
    node1.totalBorrowedMem = 0;

    // 添加 NumaMemInfo 实例到 localnumaMemInfo
    NumaMemInfo numa1;
    numa1.numaId = 0;
    numa1.reservedMem = 1048576;
    numa1.lentMem = 0;
    numa1.borrowableMem = 1048576;
    numa1.borrowedMem = 0;
    node1.localnumaMemInfo.push_back(numa1);

    // 将 node1 添加到 nodeMemMap
    MemManager::Instance().nodeMemMap["Node1"] = node1;
    MOCKER_CPP(&MempoolBorrowModule::UpdateNodeMemInfoWithNuma,
               void (*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap))
        .stubs()
        .will(returnValue(nullptr));
    MOCKER_CPP(&MempoolBorrowModule::FilterAndSortSockets,
               MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology,
                           std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowStrategy(srcParam, borrowSize, borrowStrategyResult);
    GlobalMockObject::verify();
    MemManager::Instance().nodeMemMap.clear();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

uint32_t MockRackRpcSendReturnInMigrateStrategy(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData,
                                                void* ctx, const UbseComRespHandler& handler)
{
    // 解析
    turbo::rmrs::MigrateStrategyResult* migrateStrategyResult = static_cast<turbo::rmrs::MigrateStrategyResult*>(ctx);
    turbo::rmrs::MigrateStrategyResult result;
    turbo::rmrs::VMMigrateOutParam vm_out = {-1, 16, 4};
    result.vmInfoList.push_back(vm_out);
    result.waitingTime = 60;
    *migrateStrategyResult = result;
    return MEM_POOLING_OK;
}

uint32_t MockRackRpcSendReturnInMigrateStrategySuccess(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData,
                                                       void* ctx, const UbseComRespHandler& handler)
{
    // 解析
    turbo::rmrs::MigrateStrategyResult* migrateStrategyResult = static_cast<turbo::rmrs::MigrateStrategyResult*>(ctx);
    turbo::rmrs::MigrateStrategyResult result;
    turbo::rmrs::VMMigrateOutParam vm_out = {10086, 512, 4};
    result.vmInfoList.push_back(vm_out);
    result.waitingTime = 60;
    *migrateStrategyResult = result;
    return MEM_POOLING_OK;
}

uint32_t MockRackRpcSendReturnInMigrateExecuteRpcSuccess(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData,
                                                         void* ctx, const UbseComRespHandler& handler)
{
    // 解析
    uint32_t* ret = static_cast<uint32_t*>(ctx);
    *ret = 0;
    return MEM_POOLING_OK;
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

MpResult RpcGetVmInfoImmediatelyForTest(const std::string& nodeId, std::vector<VmDomainInfo>& vmDomainInfos)
{
    VmDomainInfo vm1;
    vm1.metaData.nodeId = "node-001";
    vm1.metaData.hostName = "host1";
    vm1.metaData.uuid = "uuid-1234";
    vm1.metaData.name = "vm-1";
    vm1.metaData.maxMem = 8192; // 8GB
    vm1.metaData.pid = 10086;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 4096, 0, 1};
    vm1.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 2048, -1, 0};
    vm1.numaInfo[1] = vmNumaInfo2;
    vm1.metaData.state = "running";
    vm1.timestamp = time(nullptr);
    vmDomainInfos.push_back(vm1);
    return 0;
}

TEST_F(TestMemPoolBorrowModule, GetMigrateExecuteInfo_TestWithSuccess1)
{
    MOCKER_CPP(&migrate::MempoolMigrateExecute::RpcGetVmInfoImmediately,
               MpResult(*)(const std::string&, std::vector<VmDomainInfo>&))
        .stubs()
        .will(invoke(RpcGetVmInfoImmediatelyForTest));

    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList;
    turbo::rmrs::VMMigrateOutParam v1;
    v1.pid = 10086;
    v1.desNumaId = 4;
    v1.memSize = 512;
    vmInfoList.push_back(v1);
    std::set<turbo::rmrs::BorrowIdInfo> pidList;
    std::unordered_set<uint16_t> uniqueDesNumaIds;
    std::string node{"Node0"};
    MpResult ret = migrate::MempoolMigrateExecute::GetMigrateExecuteInfo(node, vmInfoList, pidList, uniqueDesNumaIds);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemPoolBorrowModule, GetMigrateExecuteInfo_TestFailed1)
{
    MOCKER_CPP(&migrate::MempoolMigrateExecute::RpcGetVmInfoImmediately,
               MpResult(*)(const std::string&, std::vector<VmDomainInfo>&))
        .stubs()
        .will(returnValue(1));

    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList;
    turbo::rmrs::VMMigrateOutParam v1;
    v1.pid = 10086;
    v1.desNumaId = 4;
    v1.memSize = 512;
    vmInfoList.push_back(v1);
    std::set<turbo::rmrs::BorrowIdInfo> pidList;
    std::unordered_set<uint16_t> uniqueDesNumaIds;
    std::string node{"Node0"};
    MpResult ret = migrate::MempoolMigrateExecute::GetMigrateExecuteInfo(node, vmInfoList, pidList, uniqueDesNumaIds);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemPoolBorrowModule, GetMigrateExecuteInfo_TestWithFailed2)
{
    MOCKER_CPP(&migrate::MempoolMigrateExecute::RpcGetVmInfoImmediately,
               MpResult(*)(const std::string&, std::vector<VmDomainInfo>&))
        .stubs()
        .will(returnValue(0));

    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList;
    turbo::rmrs::VMMigrateOutParam v1;
    v1.pid = 10086;
    v1.desNumaId = 4;
    v1.memSize = 512;
    vmInfoList.push_back(v1);
    vmInfoList.push_back(v1);
    std::set<turbo::rmrs::BorrowIdInfo> pidList;
    std::unordered_set<uint16_t> uniqueDesNumaIds;
    std::string node{"Node0"};
    MpResult ret = migrate::MempoolMigrateExecute::GetMigrateExecuteInfo(node, vmInfoList, pidList, uniqueDesNumaIds);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemPoolBorrowModule, ValidateBorrowParamSamePlane1)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(returnValue(1));
    SrcMemoryBorrowParam srcParam;
    std::vector<DestMemoryBorrowParam> destParams;
    MpResult ret = MempoolBorrowModule::Instance().ValidateBorrowParamSamePlane(srcParam, destParams);
    EXPECT_EQ(ret, 1);
}

uint32_t RackMemGetTopologyInfoMock(std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology)
{
    MemNodeData memNodeDataA;
    memNodeDataA.nodeId = "NodeA";
    memNodeDataA.hostname = "hostA";
    memNodeDataA.isRegisterRm = true;
    memNodeDataA.socket.socketId = "100";        // 只允许socketId=100
    memNodeDataA.socket.numas = {NumaData{"0"}}; // 只包含 numaId = 0
    nodeTopology["node0-36"].push_back(memNodeDataA);
    return 0;
}

TEST_F(TestMemPoolBorrowModule, ValidateBorrowParamSamePlane2)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(invoke(RackMemGetTopologyInfoMock));
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node0";
    srcParam.srcSocketId = 36;
    std::vector<DestMemoryBorrowParam> destParams;
    DestMemoryBorrowParam destParam;
    destParam.destSocketId = 100;
    destParams.push_back(destParam);
    MpResult ret = MempoolBorrowModule::Instance().ValidateBorrowParamSamePlane(srcParam, destParams);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemPoolBorrowModule, ValidateBorrowParamSamePlane3)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(invoke(RackMemGetTopologyInfoMock));
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node0";
    srcParam.srcSocketId = 36;
    std::vector<DestMemoryBorrowParam> destParams;
    DestMemoryBorrowParam destParam;
    destParam.destSocketId = 101;
    destParams.push_back(destParam);
    MpResult ret = MempoolBorrowModule::Instance().ValidateBorrowParamSamePlane(srcParam, destParams);
    EXPECT_EQ(ret, 1);
}

MpResult GetBorrowableListMock(MpParseGroupProviderConf* This, const std::string& curNid,
                               std::unordered_set<std::string>& borrowableNidSet)
{
    borrowableNidSet.insert("nodeB");
    return MEM_POOLING_OK;
}

TEST_F(TestMemPoolBorrowModule, ValidateDestNids_TestWithSuccess)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string& curNid,
                           std::unordered_set<std::string>& borrowableNidSet))
        .stubs()
        .will(invoke(GetBorrowableListMock));
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "nodeA";
    DestMemoryBorrowParam dstParam1;
    dstParam1.destNid = "nodeB";
    dstParam1.destNumaNum = 1;
    dstParam1.destNumaId.push_back(0);
    dstParam1.memSize.push_back(1024);
    std::vector<DestMemoryBorrowParam> destParams;
    destParams.push_back(dstParam1);
    MpResult ret = MempoolBorrowModule::Instance().ValidateDestNids(srcParam, destParams);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolBorrowModule, ValidateDestNids_TestWithFailed0)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string& curNid,
                           std::unordered_set<std::string>& borrowableNidSet))
        .stubs()
        .will(invoke(GetBorrowableListMock));
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "nodeA";
    DestMemoryBorrowParam dstParam1;
    dstParam1.destNid = "nodeB";
    dstParam1.destNumaNum = 2;
    dstParam1.destNumaId.push_back(0);
    dstParam1.memSize.push_back(1024);
    std::vector<DestMemoryBorrowParam> destParams;
    destParams.push_back(dstParam1);
    MpResult ret = MempoolBorrowModule::Instance().ValidateDestNids(srcParam, destParams);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ValidateDestNids_TestWithFailedNotEqualSize)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string& curNid,
                           std::unordered_set<std::string>& borrowableNidSet))
        .stubs()
        .will(invoke(GetBorrowableListMock));
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "nodeA";
    DestMemoryBorrowParam dstParam1;
    dstParam1.destNid = "nodeB";
    dstParam1.destNumaNum = 1;
    dstParam1.destNumaId.push_back(0);
    std::vector<DestMemoryBorrowParam> destParams;
    destParams.push_back(dstParam1);
    MpResult ret = MempoolBorrowModule::Instance().ValidateDestNids(srcParam, destParams);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ValidateDestNids_TestWithFailed1)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string& curNid,
                           std::unordered_set<std::string>& borrowableNidSet))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "nodeA";
    DestMemoryBorrowParam dstParam1;
    dstParam1.destNid = "nodeB";
    std::vector<DestMemoryBorrowParam> destParams;
    destParams.push_back(dstParam1);
    MpResult ret = MempoolBorrowModule::Instance().ValidateDestNids(srcParam, destParams);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ValidateDestNids_TestWithFailed2)
{
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string& curNid,
                           std::unordered_set<std::string>& borrowableNidSet))
        .stubs()
        .will(invoke(GetBorrowableListMock));
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "nodeA";
    DestMemoryBorrowParam dstParam1;
    dstParam1.destNid = "nodeC";
    std::vector<DestMemoryBorrowParam> destParams;
    destParams.push_back(dstParam1);
    MpResult ret = MempoolBorrowModule::Instance().ValidateDestNids(srcParam, destParams);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowExecute_TestWithSuccess)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 构造 DestMemoryBorrowParam
    std::vector<DestMemoryBorrowParam> destParams;

    // 第一个目的地参数
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1048576); // 假设 8MB 内存

    // 第二个目的地参数
    DestMemoryBorrowParam destParam2;
    destParam2.destNid = "Node2";
    destParam2.destSocketId = 1;
    destParam2.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam2.memSize.push_back(1048576); // 假设 16MB 内存

    // 将目的地参数添加到 vector 中
    destParams.push_back(destParam1);
    destParams.push_back(destParam2);
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowParamSamePlane,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::ValidateDestNids,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowExecuteParam,
               MpResult(*)(const DestMemoryBorrowParam& destParam, MemMallocAttr& memAttr,
                           const MemBorrowExecuteResult& borrowExecuteResult, uint64_t& totalSize))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MemBorrowExecutor::MemBorrow,
               MpResult(*)(const std::string& attachNode, RackCreateResourceWaterBorrowAttr& attr, std::string& name,
                           int16_t& presentNumaId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParams, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemPoolBorrowModule, ExecuteSingleBorrowShouldReturnOkWhenAllMethodWorks)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 目的地参数
    DestMemoryBorrowParam destParam;
    destParam.destNid = "Node1";
    destParam.destSocketId = 0;
    destParam.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam.memSize.push_back(1048576); // 假设 8MB 内存

    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowExecuteParam,
               MpResult(*)(const DestMemoryBorrowParam& destParam, MemMallocAttr& memAttr,
                           const MemBorrowExecuteResult& borrowExecuteResult, uint64_t& totalSize))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MemBorrowExecutor::MemBorrow,
               MpResult(*)(const std::string& attachNode, RackCreateResourceWaterBorrowAttr& attr, std::string& name,
                           int16_t& presentNumaId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().ExecuteSingleBorrow(destParam, srcParam, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemPoolBorrowModule, ExecuteSingleBorrowShouldReturnErrorWhenSizeNE)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 目的地参数
    DestMemoryBorrowParam destParam;
    destParam.destNid = "Node1";
    destParam.destSocketId = 0;
    destParam.destNumaId.push_back(0);
    destParam.destNumaId.push_back(1);
    destParam.memSize.push_back(1048576); // 假设 8MB 内存

    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowExecuteParam,
               MpResult(*)(const DestMemoryBorrowParam& destParam, MemMallocAttr& memAttr,
                           const MemBorrowExecuteResult& borrowExecuteResult, uint64_t& totalSize))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MemBorrowExecutor::MemBorrow,
               MpResult(*)(const std::string& attachNode, RackCreateResourceWaterBorrowAttr& attr, std::string& name,
                           int16_t& presentNumaId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().ExecuteSingleBorrow(destParam, srcParam, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ExecuteSingleBorrowShouldReturnErrorWhenValidateBorrowExecuteParamFailed)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 目的地参数
    DestMemoryBorrowParam destParam;
    destParam.destNid = "Node1";
    destParam.destSocketId = 0;
    destParam.destNumaId.push_back(0);
    destParam.memSize.push_back(1048576); // 假设 8MB 内存

    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowExecuteParam,
               MpResult(*)(const DestMemoryBorrowParam& destParam, MemMallocAttr& memAttr,
                           const MemBorrowExecuteResult& borrowExecuteResult, uint64_t& totalSize))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&MemBorrowExecutor::MemBorrow,
               MpResult(*)(const std::string& attachNode, RackCreateResourceWaterBorrowAttr& attr, std::string& name,
                           int16_t& presentNumaId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().ExecuteSingleBorrow(destParam, srcParam, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ExecuteSingleBorrowShouldReturnErrorWhenMemBorrowFailed)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 目的地参数
    DestMemoryBorrowParam destParam;
    destParam.destNid = "Node1";
    destParam.destSocketId = 0;
    destParam.destNumaId.push_back(0);
    destParam.memSize.push_back(1048576); // 假设 8MB 内存

    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowExecuteParam,
               MpResult(*)(const DestMemoryBorrowParam& destParam, MemMallocAttr& memAttr,
                           const MemBorrowExecuteResult& borrowExecuteResult, uint64_t& totalSize))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MemBorrowExecutor::MemBorrow,
               MpResult(*)(const std::string& attachNode, RackCreateResourceWaterBorrowAttr& attr, std::string& name,
                           int16_t& presentNumaId))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowFailedRollback,
               MpResult(*)(const MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().ExecuteSingleBorrow(destParam, srcParam, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowExecute_TestWithBadDestParamSize)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 构造 DestMemoryBorrowParam
    std::vector<DestMemoryBorrowParam> destParams;

    // 第一个目的地参数
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1048576); // 假设 8MB 内存

    // 第二个目的地参数
    DestMemoryBorrowParam destParam2;
    destParam2.destNid = "Node2";
    destParam2.destSocketId = 1;
    destParam2.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam2.destNumaId.push_back(1);    // 假设 NUMA ID 为 0
    destParam2.memSize.push_back(1048576); // 假设 16MB 内存

    // 将目的地参数添加到 vector 中
    destParams.push_back(destParam1);
    destParams.push_back(destParam2);

    MOCKER_CPP(&MemBorrowExecutor::MemBorrow,
               MpResult(*)(const std::string& attachNode, RackCreateResourceWaterBorrowAttr& attr, std::string& name,
                           int16_t& presentNumaId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParams, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowExecute_TestWithBadDestParaMemSize)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 构造 DestMemoryBorrowParam
    std::vector<DestMemoryBorrowParam> destParams;

    // 第一个目的地参数
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(0); // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1024); // 假设 8MB 内存

    // 第二个目的地参数
    DestMemoryBorrowParam destParam2;
    destParam2.destNid = "Node2";
    destParam2.destSocketId = 0;
    destParam2.destNumaId.push_back(0); // 假设 NUMA ID 为 0
    destParam2.memSize.push_back(1024); // 假设 16MB 内存

    // 将目的地参数添加到 vector 中
    destParams.push_back(destParam1);
    destParams.push_back(destParam2);

    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParams, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowExecute_TestWithBadDestParamNumaId)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 构造 DestMemoryBorrowParam
    std::vector<DestMemoryBorrowParam> destParams;

    // 第一个目的地参数
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(-1);   // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1048576); // 假设 8MB 内存

    // 第二个目的地参数
    DestMemoryBorrowParam destParam2;
    destParam2.destNid = "Node2";
    destParam2.destSocketId = 0;
    destParam2.destNumaId.push_back(-1);   // 假设 NUMA ID 为 0
    destParam2.memSize.push_back(1048576); // 假设 16MB 内存

    // 将目的地参数添加到 vector 中
    destParams.push_back(destParam1);
    destParams.push_back(destParam2);

    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParams, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowExecute_TestWithFailed)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 构造 DestMemoryBorrowParam
    std::vector<DestMemoryBorrowParam> destParams;

    // 第一个目的地参数
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1048576); // 假设 8MB 内存

    // 第二个目的地参数
    DestMemoryBorrowParam destParam2;
    destParam2.destNid = "Node2";
    destParam2.destSocketId = 1;
    destParam2.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam2.memSize.push_back(1048576); // 假设 16MB 内存

    // 将目的地参数添加到 vector 中
    destParams.push_back(destParam1);
    destParams.push_back(destParam2);

    // MemBorrowExecutor::Instance().MemBorrow
    MOCKER_CPP(&MemBorrowExecutor::MemBorrow,
               MpResult(*)(const std::string& attachNode, RackCreateResourceWaterBorrowAttr& attr, std::string& name,
                           int16_t& presentNumaId))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParams, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowExecute_TestWithFailed1)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = -1;

    // 构造 DestMemoryBorrowParam
    std::vector<DestMemoryBorrowParam> destParams;
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParams, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowExecute_TestWithFailed2)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 构造 DestMemoryBorrowParam
    std::vector<DestMemoryBorrowParam> destParams;

    // 第一个目的地参数
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1048576); // 假设 8MB 内存

    // 第二个目的地参数
    DestMemoryBorrowParam destParam2;
    destParam2.destNid = "Node2";
    destParam2.destSocketId = 1;
    destParam2.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam2.memSize.push_back(1048576); // 假设 16MB 内存

    // 将目的地参数添加到 vector 中
    destParams.push_back(destParam1);
    destParams.push_back(destParam2);
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowParamSamePlane,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::ExecuteSingleBorrow,
               MpResult(*)(const DestMemoryBorrowParam& destParam, const SrcMemoryBorrowParam& srcParam,
                           MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParams, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowExecute_TestWithFailed3)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 构造 DestMemoryBorrowParam
    std::vector<DestMemoryBorrowParam> destParams;

    // 第一个目的地参数
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1048576); // 假设 8MB 内存

    // 第二个目的地参数
    DestMemoryBorrowParam destParam2;
    destParam2.destNid = "Node2";
    destParam2.destSocketId = 1;
    destParam2.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam2.memSize.push_back(1048576); // 假设 16MB 内存

    // 将目的地参数添加到 vector 中
    destParams.push_back(destParam1);
    destParams.push_back(destParam2);
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowParamSamePlane,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::ValidateDestNids,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::ExecuteSingleBorrow,
               MpResult(*)(const DestMemoryBorrowParam& destParam, const SrcMemoryBorrowParam& srcParam,
                           MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowIdsCompleted::Query,
               MpResult(*)(BorrowIdsCompleted * This, std::vector<std::string> & borrowIdsCompletedList))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParams, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

MpResult GetBorrowIdsCompletedListMocks(BorrowIdsCompleted* This, std::vector<std::string>& borrowIdsCompletedList)
{
    std::string borrowId = "test1";
    borrowIdsCompletedList.push_back(borrowId);
    return 0;
}

TEST_F(TestMemPoolBorrowModule, MemBorrowExecute_TestWithFailed4)
{
    // 构造 SrcMemoryBorrowParam
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    // 构造 DestMemoryBorrowParam
    std::vector<DestMemoryBorrowParam> destParams;

    // 第一个目的地参数
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1048576); // 假设 8MB 内存

    // 第二个目的地参数
    DestMemoryBorrowParam destParam2;
    destParam2.destNid = "Node2";
    destParam2.destSocketId = 1;
    destParam2.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam2.memSize.push_back(1048576); // 假设 16MB 内存

    // 将目的地参数添加到 vector 中
    destParams.push_back(destParam1);
    destParams.push_back(destParam2);
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowParamSamePlane,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::ValidateDestNids,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<DestMemoryBorrowParam>& destParams))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&MempoolBorrowModule::ExecuteSingleBorrow,
               MpResult(*)(const DestMemoryBorrowParam& destParam, const SrcMemoryBorrowParam& srcParam,
                           MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowIdsCompleted::Query,
               MpResult(*)(BorrowIdsCompleted * This, std::vector<std::string> & borrowIdsCompletedList))
        .stubs()
        .will(invoke(GetBorrowIdsCompletedListMocks));
    MOCKER_CPP(&BorrowIdsCompleted::Remove, MpResult(*)(const std::string borrowId))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MemBorrowExecuteResult borrowExecuteResult;
    MpResult ret = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParams, borrowExecuteResult);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolBorrowModule, MemBackExecute_TestWithBadReq)
{
    std::string nodeId = "";
    int16_t numaId = -1;
    MpResult ret = MempoolBorrowModule::Instance().MemBackExecute(nodeId, numaId);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemPoolBorrowModule, MemBackExecute_TestWithSuccess)
{
    std::string nodeId = "Node0";
    uint16_t numaId = 0;

    MemManager::Instance().nodeMemMap.clear();
    // 初始化一个 NodeMemInfo 实例
    NodeMemInfo node1;
    node1.totalReservedMem = 1048576;
    node1.totalBorrowableMem = 1048576;
    node1.totalLentMem = 0;
    node1.totalBorrowedMem = 0;

    // 添加 NumaMemInfo 实例到 localnumaMemInfo
    NumaMemInfo numa1;
    numa1.numaId = 0;
    numa1.reservedMem = 1048576;
    numa1.lentMem = 0;
    numa1.borrowableMem = 1048576;
    numa1.borrowedMem = 0;
    node1.localnumaMemInfo.push_back(numa1);

    // 将 node1 添加到 nodeMemMap
    MemManager::Instance().nodeMemMap["Node0"] = node1;

    MOCKER_CPP(&MemManager::GetNodeMemInfo, MpResult(*)(const std::string& nodeId, NodeMemInfo& outInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowRecordHelper::GetBorrowIdByNumaId, MpResult(*)(const uint16_t numaId, const std::string nodeId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    // MemFree
    MOCKER_CPP(&MemBorrowExecutor::MemFree, MpResult(*)(std::string nodeId)).stubs().will(returnValue(MEM_POOLING_OK));
    MpResult ret = MempoolBorrowModule::Instance().MemBackExecute(nodeId, numaId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolBorrowModule, MemBackExecute_TestWithGetNodeMemInfoFailed)
{
    std::string nodeId = "Node0";
    uint16_t numaId = 0;

    MOCKER_CPP(&MemManager::GetNodeMemInfo, MpResult(*)(const std::string& nodeId, NodeMemInfo& outInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&BorrowRecordHelper::GetBorrowIdByNumaId, MpResult(*)(const uint16_t numaId, const std::string nodeId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult ret = MempoolBorrowModule::Instance().MemBackExecute(nodeId, numaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBackExecute_TestWithMemFreeFailed)
{
    std::string nodeId = "Node0";
    uint16_t numaId = 0;

    MemManager::Instance().nodeMemMap.clear();
    // 初始化一个 NodeMemInfo 实例
    NodeMemInfo node1;
    node1.totalReservedMem = 1048576;
    node1.totalBorrowableMem = 1048576;
    node1.totalLentMem = 0;
    node1.totalBorrowedMem = 0;

    // 添加 NumaMemInfo 实例到 localnumaMemInfo
    NumaMemInfo numa1;
    numa1.numaId = 0;
    numa1.reservedMem = 1048576;
    numa1.lentMem = 0;
    numa1.borrowableMem = 1048576;
    numa1.borrowedMem = 0;
    node1.localnumaMemInfo.push_back(numa1);

    // 将 node1 添加到 nodeMemMap
    MemManager::Instance().nodeMemMap["Node0"] = node1;

    // MemFree
    MOCKER_CPP(&MemBorrowExecutor::MemFree, MpResult(*)(std::string nodeId))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    // MemManager::Instance().RemoveName
    MpResult ret = MempoolBorrowModule::Instance().MemBackExecute(nodeId, numaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBackExecute_TestWithRemoveNameFailed)
{
    std::string nodeId = "Node0";
    uint16_t numaId = 0;

    MemManager::Instance().nodeMemMap.clear();
    // 初始化一个 NodeMemInfo 实例
    NodeMemInfo node1;
    node1.totalReservedMem = 1048576;
    node1.totalBorrowableMem = 1048576;
    node1.totalLentMem = 0;
    node1.totalBorrowedMem = 0;

    // 添加 NumaMemInfo 实例到 localnumaMemInfo
    NumaMemInfo numa1;
    numa1.numaId = 0;
    numa1.reservedMem = 1048576;
    numa1.lentMem = 0;
    numa1.borrowableMem = 1048576;
    numa1.borrowedMem = 0;
    node1.localnumaMemInfo.push_back(numa1);

    // 将 node1 添加到 nodeMemMap
    MemManager::Instance().nodeMemMap["Node0"] = node1;

    // MemFree
    MOCKER_CPP(&MemBorrowExecutor::MemFree, MpResult(*)(std::string nodeId)).stubs().will(returnValue(MEM_POOLING_OK));
    MpResult ret = MempoolBorrowModule::Instance().MemBackExecute(nodeId, numaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, AddMemoryParamsToResult_TestWithOk)
{
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1048576); // 假设 8MB 内存

    uint32_t haveFourGbNum = 1;
    uint32_t moreBorrowNum = 1;
    MemBorrowStrategyResult borrowStrategyResult;
    MempoolBorrowModule::Instance().AddMemoryParamsToResult(haveFourGbNum, moreBorrowNum, destParam1,
                                                            borrowStrategyResult);
}

TEST_F(TestMemPoolBorrowModule, GetSocketInfo_TestFailed)
{
    MOCKER_CPP(&MemManager::GetSocketId, MpResult(*)(const std::string& nodeId, const int& numaId, int& socketId))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    DestMemoryBorrowParam destParam1;
    destParam1.destNid = "Node1";
    destParam1.destSocketId = 0;
    destParam1.destNumaId.push_back(0);    // 假设 NUMA ID 为 0
    destParam1.memSize.push_back(1048576); // 假设 8MB 内存
    MpResult ret = MempoolBorrowModule::Instance().GetSocketInfo(destParam1, 0);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

MpResult GetMemoryInfoForUnitTest(MempoolBorrowModule*, std::unordered_map<std::string, NodeMemInfo>& nodeMemMap,
                                  const SrcMemoryBorrowParam& srcParam, std::vector<std::string>& antiNodeMemVec)
{
    NodeMemInfo node1;
    node1.totalReservedMem = 1000;
    node1.totalBorrowableMem = 500;
    node1.totalLentMem = 200;
    node1.totalBorrowedMem = 300;
    node1.blockSize = 128;

    NumaMemInfo numa1;
    numa1.numaId = 1;
    numa1.reservedMem = 100;
    numa1.borrowableMem = 30;
    numa1.borrowedMem = 20;
    node1.localnumaMemInfo.push_back(numa1);
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap1 = {{"Node1", node1}};
    nodeMemMap = nodeMemMap1;
    return 0;
}

void FilterAndSortNodesForUT(MempoolBorrowModule*, std::unordered_map<std::string, NodeMemInfo>& nodeMemMap,
                             std::string srcNid, const std::vector<std::string>& antiNodeMemVec,
                             std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec)
{
    // do nothing
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategyMultipleTest)
{
    MOCKER_CPP(&MempoolBorrowModule::GetMemoryInfo,
               MpResult(*)(MempoolBorrowModule*, std::unordered_map<std::string, NodeMemInfo>&,
                           const SrcMemoryBorrowParam&, std::vector<std::string>&))
        .stubs()
        .will(invoke(GetMemoryInfoForUnitTest));

    MOCKER_CPP(&MempoolBorrowModule::FilterAndSortNodes,
               void (*)(MempoolBorrowModule*, std::unordered_map<std::string, NodeMemInfo>&, std::string,
                        const std::vector<std::string>&, std::vector<std::pair<std::string, NodeMemInfo>>&))
        .stubs()
        .will(invoke(FilterAndSortNodesForUT));

    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    uint64_t borrowSize = 1048576;
    std::vector<uint64_t> borrowSizes;
    borrowSizes.push_back(borrowSize);

    std::string destPreNid = "Node1";
    MemBorrowStrategyMultiResult borrowStrategyResult;

    MpResult ret = MempoolBorrowModule::Instance().MemBorrowStrategyMultiple(srcParam, borrowSizes, destPreNid,
                                                                             borrowStrategyResult);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::reset();
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategyMultipleTest2)
{
    MOCKER_CPP(&MempoolBorrowModule::GetMemoryInfo,
               MpResult(*)(MempoolBorrowModule*, std::unordered_map<std::string, NodeMemInfo>&,
                           const SrcMemoryBorrowParam&, std::vector<std::string>&))
        .stubs()
        .will(invoke(GetMemoryInfoForUnitTest));

    MOCKER_CPP(&MempoolBorrowModule::FilterAndSortNodes,
               void (*)(MempoolBorrowModule*, std::unordered_map<std::string, NodeMemInfo>&, std::string,
                        const std::vector<std::string>&, std::vector<std::pair<std::string, NodeMemInfo>>&))
        .stubs()
        .will(invoke(FilterAndSortNodesForUT));

    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    uint64_t borrowSize = 1048576;
    std::vector<uint64_t> borrowSizes;
    borrowSizes.push_back(borrowSize);

    std::string destPreNid = "Node0";
    MemBorrowStrategyMultiResult borrowStrategyResult;

    MpResult ret = MempoolBorrowModule::Instance().MemBorrowStrategyMultiple(srcParam, borrowSizes, destPreNid,
                                                                             borrowStrategyResult);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::reset();
}

void sortNumaMemInfo(NodeMemInfo& nodeMemInfo);

TEST_F(TestMemPoolBorrowModule, sortNumaMemInfo1)
{
    NodeMemInfo nodeMemInfo;
    NumaMemInfo info1;
    info1.borrowableMem = 3;
    nodeMemInfo.localnumaMemInfo.push_back(info1);
    NumaMemInfo info2;
    info1.borrowableMem = 2;
    nodeMemInfo.localnumaMemInfo.push_back(info2);
    sortNumaMemInfo(nodeMemInfo);
}

TEST_F(TestMemPoolBorrowModule, GetMemoryInfo1)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    SrcMemoryBorrowParam srcParam;
    std::vector<std::string> antiNodeMemVec;
    MOCKER_CPP(&MemManager::GetNodeMemMap, MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & outMap))
        .stubs()
        .will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().GetMemoryInfo(nodeMemMap, srcParam, antiNodeMemVec);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, GetMemoryInfo2)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    SrcMemoryBorrowParam srcParam;
    std::vector<std::string> antiNodeMemVec;
    MOCKER_CPP(&MemManager::GetNodeMemMap, MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & outMap))
        .stubs()
        .will(returnValue(0));
    auto res = MempoolBorrowModule::Instance().GetMemoryInfo(nodeMemMap, srcParam, antiNodeMemVec);
    EXPECT_EQ(res, 0);
}

void MempoolBorrowModuleUpdateNodeMemInfoWithNuma(MempoolBorrowModule* This,
                                                  std::unordered_map<std::string, NodeMemInfo>& nodeMemMap)
{
    NodeMemInfo node0;
    node0.timestamp = time(nullptr);
    node0.totalReservedMem = 8192;
    node0.totalBorrowableMem = 4096;
    node0.totalLentMem = 2048;
    node0.totalBorrowedMem = 1024;

    node0.localnumaMemInfo.push_back(NumaMemInfo{.numaId = 0,
                                                 .socketId = 0,
                                                 .reservedMem = 4096,
                                                 .lentMem = 1024,
                                                 .borrowableMem = 2048,
                                                 .borrowedMem = 512,
                                                 .memFree = 8192,
                                                 .vmMemFree = 4096});

    node0.localnumaMemInfo.push_back(NumaMemInfo{.numaId = 1,
                                                 .socketId = 0,
                                                 .reservedMem = 4096,
                                                 .lentMem = 1024,
                                                 .borrowableMem = 2048,
                                                 .borrowedMem = 512,
                                                 .memFree = 8192,
                                                 .vmMemFree = 4096});

    nodeMemMap["node-0"] = node0;
}

TEST_F(TestMemPoolBorrowModule, GetMemoryInfo3)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    SrcMemoryBorrowParam srcParam;
    std::vector<std::string> antiNodeMemVec;
    MOCKER_CPP(&MemManager::GetNodeMemMap, MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & outMap))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&AntiNode::Query, MpResult(*)(const std::string& srcNid, std::vector<std::string>& antiNodeMemVec))
        .stubs()
        .will(returnValue(1))
        .then(returnValue(0));
    auto res = MempoolBorrowModule::Instance().GetMemoryInfo(nodeMemMap, srcParam, antiNodeMemVec);
    EXPECT_EQ(res, 1);
    res = MempoolBorrowModule::Instance().GetMemoryInfo(nodeMemMap, srcParam, antiNodeMemVec);
    EXPECT_EQ(res, 0);
}

TEST_F(TestMemPoolBorrowModule, GetMemoryInfo4)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    SrcMemoryBorrowParam srcParam;
    std::vector<std::string> antiNodeMemVec;
    MOCKER_CPP(&MemManager::GetNodeMemMap, MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & outMap))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::UpdateNodeMemInfoWithNuma,
               void (*)(MempoolBorrowModule * This, std::unordered_map<std::string, NodeMemInfo> & nodeMemMap))
        .stubs()
        .will(invoke(MempoolBorrowModuleUpdateNodeMemInfoWithNuma));
    MOCKER_CPP(&AntiNode::Query, MpResult(*)(const std::string& srcNid, std::vector<std::string>& antiNodeMemVec))
        .stubs()
        .will(returnValue(1))
        .then(returnValue(0));
    auto res = MempoolBorrowModule::Instance().GetMemoryInfo(nodeMemMap, srcParam, antiNodeMemVec);
    EXPECT_EQ(res, 1);
    res = MempoolBorrowModule::Instance().GetMemoryInfo(nodeMemMap, srcParam, antiNodeMemVec);
    EXPECT_EQ(res, 0);
}

TEST_F(TestMemPoolBorrowModule, FilterNodesBySocketProximity1)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    std::vector<MemNodeData> foundNodeData;
    std::unordered_map<std::string, std::unordered_set<uint16_t>> nodeIdToNumaIdSetMap;

    // 构造 NodeA，包含两个 NUMA 节点：numaId=0,1，socketId=100,101
    NodeMemInfo nodeAInfo;
    nodeAInfo.localnumaMemInfo = {NumaMemInfo{0, 100, 1000, 200, 300, 100, 400, 50},
                                  NumaMemInfo{1, 101, 2000, 400, 600, 200, 800, 100}};
    nodeMemMap["NodeA"] = nodeAInfo;

    // 构造 NodeB，包含一个 NUMA 节点：numaId=0，socketId=200（不在foundNodeData中，后面应被过滤掉）
    NodeMemInfo nodeBInfo;
    nodeBInfo.localnumaMemInfo = {NumaMemInfo{0, 200, 1500, 300, 400, 150, 500, 70}};
    nodeMemMap["NodeB"] = nodeBInfo;

    // 构造 foundNodeData，只包含 NodeA 的 socketId=100
    MemNodeData memNodeDataA;
    memNodeDataA.nodeId = "NodeA";
    memNodeDataA.hostname = "hostA";
    memNodeDataA.isRegisterRm = true;
    memNodeDataA.socket.socketId = "100";        // 只允许socketId=100
    memNodeDataA.socket.numas = {NumaData{"0"}}; // 只包含 numaId = 0
    foundNodeData.push_back(memNodeDataA);

    MempoolBorrowModule::Instance().FilterNodesBySocketProximity(nodeMemMap, foundNodeData, nodeIdToNumaIdSetMap);
}

TEST_F(TestMemPoolBorrowModule, FilterAndSortNodes1)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.totalBorrowableMem = 1024;
    nodeMemMap["node0"] = nodeMemInfo1;
    std::string srcNid;
    std::vector<std::string> antiNodeMemVec = {"node1"};
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(returnValue(1));
    MempoolBorrowModule::Instance().FilterAndSortNodes(nodeMemMap, srcNid, antiNodeMemVec, nodeVec);
}

TEST_F(TestMemPoolBorrowModule, FilterAndSortNodes_WhenFilterBorrowableNodesFailed)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.totalBorrowableMem = 1024;
    nodeMemMap["node0"] = nodeMemInfo1;
    std::string srcNid;
    std::vector<std::string> antiNodeMemVec = {"node1"};
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    MOCKER_CPP(&MempoolBorrowModule::FilterBorrowableNodes,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, NodeMemInfo>& nodeMemMap))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MempoolBorrowModule::Instance().FilterAndSortNodes(nodeMemMap, srcNid, antiNodeMemVec, nodeVec);
}

TEST_F(TestMemPoolBorrowModule, FilterAndSortSockets1)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.totalBorrowableMem = 1024;
    nodeMemMap["node0"] = nodeMemInfo1;
    SrcMemoryBorrowParam srcParam;
    std::vector<std::string> antiNodeMemVec = {"node1"};
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    MOCKER_CPP(&MempoolBorrowModule::FilterBorrowableNodes,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, NodeMemInfo>& nodeMemMap))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(returnValue(1));
    MempoolBorrowModule::Instance().FilterAndSortSockets(nodeMemMap, srcParam, antiNodeMemVec, nodeTopology, nodeVec);
}

TEST_F(TestMemPoolBorrowModule, FilterBorrowableNodesSuccess)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    nodeMemMap["nodeA"] = NodeMemInfo{};
    nodeMemMap["nodeB"] = NodeMemInfo{};
    nodeMemMap["nodeC"] = NodeMemInfo{};
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "nodeA";
    std::vector<std::string> antiNodeMemVec = {"nodeC"};
    MOCKER_CPP(&MpParseGroupProviderConf::GetBorrowableList,
               MpResult(*)(MpParseGroupProviderConf*, const std::string& curNid,
                           std::unordered_set<std::string>& borrowableNidSet))
        .stubs()
        .will(invoke(GetBorrowableListMock));
    auto ret = MempoolBorrowModule::Instance().FilterBorrowableNodes(srcParam, antiNodeMemVec, nodeMemMap);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

MpResult CollectBorrowRecordsAllMock(BorrowRecordHelper* This, std::vector<BorrowRecord>& borrowRecords)
{
    BorrowRecord borrowRecord;
    borrowRecord.borrowNode = "node1";
    borrowRecords.push_back(borrowRecord);
    return MEM_POOLING_OK;
}

TEST_F(TestMemPoolBorrowModule, FilterAndSortSockets2)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.totalBorrowableMem = 1024;
    nodeMemMap["node0"] = nodeMemInfo1;
    SrcMemoryBorrowParam srcParam;
    std::vector<std::string> antiNodeMemVec = {"node1"};
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    MOCKER_CPP(&MempoolBorrowModule::FilterBorrowableNodes,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, NodeMemInfo>& nodeMemMap))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsAllMock));
    MpResult ret = MempoolBorrowModule::Instance().FilterAndSortSockets(nodeMemMap, srcParam, antiNodeMemVec,
                                                                        nodeTopology, nodeVec);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemPoolBorrowModule, FilterAndSortSockets3)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.totalBorrowableMem = 1024;
    nodeMemMap["node0"] = nodeMemInfo1;
    SrcMemoryBorrowParam srcParam;
    std::vector<std::string> antiNodeMemVec = {"node1"};
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    MemNodeData memNodeDataA;
    memNodeDataA.nodeId = "NodeA";
    memNodeDataA.hostname = "hostA";
    memNodeDataA.isRegisterRm = true;
    memNodeDataA.socket.socketId = "100";        // 只允许socketId=100
    memNodeDataA.socket.numas = {NumaData{"0"}}; // 只包含 numaId = 0
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    nodeTopology["node0"].push_back(memNodeDataA);
    MOCKER_CPP(&MempoolBorrowModule::FilterBorrowableNodes,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, NodeMemInfo>& nodeMemMap))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsAllMock));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(const std::string&, NodeMemoryInfoWithReservedMem&))
        .stubs()
        .will(returnValue(1));
    MpResult ret = MempoolBorrowModule::Instance().FilterAndSortSockets(nodeMemMap, srcParam, antiNodeMemVec,
                                                                        nodeTopology, nodeVec);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemPoolBorrowModule, FilterAndSortSockets4)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.totalBorrowableMem = 1024;
    nodeMemMap["node0"] = nodeMemInfo1;
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node0";
    srcParam.srcSocketId = 36;
    std::vector<std::string> antiNodeMemVec = {"node1"};
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    MemNodeData memNodeDataA;
    memNodeDataA.nodeId = "NodeA";
    memNodeDataA.hostname = "hostA";
    memNodeDataA.isRegisterRm = true;
    memNodeDataA.socket.socketId = "100";        // 只允许socketId=100
    memNodeDataA.socket.numas = {NumaData{"0"}}; // 只包含 numaId = 0
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    nodeTopology["node0-36"].push_back(memNodeDataA);
    MOCKER_CPP(&MempoolBorrowModule::FilterBorrowableNodes,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, NodeMemInfo>& nodeMemMap))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsAllMock));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(const std::string&, NodeMemoryInfoWithReservedMem&))
        .stubs()
        .will(returnValue(1));
    MpResult ret = MempoolBorrowModule::Instance().FilterAndSortSockets(nodeMemMap, srcParam, antiNodeMemVec,
                                                                        nodeTopology, nodeVec);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemPoolBorrowModule, FilterAndSortSockets5)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.totalBorrowableMem = 1024;
    nodeMemMap["node0"] = nodeMemInfo1;
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node0";
    srcParam.srcSocketId = 36;
    std::vector<std::string> antiNodeMemVec = {"node1"};
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    MemNodeData memNodeDataA;
    memNodeDataA.nodeId = "NodeA";
    memNodeDataA.hostname = "hostA";
    memNodeDataA.isRegisterRm = true;
    memNodeDataA.socket.socketId = "100";        // 只允许socketId=100
    memNodeDataA.socket.numas = {NumaData{"0"}}; // 只包含 numaId = 0
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    nodeTopology["node0-36"].push_back(memNodeDataA);
    MOCKER_CPP(&MempoolBorrowModule::FilterBorrowableNodes,
               MpResult(*)(const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, NodeMemInfo>& nodeMemMap))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsAllMock));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(const std::string&, NodeMemoryInfoWithReservedMem&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult ret = MempoolBorrowModule::Instance().FilterAndSortSockets(nodeMemMap, srcParam, antiNodeMemVec,
                                                                        nodeTopology, nodeVec);
    EXPECT_EQ(ret, 0);
}

MpResult CollectBorrowableInfoMock(BorrowRecordHelper* This, const std::string& nodeId,
                                   NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem)
{
    // 填充基础字段（可选）
    nodeMemoryInfoWithReservedMem.timestamp = std::time(nullptr);
    nodeMemoryInfoWithReservedMem.nodeId = "NodeA";
    nodeMemoryInfoWithReservedMem.totalMemory = 65536; // 64 GB
    nodeMemoryInfoWithReservedMem.usedMemory = 32768;  // 32 GB
    nodeMemoryInfoWithReservedMem.freeMemory = 16384;  // 16 GB
    nodeMemoryInfoWithReservedMem.lentMemory = 4096;   // 4 GB
    nodeMemoryInfoWithReservedMem.reservedMem = 2048;  // 2 GB
    nodeMemoryInfoWithReservedMem.sharedMem = 1024;    // 1 GB
    nodeMemoryInfoWithReservedMem.socketId = 0;
    nodeMemoryInfoWithReservedMem.canBorrowMem = 4096;
}

TEST_F(TestMemPoolBorrowModule, FilterAndSortSockets6)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.totalBorrowableMem = 1024;
    nodeMemMap["node0"] = nodeMemInfo1;
    SrcMemoryBorrowParam srcParam;
    std::vector<std::string> antiNodeMemVec = {"node1"};
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    MemNodeData memNodeDataA;
    memNodeDataA.nodeId = "NodeA";
    memNodeDataA.hostname = "hostA";
    memNodeDataA.isRegisterRm = true;
    memNodeDataA.socket.socketId = "100";        // 只允许socketId=100
    memNodeDataA.socket.numas = {NumaData{"0"}}; // 只包含 numaId = 0
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    nodeTopology["node0"].push_back(memNodeDataA);
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsAllMock));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(BorrowRecordHelper * This, const std::string&, NodeMemoryInfoWithReservedMem&))
        .stubs()
        .will(invoke(CollectBorrowableInfoMock));
    MpResult ret = MempoolBorrowModule::Instance().FilterAndSortSockets(nodeMemMap, srcParam, antiNodeMemVec,
                                                                        nodeTopology, nodeVec);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemPoolBorrowModule, FilterAndSortSockets7)
{
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.totalBorrowableMem = 1024;
    nodeMemMap["node0"] = nodeMemInfo1;
    SrcMemoryBorrowParam srcParam;
    std::vector<std::string> antiNodeMemVec = {"node1"};
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    MemNodeData memNodeDataA;
    memNodeDataA.nodeId = "NodeA";
    memNodeDataA.hostname = "hostA";
    memNodeDataA.isRegisterRm = true;
    memNodeDataA.socket.socketId = "100";        // 只允许socketId=100
    memNodeDataA.socket.numas = {NumaData{"0"}}; // 只包含 numaId = 0
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    nodeTopology["node0"].push_back(memNodeDataA);
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsAllMock));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo,
               MpResult(*)(BorrowRecordHelper * This, const std::string&, NodeMemoryInfoWithReservedMem&))
        .stubs()
        .will(invoke(CollectBorrowableInfoMock));
    MpConfiguration::GetInstance().mustSamePlane = false;
    MpResult ret = MempoolBorrowModule::Instance().FilterAndSortSockets(nodeMemMap, srcParam, antiNodeMemVec,
                                                                        nodeTopology, nodeVec);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemPoolBorrowModule, ProcessNodeMemBorrow1)
{
    NumaMemInfo numaMemInfo;
    numaMemInfo.numaId = 1;
    numaMemInfo.borrowableMem = 1;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.localnumaMemInfo.push_back(numaMemInfo);
    std::pair<std::string, NodeMemInfo> node = {"node0", nodeMemInfo1};
    uint32_t needBorrowNum;
    MemBorrowStrategyResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::GetSocketInfo, MpResult(*)(DestMemoryBorrowParam & tempParam, uint32_t numaId))
        .stubs()
        .will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().ProcessNodeMemBorrow(node, needBorrowNum, borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, ProcessNodeMemBorrow2)
{
    NumaMemInfo numaMemInfo;
    numaMemInfo.numaId = 1;
    numaMemInfo.borrowableMem = 128 * 1024 * 1024;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.localnumaMemInfo.push_back(numaMemInfo);
    std::pair<std::string, NodeMemInfo> node = {"node0", nodeMemInfo1};
    uint32_t needBorrowNum = 128 * 1024 * 1024 * 2;
    MemBorrowStrategyResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::GetSocketInfo, MpResult(*)(DestMemoryBorrowParam & tempParam, uint32_t numaId))
        .stubs()
        .will(returnValue(0));
    auto res = MempoolBorrowModule::Instance().ProcessNodeMemBorrow(node, needBorrowNum, borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, ProcessNodeMemBorrow3)
{
    NumaMemInfo numaMemInfo;
    numaMemInfo.numaId = 1;
    numaMemInfo.borrowableMem = 128 * 1024 * 1024;
    NodeMemInfo nodeMemInfo1;
    nodeMemInfo1.localnumaMemInfo.push_back(numaMemInfo);
    std::pair<std::string, NodeMemInfo> node = {"node0", nodeMemInfo1};
    uint32_t needBorrowNum = 0;
    MemBorrowStrategyResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::GetSocketInfo, MpResult(*)(DestMemoryBorrowParam & tempParam, uint32_t numaId))
        .stubs()
        .will(returnValue(0));
    auto res = MempoolBorrowModule::Instance().ProcessNodeMemBorrow(node, needBorrowNum, borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, GetSocketInfo_Success)
{
    DestMemoryBorrowParam tempParam;
    tempParam.destNid = "node0";
    uint32_t numaId = 1;
    MOCKER_CPP(&MemManager::GetSocketId, MpResult(*)(const std::string& nodeId, const int& numaId, int& socketId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    auto res = MempoolBorrowModule::Instance().GetSocketInfo(tempParam, numaId);
    EXPECT_EQ(res, 0);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategy1)
{
    SrcMemoryBorrowParam srcParam;
    uint64_t borrowSize = 1024;
    MemBorrowStrategyResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const std::string srcNid))
        .stubs()
        .will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategy(srcParam, borrowSize, borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategy2)
{
    SrcMemoryBorrowParam srcParam;
    uint64_t borrowSize = 1024;
    MemBorrowStrategyResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const std::string srcNid))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::ValidateSrcparam, MpResult(*)(const SrcMemoryBorrowParam& srcParam))
        .stubs()
        .will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategy(srcParam, borrowSize, borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategy3)
{
    SrcMemoryBorrowParam srcParam;
    uint64_t borrowSize = 1024;
    MemBorrowStrategyResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const std::string srcNid))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::ValidateSrcparam, MpResult(*)(const SrcMemoryBorrowParam& srcParam))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::GetMemoryInfo,
               MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, std::vector<std::string>& antiNodeMemVec))
        .stubs()
        .will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategy(srcParam, borrowSize, borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

void MempoolBorrowModuleFilterAndSortNodes1(MempoolBorrowModule* This,
                                            std::unordered_map<std::string, NodeMemInfo>& nodeMemMap,
                                            std::string srcNid, const std::vector<std::string>& antiNodeMemVec,
                                            std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec)
{
    NodeMemInfo nodeMemInfo1;
    std::pair<std::string, NodeMemInfo> node = {"node0", nodeMemInfo1};
    nodeVec.push_back(node);
}

MpResult MempoolBorrowModuleFilterAndSortSockets1(
    MempoolBorrowModule* This, std::unordered_map<std::string, NodeMemInfo>& nodeMemMap,
    const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
    std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology,
    std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec)
{
    NodeMemInfo nodeMemInfo1;
    std::pair<std::string, NodeMemInfo> node = {"node0", nodeMemInfo1};
    nodeVec.push_back(node);
    return MEM_POOLING_OK;
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategy4)
{
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "node1";
    uint64_t borrowSize = 128 * 1024 * 2;
    MemBorrowStrategyResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const std::string srcNid))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::ValidateSrcparam, MpResult(*)(const SrcMemoryBorrowParam& srcParam))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::GetMemoryInfo,
               MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, std::vector<std::string>& antiNodeMemVec))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::FilterAndSortSockets,
               MpResult(*)(MempoolBorrowModule * This, std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology,
                           std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec))
        .stubs()
        .will(invoke(MempoolBorrowModuleFilterAndSortSockets1));
    MOCKER_CPP(&MempoolBorrowModule::ProcessNodeMemBorrow,
               MpResult(*)(const std::pair<std::string, NodeMemInfo>& node, uint32_t& needBorrowNum,
                           MemBorrowStrategyResult& borrowStrategyResult))
        .stubs()
        .will(returnValue(0));
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategy(srcParam, borrowSize, borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

struct MultiBorrowStrategyParam {
    struct Numa {
        Numa(std::string nodeId, uint16_t numaId, uint64_t available)
            : nodeId(nodeId),
              numaId(numaId),
              available(available)
        {
            return;
        }
        std::string nodeId;
        uint16_t numaId;
        uint64_t available;
    };
    struct Node {
        Node(std::string nodeId, uint64_t available) : nodeId(nodeId), available(available)
        {
            return;
        }
        std::string nodeId;
        uint64_t available;
        std::vector<Numa> numaList;
    };
    std::vector<Node> nodeList;
    std::vector<uint64_t> need;
    std::vector<Numa*> recordList;
    bool succeed{false};
};

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategyMultiple1)
{
    SrcMemoryBorrowParam srcParam;
    std::vector<uint64_t> borrowSizes = {1};
    std::string destPreNid;
    MemBorrowStrategyMultiResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const uint64_t borrowSize))
        .stubs()
        .will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategyMultiple(srcParam, borrowSizes, destPreNid,
                                                                         borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategyMultipleUB1)
{
    SrcMemoryBorrowParam srcParam;
    std::vector<uint64_t> borrowSizes = {1};
    std::string destPreNid;
    MemBorrowStrategyMultiResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const uint64_t borrowSize))
        .stubs()
        .will(returnValue(1));
    uint16_t socketId = 0;
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategyMultipleUB(srcParam, borrowSizes, destPreNid, socketId,
                                                                           borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategyMultiple2)
{
    SrcMemoryBorrowParam srcParam;
    std::vector<uint64_t> borrowSizes = {1};
    std::string destPreNid;
    MemBorrowStrategyMultiResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const uint64_t borrowSize))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::GetMemoryInfo,
               MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, std::vector<std::string>& antiNodeMemVec))
        .stubs()
        .will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategyMultiple(srcParam, borrowSizes, destPreNid,
                                                                         borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategyMultipleUB2)
{
    SrcMemoryBorrowParam srcParam;
    std::vector<uint64_t> borrowSizes = {1};
    std::string destPreNid;
    MemBorrowStrategyMultiResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const uint64_t borrowSize))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::GetMemoryInfo,
               MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, std::vector<std::string>& antiNodeMemVec))
        .stubs()
        .will(returnValue(1));
    uint16_t socketId = 0;
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategyMultipleUB(srcParam, borrowSizes, destPreNid, socketId,
                                                                           borrowStrategyResult);
    EXPECT_EQ(res, 1);
}

void MempoolBorrowModuleFilterAndSortNodes2(MempoolBorrowModule* This,
                                            std::unordered_map<std::string, NodeMemInfo>& nodeMemMap,
                                            std::string srcNid, const std::vector<std::string>& antiNodeMemVec,
                                            std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec)
{
    NodeMemInfo nodeMemInfo;
    nodeMemInfo.totalBorrowableMem = 1;
    NumaMemInfo numaMemInfo;
    numaMemInfo.numaId = 1;
    numaMemInfo.reservedMem = 1;
    numaMemInfo.borrowedMem = 1;
    numaMemInfo.borrowableMem = 1;
    nodeMemInfo.localnumaMemInfo.push_back(numaMemInfo);
    nodeMemMap["key"] = nodeMemInfo;
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategyMultiple3)
{
    SrcMemoryBorrowParam srcParam;
    std::vector<uint64_t> borrowSizes = {0};
    std::string destPreNid;
    MemBorrowStrategyMultiResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const uint64_t borrowSize))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::GetMemoryInfo,
               MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, std::vector<std::string>& antiNodeMemVec))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::FilterAndSortNodes,
               void (*)(MempoolBorrowModule * This, std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                        std::string srcNid, const std::vector<std::string>& antiNodeMemVec,
                        std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec))
        .stubs()
        .will(invoke(MempoolBorrowModuleFilterAndSortNodes2));
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategyMultiple(srcParam, borrowSizes, destPreNid,
                                                                         borrowStrategyResult);
    EXPECT_EQ(res, 0);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategyMultipleUB3)
{
    SrcMemoryBorrowParam srcParam;
    std::vector<uint64_t> borrowSizes = {0};
    std::string destPreNid;
    MemBorrowStrategyMultiResult borrowStrategyResult;
    MOCKER_CPP(&MempoolBorrowModule::ValidateBorrowSize, MpResult(*)(const uint64_t borrowSize))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::GetMemoryInfo,
               MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, std::vector<std::string>& antiNodeMemVec))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::FilterAndSortNodes,
               void (*)(MempoolBorrowModule * This, std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                        std::string srcNid, const std::vector<std::string>& antiNodeMemVec,
                        std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec))
        .stubs()
        .will(invoke(MempoolBorrowModuleFilterAndSortNodes2));
    uint16_t socketId = 0;
    auto res = MempoolBorrowModule::Instance().MemBorrowStrategyMultipleUB(srcParam, borrowSizes, destPreNid, socketId,
                                                                           borrowStrategyResult);
    EXPECT_EQ(res, 0);
}

TEST_F(TestMemPoolBorrowModule, MemBorrowStrategyMultipleUBTest)
{
    MOCKER_CPP(&MempoolBorrowModule::GetMemoryInfo,
               MpResult(*)(std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, std::vector<std::string>& antiNodeMemVec))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MempoolBorrowModule::FilterAndSortSockets,
               MpResult(*)(MempoolBorrowModule * This, std::unordered_map<std::string, NodeMemInfo> & nodeMemMap,
                           const SrcMemoryBorrowParam& srcParam, const std::vector<std::string>& antiNodeMemVec,
                           std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology,
                           std::vector<std::pair<std::string, NodeMemInfo>>& nodeVec))
        .stubs()
        .will(invoke(MempoolBorrowModuleFilterAndSortSockets1));

    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = "Node0";
    srcParam.srcSocketId = 0;
    srcParam.srcNumaId = 0;

    uint64_t borrowSize = 1048576;
    std::vector<uint64_t> borrowSizes;
    borrowSizes.push_back(borrowSize);

    std::string destPreNid = "Node1";
    uint16_t socketId = 0;
    MemBorrowStrategyMultiResult borrowStrategyResult;

    MpResult ret = MempoolBorrowModule::Instance().MemBorrowStrategyMultipleUB(srcParam, borrowSizes, destPreNid,
                                                                               socketId, borrowStrategyResult);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::reset();
}

TEST_F(TestMemPoolBorrowModule, MigrateExecute1)
{
    std::string borrowInNode = "node1";
    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList;
    uint64_t waitingTime = 10;
    std::vector<std::string> borrowIdList = {"id1"};

    turbo::rmrs::VMMigrateOutParam vm;
    vm.pid = 1;
    vm.memSize = 25;
    vmInfoList.push_back(vm);
    MOCKER_CPP(&Name2VmInfo::Update, MpResult(*)(std::map<std::string, std::set<turbo::rmrs::BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(1));
    auto res = migrate::MempoolMigrateExecute::MigrateExecute(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, MigrateExecute2)
{
    std::string borrowInNode = "node1";
    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList;
    uint64_t waitingTime = 10;
    std::vector<std::string> borrowIdList = {"id1"};

    turbo::rmrs::VMMigrateOutParam vm;
    vm.pid = 1;
    vm.memSize = 25;
    vmInfoList.push_back(vm);
    MOCKER_CPP(&Name2VmInfo::Update, MpResult(*)(std::map<std::string, std::set<turbo::rmrs::BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(1));

    auto res = migrate::MempoolMigrateExecute::MigrateExecute(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    EXPECT_EQ(res, 1);
}

MpResult MockGetMigrateExecuteInfo(std::string& borrowInNode, std::vector<turbo::rmrs::VMMigrateOutParam>& vmInfoList,
                                   std::set<turbo::rmrs::BorrowIdInfo>& pidList,
                                   std::unordered_set<uint16_t>& uniqueDesNumaIds)
{
    uniqueDesNumaIds.insert(1);
    uniqueDesNumaIds.insert(2);
    pidList.insert(turbo::rmrs::BorrowIdInfo{10086, 2048});
    pidList.insert(turbo::rmrs::BorrowIdInfo{10087, 4096});
    return MEM_POOLING_OK;
}

TEST_F(TestMemPoolBorrowModule, MigrateExecute3)
{
    // 准备输入参数
    std::string borrowInNode = "node-1";
    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList = {{10086, 2048, 1}, {10087, 4096, 2}};
    uint64_t waitingTime = 100;
    std::vector<std::string> borrowIdList = {"borrow-1", "borrow-2", "borrow-3"};

    MOCKER_CPP(&migrate::MempoolMigrateExecute::GetMigrateExecuteInfo,
               MpResult(*)(std::string&, std::vector<turbo::rmrs::VMMigrateOutParam>&,
                           std::set<turbo::rmrs::BorrowIdInfo>&, std::unordered_set<uint16_t>&))
        .stubs()
        .will(invoke(MockGetMigrateExecuteInfo));
    MOCKER_CPP(&BorrowRecordHelper::GetBorrowIdByNumaId,
               MpResult(*)(std::vector<std::string> & borrowIds, const uint16_t numaId, const std::string nodeId))
        .stubs()
        .will(returnValue(1));

    auto res = migrate::MempoolMigrateExecute::MigrateExecute(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    EXPECT_EQ(res, 1);
    GlobalMockObject::reset();
}

MpResult MockGetBorrowIdByNumaId(BorrowRecordHelper* This, std::vector<std::string>& borrowIds, const uint16_t numaId,
                                 const std::string nodeId)
{
    borrowIds.push_back("borrow-1");
    borrowIds.push_back("borrow-2");
    return MEM_POOLING_OK;
}

TEST_F(TestMemPoolBorrowModule, MigrateExecute4)
{
    // 准备输入参数
    std::string borrowInNode = "node-1";
    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList = {{10086, 2048, 1}, {10087, 4096, 2}};
    uint64_t waitingTime = 100;
    std::vector<std::string> borrowIdList = {"borrow-1", "borrow-2", "borrow-3"};

    MOCKER_CPP(&migrate::MempoolMigrateExecute::GetMigrateExecuteInfo,
               MpResult(*)(std::string&, std::vector<turbo::rmrs::VMMigrateOutParam>&,
                           std::set<turbo::rmrs::BorrowIdInfo>&, std::unordered_set<uint16_t>&))
        .stubs()
        .will(invoke(MockGetMigrateExecuteInfo));
    MOCKER_CPP(&BorrowRecordHelper::GetBorrowIdByNumaId, MpResult(*)(MemManager*, std::vector<std::string> & borrowIds,
                                                                     const uint16_t numaId, const std::string nodeId))
        .stubs()
        .will(invoke(MockGetBorrowIdByNumaId));
    MOCKER_CPP(&Name2VmInfo::Update, MpResult(*)(std::map<std::string, std::set<turbo::rmrs::BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(1));
    auto res = migrate::MempoolMigrateExecute::MigrateExecute(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    EXPECT_EQ(res, 1);
    GlobalMockObject::reset();
}

TEST_F(TestMemPoolBorrowModule, MigrateExecute5)
{
    // 准备输入参数
    std::string borrowInNode = "node-1";
    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList = {{10086, 2048, 1}, {10087, 4096, 2}};
    uint64_t waitingTime = 100;
    std::vector<std::string> borrowIdList = {"borrow-1", "borrow-2", "borrow-3"};

    MOCKER_CPP(&migrate::MempoolMigrateExecute::GetMigrateExecuteInfo,
               MpResult(*)(std::string&, std::vector<turbo::rmrs::VMMigrateOutParam>&,
                           std::set<turbo::rmrs::BorrowIdInfo>&, std::unordered_set<uint16_t>&))
        .stubs()
        .will(invoke(MockGetMigrateExecuteInfo));
    MOCKER_CPP(&BorrowRecordHelper::GetBorrowIdByNumaId,
               MpResult(*)(BorrowRecordHelper*, std::vector<std::string> & borrowIds, const uint16_t numaId,
                           const std::string nodeId))
        .stubs()
        .will(invoke(MockGetBorrowIdByNumaId));
    MOCKER_CPP(&Name2VmInfo::Update, MpResult(*)(std::map<std::string, std::set<turbo::rmrs::BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&mempooling::migrate::MempoolMigrateExecute::MigrateExecuteRpc,
               MpResult(*)(std::string borrowInNode, std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList,
                           uint64_t waitingTime, std::vector<std::string> borrowIdList))
        .stubs()
        .will(returnValue(1));
    auto res = migrate::MempoolMigrateExecute::MigrateExecute(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    EXPECT_EQ(res, 1);
    GlobalMockObject::reset();
}

TEST_F(TestMemPoolBorrowModule, MigrateExecute6)
{
    // 准备输入参数
    std::string borrowInNode = "node-1";
    std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList = {{10086, 2048, 1}, {10087, 4096, 2}};
    uint64_t waitingTime = 100;
    std::vector<std::string> borrowIdList = {"borrow-1", "borrow-2", "borrow-3"};

    MOCKER_CPP(&migrate::MempoolMigrateExecute::GetMigrateExecuteInfo,
               MpResult(*)(std::string&, std::vector<turbo::rmrs::VMMigrateOutParam>&,
                           std::set<turbo::rmrs::BorrowIdInfo>&, std::unordered_set<uint16_t>&))
        .stubs()
        .will(invoke(MockGetMigrateExecuteInfo));
    MOCKER_CPP(&BorrowRecordHelper::GetBorrowIdByNumaId,
               MpResult(*)(BorrowRecordHelper*, std::vector<std::string> & borrowIds, const uint16_t numaId,
                           const std::string nodeId))
        .stubs()
        .will(invoke(MockGetBorrowIdByNumaId));
    MOCKER_CPP(&Name2VmInfo::Update, MpResult(*)(std::map<std::string, std::set<turbo::rmrs::BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&mempooling::migrate::MempoolMigrateExecute::MigrateExecuteRpc,
               MpResult(*)(std::string borrowInNode, std::vector<turbo::rmrs::VMMigrateOutParam> vmInfoList,
                           uint64_t waitingTime, std::vector<std::string> borrowIdList))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&migrate::MempoolMigrateExecute::ValidMigrateExecuteParam,
               MpResult(*)(std::string borrowInNode, std::unordered_set<uint16_t> uniqueDesNumaIdList))
        .stubs()
        .will(returnValue(0));
    auto res = migrate::MempoolMigrateExecute::MigrateExecute(borrowInNode, vmInfoList, waitingTime, borrowIdList);
    EXPECT_EQ(res, 0);
    GlobalMockObject::reset();
}

TEST_F(TestMemPoolBorrowModule, MemBorrowFailedRollback_TestWithFailed)
{
    MemBorrowExecuteResult borrowExecuteResult;
    borrowExecuteResult.borrowIds.push_back("borrowId_1");

    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
               MpResult(*)(const std::string& name, bool isForceDelete, bool smapBack))
        .stubs()
        .will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().MemBorrowFailedRollback(borrowExecuteResult);
    EXPECT_EQ(res, 1);
    GlobalMockObject::reset();
}

TEST_F(TestMemPoolBorrowModule, MemBorrowFailedRollback_TestWithSuccess)
{
    MemBorrowExecuteResult borrowExecuteResult;
    borrowExecuteResult.borrowIds.push_back("borrowId_1");

    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
               MpResult(*)(const std::string& name, bool isForceDelete, bool smapBack))
        .stubs()
        .will(returnValue(0));
    auto res = MempoolBorrowModule::Instance().MemBorrowFailedRollback(borrowExecuteResult);
    EXPECT_EQ(res, 0);
    GlobalMockObject::reset();
}

TEST_F(TestMemPoolBorrowModule, ValidateBorrowExecuteParam_Failed1)
{
    DestMemoryBorrowParam param;
    param.destNid = "NodeA";
    param.destSocketId = 0;
    param.destNumaNum = 1;
    param.destNumaId = {0};
    param.memSize = {127 * 1024}; // 128MB，符合要求

    MemMallocAttr memAttr;
    memAttr.srcNid = "NodeB";
    memAttr.srcSocket = 0;
    memAttr.srcNuma = 0;
    memAttr.dstNodeNum = 1;
    memAttr.lenderNumaSize = 1;

    MemBorrowExecuteResult result;
    result.borrowIds = {"borrow_id_001"};
    result.presentNumaId = {0};
    uint64_t totalSize = 0;

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowFailedRollback,
               MpResult(*)(const MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = MempoolBorrowModule::Instance().ValidateBorrowExecuteParam(param, memAttr, result, totalSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ValidateBorrowExecuteParam_Failed2)
{
    DestMemoryBorrowParam param;
    param.destNid = "NodeA";
    param.destSocketId = 0;
    param.destNumaNum = 1;
    param.destNumaId = {-1};
    param.memSize = {128 * 1024}; // 128MB，符合要求

    MemMallocAttr memAttr;
    memAttr.srcNid = "NodeB";
    memAttr.srcSocket = 0;
    memAttr.srcNuma = 0;
    memAttr.dstNodeNum = 1;
    memAttr.lenderNumaSize = 1;

    MemBorrowExecuteResult result;
    result.borrowIds = {"borrow_id_001"};
    result.presentNumaId = {0};
    uint64_t totalSize = 0;

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowFailedRollback,
               MpResult(*)(const MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = MempoolBorrowModule::Instance().ValidateBorrowExecuteParam(param, memAttr, result, totalSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ValidateBorrowExecuteParam_Failed3)
{
    DestMemoryBorrowParam param;
    param.destNid = "NodeA";
    param.destSocketId = 0;
    param.destNumaNum = 1;
    param.destNumaId = {-1};
    param.memSize = {128 * 1024}; // 128MB，符合要求

    MemMallocAttr memAttr;
    memAttr.srcNid = "NodeB";
    memAttr.srcSocket = 0;
    memAttr.srcNuma = 0;
    memAttr.dstNodeNum = 1;
    memAttr.lenderNumaSize = 1;

    MemBorrowExecuteResult result;
    result.borrowIds = {"borrow_id_001"};
    result.presentNumaId = {0};
    uint64_t totalSize = 0;

    MOCKER_CPP(&MempoolBorrowModule::MemBorrowFailedRollback,
               MpResult(*)(const MemBorrowExecuteResult& borrowExecuteResult))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult ret = MempoolBorrowModule::Instance().ValidateBorrowExecuteParam(param, memAttr, result, totalSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, ValidateBorrowSize_Failed)
{
    uint64_t borrowSize = 127 * 1024;
    MpResult res = MempoolBorrowModule::Instance().ValidateBorrowSize(borrowSize);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBackExecute1)
{
    std::string nodeId = "node1";
    uint16_t numaId = 1;
    MOCKER_CPP(&MemManager::GetNodeMemInfo, MpResult(*)(const std::string& nodeId, NodeMemInfo& outInfo))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&BorrowRecordHelper::GetBorrowIdByNumaId,
               MpResult(*)(std::vector<std::string> & borrowIds, const uint16_t numaId, const std::string nodeId))
        .stubs()
        .will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().MemBackExecute(nodeId, numaId);
    EXPECT_EQ(res, 1);
}

MpResult MemManagerGetBorrowIdByNumaId(BorrowRecordHelper* This, std::vector<std::string>& borrowIds,
                                       const uint16_t numaId, const std::string nodeId)
{
    borrowIds = {"1"};
    return 0;
}

TEST_F(TestMemPoolBorrowModule, MemBackExecute2)
{
    std::string nodeId = "node1";
    uint16_t numaId = 1;
    MOCKER_CPP(&MemManager::GetNodeMemInfo, MpResult(*)(const std::string& nodeId, NodeMemInfo& outInfo))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&BorrowRecordHelper::GetBorrowIdByNumaId,
               MpResult(*)(BorrowRecordHelper * This, std::vector<std::string> & borrowIds, const uint16_t numaId,
                           const std::string nodeId))
        .stubs()
        .will(invoke(MemManagerGetBorrowIdByNumaId));
    MOCKER_CPP(&MemBorrowExecutor::MemFree, MpResult(*)(const std::string& name)).stubs().will(returnValue(1));
    auto res = MempoolBorrowModule::Instance().MemBackExecute(nodeId, numaId);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolBorrowModule, MemBackExecute3)
{
    std::string nodeId = "node1";
    uint16_t numaId = 1;
    MOCKER_CPP(&MemManager::GetNodeMemInfo, MpResult(*)(const std::string& nodeId, NodeMemInfo& outInfo))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&BorrowRecordHelper::GetBorrowIdByNumaId,
               MpResult(*)(BorrowRecordHelper * This, std::vector<std::string> & borrowIds, const uint16_t numaId,
                           const std::string nodeId))
        .stubs()
        .will(invoke(MemManagerGetBorrowIdByNumaId));
    MOCKER_CPP(&MemBorrowExecutor::MemFree, MpResult(*)(const std::string& name)).stubs().will(returnValue(0));
    auto res = MempoolBorrowModule::Instance().MemBackExecute(nodeId, numaId);
    EXPECT_EQ(res, 0);
}

MpResult TestCollectBorrowRecordsForValidMigrateParam(BorrowRecordHelper*, const std::string nodeId,
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
    // 远端numaId
    int16_t borrowRemoteNuma = 4;
    record.borrowRemoteNuma = borrowRemoteNuma;
    record.borrowMemId = {1};
    LentNuma lentNuma{1, 1};
    record.lentNuma.push_back(lentNuma);
    std::vector<BorrowRecord> borrowRecordList{record};
    borrowRecords = borrowRecordList;
    return 0;
}

TEST_F(TestMemPoolBorrowModule, ValidMigrateExecuteParamSuccess)
{
    MOCKER_CPP(
        &BorrowRecordHelper::CollectBorrowRecords,
        MpResult(*)(BorrowRecordHelper * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(TestCollectBorrowRecordsForValidMigrateParam));
    std::string borrowInNode = "Node1";
    std::unordered_set<uint16_t> uniqueDesNumaIdList = {4};
    auto res = migrate::MempoolMigrateExecute::ValidMigrateExecuteParam(borrowInNode, uniqueDesNumaIdList);
    EXPECT_EQ(res, 0);
}

TEST_F(TestMemPoolBorrowModule, ValidMigrateExecuteParamFailed_EmptyNumaIdList)
{
    std::string borrowInNode = "Node1";
    std::unordered_set<uint16_t> uniqueDesNumaIdList;
    auto res = migrate::MempoolMigrateExecute::ValidMigrateExecuteParam(borrowInNode, uniqueDesNumaIdList);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, ValidMigrateExecuteParamFailed_CollectBorrowRecordFailed)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(MemManager * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(returnValue(1));
    std::string borrowInNode = "Node1";
    std::unordered_set<uint16_t> uniqueDesNumaIdList = {4};
    auto res = migrate::MempoolMigrateExecute::ValidMigrateExecuteParam(borrowInNode, uniqueDesNumaIdList);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, ValidMigrateExecuteParamFailed_EmptyBorrowRecord)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(MemManager * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(returnValue(0));
    std::string borrowInNode = "Node1";
    std::unordered_set<uint16_t> uniqueDesNumaIdList = {4};
    auto res = migrate::MempoolMigrateExecute::ValidMigrateExecuteParam(borrowInNode, uniqueDesNumaIdList);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, ValidMigrateExecuteParamFailed_NotMatchNumaId)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(MemManager * This, const std::string nodeId, std::vector<BorrowRecord>& borrowRecords))
        .stubs()
        .will(invoke(TestCollectBorrowRecordsForValidMigrateParam));
    std::string borrowInNode = "Node1";
    std::unordered_set<uint16_t> uniqueDesNumaIdList = {6};
    auto res = migrate::MempoolMigrateExecute::ValidMigrateExecuteParam(borrowInNode, uniqueDesNumaIdList);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, RpcGetVmInfoImmediatelyFailed1)
{
    MOCKER(UbseRpcSend).stubs().will(returnValue(1));
    std::string nodeId;
    std::vector<VmDomainInfo> vmDomainInfos;
    auto res = migrate::MempoolMigrateExecute::RpcGetVmInfoImmediately(nodeId, vmDomainInfos);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, RpcGetVmInfoImmediatelySucceed)
{
    MOCKER(UbseRpcSend).stubs().will(returnValue(0));
    std::string nodeId;
    std::vector<VmDomainInfo> vmDomainInfos;
    auto res = migrate::MempoolMigrateExecute::RpcGetVmInfoImmediately(nodeId, vmDomainInfos);
    EXPECT_EQ(res, 0);
}

static uint32_t MockOsturboFunctionCallerReturn0(const std::string& function, const TurboByteBuffer& params,
                                                 TurboByteBuffer& result)
{
    return 0;
}

static uint32_t MockRmrsMigrateStrategyReturn0(const turbo::rmrs::MigrateStrategyParamRMRS& migrateStrategyParam,
                                               turbo::rmrs::MigrateStrategyResult& migrateStrategyResult)
{
    return 0;
}

static uint32_t MockRmrsMigrateStrategyReturn1(const turbo::rmrs::MigrateStrategyParamRMRS& migrateStrategyParam,
                                               turbo::rmrs::MigrateStrategyResult& migrateStrategyResult)
{
    return 1;
}

static uint32_t MockRmrsMigrateStrategyReturn3(const turbo::rmrs::MigrateStrategyParamRMRS& migrateStrategyParam,
                                               turbo::rmrs::MigrateStrategyResult& migrateStrategyResult)
{
    return 3;
}

uint32_t MockOsturboFunctionCallerReturn1(const std::string& function, const TurboByteBuffer& params,
                                          TurboByteBuffer& result)
{
    return 1;
}

uint32_t MockOsturboFunctionCallerReturn3(const std::string& function, const TurboByteBuffer& params,
                                          TurboByteBuffer& result)
{
    return 3;
}

TEST_F(TestMemPoolBorrowModule, ConvertMigrateStrategyParamTestSucceed)
{
    turbo::rmrs::MigrateStrategyParam migrateStrategyParam;
    migrateStrategyParam.borrowSize = 0;
    turbo::rmrs::MigrateStrategyParamRMRS migrateStrategyParamRMRS;

    MOCKER_CPP(&GetNumaData,
               uint32_t(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos,
                           std::vector<uint16_t> & remoteNumaIdList, std::map<uint16_t, NumaHugePageInfo> & numaInfoMap,
                           std::vector<NumaHugePageInfo> & numaHugePageInfoSumList))
        .stubs()
        .will(returnValue(0));

    MOCKER_CPP(&GetVMData, uint32_t(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos,
                                       std::vector<VmNumaInfoBrr> & allVmNumaInfoInfoList,
                                       std::map<pid_t, VmNumaInfoBrr> & vmNumaInfoMap))
        .stubs()
        .will(returnValue(0));

    auto res = ConvertMigrateStrategyParam(migrateStrategyParam, migrateStrategyParamRMRS);
    EXPECT_EQ(res, 0);
    EXPECT_EQ(migrateStrategyParam.borrowSize, migrateStrategyParamRMRS.borrowSize);
}

TEST_F(TestMemPoolBorrowModule, GetVMDataTestFailed)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    std::vector<VmNumaInfoBrr> allVmNumaInfoInfoList;
    std::map<pid_t, VmNumaInfoBrr> vmNumaInfoMap;

    MOCKER_CPP(&Exporter::GetVmInfoImmediately, uint32_t(*)(std::vector<VmDomainInfo> & vmInfos))
        .stubs()
        .will(returnValue(1));

    auto res = GetVMData(vmDomainInfos, allVmNumaInfoInfoList, vmNumaInfoMap);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, GetNumaDataTestFailed)
{
    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    std::vector<uint16_t> remoteNumaIdList;
    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;
    std::vector<NumaHugePageInfo> numaHugePageInfoSumList;

    MOCKER_CPP(&Exporter::GetNumaInfoImmediately, uint32_t(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(1));

    auto res = GetNumaData(numaInfos, remoteNumaIdList, numaInfoMap, numaHugePageInfoSumList);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMemPoolBorrowModule, GetLocalVmInfoTestSucceed)
{
    std::vector<VmNumaInfoBrr> allVmNumaInfoInfoList;
    std::map<pid_t, VmNumaInfoBrr> VmNumaInfoMap;
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    VmDomainInfo vm1;
    vm1.metaData.nodeId = "node-1";
    vm1.metaData.hostName = "hostA";
    vm1.metaData.uuid = "550e8400-e29b-41d4-a716-446655440000";
    vm1.metaData.name = "vm-test-A";
    vm1.metaData.state = "running";
    vm1.metaData.vmCreateTime = std::time(nullptr);
    vm1.metaData.maxMem = 2048000;
    vm1.metaData.pid = 12345;
    VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 1024000, 0, 1};
    vm1.numaInfo[0] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 512000, -1, 0};
    vm1.numaInfo[1] = vmNumaInfo2;
    vmDomainInfos.push_back(vm1);

    GetLocalVmInfo(allVmNumaInfoInfoList, VmNumaInfoMap, vmDomainInfos);
    EXPECT_EQ(allVmNumaInfoInfoList.size(), 1);
}

TEST_F(TestMemPoolBorrowModule, DistributeNumaMemInfoTestSucceed)
{
    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    std::map<uint16_t, NumaHugePageInfo> numaInfoMap;
    std::vector<NumaHugePageInfo> numaHugePageInfoSumList;

    // 本地 NUMA 节点
    mempooling::exportV2::NumaInfo numa0;
    numa0.timestamp = std::time(nullptr);
    numa0.metaData.nodeId = "node-1";
    numa0.metaData.hostName = "hostA";
    numa0.metaData.numaId = 0;
    numa0.metaData.socketId = 0;
    numa0.metaData.isLocal = true;
    numa0.metaData.memTotal = 32768000; // 32GB
    numa0.metaData.memFree = 8192000;   // 8GB
    mempooling::exportV2::NumaPageData numaPageData1 = {2048, 1024, 512};
    numa0.metaData.numaPageInfo[2048] = numaPageData1;
    numaInfos.push_back(numa0);

    DistributeNumaMemInfo(numaInfos, numaInfoMap, numaHugePageInfoSumList);
    EXPECT_EQ(numaHugePageInfoSumList.size(), 1);
}

TEST_F(TestMemPoolBorrowModule, GetRemoteNumaListTestSucceed)
{
    std::vector<NumaHugePageInfo> numaHugePageInfoSumList;
    std::vector<uint16_t> remoteNumaIdList;
    NumaHugePageInfo numa0;
    numa0.numaId = 0;
    numa0.hugePageTotal = 1024;
    numa0.hugePageFree = 0;
    numa0.isLocal = true;
    numa0.isRemote = false;
    numaHugePageInfoSumList.push_back(numa0);

    GetRemoteNumaList(numaHugePageInfoSumList, remoteNumaIdList);
    EXPECT_EQ(remoteNumaIdList.size(), 0);
}

} // namespace mempooling