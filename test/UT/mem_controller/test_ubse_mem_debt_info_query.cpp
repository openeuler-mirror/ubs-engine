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

#include "message/ubse_mem_debt_info_query_req_simpo.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_node_controller.h"

namespace ubse::mem_controller::ut {
using namespace com;
using namespace context;
using namespace ubse::election;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::debt;

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

void AddFdObj(const std::string &importNodeId, const std::string &exportNodeId, const std::string &name)
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
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(exportNodeId, name + "_" + importNodeId, fdExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(importNodeId, name, fdImportObj);
}

void AddNumaObj(const std::string &importNodeId, const std::string &exportNodeId, const std::string &name)
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
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource(exportNodeId, name + "_" + importNodeId, numaExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource(importNodeId, name, numaImportObj);
}

void AddAddrObj(const std::string &importNodeId, const std::string &exportNodeId, const std::string &name)
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
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportNodeId, name + "_" + importNodeId, addrExportObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(importNodeId, name, addrImportObj);
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

template <typename T, typename = void> struct HasImportResultsInStruct : std::false_type {};
template <typename T>
struct HasImportResultsInStruct<T, std::void_t<decltype(std::declval<T>().status.importResults)>> : std::true_type {};
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
} // namespace ubse::mem_controller::ut