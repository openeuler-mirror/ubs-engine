/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_account.h"

#include <utility>

#include "ubse_node_controller.h"
#include "ubse_conf.h"
#include "ubse_mmi_interface.h"
#include "ubse_mem_controller_module.h"
namespace ubse::obj::ut {
using namespace ubse::mem::account;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::controller;
const size_t NODE_NUM = 3;
const size_t NUMA_NUM = 3;
const std::string BASE_RESOURCEID = "resourceId";
const std::string BASE_NODEID = "Node";
const std::string BASE_HOST_NAME = "host";
const uint16_t CPU_NUM = 8;
const uint64_t MEM_SIZE = 512;
const uint64_t TIME_STAMP = 1746616900;
const size_t MEMID_NUM = 2;

void TestUbseMemAccount::SetUp()
{
    Test::SetUp();
}

void TestUbseMemAccount::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

uint32_t MockUbseGetMemDebtInfoReturnEmpty([[maybe_unused]] const std::string &nodeId,
    NodeMemDebtInfoMap &memDebtInfoMap)
{
    memDebtInfoMap.clear();
    return 0;
}

nodeController::UbseNumaLocation MockNumaLocation(int nodeId, uint32_t numaId)
{
    return nodeController::UbseNumaLocation{ BASE_NODEID + std::to_string(nodeId), numaId };
}

ubse::nodeController::UbseNumaInfo MockNumaInfo(nodeController::UbseNumaLocation numaLocation, uint32_t socketId)
{
    std::vector<uint16_t> cores;
    for (int i = 0; i < CPU_NUM; ++i) {
        cores.emplace_back(i + socketId);
    }
    return ubse::nodeController::UbseNumaInfo{
        std::move(numaLocation), socketId,     cores,    (socketId + 1) * MEM_SIZE,
        socketId * MEM_SIZE,     socketId * 2, socketId, TIME_STAMP + socketId
    };
}

std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> MockGetAllNodes()
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> res;
    for (int i = 0; i < NODE_NUM; ++i) {
        nodeController::UbseNodeInfo nodeInfo{};
        nodeInfo.nodeId = BASE_NODEID + std::to_string(i);
        nodeInfo.slotId = i;
        nodeInfo.hostName = BASE_HOST_NAME + std::to_string(i);
        for (int j = 0; j < NUMA_NUM; ++j) {
            nodeController::UbseNumaLocation numaLocation = MockNumaLocation(i, j);
            nodeInfo.numaInfos[numaLocation] = MockNumaInfo(numaLocation, j);
        }
        res[BASE_NODEID + std::to_string(i)] = nodeInfo;
    }
    return res;
}

void MockImportObj(UbseMemBorrowImportBaseObj &importObj)
{
    importObj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    for (int i = 0; i < MEMID_NUM; ++i) {
        UbseMemImportResult importResult{};
        importResult.memId = i;
        importResult.numaId = NUMA_NUM - 1;
        importObj.status.importResults.emplace_back(importResult);
        UbseMemObmmInfo obmmInfo{};
        obmmInfo.memId = i;
        importObj.exportObmmInfo.emplace_back(obmmInfo);
    }
    UbseMemDebtNumaInfo debtNumaInfo{};
    debtNumaInfo.nodeId = BASE_NODEID + "0";
    debtNumaInfo.socketId = 0;
    debtNumaInfo.numaId = NUMA_NUM - 1;
    debtNumaInfo.size = MEM_SIZE;
    importObj.algoResult.importNumaInfos.emplace_back(debtNumaInfo);
    importObj.algoResult.exportNumaInfos.emplace_back(debtNumaInfo);
}

void MockExportObj(UbseMemBorrowExportBaseObj &exportObj)
{
    exportObj.status.state = UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    for (int i = 0; i < MEMID_NUM; ++i) {
        UbseMemObmmInfo obmmInfo{};
        obmmInfo.memId = i;
        exportObj.status.exportObmmInfo.emplace_back(obmmInfo);
    }
    UbseMemDebtNumaInfo debtNumaInfo{};
    debtNumaInfo.nodeId = BASE_NODEID + "1";
    debtNumaInfo.socketId = 1;
    debtNumaInfo.numaId = 0;
    debtNumaInfo.size = MEM_SIZE;
    exportObj.algoResult.exportNumaInfos.emplace_back(debtNumaInfo);
}

uint32_t MockUbseGetMemDebtInfo([[maybe_unused]] const std::string &nodeId,
    NodeMemDebtInfoMap &memDebtInfoMap)
{
    UbseMemFdBorrowImportObj importObj{};
    UbseMemFdBorrowExportObj exportObj{};
    UbseMemNumaBorrowImportObj memNumaBorrowImportObj{};
    UbseMemNumaBorrowExportObj memNumaBorrowExportObj{};
    UbseMemAddrBorrowImportObj memAddrBorrowImportObj{};
    UbseMemAddrBorrowExportObj memAddrBorrowExportObj{};
    MockImportObj(importObj);
    MockExportObj(exportObj);
    MockImportObj(memNumaBorrowImportObj);
    MockExportObj(memNumaBorrowExportObj);
    MockImportObj(memAddrBorrowImportObj);
    MockExportObj(memAddrBorrowExportObj);
    UbseMemFdImportObjMap fdImportObjMap{};
    UbseMemNumaImportObjMap numaImportObjMap{};
    UbseMemAddrImportObjMap addrImportObjMap{};
    fdImportObjMap[BASE_RESOURCEID + "0"] = importObj;
    numaImportObjMap[BASE_RESOURCEID + "1"] = memNumaBorrowImportObj;
    addrImportObjMap[BASE_RESOURCEID + "2"] = memAddrBorrowImportObj;
    UbseMemFdExportObjMap fdExportObjMap{};
    UbseMemNumaExportObjMap numaExportObjMap{};
    UbseMemAddrExportObjMap addrExportObjMap{};
    fdExportObjMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0"] = exportObj;
    numaExportObjMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0"] = memNumaBorrowExportObj;
    addrExportObjMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0"] = memAddrBorrowExportObj;
    NodeMemDebtInfo debtInfo0{};
    NodeMemDebtInfo debtInfo1{};
    debtInfo0.fdImportObjMap = fdImportObjMap;
    debtInfo0.numaImportObjMap = numaImportObjMap;
    debtInfo0.addrImportObjMap = addrImportObjMap;
    debtInfo1.fdExportObjMap = fdExportObjMap;
    debtInfo1.numaExportObjMap = numaExportObjMap;
    debtInfo1.addrExportObjMap = addrExportObjMap;
    memDebtInfoMap[BASE_NODEID + "0"] = debtInfo0;
    memDebtInfoMap[BASE_NODEID + "1"] = debtInfo1;
    return 0;
}

uint32_t MockUbseGetUInt([[maybe_unused]] const std::string &section, [[maybe_unused]] const std::string &configKey,
    uint32_t &configValue)
{
    configValue = CPU_NUM;
    return 0;
}

/*
 * 用例描述： 当nodeId为空，查询借用账本信息返回正常
 * 测试步骤： 1.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息为空
 * 2.nodeId为空，调用UbseAllBorrowAccountInfo(nodeId,accountMap);
 * 3.判断返回值accountMap的大小为0
 * 4.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息不为空
 * 5.判断返回值accountMap是否符合预期
 * 预期结果：
 * 返回accountMap符合预期
 */
TEST_F(TestUbseMemAccount, ReturnsAccountMapCorrectlyWhenNodeIdEmptyWithoutAccount)
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> emptyNodeInfo{};
    MOCKER(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(emptyNodeInfo));
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfoReturnEmpty));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    const std::string nodeId;
    UbseBorrowAccountMap accountMap;
    UbseAllBorrowAccountInfo(nodeId, accountMap);
    EXPECT_EQ(0, accountMap.size());
}

TEST_F(TestUbseMemAccount, ReturnsAccountMapCorrectlyWhenNodeIdEmptyAndWithAccount)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfo));
    const std::string nodeId;
    UbseBorrowAccountMap accountMap;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    UbseAllBorrowAccountInfo(nodeId, accountMap);
    EXPECT_EQ(NUMA_NUM, accountMap.size());
    EXPECT_EQ("Node0", accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowNodeId);
    EXPECT_EQ(NUMA_NUM - 1, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowNumaSizeList[0]);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowMemId[0]);
    EXPECT_EQ("Node1", accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].lentNodeId);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].lentNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].lentNumaSizeList[0]);
    EXPECT_EQ("Node0", accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowNodeId);
    EXPECT_EQ(NUMA_NUM - 1, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowNumaSizeList[0]);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowMemId[0]);
    EXPECT_EQ("Node1", accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].lentNodeId);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].lentNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].lentNumaSizeList[0]);
    EXPECT_EQ("Node0", accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowNodeId);
    EXPECT_EQ(NUMA_NUM - 1, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowNumaSizeList[0]);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowMemId[0]);
    EXPECT_EQ("Node1", accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].lentNodeId);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].lentNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].lentNumaSizeList[0]);
}

/*
 * 用例描述： 当nodeId不为空，查询借用账本信息返回正常
 * 测试步骤： 1.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息为空
 * 2.nodeId为Node1，调用UbseAllBorrowAccountInfo(nodeId,accountMap);
 * 3.判断返回值accountMap的大小为0
 * 4.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息不为空
 * 5.判断返回值accountMap是否符合预期
 * 预期结果：
 * 返回accountMap符合预期
 */
TEST_F(TestUbseMemAccount, ReturnsAccountMapCorrectlyWhenNodeIdNotEmptyWithEmptyAccount)
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> emptyNodeInfo{};
    MOCKER(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(emptyNodeInfo));
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfoReturnEmpty));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    const std::string nodeId = "Node0";
    UbseBorrowAccountMap accountMap;
    UbseAllBorrowAccountInfo(nodeId, accountMap);
    EXPECT_EQ(0, accountMap.size());
}

TEST_F(TestUbseMemAccount, ReturnsAccountMapCorrectlyWhenNodeIdNotEmptyAndWithAccount)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfo));
    const std::string nodeId = "Node0";
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    UbseBorrowAccountMap accountMap;
    UbseAllBorrowAccountInfo(nodeId, accountMap);
    EXPECT_EQ(NUMA_NUM, accountMap.size());
    EXPECT_EQ("Node0", accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowNodeId);
    EXPECT_EQ(NUMA_NUM - 1, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowNumaSizeList[0]);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowMemId[0]);
    EXPECT_EQ("Node0", accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowNodeId);
    EXPECT_EQ(NUMA_NUM - 1, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowNumaSizeList[0]);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowMemId[0]);
    EXPECT_EQ("Node0", accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowNodeId);
    EXPECT_EQ(NUMA_NUM - 1, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowNumaSizeList[0]);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowMemId[0]);
}

/*
 * 用例描述：
 * 当nodeId为空，查询借用账本统计信息返回正常
 * 测试步骤： 1.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息为空
 * 2.nodeId为空，调用UbseGetBorrowedLentInfo(nodeId, outList);
 * 3.判断返回值outList的大小为0
 * 4.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息不为空
 * 5.判断返回值outList是否符合预期
 * 预期结果：
 * 返回accountMap符合预期
 */
TEST_F(TestUbseMemAccount, ReturnsBorrowedLentInfoListCorrectlyWhenNodeIdEmptyWithEmptyAccount)
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> emptyNodeInfo{};
    MOCKER(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(emptyNodeInfo));
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfoReturnEmpty));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    const std::string nodeId;
    UbseBorrowedLentInfoList outList;
    UbseGetBorrowedLentInfo(nodeId, outList);
    EXPECT_EQ(0, outList.size());
}

TEST_F(TestUbseMemAccount, ReturnsBorrowedLentInfoListCorrectlyWhenNodeIdEmptyWithAccount)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfo));
    const std::string nodeId;
    UbseBorrowAccountMap accountMap;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    UbseBorrowedLentInfoList outList;
    UbseGetBorrowedLentInfo(nodeId, outList);
    EXPECT_EQ(MEMID_NUM, outList.size()); // 凑个数字，没有实际关联
    std::unordered_map<std::string, UbseNodeBorrowLentInfo> nodeBorrowLentInfoMap;
    std::unordered_map<std::string, uint64_t> borrowMap; // key=importNodeId,exportNodeId,numaId
    std::unordered_map<std::string, uint64_t> lentMap;   // exportNodeId,importNodeId,numaId
    for (const auto &item : outList) {
        nodeBorrowLentInfoMap.emplace(item.nodeId, item);
        for (const auto &borrowedItem : item.borrowedItem) {
            borrowMap[item.nodeId + borrowedItem.nodeId + std::to_string(borrowedItem.numaId)] = borrowedItem.size;
        }
        for (const auto &borrowedItem : item.lentItem) {
            lentMap[item.nodeId + borrowedItem.nodeId + std::to_string(borrowedItem.numaId)] = borrowedItem.size;
        }
    }
    EXPECT_EQ("Node0", nodeBorrowLentInfoMap["Node0"].nodeId);
    EXPECT_EQ(1, nodeBorrowLentInfoMap["Node0"].borrowedItem.size());
    EXPECT_EQ(0, nodeBorrowLentInfoMap["Node0"].lentItem.size());
    EXPECT_EQ("Node1", nodeBorrowLentInfoMap["Node1"].nodeId);
    EXPECT_EQ(0, nodeBorrowLentInfoMap["Node1"].borrowedItem.size());
    EXPECT_EQ(1, nodeBorrowLentInfoMap["Node1"].lentItem.size());
    EXPECT_EQ(MEM_SIZE * NUMA_NUM, borrowMap["Node0Node12"]);
}

/*
 * 用例描述： 当nodeId不为空，查询借用账本统计信息返回正常
 * 测试步骤： 1.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息为空
 * 2.nodeId为Node1，调用UbseGetBorrowedLentInfo(nodeId, outList);
 * 3.判断返回值outList的大小为0
 * 4.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息不为空
 * 5.判断返回值outList是否符合预期
 * 预期结果：
 * 返回accountMap符合预期
 */
TEST_F(TestUbseMemAccount, ReturnsBorrowedLentInfoListCorrectlyWhenNodeIdNotEmpty)
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> emptyNodeInfo{};
    MOCKER(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(emptyNodeInfo));
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfoReturnEmpty));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    const std::string nodeId = "Node0";
    UbseBorrowedLentInfoList outList;
    UbseGetBorrowedLentInfo(nodeId, outList);
    EXPECT_EQ(0, outList.size());
}

TEST_F(TestUbseMemAccount, ReturnsBorrowedLentInfoListCorrectlyWhenNodeIdNotEmptyWithAccount)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfo));
    const std::string nodeId = "Node0";
    UbseBorrowAccountMap accountMap;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    UbseBorrowedLentInfoList outList;
    UbseGetBorrowedLentInfo(nodeId, outList);
    EXPECT_EQ(1, outList.size());
    std::unordered_map<std::string, UbseNodeBorrowLentInfo> nodeBorrowLentInfoMap;
    std::unordered_map<std::string, uint64_t> borrowMap; // key=importNodeId,exportNodeId,numaId
    std::unordered_map<std::string, uint64_t> lentMap;   // exportNodeId,importNodeId,numaId
    for (const auto &item : outList) {
        nodeBorrowLentInfoMap.emplace(item.nodeId, item);
        for (const auto &borrowedItem : item.borrowedItem) {
            borrowMap[item.nodeId + borrowedItem.nodeId + std::to_string(borrowedItem.numaId)] = borrowedItem.size;
        }
        for (const auto &borrowedItem : item.lentItem) {
            lentMap[item.nodeId + borrowedItem.nodeId + std::to_string(borrowedItem.numaId)] = borrowedItem.size;
        }
    }
    EXPECT_EQ("Node0", nodeBorrowLentInfoMap["Node0"].nodeId);
    EXPECT_EQ(1, nodeBorrowLentInfoMap["Node0"].borrowedItem.size());
    EXPECT_EQ(0, nodeBorrowLentInfoMap["Node0"].lentItem.size());
    EXPECT_EQ(MEM_SIZE * NUMA_NUM, borrowMap["Node0Node12"]);
}

/*
 * 用例描述： 查询NUMA信息返回正常
 * 测试步骤： 1.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息;
 * 2.调用UbseAllNumaInfo;
 * 3.判断返回值为0
 * 预期结果： 返回值为0;
 */
TEST_F(TestUbseMemAccount, UbseAllNumaInfoFullProcedure)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfo));
    const std::string nodeId = "Node0";
    UbseBorrowAccountMap accountMap;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    std::vector<UbseNumaNodeInfo> numaNodeInfoVec{};
    auto ret = UbseAllNumaInfo(numaNodeInfoVec);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(numaNodeInfoVec.size(), NODE_NUM * NUMA_NUM);
}

/*
 * 用例描述：
 * 查询NUMA信息返回正常
 * 测试步骤： 1.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息为空;
 * 2.调用UbseAllNumaInfo;
 * 3.判断返回值为0;
 * 4.判断出参大小为空;
 * 预期结果： 1.返回值为0;
 * 2.出参大小为空;
 */
TEST_F(TestUbseMemAccount, UbseAllNumaInfoFullProcedureWithEmptyNumaInfo)
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> emptyNodeInfo{};
    MOCKER(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(emptyNodeInfo));
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfoReturnEmpty));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    std::vector<UbseNumaNodeInfo> numaNodeInfoVec{};
    auto ret = UbseAllNumaInfo(numaNodeInfoVec);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(numaNodeInfoVec.size(), 0);
}

uint32_t MockUbseGetMemShareDebtInfo([[maybe_unused]] const std::string &nodeId,
    NodeMemDebtInfoMap &memDebtInfoMap)
{
    UbseMemShareBorrowImportObj importObj{};
    UbseMemShareBorrowExportObj exportObj{};
    MockImportObj(importObj);
    importObj.importNodeId = BASE_NODEID + "0";
    MockExportObj(exportObj);
    UbseMemShareImportObjMap shareImportObjMap;
    shareImportObjMap[BASE_RESOURCEID + "0"] = importObj;
    UbseMemShareExportObjMap shareExportObjMap;
    shareExportObjMap[BASE_RESOURCEID + "0"] = exportObj;
    NodeMemDebtInfo debtInfo0{};
    NodeMemDebtInfo debtInfo1{};
    debtInfo0.shareImportObjMap = shareImportObjMap;
    debtInfo1.shareExportObjMap = shareExportObjMap;
    memDebtInfoMap[BASE_NODEID + "0"] = debtInfo0;
    memDebtInfoMap[BASE_NODEID + "1"] = debtInfo1;
    return 0;
}

/*
 * 用例描述：
 * 查询共享内存信息返回正常
 * 测试步骤: 1.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息;
 * 2.调用UbseAllShmAccountInfo;
 * 3.判断返回值为0
 * 预期结果：
 * 1.返回值为0;
 * 2.出参内容与构造值相同;
 */
TEST_F(TestUbseMemAccount, UbseAllShmAccountInfo)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemShareDebtInfo));
    const std::string nodeId;
    UbseBorrowAccountMap accountMap;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    UbseShmAccountMap outMap{};
    auto ret = UbseAllShmAccountInfo(outMap);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(outMap.begin()->first, "resourceId0");
    EXPECT_EQ(outMap.begin()->second.exportNode, "Node1");
    EXPECT_EQ(outMap.begin()->second.size, MEM_SIZE);
    EXPECT_EQ(outMap.begin()->second.exportMemId[0], 0);
    EXPECT_EQ(outMap.begin()->second.exportMemId[1], 1);
    EXPECT_EQ(outMap.begin()->second.importMap["Node0"][0], 0);
    EXPECT_EQ(outMap.begin()->second.importMap["Node0"][1], 1);
}

/*
 * 用例描述：
 * 查询共享内存信息返回正常
 * 测试步骤：1.MOCKER UbseCluster::GetMemInfoRemote，构造返回的账本信息为空;
 * 2.调用UbseAllShmAccountInfo;
 * 3.判断返回值为0
 * 预期结果：
 * 返回值为0
 */
TEST_F(TestUbseMemAccount, UbseAllShmAccountInfoWithEmptyShmInfo)
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> emptyNodeInfo{};
    MOCKER(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(emptyNodeInfo));
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemDebtInfoReturnEmpty));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    UbseShmAccountMap outMap{};
    auto ret = UbseAllShmAccountInfo(outMap);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(outMap.size(), 0);
}

uint32_t MockUbseGetMemSingleDebtInfo([[maybe_unused]] const std::string &nodeId,
    NodeMemDebtInfoMap &memDebtInfoMap)
{
    UbseMemFdBorrowImportObj importObj{};
    UbseMemNumaBorrowImportObj memNumaBorrowImportObj{};
    UbseMemAddrBorrowImportObj memAddrBorrowImportObj{};
    MockImportObj(importObj);
    MockImportObj(memNumaBorrowImportObj);
    MockImportObj(memAddrBorrowImportObj);
    UbseMemFdImportObjMap fdImportObjMap{};
    UbseMemNumaImportObjMap numaImportObjMap{};
    UbseMemAddrImportObjMap addrImportObjMap{};
    fdImportObjMap[BASE_RESOURCEID + "0"] = importObj;
    numaImportObjMap[BASE_RESOURCEID + "1"] = memNumaBorrowImportObj;
    addrImportObjMap[BASE_RESOURCEID + "2"] = memAddrBorrowImportObj;
    NodeMemDebtInfo debtInfo0{};
    NodeMemDebtInfo debtInfo1{};
    debtInfo0.fdImportObjMap = fdImportObjMap;
    debtInfo0.numaImportObjMap = numaImportObjMap;
    debtInfo0.addrImportObjMap = addrImportObjMap;
    memDebtInfoMap[BASE_NODEID + "0"] = debtInfo0;
    memDebtInfoMap[BASE_NODEID + "1"] = debtInfo1;
    return 0;
}

TEST_F(TestUbseMemAccount, SingleLedgerRefill)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemSingleDebtInfo));
    const std::string nodeId;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    UbseBorrowAccountMap accountMap;
    UbseAllBorrowAccountInfo(nodeId, accountMap);
    EXPECT_EQ(NUMA_NUM, accountMap.size());
    EXPECT_EQ("Node0", accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowNodeId);
    EXPECT_EQ(NUMA_NUM - 1, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowNumaSizeList[0]);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "0" + "_" + BASE_NODEID + "0_2"].borrowMemId[0]);
    EXPECT_EQ("Node0", accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowNodeId);
    EXPECT_EQ(NUMA_NUM - 1, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowNumaSizeList[0]);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "1" + "_" + BASE_NODEID + "0_1"].borrowMemId[0]);
    EXPECT_EQ("Node0", accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowNodeId);
    EXPECT_EQ(NUMA_NUM - 1, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowNumaIdList[0]);
    EXPECT_EQ(MEM_SIZE, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowNumaSizeList[0]);
    EXPECT_EQ(0, accountMap[BASE_RESOURCEID + "2" + "_" + BASE_NODEID + "0_4"].borrowMemId[0]);
}

uint32_t MockUbseGetMemShareDebtInfoForSameNumaBorrows([[maybe_unused]] const std::string &nodeId,
    NodeMemDebtInfoMap &memDebtInfoMap)
{
    UbseMemShareBorrowExportObj exportObj{};
    MockExportObj(exportObj);
    UbseMemShareExportObjMap shareExportObjMap;
    shareExportObjMap[BASE_RESOURCEID + "0"] = exportObj;
    shareExportObjMap[BASE_RESOURCEID + "1"] = exportObj;
    NodeMemDebtInfo debtInfo{};
    debtInfo.shareExportObjMap = shareExportObjMap;
    memDebtInfoMap[BASE_NODEID + "1"] = debtInfo;
    return 0;
}

/*
 * 用例描述：
 * DTS2025091030893查询numa共享内存信息返回正常
 * 测试步骤：1.MOCKER UbseGetMemDebtInfo，构造同一个节点同一个numa两次借出SHARE内存;
 * 2.调用UbseAllNumaInfo;
 * 3.判断返回值为0，且mMemShared为两次借用的和
 * 预期结果：
 * 返回值为0
 */

TEST_F(TestUbseMemAccount, UbseAllNumaInfoForShareSameNumaBorrows)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemShareDebtInfoForSameNumaBorrows));
    const std::string nodeId;
    UbseBorrowAccountMap accountMap;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    std::vector<UbseNumaNodeInfo> numaNodeInfoVec{};
    auto ret = UbseAllNumaInfo(numaNodeInfoVec);
    EXPECT_EQ(ret, 0);
    for (const auto &numaInfo : numaNodeInfoVec) {
        if (numaInfo.nodeId == BASE_NODEID + "1" && numaInfo.socketId == 1 && numaInfo.numaId == 0) {
            EXPECT_EQ(numaInfo.mMemShared, MEM_SIZE * 2);
        }
    }
}

uint32_t MockUbseGetMemNumaDebtInfoForSameNumaBorrows([[maybe_unused]] const std::string &nodeId,
    NodeMemDebtInfoMap &memDebtInfoMap)
{
    UbseMemNumaBorrowExportObj exportObj{};
    MockExportObj(exportObj);
    UbseMemNumaExportObjMap numaExportObjMap;
    numaExportObjMap[BASE_RESOURCEID + "0"] = exportObj;
    numaExportObjMap[BASE_RESOURCEID + "1"] = exportObj;
    NodeMemDebtInfo debtInfo{};
    debtInfo.numaExportObjMap = numaExportObjMap;
    memDebtInfoMap[BASE_NODEID + "1" + "_" + BASE_NODEID + "0"] = debtInfo;

    UbseMemNumaBorrowImportObj importObj{};
    MockImportObj(importObj);
    UbseMemNumaImportObjMap numaImportObjMap;
    numaImportObjMap[BASE_RESOURCEID + "0"] = importObj;
    numaImportObjMap[BASE_RESOURCEID + "1"] = importObj;
    NodeMemDebtInfo debtInfo1{};
    debtInfo1.numaImportObjMap = numaImportObjMap;
    memDebtInfoMap[BASE_NODEID + "0"] = debtInfo1;
    return 0;
}

/*
 * 用例描述：
 * DTS2025082836736查询numa内存信息返回正常
 * 测试步骤：1.MOCKER UbseGetMemDebtInfo，构造同一个节点同一个numa两次借出numa内存;
 * 2.调用UbseAllNumaInfo;
 * 3.判断返回值为0，且mMemLent、mMemBorrowed为两次借用的和
 * 预期结果：
 * 返回值为0
 */

TEST_F(TestUbseMemAccount, UbseAllNumaInfoForNumaSameNumaBorrows)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemNumaDebtInfoForSameNumaBorrows));
    const std::string nodeId;
    UbseBorrowAccountMap accountMap;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    std::vector<UbseNumaNodeInfo> numaNodeInfoVec{};
    auto ret = UbseAllNumaInfo(numaNodeInfoVec);
    EXPECT_EQ(ret, 0);
    for (const auto &numaInfo : numaNodeInfoVec) {
        if (numaInfo.nodeId == BASE_NODEID + "1" && numaInfo.socketId == 1 && numaInfo.numaId == 0) {
            EXPECT_EQ(numaInfo.mMemLent, MEM_SIZE * 2);
        }
        if (numaInfo.nodeId == BASE_NODEID + "0" && numaInfo.socketId == 0 && numaInfo.numaId == 2) {
            EXPECT_EQ(numaInfo.mMemBorrowed, MEM_SIZE * 2);
        }
    }
}

/*
 * 用例描述：
 * DTS2025082830291查询numa借用的lentSize返回正常
 * 测试步骤：1.MOCKER UbseGetMemDebtInfo，构造同一个节点同一个numa两次借出numa内存;
 * 2.调用UbseAllNumaInfo;
 * 3.判断返回值为0，且mMemLent、mMemBorrowed为两次借用的和
 * 预期结果：
 * 返回值为0
 */
TEST_F(TestUbseMemAccount, UbseAllBorrowAccountInfoLentSizeCorrect)
{
    uint32_t (*func)(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap) =
        &UbseGetMemDebtInfo;
    MOCKER_CPP(func).stubs().will(invoke(MockUbseGetMemNumaDebtInfoForSameNumaBorrows));
    const std::string nodeId;
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetAllNodes).stubs().will(invoke(MockGetAllNodes));
    MOCKER_CPP(&ubse::config::UbseGetUInt).stubs().will(invoke(MockUbseGetUInt));
    UbseBorrowAccountMap accountMap;
    UbseAllBorrowAccountInfo(nodeId, accountMap);
    for (const auto &accountMap1 : accountMap) {
        EXPECT_EQ(accountMap1.second.lentNumaSizeList.size(), 1);
        EXPECT_EQ(accountMap1.second.lentNumaSizeList[0], MEM_SIZE);
    }
}
} // namespace ubse::obj::ut