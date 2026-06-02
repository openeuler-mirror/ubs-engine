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

#include "test_ubse_mem_debt_info_query.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_node_controller.h"
#include "message/ubse_mem_debt_info_query_req_simpo.h"

namespace ubse::mem_controller::ut {
using namespace com;
using namespace context;
using namespace ubse::election;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::debt;
using namespace ubse::mem::def;
using namespace ubse::adapter_plugins::mmi;

void TestUbseMemDebtInfoQuery::SetUp()
{
    Test::SetUp();
}

void TestUbseMemDebtInfoQuery::TearDown()
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemDebtInfoQuery, UbseGetMemDebtInfo)
{
    std::string nodeId = "1";
    NodeMemDebtInfoMap memDebtInfoMap{};
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    const auto getModuleFunc = &context::UbseContext::GetModule<UbseComModule>;
    MOCKER(getModuleFunc).stubs().will(returnValue(nullModule)).then(returnValue(module));

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::NodeMemDebtInfoQueryReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    EXPECT_EQ(UbseGetMemDebtInfo(nodeId, memDebtInfoMap), UBSE_ERROR);
    EXPECT_EQ(UbseGetMemDebtInfo(nodeId, memDebtInfoMap), UBSE_ERROR_MODULE_LOAD_FAILED);
    EXPECT_EQ(UbseGetMemDebtInfo(nodeId, memDebtInfoMap), UBSE_ERROR);
    EXPECT_EQ(UbseGetMemDebtInfo(nodeId, memDebtInfoMap), UBSE_OK);
}

void AddFdObj(const std::string& importNodeId, const std::string& exportNodeId, const std::string& name)
{
    // 构造 fdImportObj
    UbseMemFdBorrowImportObj fdImportObj{};
    fdImportObj.req.name = name;
    fdImportObj.req.importNodeId = importNodeId;
    UbseMemDebtNumaInfo fdImportNmaInfo{.nodeId = importNodeId, .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo fdExportNmaInfo1{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo fdExportNmaInfo2{.nodeId = exportNodeId, .socketId = 0, .numaId = 1, .size = 128};
    fdImportObj.algoResult.exportNumaInfos.emplace_back(fdExportNmaInfo1);
    fdImportObj.algoResult.exportNumaInfos.emplace_back(fdExportNmaInfo2);
    fdImportObj.algoResult.importNumaInfos.emplace_back(fdImportNmaInfo);
    fdImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    // 构造 fdExportObj
    UbseMemFdBorrowExportObj fdExportObj{};
    fdExportObj.req = fdImportObj.req;
    fdExportObj.algoResult = fdImportObj.algoResult;
    fdExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 填充 ledger
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(
        exportNodeId, name + "_" + importNodeId, fdExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(importNodeId, name,
                                                                                        fdImportObj);
}

void AddNumaObj(const std::string& importNodeId, const std::string& exportNodeId, const std::string& name)
{
    // 构造 numaImportObj
    UbseMemNumaBorrowImportObj numaImportObj{};
    numaImportObj.req.name = name;
    numaImportObj.req.importNodeId = importNodeId;
    UbseMemDebtNumaInfo numaImportNmaInfo{.nodeId = importNodeId, .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo numaExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    numaImportObj.algoResult.exportNumaInfos.emplace_back(numaExportNmaInfo);
    numaImportObj.algoResult.importNumaInfos.emplace_back(numaImportNmaInfo);
    numaImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    // 构造 numaExportObj
    UbseMemNumaBorrowExportObj numaExportObj{};
    numaExportObj.req = numaImportObj.req;
    numaExportObj.algoResult = numaImportObj.algoResult;
    numaExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 填充 ledger
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource(
        exportNodeId, name + "_" + importNodeId, numaExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource(importNodeId, name,
                                                                                          numaImportObj);
}

void AddAddrObj(const std::string& importNodeId, const std::string& exportNodeId, const std::string& name)
{
    // 构造 addrImportObj
    UbseMemAddrBorrowImportObj addrImportObj{};
    addrImportObj.req.name = name;
    addrImportObj.req.importNodeId = importNodeId;
    UbseMemDebtNumaInfo addrImportNmaInfo{.nodeId = importNodeId, .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo addrExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    addrImportObj.algoResult.exportNumaInfos.emplace_back(addrExportNmaInfo);
    addrImportObj.algoResult.importNumaInfos.emplace_back(addrImportNmaInfo);
    addrImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    // 构造 addrExportObj
    UbseMemAddrBorrowExportObj addrExportObj{};
    addrExportObj.req = addrImportObj.req;
    addrExportObj.algoResult = addrImportObj.algoResult;
    addrExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 填充 ledger
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(
        exportNodeId, name + "_" + importNodeId, addrExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(importNodeId, name,
                                                                                          addrImportObj);
}

void AddShareImportObj(const std::string& importNodeId, const std::string& name, uint64_t memId)
{
    UbseMemShareBorrowImportObj shareImportObj{};
    shareImportObj.req.name = name;
    shareImportObj.importNodeId = importNodeId;
    shareImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    shareImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = memId});

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(importNodeId, name,
                                                                                           shareImportObj);
}

void BuildDebtMap()
{
    std::string importNodeId = "1";
    std::string exportNodeId = "2";
    // 添加Fd账本
    AddFdObj("1", "2", "fdSuccess");
    AddFdObj("3", "1", "fdSuccess");
    // 添加Numa账本
    AddNumaObj("1", "2", "numaSuccess");
    // 添加Addr账本
    AddAddrObj("1", "2", "addrSuccess");
}

std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> BuildNodeInfos()
{
    ubse::nodeController::UbseNodeInfo node1{.slotId = 1, .hostName = "node1"};
    ubse::nodeController::UbseNodeInfo node2{.slotId = 2, .hostName = "node2"};
    ubse::nodeController::UbseNodeInfo node3{.slotId = 3, .hostName = "node3"};
    return {{"1", node1}, {"2", node2}, {"3", node3}};
}

TEST_F(TestUbseMemDebtInfoQuery, UbseMemNodeBorrowQueryWhenSuccess)
{
    std::vector<UbseNodeBorrowInfo> nodeBorrowInfo{};
    UbseRoleInfo roleInfo{};
    roleInfo.nodeRole = ELECTION_ROLE_MASTER;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    BuildDebtMap();
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos = BuildNodeInfos();
    MOCKER_CPP(&nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    auto ret = UbseMemNodeBorrowQuery(nodeBorrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(nodeBorrowInfo.size(), 2); // 1向2借用 3向1借用
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemDebtInfoQuery, UbseMemNodeBorrowQueryWhenNotMaster)
{
    std::vector<UbseNodeBorrowInfo> nodeBorrowInfo{};
    UbseRoleInfo roleInfo{};
    roleInfo.nodeRole = ELECTION_ROLE_AGENT;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseMemNodeBorrowQuery(nodeBorrowInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_TRUE(nodeBorrowInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseMemNodeBorrowQueryWhenGetNodeInfoFailed)
{
    std::vector<UbseNodeBorrowInfo> nodeBorrowInfo{};
    UbseRoleInfo roleInfo{};
    roleInfo.nodeRole = ELECTION_ROLE_MASTER;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    BuildDebtMap();
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos{};
    MOCKER_CPP(&nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    auto ret = UbseMemNodeBorrowQuery(nodeBorrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(nodeBorrowInfo.empty());
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

TEST_F(TestUbseMemDebtInfoQuery, UbseMemNodeBorrowQueryWhenGetCurrentNodeInfoFailed)
{
    std::vector<UbseNodeBorrowInfo> nodeBorrowInfo{};
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseMemNodeBorrowQuery(nodeBorrowInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_TRUE(nodeBorrowInfo.empty());
}

template <typename T, typename = void>
struct HasImportResultsInStruct : std::false_type {
};
template <typename T>
struct HasImportResultsInStruct<T, std::void_t<decltype(std::declval<T>().status.importResults)>> : std::true_type {
};
TEST_F(TestUbseMemDebtInfoQuery, UbseMemImportObjHasImportResults)
{
    bool hasImportResults = true;
    if (!HasImportResultsInStruct<UbseMemNumaBorrowImportObj>::value) {
        hasImportResults = false;
    }
    if (!HasImportResultsInStruct<UbseMemFdBorrowImportObj>::value) {
        hasImportResults = false;
    }
    if (!HasImportResultsInStruct<UbseMemShareBorrowImportObj>::value) {
        hasImportResults = false;
    }
    if (!HasImportResultsInStruct<UbseMemAddrBorrowImportObj>::value) {
        hasImportResults = false;
    }
    EXPECT_TRUE(hasImportResults);
}

TEST_F(TestUbseMemDebtInfoQuery, UbseMemShmGetShouldFilterImportByRequestImportNodeId)
{
    UbseRoleInfo roleInfo{};
    roleInfo.nodeRole = ELECTION_ROLE_MASTER;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));

    AddShareImportObj("1", "shareFilter", 101);
    AddShareImportObj("2", "shareFilter", 202);

    UbseMemDebtQueryRequest request{.name = "shareFilter", .importNodeId = "1"};
    UbseMemShmDesc shmDesc{};
    EXPECT_EQ(UbseMemShmGet(request, shmDesc), UBSE_OK);
    ASSERT_EQ(shmDesc.importDesc.size(), 1);
    ASSERT_EQ(shmDesc.importDesc[0].memIds.size(), 1);
    EXPECT_EQ(shmDesc.importDesc[0].memIds[0], 101);
}

// Test for UbseMemGetMemIdByImport - FD_BORROW type success case
TEST_F(TestUbseMemDebtInfoQuery, UbseMemGetMemIdByImport_FdSuccess)
{
    // 构造测试数据
    std::string importNodeId = "1";
    std::string exportNodeId = "2";
    std::string name = "test_fd";
    uint64_t importMemId = 1001;
    uint64_t exportMemId = 2001;

    // 构造 fdImportObj
    UbseMemFdBorrowImportObj fdImportObj{};
    fdImportObj.req.name = name;
    fdImportObj.req.importNodeId = importNodeId;
    fdImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    fdImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = importMemId});
    fdImportObj.exportObmmInfo.emplace_back(UbseMemObmmInfo{.memId = exportMemId});
    UbseMemDebtNumaInfo fdExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    fdImportObj.algoResult.exportNumaInfos.emplace_back(fdExportNmaInfo);

    // 构造 fdExportObj
    UbseMemFdBorrowExportObj fdExportObj{};
    fdExportObj.req = fdImportObj.req;
    fdExportObj.algoResult = fdImportObj.algoResult;
    fdExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 填充 ledger
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(
        exportNodeId, name + "_" + importNodeId, fdExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(importNodeId, name,
                                                                                        fdImportObj);

    // 构造请求和响应
    UbseMemIdQueryRequest request{};
    request.borrowType = static_cast<uint32_t>(UbseMemBorrowType::FD_BORROW);
    request.name = name;
    request.importNodeId = importNodeId;
    request.importMemId = importMemId;

    UbseExportMemDesc memDesc{};

    // 执行测试
    uint32_t ret = UbseMemGetMemIdByImport(request, memDesc);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memDesc.exportSlotId, 2);
    EXPECT_EQ(memDesc.exportMemId, exportMemId);

    // 清理数据
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

// Test for UbseMemGetMemIdByImport - NUMA_BORROW type success case
TEST_F(TestUbseMemDebtInfoQuery, UbseMemGetMemIdByImport_NumaSuccess)
{
    // 构造测试数据
    std::string importNodeId = "1";
    std::string exportNodeId = "3";
    std::string name = "test_numa";
    uint64_t importMemId = 1002;
    uint64_t exportMemId = 3002;

    // 构造 numaImportObj
    UbseMemNumaBorrowImportObj numaImportObj{};
    numaImportObj.req.name = name;
    numaImportObj.req.importNodeId = importNodeId;
    numaImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    numaImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = importMemId});
    numaImportObj.exportObmmInfo.emplace_back(UbseMemObmmInfo{.memId = exportMemId});
    UbseMemDebtNumaInfo numaExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    numaImportObj.algoResult.exportNumaInfos.emplace_back(numaExportNmaInfo);

    // 构造 numaExportObj
    UbseMemNumaBorrowExportObj numaExportObj{};
    numaExportObj.req = numaImportObj.req;
    numaExportObj.algoResult = numaImportObj.algoResult;
    numaExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 填充 ledger
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource(
        exportNodeId, name + "_" + importNodeId, numaExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource(importNodeId, name,
                                                                                          numaImportObj);

    // 构造请求和响应
    UbseMemIdQueryRequest request{};
    request.borrowType = static_cast<uint32_t>(UbseMemBorrowType::NUMA_BORROW);
    request.name = name;
    request.importNodeId = importNodeId;
    request.importMemId = importMemId;

    UbseExportMemDesc memDesc{};

    // 执行测试
    uint32_t ret = UbseMemGetMemIdByImport(request, memDesc);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memDesc.exportSlotId, 3);
    EXPECT_EQ(memDesc.exportMemId, exportMemId);

    // 清理数据
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

// Test for UbseMemGetMemIdByImport - SHM_BORROW type success case
TEST_F(TestUbseMemDebtInfoQuery, UbseMemGetMemIdByImport_ShmSuccess)
{
    // 构造测试数据
    std::string importNodeId = "1";
    std::string exportNodeId = "2";
    std::string name = "test_shm";
    uint64_t importMemId = 1003;
    uint64_t exportMemId = 2003;

    // 构造 shareImportObj
    UbseMemShareBorrowImportObj shareImportObj{};
    shareImportObj.req.name = name;
    shareImportObj.importNodeId = importNodeId;
    shareImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    shareImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = importMemId});
    shareImportObj.exportObmmInfo.emplace_back(UbseMemObmmInfo{.memId = exportMemId});
    UbseMemDebtNumaInfo shareExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    shareImportObj.algoResult.exportNumaInfos.emplace_back(shareExportNmaInfo);

    // 构造 shareExportObj
    UbseMemShareBorrowExportObj shareExportObj{};
    shareExportObj.req.name = name;
    shareExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    shareExportObj.status.exportObmmInfo.emplace_back(UbseMemObmmInfo{.memId = exportMemId});

    // 填充 ledger
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(exportNodeId, name,
                                                                                           shareExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(importNodeId, name,
                                                                                           shareImportObj);

    // 构造请求和响应
    UbseMemIdQueryRequest request{};
    request.borrowType = static_cast<uint32_t>(UbseMemBorrowType::SHM_BORROW);
    request.name = name;
    request.importNodeId = importNodeId;
    request.importMemId = importMemId;

    UbseExportMemDesc memDesc{};

    // 执行测试
    uint32_t ret = UbseMemGetMemIdByImport(request, memDesc);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memDesc.exportSlotId, 2);
    EXPECT_EQ(memDesc.exportMemId, exportMemId);

    // 清理数据
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

// Test for UbseMemGetMemIdByImport - unsupported borrow type
TEST_F(TestUbseMemDebtInfoQuery, UbseMemGetMemIdByImport_UnsupportedType)
{
    // 构造请求和响应
    UbseMemIdQueryRequest request{};
    request.borrowType = 999; // 不支持的类型
    request.name = "test_unsupported";
    request.importNodeId = "1";
    request.importMemId = 1001;

    UbseExportMemDesc memDesc{};

    // 执行测试
    uint32_t ret = UbseMemGetMemIdByImport(request, memDesc);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

// Test for UbseMemGetMemIdByImport - import mem id not found
TEST_F(TestUbseMemDebtInfoQuery, UbseMemGetMemIdByImport_ImportMemIdNotFound)
{
    // 构造测试数据
    std::string importNodeId = "1";
    std::string exportNodeId = "2";
    std::string name = "test_not_found";
    uint64_t importMemId = 1001;
    uint64_t wrongImportMemId = 9999;
    uint64_t exportMemId = 2001;

    // 构造 fdImportObj
    UbseMemFdBorrowImportObj fdImportObj{};
    fdImportObj.req.name = name;
    fdImportObj.req.importNodeId = importNodeId;
    fdImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    fdImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = importMemId});
    fdImportObj.exportObmmInfo.emplace_back(UbseMemObmmInfo{.memId = exportMemId});
    UbseMemDebtNumaInfo fdExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    fdImportObj.algoResult.exportNumaInfos.emplace_back(fdExportNmaInfo);

    // 构造 fdExportObj
    UbseMemFdBorrowExportObj fdExportObj{};
    fdExportObj.req = fdImportObj.req;
    fdExportObj.algoResult = fdImportObj.algoResult;
    fdExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 填充 ledger
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(
        exportNodeId, name + "_" + importNodeId, fdExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(importNodeId, name,
                                                                                        fdImportObj);

    // 构造请求和响应
    UbseMemIdQueryRequest request{};
    request.borrowType = static_cast<uint32_t>(UbseMemBorrowType::FD_BORROW);
    request.name = name;
    request.importNodeId = importNodeId;
    request.importMemId = wrongImportMemId;

    UbseExportMemDesc memDesc{};

    // 执行测试
    uint32_t ret = UbseMemGetMemIdByImport(request, memDesc);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);

    // 清理数据
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

/**
 * 添加 FD 类型的 Import 对象，用于测试按 exportNodeId 查询
 */
void AddFdImportObjWithExportNode(const std::string& importNodeId, const std::string& exportNodeId,
                                  const std::string& name, UbseMemState state = UBSE_MEM_IMPORT_SUCCESS,
                                  const std::vector<uint64_t>& memIds = {100})
{
    UbseMemFdBorrowImportObj fdImportObj{};
    fdImportObj.req.name = name;
    fdImportObj.req.importNodeId = importNodeId;
    fdImportObj.status.state = state;
    for (auto memId : memIds) {
        fdImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = memId});
    }
    UbseMemDebtNumaInfo fdExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    fdImportObj.algoResult.exportNumaInfos.emplace_back(fdExportNmaInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(importNodeId, name,
                                                                                        fdImportObj);
}

/**
 * 添加 NUMA 类型的 Import 对象，用于测试按 exportNodeId 查询
 */
void AddNumaImportObjWithExportNode(const std::string& importNodeId, const std::string& exportNodeId,
                                    const std::string& name, UbseMemState state = UBSE_MEM_IMPORT_SUCCESS,
                                    const std::vector<int64_t>& numaIds = {0})
{
    UbseMemNumaBorrowImportObj numaImportObj{};
    numaImportObj.req.name = name;
    numaImportObj.req.importNodeId = importNodeId;
    numaImportObj.status.state = state;
    for (auto numaId : numaIds) {
        numaImportObj.status.importResults.emplace_back(UbseMemImportResult{.numaId = numaId});
    }
    UbseMemDebtNumaInfo numaExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    numaImportObj.algoResult.exportNumaInfos.emplace_back(numaExportNmaInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource(importNodeId, name,
                                                                                          numaImportObj);
}

/**
 * 添加 Share 类型的 Import 对象，用于测试按 exportNodeId 查询
 */
void AddShareImportObjWithExportNode(const std::string& importNodeId, const std::string& exportNodeId,
                                     const std::string& name, UbseMemState state = UBSE_MEM_IMPORT_SUCCESS,
                                     const std::vector<uint64_t>& memIds = {100})
{
    UbseMemShareBorrowImportObj shareImportObj{};
    shareImportObj.req.name = name;
    shareImportObj.importNodeId = importNodeId;
    shareImportObj.status.state = state;
    for (auto memId : memIds) {
        shareImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = memId});
    }
    UbseMemDebtNumaInfo shareExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    shareImportObj.algoResult.exportNumaInfos.emplace_back(shareExportNmaInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(importNodeId, name,
                                                                                           shareImportObj);
}

/**
 * 添加带端口信息的 FD 类型 Import 对象，用于测试端口故障场景
 */
void AddFdImportObjWithPortInfo(const std::string& importNodeId, const std::string& exportNodeId,
                                const std::string& name, UbseMemState state, uint32_t chipId, uint32_t portId,
                                const std::vector<uint64_t>& memIds = {100})
{
    UbseMemFdBorrowImportObj fdImportObj{};
    fdImportObj.req.name = name;
    fdImportObj.req.importNodeId = importNodeId;
    fdImportObj.status.state = state;
    for (auto memId : memIds) {
        fdImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = memId});
    }
    UbseMemDebtNumaInfo fdExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    fdImportObj.algoResult.exportNumaInfos.emplace_back(fdExportNmaInfo);
    UbseMemDebtNumaInfo fdImportNmaInfo{
        .nodeId = importNodeId, .socketId = 0, .numaId = 0, .size = 128, .portId = portId, .chipId = chipId};
    fdImportObj.algoResult.importNumaInfos.emplace_back(fdImportNmaInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(importNodeId, name,
                                                                                        fdImportObj);
}

/**
 * 添加带端口信息的 NUMA 类型 Import 对象，用于测试端口故障场景
 */
void AddNumaImportObjWithPortInfo(const std::string& importNodeId, const std::string& exportNodeId,
                                  const std::string& name, UbseMemState state, uint32_t chipId, uint32_t portId,
                                  const std::vector<int64_t>& numaIds = {0})
{
    UbseMemNumaBorrowImportObj numaImportObj{};
    numaImportObj.req.name = name;
    numaImportObj.req.importNodeId = importNodeId;
    numaImportObj.status.state = state;
    for (auto numaId : numaIds) {
        numaImportObj.status.importResults.emplace_back(UbseMemImportResult{.numaId = numaId});
    }
    UbseMemDebtNumaInfo numaExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    numaImportObj.algoResult.exportNumaInfos.emplace_back(numaExportNmaInfo);
    UbseMemDebtNumaInfo numaImportNmaInfo{
        .nodeId = importNodeId, .socketId = 0, .numaId = 0, .size = 128, .portId = portId, .chipId = chipId};
    numaImportObj.algoResult.importNumaInfos.emplace_back(numaImportNmaInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource(importNodeId, name,
                                                                                          numaImportObj);
}

/**
 * 添加带端口信息的 Share 类型 Import 对象，用于测试端口故障场景
 */
void AddShareImportObjWithPortInfo(const std::string& importNodeId, const std::string& exportNodeId,
                                   const std::string& name, UbseMemState state, uint32_t chipId, uint32_t portId,
                                   const std::vector<uint64_t>& memIds = {100})
{
    UbseMemShareBorrowImportObj shareImportObj{};
    shareImportObj.req.name = name;
    shareImportObj.importNodeId = importNodeId;
    shareImportObj.status.state = state;
    for (auto memId : memIds) {
        shareImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = memId});
    }
    UbseMemDebtNumaInfo shareExportNmaInfo{.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128};
    shareImportObj.algoResult.exportNumaInfos.emplace_back(shareExportNmaInfo);
    UbseMemDebtNumaInfo shareImportNmaInfo{
        .nodeId = importNodeId, .socketId = 0, .numaId = 0, .size = 128, .portId = portId, .chipId = chipId};
    shareImportObj.algoResult.importNumaInfos.emplace_back(shareImportNmaInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(importNodeId, name,
                                                                                           shareImportObj);
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdImportHandleByExportNodeId_NoImportData)
{
    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdImportHandleByExportNodeId_Success)
{
    AddFdImportObjWithExportNode("1", "2", "fd1", UBSE_MEM_IMPORT_SUCCESS, {100, 200});
    AddFdImportObjWithExportNode("1", "3", "fd2", UBSE_MEM_IMPORT_SUCCESS, {300});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "fd1");
    EXPECT_EQ(handleInfo[0].memIds.size(), 2);
    EXPECT_NE(handleInfo[0].memIds.find(100), handleInfo[0].memIds.end());
    EXPECT_NE(handleInfo[0].memIds.find(200), handleInfo[0].memIds.end());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdImportHandleByExportNodeId_RunningState)
{
    AddFdImportObjWithExportNode("1", "2", "fdRunning", UBSE_MEM_IMPORT_RUNNING, {100});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "fdRunning");
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdImportHandleByExportNodeId_WrongStateFiltered)
{
    AddFdImportObjWithExportNode("1", "2", "fdInit", UBSE_MEM_STATE_INIT, {100});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdImportHandleByExportNodeId_ExportNodeIdMismatch)
{
    AddFdImportObjWithExportNode("1", "3", "fdMismatch", UBSE_MEM_IMPORT_SUCCESS, {100});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdImportHandleByExportNodeId_EmptyExportNumaInfos)
{
    UbseMemFdBorrowImportObj fdImportObj{};
    fdImportObj.req.name = "fdEmptyExport";
    fdImportObj.req.importNodeId = "1";
    fdImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    fdImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = 100});
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("1", "fdEmptyExport",
                                                                                        fdImportObj);

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryNumaImportHandleByExportNodeId_Success)
{
    AddNumaImportObjWithExportNode("1", "2", "numa1", UBSE_MEM_IMPORT_SUCCESS, {0, 1});

    NumaHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryNumaImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "numa1");
    EXPECT_EQ(handleInfo[0].numaIds.size(), 2);
    EXPECT_NE(handleInfo[0].numaIds.find(0), handleInfo[0].numaIds.end());
    EXPECT_NE(handleInfo[0].numaIds.find(1), handleInfo[0].numaIds.end());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryNumaImportHandleByExportNodeId_NoImportData)
{
    NumaHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryNumaImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryShareImportHandleByExportNodeId_Success)
{
    AddShareImportObjWithExportNode("1", "2", "share1", UBSE_MEM_IMPORT_SUCCESS, {500, 600});

    ShareHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "share1");
    EXPECT_EQ(handleInfo[0].memIds.size(), 2);
    EXPECT_NE(handleInfo[0].memIds.find(500), handleInfo[0].memIds.end());
    EXPECT_NE(handleInfo[0].memIds.find(600), handleInfo[0].memIds.end());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryShareImportHandleByExportNodeId_MultipleObjs)
{
    AddShareImportObjWithExportNode("1", "2", "shareA", UBSE_MEM_IMPORT_SUCCESS, {100});
    AddShareImportObjWithExportNode("1", "2", "shareB", UBSE_MEM_IMPORT_RUNNING, {200});
    AddShareImportObjWithExportNode("1", "3", "shareC", UBSE_MEM_IMPORT_SUCCESS, {300});
    AddShareImportObjWithExportNode("1", "2", "shareD", UBSE_MEM_STATE_INIT, {400});

    ShareHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryShareImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 2);
    EXPECT_EQ(handleInfo[0].name, "shareB");
    EXPECT_EQ(handleInfo[1].name, "shareA");
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_InvalidChipIdFormat)
{
    AddFdImportObjWithPortInfo("1", "2", "fdFault", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {100});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "invalid_chip", "5", handleInfo), UBSE_ERROR_INVAL);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_NoImportData)
{
    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_FaultPortCollected)
{
    AddFdImportObjWithPortInfo("1", "2", "fdFault1", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {100, 200});
    AddFdImportObjWithPortInfo("1", "2", "fdFault2", UBSE_MEM_IMPORT_SUCCESS, 1, 6, {300});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "fdFault1");
    EXPECT_EQ(handleInfo[0].memIds.size(), 2);
    EXPECT_NE(handleInfo[0].memIds.find(100), handleInfo[0].memIds.end());
    EXPECT_NE(handleInfo[0].memIds.find(200), handleInfo[0].memIds.end());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_PortNotMatched)
{
    AddFdImportObjWithPortInfo("1", "2", "fdNormal", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {100});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "3", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_ChipIdMismatch)
{
    AddFdImportObjWithPortInfo("1", "2", "fdOtherChip", UBSE_MEM_IMPORT_SUCCESS, 2, 5, {100});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_WrongStateFiltered)
{
    AddFdImportObjWithPortInfo("1", "2", "fdInit", UBSE_MEM_STATE_INIT, 1, 5, {100});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_SinglePortMatched)
{
    AddFdImportObjWithPortInfo("1", "2", "fdAllFault", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {100});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "fdAllFault");
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQuerySharePortFaultHandleInfo_FaultPortCollected)
{
    AddShareImportObjWithPortInfo("1", "2", "shareFault", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {500});

    ShareHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQuerySharePortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "shareFault");
    EXPECT_EQ(handleInfo[0].memIds.size(), 1);
    EXPECT_NE(handleInfo[0].memIds.find(500), handleInfo[0].memIds.end());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQuerySharePortFaultHandleInfo_PortNotMatched)
{
    AddShareImportObjWithPortInfo("1", "2", "shareNormal", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {500});

    ShareHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQuerySharePortFaultHandleInfo("1", "1", "3", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryNumaPortFaultHandleInfo_FaultPortCollected)
{
    AddNumaImportObjWithPortInfo("1", "2", "numaFault", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {0, 1});

    NumaHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryNumaPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "numaFault");
    EXPECT_EQ(handleInfo[0].numaIds.size(), 2);
    EXPECT_NE(handleInfo[0].numaIds.find(0), handleInfo[0].numaIds.end());
    EXPECT_NE(handleInfo[0].numaIds.find(1), handleInfo[0].numaIds.end());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryNumaPortFaultHandleInfo_PortNotMatched)
{
    AddNumaImportObjWithPortInfo("1", "2", "numaNormal", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {0});

    NumaHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryNumaPortFaultHandleInfo("1", "1", "3", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryNumaPortFaultHandleInfo_RunningStateCollected)
{
    AddNumaImportObjWithPortInfo("1", "2", "numaRunning", UBSE_MEM_IMPORT_RUNNING, 1, 5, {0});

    NumaHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryNumaPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "numaRunning");
}

/**
 * 测试：多芯片多端口场景下的 FD 故障查询
 * 场景：
 *   - fdChip1Port5: chipId=1, portId=5 -> chipId匹配，portId=5匹配 -> 故障
 *   - fdChip1Port3: chipId=1, portId=3 -> chipId匹配，portId=3不匹配 -> 正常
 *   - fdChip2Port5: chipId=2, portId=5 -> chipId不匹配 -> 过滤
 * 预期：只返回 fdChip1Port5
 */
TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_MultipleChipsAndPorts)
{
    AddFdImportObjWithPortInfo("1", "2", "fdChip1Port5", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {100});
    AddFdImportObjWithPortInfo("1", "2", "fdChip1Port3", UBSE_MEM_IMPORT_SUCCESS, 1, 3, {200});
    AddFdImportObjWithPortInfo("1", "2", "fdChip2Port5", UBSE_MEM_IMPORT_SUCCESS, 2, 5, {300});

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "fdChip1Port5");
}

/**
 * 测试：Numa Handle Info 使用 unordered_set 存储 numaIds（自动去重）
 * 场景：添加包含重复 numaId {0, 1, 0} 的对象
 * 预期：numaIds 集合大小为 2（0 被去重）
 */
TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_InvalidPortIdFormat)
{
    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "invalid_port", handleInfo), UBSE_ERROR_INVAL);
    EXPECT_TRUE(handleInfo.empty());
}

TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_EmptyParam)
{
    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("", "1", "5", handleInfo), UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "", "5", handleInfo), UBSE_ERROR_INVAL);
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "", handleInfo), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemDebtInfoQuery, NumaHandleInfo_UsesUnorderedSetNumaIds)
{
    AddNumaImportObjWithExportNode("1", "2", "numaSet", UBSE_MEM_IMPORT_SUCCESS, {0, 1, 0});

    NumaHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryNumaImportHandleByExportNodeId("1", "2", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].numaIds.size(), 2);
}

/**
 * 测试：FD 故障查询时，importNumaInfos 中 nodeId 与目标 nodeId 不匹配的对象应被过滤
 * 场景：
 *   - fdOtherNode: 导入节点的 importNumaInfo.nodeId="3"，目标查询 nodeId="1" -> 过滤
 *   - fdMatchNode: 导入节点的 importNumaInfo.nodeId="1"，目标查询 nodeId="1" -> 命中
 * 预期：只返回 fdMatchNode
 */
TEST_F(TestUbseMemDebtInfoQuery, UbseQueryFdPortFaultHandleInfo_NodeIdMismatch)
{
    AddFdImportObjWithPortInfo("1", "2", "fdOtherNode", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {100});
    AddFdImportObjWithPortInfo("1", "2", "fdMatchNode", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {200});

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().FindNodeMap("1")->Modify(
        "fdOtherNode", [](UbseMemFdBorrowImportObj& obj) { obj.algoResult.importNumaInfos[0].nodeId = "3"; });

    FdHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryFdPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    ASSERT_EQ(handleInfo.size(), 1);
    EXPECT_EQ(handleInfo[0].name, "fdMatchNode");
}

/**
 * 测试：Share 故障查询时，importNumaInfos 中 nodeId 与目标 nodeId 不匹配的对象应被过滤
 * 场景：构造一个 share 对象的 importNumaInfo.nodeId="2"，但目标查询 nodeId="1" -> 过滤
 * 预期：返回结果为空
 */
TEST_F(TestUbseMemDebtInfoQuery, UbseQuerySharePortFaultHandleInfo_NodeIdMismatch)
{
    AddShareImportObjWithPortInfo("1", "2", "shareOtherNode", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {500});

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().FindNodeMap("1")->Modify(
        "shareOtherNode", [](UbseMemShareBorrowImportObj& obj) { obj.algoResult.importNumaInfos[0].nodeId = "2"; });

    ShareHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQuerySharePortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}

/**
 * 测试：Numa 故障查询时，importNumaInfos 中 nodeId 与目标 nodeId 不匹配的对象应被过滤
 * 场景：构造一个 numa 对象的 importNumaInfo.nodeId="2"，但目标查询 nodeId="1" -> 过滤
 * 预期：返回结果为空
 */
TEST_F(TestUbseMemDebtInfoQuery, UbseQueryNumaPortFaultHandleInfo_NodeIdMismatch)
{
    AddNumaImportObjWithPortInfo("1", "2", "numaOtherNode", UBSE_MEM_IMPORT_SUCCESS, 1, 5, {0});

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().FindNodeMap("1")->Modify(
        "numaOtherNode", [](UbseMemNumaBorrowImportObj& obj) { obj.algoResult.importNumaInfos[0].nodeId = "2"; });

    NumaHandleInfoVec handleInfo;
    EXPECT_EQ(UbseQueryNumaPortFaultHandleInfo("1", "1", "5", handleInfo), UBSE_OK);
    EXPECT_TRUE(handleInfo.empty());
}
} // namespace ubse::mem_controller::ut
