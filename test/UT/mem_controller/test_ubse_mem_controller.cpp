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

#include "test_ubse_mem_controller.h"
#include "message/ubse_mem_opt_req_simpo.h"
#include "ubse_com_module.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_mem_controller.cpp"
#include "ubse_mem_controller.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::election;
using namespace com;
using namespace ubse::mem::controller::message;

void TestUbseMemController::SetUp()
{
    Test::SetUp();
}
void TestUbseMemController::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

void SetTestObjects(UbseMemFdBorrowReq &fdBorrowReq, UbseMemNumaBorrowReq &numaBorrowReq,
                    UbseMemFdBorrowImportObj &fdImportObj, UbseMemFdBorrowExportObj &fdExportObj,
                    UbseMemNumaBorrowImportObj &numaImportObj, UbseMemNumaBorrowExportObj &numaExportObj,
                    NodeMemDebtInfoMap &nodeDebtInfoMap)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};

    // 初始化 fdBorrowReq
    fdBorrowReq.name = "test";
    fdBorrowReq.requestNodeId = "1";
    fdBorrowReq.importNodeId = "1";
    fdBorrowReq.size = 128;
    fdBorrowReq.udsInfo = udsInfo;

    // 初始化 numaBorrowReq
    numaBorrowReq.name = "test";
    numaBorrowReq.requestNodeId = "1";
    numaBorrowReq.importNodeId = "1";
    numaBorrowReq.size = 128;
    numaBorrowReq.udsInfo = udsInfo;

    // 构造 fdImportObj
    fdImportObj.req = fdBorrowReq;
    UbseMemDebtNumaInfo fdImportNmaInfo{.nodeId = "1", .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo fdExportNmaInfo{.nodeId = "2", .socketId = 0, .numaId = 0, .size = 128};
    fdImportObj.algoResult.exportNumaInfos.emplace_back(fdExportNmaInfo);
    fdImportObj.algoResult.importNumaInfos.emplace_back(fdImportNmaInfo);
    fdImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    // 构造 fdExportObj
    fdExportObj.req = fdBorrowReq;
    fdExportObj.algoResult = fdImportObj.algoResult;
    fdExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 构造 numaImportObj
    numaImportObj.req = numaBorrowReq;
    UbseMemDebtNumaInfo numaImportNmaInfo{.nodeId = "1", .socketId = 0, .numaId = 0, .size = 128};
    UbseMemDebtNumaInfo numaExportNmaInfo{.nodeId = "2", .socketId = 0, .numaId = 0, .size = 128};
    numaImportObj.algoResult.exportNumaInfos.emplace_back(numaExportNmaInfo);
    numaImportObj.algoResult.importNumaInfos.emplace_back(numaImportNmaInfo);
    numaImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    // 构造 numaExportObj
    numaExportObj.req = numaBorrowReq;
    numaExportObj.algoResult = numaImportObj.algoResult;
    numaExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    // 填充 nodeDebtInfoMap
    nodeDebtInfoMap[fdExportObj.algoResult.exportNumaInfos[0].nodeId].fdExportObjMap[fdBorrowReq.name] = fdExportObj;
    nodeDebtInfoMap[fdBorrowReq.importNodeId].fdImportObjMap[fdBorrowReq.name] = fdImportObj;

    nodeDebtInfoMap[numaExportObj.algoResult.importNumaInfos[0].nodeId].numaExportObjMap[numaBorrowReq.name] =
        numaExportObj;
    nodeDebtInfoMap[numaBorrowReq.importNodeId].numaImportObjMap[numaBorrowReq.name] = numaImportObj;
}

TEST_F(TestUbseMemController, UbseGetNumaMemDebtInfoWithNode)
{
    std::string nodeId;
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos));
    nodeId = "1";
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos));
    std::set<uint32_t> staticNodeInfoList{};
    ubse::nodeController::UbseNodeInfo staticNodeInfo1;
    ubse::nodeController::UbseNodeInfo staticNodeInfo2;
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    staticNodeInfo1.nodeId = "1";
    staticNodeInfo2.nodeId = "2";
    staticNodeInfo1.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    staticNodeInfo2.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    staticNodeInfoList.insert({1, 2});
    MOCKER(&UbseNodeController::UbseGetAllDeployedNode).stubs().will(returnValue(staticNodeInfoList));
    nodeMap["1"] = staticNodeInfo1;
    nodeMap["2"] = staticNodeInfo2;
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeMap));
    UbseMemFdBorrowReq fdBorrowReq;
    UbseMemNumaBorrowReq numaBorrowReq;
    UbseMemFdBorrowImportObj fdImportObj;
    UbseMemFdBorrowExportObj fdExportObj;
    UbseMemNumaBorrowImportObj numaImportObj;
    UbseMemNumaBorrowExportObj numaExportObj;
    NodeMemDebtInfoMap ubse_node_mem_debt_info;
    SetTestObjects(fdBorrowReq, numaBorrowReq, fdImportObj, fdExportObj, numaImportObj, numaExportObj,
                   ubse_node_mem_debt_info);
    MOCKER_CPP(&UbseGetMemDebtInfo, uint32_t(const std::string &, NodeMemDebtInfoMap &))
        .stubs()
        .with(any(), outBound(ubse_node_mem_debt_info))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos), UBSE_OK);
    staticNodeInfo1.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_INIT;
    staticNodeInfoList.clear();
    staticNodeInfoList.insert({1, 2});
    MOCKER(&UbseNodeController::GetStaticNodeInfo).reset();
    MOCKER(&UbseNodeController::GetStaticNodeInfo).stubs().will(returnValue(staticNodeInfoList));
    EXPECT_EQ(UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos),
              UBSE_OK);
}

TEST_F(TestUbseMemController, UbseQueryResult)
{
    std::string name = "test";
    UbseMemResult result;
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseQueryResult(name, result, UbseMemBorrowType::NUMA_BORROW), UBSE_ERROR);

    std::shared_ptr<UbseComModule> comModule = nullptr;
    auto func = &UbseContext::GetModule<UbseComModule>;
    MOCKER(func).stubs().will(returnValue(comModule)).then(returnValue(std::make_shared<UbseComModule>()));
    EXPECT_EQ(UbseQueryResult(name, result, UbseMemBorrowType::NUMA_BORROW), UBSE_ERROR_MODULE_LOAD_FAILED);

    const auto sendFunc = &UbseComModule::RpcSend<UbseMemOptReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseQueryResult(name, result, UbseMemBorrowType::NUMA_BORROW), UBSE_ERROR);
    EXPECT_EQ(UbseQueryResult(name, result, UbseMemBorrowType::NUMA_BORROW), UBSE_OK);
}

TEST_F(TestUbseMemController, CheckReconciliationStatus)
{
    // 场景 1: targetNodeId 不为空且节点工作正常
    std::set<uint32_t> staticNodeInfoList = {1, 2, 3};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    nodeMap["1"] = ubse::nodeController::UbseNodeInfo{
        .clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING};
    nodeMap["2"] = ubse::nodeController::UbseNodeInfo{
        .clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING};
    nodeMap["3"] = ubse::nodeController::UbseNodeInfo{
        .clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING};
    std::string targetNodeId = "1";
    EXPECT_EQ(CheckReconciliationStatus(staticNodeInfoList, nodeMap, targetNodeId), UBSE_OK);

    // 场景 2: targetNodeId 为空，且所有节点工作正常
    targetNodeId = "";
    EXPECT_EQ(CheckReconciliationStatus(staticNodeInfoList, nodeMap, targetNodeId), UBSE_OK);

    // 场景 3: targetNodeId 为空，且节点不存在于 nodeMap 中
    nodeMap.erase("1");
    EXPECT_EQ(CheckReconciliationStatus(staticNodeInfoList, nodeMap, targetNodeId),
              UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS);

    // 场景 4: targetNodeId 为空，且节点存在于 nodeMap 中但不在工作状态
    nodeMap["1"] =
        ubse::nodeController::UbseNodeInfo{.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_INIT};
    EXPECT_EQ(CheckReconciliationStatus(staticNodeInfoList, nodeMap, targetNodeId),
              UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS);
}

TEST_F(TestUbseMemController, UbseGetNumaMemDebtInfo)
{
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;

    // 场景 1: 获取账本信息失败
    MOCKER(UbseGetMemDebtInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseGetNumaMemDebtInfo(debtInfos), UBSE_ERR_INTERNAL);

    // 场景 2: 获取账本信息成功
    MOCKER(UbseGetMemDebtInfo).reset();
    MOCKER(UbseGetMemDebtInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(CheckReconciliationStatus).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseGetNumaMemDebtInfo(debtInfos), UBSE_OK);
}

uint32_t MockUbseMemNumaBorrowRespError(const UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp)
{
    resp.errorCode = static_cast<uint32_t>(UBSE_ERR_INTERNAL);
    return UBSE_ERROR;
}

uint32_t MockUbseMemNumaBorrowRespSuccess(const UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp)
{
    resp.errorCode = static_cast<uint32_t>(UBSE_OK);
    resp.name = "test";
    return UBSE_OK;
}

TEST_F(TestUbseMemController, UbseMemNumaCreateWithLender)
{
    std::string name = "test";
    UbseMemBorrower borrower{.nodeId = "1"};
    std::vector<UbseMemNumaLender> lenders{};
    UbseMemNumaLender lender{.slotId = 2};
    lenders.emplace_back(lender);
    uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN] = {0};
    UbseMemNumaDesc desc;

    // 场景 1: 参数校验失败
    MOCKER(UbseMemCreateWithLenderReqIsValid).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    EXPECT_EQ(UbseMemNumaCreateWithLender(name, borrower, lenders, usrInfo, desc), UBSE_ERR_INVALID_ARG);

    // 场景 2: 请求转换失败
    MOCKER(UbseMemCreateWithLenderReqIsValid).reset();
    MOCKER(UbseMemCreateWithLenderReqIsValid).stubs().will(returnValue(UBSE_OK));
    MOCKER(ConvertUbseMemNumaCreateWithLenderReq).stubs().will(returnValue(UBSE_ERR_INTERNAL));
    EXPECT_EQ(UbseMemNumaCreateWithLender(name, borrower, lenders, usrInfo, desc), UBSE_ERR_INTERNAL);

    // 场景 3: 调用内部接口失败
    MOCKER(ConvertUbseMemNumaCreateWithLenderReq).reset();
    MOCKER(ConvertUbseMemNumaCreateWithLenderReq).stubs().will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::controller::agent::UbseMemNumaBorrow).stubs().will(invoke(MockUbseMemNumaBorrowRespError));
    EXPECT_EQ(UbseMemNumaCreateWithLender(name, borrower, lenders, usrInfo, desc), UBSE_ERR_INTERNAL);

    // 场景 4: 成功
    MOCKER(ubse::mem::controller::agent::UbseMemNumaBorrow).reset();
    MOCKER(ubse::mem::controller::agent::UbseMemNumaBorrow).stubs().will(invoke(MockUbseMemNumaBorrowRespSuccess));
    MOCKER(ubse::mem::controller::UbseMemNumaGetWithImportNode).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaCreateWithLender(name, borrower, lenders, usrInfo, desc), UBSE_OK);
}

TEST_F(TestUbseMemController, UbseMemNumaCreate)
{
    std::string name = "test";
    UbseMemBorrower borrower{.nodeId = "1"};
    UbseMemNumaCreateOpt opt;
    UbseMemNumaDesc desc;

    // 场景 1: 参数校验失败
    MOCKER(UbseMemCreateReqIsValid).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    EXPECT_EQ(UbseMemNumaCreate(name, borrower, opt, desc), UBSE_ERR_INVALID_ARG);

    // 场景 2: 请求转换失败
    MOCKER(UbseMemCreateReqIsValid).reset();
    MOCKER(UbseMemCreateReqIsValid).stubs().will(returnValue(UBSE_OK));
    MOCKER(ConvertUbseMemNumaCreateReq).stubs().will(returnValue(UBSE_ERR_INTERNAL));
    EXPECT_EQ(UbseMemNumaCreate(name, borrower, opt, desc), UBSE_ERR_INTERNAL);

    // 场景 3: 调用内部接口失败
    MOCKER(ConvertUbseMemNumaCreateReq).reset();
    MOCKER(ConvertUbseMemNumaCreateReq).stubs().will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::controller::agent::UbseMemNumaBorrow).stubs().will(invoke(MockUbseMemNumaBorrowRespError));
    EXPECT_EQ(UbseMemNumaCreate(name, borrower, opt, desc), UBSE_ERR_INTERNAL);

    // 场景 4: 成功
    MOCKER(ubse::mem::controller::agent::UbseMemNumaBorrow).reset();
    MOCKER(ubse::mem::controller::agent::UbseMemNumaBorrow).stubs().will(invoke(MockUbseMemNumaBorrowRespSuccess));
    MOCKER(ubse::mem::controller::UbseMemNumaGetWithImportNode).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaCreate(name, borrower, opt, desc), UBSE_OK);
}

TEST_F(TestUbseMemController, UbseMemNumaCreateWithCandidate)
{
    std::string name = "test";
    UbseMemBorrower borrower{.nodeId = "1"};
    UbseMemNumaCandidateOpt opt;
    UbseMemNumaDesc desc;

    // 场景 1: 参数校验失败
    MOCKER(UbseMemCreateWithCandidateReqIsValid).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    EXPECT_EQ(UbseMemNumaCreateWithCandidate(name, borrower, opt, desc), UBSE_ERR_INVALID_ARG);

    // 场景 2: 请求转换失败
    MOCKER(UbseMemCreateWithCandidateReqIsValid).reset();
    MOCKER(UbseMemCreateWithCandidateReqIsValid).stubs().will(returnValue(UBSE_OK));
    MOCKER(ConvertUbseMemNumaCreateWithCandidateReq).stubs().will(returnValue(UBSE_ERR_INTERNAL));
    EXPECT_EQ(UbseMemNumaCreateWithCandidate(name, borrower, opt, desc), UBSE_ERR_INTERNAL);

    // 场景 3: 调用内部接口失败
    MOCKER(ConvertUbseMemNumaCreateWithCandidateReq).reset();
    MOCKER(ConvertUbseMemNumaCreateWithCandidateReq).stubs().will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::controller::agent::UbseMemNumaBorrow).stubs().will(invoke(MockUbseMemNumaBorrowRespError));
    EXPECT_EQ(UbseMemNumaCreateWithCandidate(name, borrower, opt, desc), UBSE_ERR_INTERNAL);

    // 场景 4: 成功
    MOCKER(ubse::mem::controller::agent::UbseMemNumaBorrow).reset();
    MOCKER(ubse::mem::controller::agent::UbseMemNumaBorrow).stubs().will(invoke(MockUbseMemNumaBorrowRespSuccess));
    MOCKER(ubse::mem::controller::UbseMemNumaGetWithImportNode).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaCreateWithCandidate(name, borrower, opt, desc), UBSE_OK);
}

uint32_t MockUbseMemReturnRespError(const UbseMemReturnReq &req, const MemOperationType &type,
                                    UbseMemOperationResp &resp)
{
    resp.errorCode = static_cast<uint32_t>(UBSE_ERR_INTERNAL);
    return UBSE_ERROR;
}

uint32_t MockUbseMemReturnRespSuccess(const UbseMemReturnReq &req, const MemOperationType &type,
                                      UbseMemOperationResp &resp)
{
    resp.errorCode = static_cast<uint32_t>(UBSE_OK);
    resp.name = "test";
    return UBSE_OK;
}

TEST_F(TestUbseMemController, UbseMemNumaDelete)
{
    std::string name = "test";
    UbseMemBorrower borrower{.nodeId = "1"};

    // 场景 1: 参数校验失败
    MOCKER(UbseMemDeleteReqIsValid).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    EXPECT_EQ(UbseMemNumaDelete(name, borrower), UBSE_ERR_INVALID_ARG);

    // 场景 2: 调用内部接口失败
    MOCKER(UbseMemDeleteReqIsValid).reset();
    MOCKER(UbseMemDeleteReqIsValid).stubs().will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::controller::agent::UbseMemReturn).stubs().will(invoke(MockUbseMemReturnRespError));
    EXPECT_EQ(UbseMemNumaDelete(name, borrower), UBSE_ERR_INTERNAL);

    // 场景 3: 成功
    MOCKER(ubse::mem::controller::agent::UbseMemReturn).reset();
    MOCKER(ubse::mem::controller::agent::UbseMemReturn).stubs().will(invoke(MockUbseMemReturnRespSuccess));
    EXPECT_EQ(UbseMemNumaDelete(name, borrower), UBSE_OK);
}

uint32_t MockUbseMemAddrBorrowRespError(const UbseMemAddrBorrowReq &req, UbseMemOperationResp &resp)
{
    resp.errorCode = static_cast<uint32_t>(UBSE_ERR_INTERNAL);
    return UBSE_ERROR;
}

uint32_t MockUbseMemAddrBorrowRespSuccess(const UbseMemAddrBorrowReq &req, UbseMemOperationResp &resp)
{
    resp.errorCode = static_cast<uint32_t>(UBSE_OK);
    resp.name = "test";
    return UBSE_ERROR;
}

TEST_F(TestUbseMemController, UbseMemAddrCreate)
{
    std::string name = "test";
    UbseMemBorrower borrower{.nodeId = "1"};
    UbseMemProcessLender lender;
    uint32_t flag = 0;
    uint8_t exportAccessMode = 0;
    UbseMemAddrDesc desc;

    // 场景 1: 参数校验失败
    MOCKER(UbseMemAddrCreateReqIsValid).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    EXPECT_EQ(UbseMemAddrCreate(name, borrower, lender, flag, exportAccessMode, desc), UBSE_ERR_INVALID_ARG);

    // 场景 2: 调用内部接口失败
    MOCKER(UbseMemAddrCreateReqIsValid).reset();
    MOCKER(UbseMemAddrCreateReqIsValid).stubs().will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::controller::agent::UbseMemAddrBorrow).stubs().will(invoke(MockUbseMemAddrBorrowRespError));
    EXPECT_EQ(UbseMemAddrCreate(name, borrower, lender, flag, exportAccessMode, desc), UBSE_ERR_INTERNAL);

    // 场景 3: 成功
    MOCKER(ubse::mem::controller::agent::UbseMemAddrBorrow).reset();
    MOCKER(ubse::mem::controller::agent::UbseMemAddrBorrow).stubs().will(invoke(MockUbseMemAddrBorrowRespSuccess));
    MOCKER(ubse::mem::controller::UbseMemAddrGet).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemAddrCreate(name, borrower, lender, flag, exportAccessMode, desc), UBSE_OK);
}

TEST_F(TestUbseMemController, UbseMemAddrDelete)
{
    std::string name = "test";
    UbseMemBorrower borrower{.nodeId = "1"};

    // 场景 1: 参数校验失败
    MOCKER(UbseMemDeleteReqIsValid).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    EXPECT_EQ(UbseMemAddrDelete(name, borrower), UBSE_ERR_INVALID_ARG);

    // 场景 2: 调用内部接口失败
    MOCKER(UbseMemDeleteReqIsValid).reset();
    MOCKER(UbseMemDeleteReqIsValid).stubs().will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::controller::agent::UbseMemReturn).stubs().will(invoke(MockUbseMemReturnRespError));
    EXPECT_EQ(UbseMemAddrDelete(name, borrower), UBSE_ERR_INTERNAL);

    // 场景 3: 成功
    MOCKER(ubse::mem::controller::agent::UbseMemReturn).reset();
    MOCKER(ubse::mem::controller::agent::UbseMemReturn).stubs().will(invoke(MockUbseMemReturnRespSuccess));
    EXPECT_EQ(UbseMemAddrDelete(name, borrower), UBSE_OK);
}

TEST_F(TestUbseMemController, GetNodeNumaInfoFromAccountAndSort)
{
    std::vector<ubse::mem::account::UbseNumaNodeInfo> numaNodeInfos;

    // 场景 1: 获取 NUMA 信息失败
    MOCKER(ubse::mem::account::UbseAllNumaInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(GetNodeNumaInfoFromAccountAndSort(numaNodeInfos), UBSE_ERR_INTERNAL);

    // 场景 2: 获取 NUMA 信息成功
    MOCKER(ubse::mem::account::UbseAllNumaInfo).reset();
    MOCKER(ubse::mem::account::UbseAllNumaInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(GetNodeNumaInfoFromAccountAndSort(numaNodeInfos), UBSE_OK);
}

TEST_F(TestUbseMemController, UbseGetAllNodeNumaInfo)
{
    std::vector<UbseNodeNumaInfo> numaNodeInfoList;

    // 场景 1: 参数不合法
    numaNodeInfoList.push_back(UbseNodeNumaInfo{});
    EXPECT_EQ(UbseGetAllNodeNumaInfo(numaNodeInfoList), UBSE_ERR_INVALID_ARG);
    numaNodeInfoList.clear();

    // 场景 2: 获取 NUMA 信息失败
    MOCKER(GetNodeNumaInfoFromAccountAndSort).stubs().will(returnValue(UBSE_ERR_INTERNAL));
    EXPECT_EQ(UbseGetAllNodeNumaInfo(numaNodeInfoList), UBSE_ERR_INTERNAL);

    // 场景 3: 获取 NUMA 信息成功
    MOCKER(GetNodeNumaInfoFromAccountAndSort).reset();
    MOCKER(GetNodeNumaInfoFromAccountAndSort).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseGetAllNodeNumaInfo(numaNodeInfoList), UBSE_OK);
}

UbseResult MockGetNodeNumaInfoFromAccountAndSort(std::vector<ubse::mem::account::UbseNumaNodeInfo> &numaNodeInfos)
{
    ubse::mem::account::UbseNumaNodeInfo info;
    info.nodeId = "1";
    numaNodeInfos.emplace_back(info);
    return UBSE_OK;
}

TEST_F(TestUbseMemController, UbseGetNodeNumaInfoByNodeId)
{
    std::string nodeId = "1";
    std::vector<UbseNodeNumaInfo> numaNodeInfoList;

    // 场景 1: 参数不合法
    EXPECT_EQ(UbseGetNodeNumaInfoByNodeId("", numaNodeInfoList), UBSE_ERR_INVALID_ARG);
    numaNodeInfoList.push_back(UbseNodeNumaInfo{});
    EXPECT_EQ(UbseGetNodeNumaInfoByNodeId(nodeId, numaNodeInfoList), UBSE_ERR_INVALID_ARG);
    numaNodeInfoList.clear();

    // 场景 2: 获取 NUMA 信息失败
    MOCKER(GetNodeNumaInfoFromAccountAndSort).stubs().will(returnValue(UBSE_ERR_INTERNAL));
    EXPECT_EQ(UbseGetNodeNumaInfoByNodeId(nodeId, numaNodeInfoList), UBSE_ERR_INTERNAL);

    // 场景 3: 获取 NUMA 信息成功
    MOCKER(GetNodeNumaInfoFromAccountAndSort).reset();
    MOCKER(GetNodeNumaInfoFromAccountAndSort).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseGetNodeNumaInfoByNodeId(nodeId, numaNodeInfoList), UBSE_ERR_INVALID_ARG);

    MOCKER(GetNodeNumaInfoFromAccountAndSort).reset();
    MOCKER(GetNodeNumaInfoFromAccountAndSort).stubs().will(invoke(MockGetNodeNumaInfoFromAccountAndSort));
    EXPECT_EQ(UbseGetNodeNumaInfoByNodeId(nodeId, numaNodeInfoList), UBSE_OK);
}

uint32_t MockGetMemInfoFromInnerFD(std::vector<NumaStaticInfo> &numaInfo, std::vector<LedgerDymaticInfo> &ledgerInfo)
{
    std::string srcNodeId = "1";
    std::string dstNodeId = "2";
    LedgerDymaticInfo info{};
    info.type = LedgerType::FD;
    info.lentNodeId = srcNodeId;
    info.borrowNodeId = {dstNodeId};
    ledgerInfo.emplace_back(info);
    return UBSE_OK;
}

uint32_t MockGetMemInfoFromInnerSHARE(std::vector<NumaStaticInfo> &numaInfo, std::vector<LedgerDymaticInfo> &ledgerInfo)
{
    std::string srcNodeId = "1";
    std::string dstNodeId = "2";
    LedgerDymaticInfo info{};
    info.type = LedgerType::SHARE;
    info.lentNodeId = srcNodeId;
    info.borrowNodeId = {dstNodeId};
    ledgerInfo.emplace_back(info);
    return UBSE_OK;
}

TEST_F(TestUbseMemController, UbseMemDebtCircleCheck)
{
    std::string srcNodeId = "1";
    std::string dstNodeId = "2";
    bool isCircle = false;

    // 场景 1: 源节点和目标节点相同
    EXPECT_EQ(UbseMemDebtCircleCheck(srcNodeId, srcNodeId, isCircle), UBSE_ERR_INVALID_ARG);

    // 场景 2: 获取 NUMA 信息失败
    MOCKER(GetMemInfoFromInner).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemDebtCircleCheck(srcNodeId, dstNodeId, isCircle), UBSE_MEMCONTROLLER_ERROR_GET_INFO_FAIL);

    // 场景 3: 检测到循环
    MOCKER(GetMemInfoFromInner).reset();
    MOCKER(GetMemInfoFromInner).stubs().will(invoke(MockGetMemInfoFromInnerFD));
    EXPECT_EQ(UbseMemDebtCircleCheck(srcNodeId, dstNodeId, isCircle), UBSE_OK);
    EXPECT_TRUE(isCircle);

    // 场景 4: 未检测到循环
    MOCKER(GetMemInfoFromInner).reset();
    MOCKER(GetMemInfoFromInner).stubs().will(invoke(MockGetMemInfoFromInnerSHARE));
    EXPECT_EQ(UbseMemDebtCircleCheck(srcNodeId, dstNodeId, isCircle), UBSE_OK);
    EXPECT_FALSE(isCircle);
}

TEST_F(TestUbseMemController, ProcessAddrImportObj)
{
    std::string resourceId = "test";
    UbseMemAddrBorrowImportObj addrBorrowImportObj;
    std::string nodeId = "1";
    std::unordered_map<std::string, UbseMemAddrDesc> addrMemoryDebtInfoMap;
    UbseMemImportResult importResult{};
    addrBorrowImportObj.status.importResults.emplace_back(importResult);
    // 场景 1: 导入对象状态不合法
    addrBorrowImportObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_DESTROYING;
    ProcessAddrImportObj(resourceId, addrBorrowImportObj, nodeId, addrMemoryDebtInfoMap);
    EXPECT_TRUE(addrMemoryDebtInfoMap.empty());

    // 场景 2: 导入对象状态合法
    addrBorrowImportObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    addrBorrowImportObj.req.importNodeId = nodeId;
    addrBorrowImportObj.req.exportNodeId = "2";
    ProcessAddrImportObj(resourceId, addrBorrowImportObj, nodeId, addrMemoryDebtInfoMap);
    EXPECT_FALSE(addrMemoryDebtInfoMap.empty());
}

TEST_F(TestUbseMemController, ProcessAddrExportObj)
{
    std::string resourceIdImportNodeId = "test_1";
    UbseMemAddrBorrowExportObj addrBorrowExportObj;
    std::string nodeId = "1";
    std::unordered_map<std::string, UbseMemAddrDesc> addrMemoryDebtInfoMap;

    // 场景 1: 导出对象状态不合法
    addrBorrowExportObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_DESTROYED;
    ProcessAddrExportObj(resourceIdImportNodeId, addrBorrowExportObj, nodeId, addrMemoryDebtInfoMap);
    EXPECT_TRUE(addrMemoryDebtInfoMap.empty());

    // 场景 2: 导出对象状态合法
    addrBorrowExportObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    addrBorrowExportObj.req.importNodeId = "2";
    addrBorrowExportObj.req.exportNodeId = nodeId;
    ProcessAddrExportObj(resourceIdImportNodeId, addrBorrowExportObj, nodeId, addrMemoryDebtInfoMap);
    EXPECT_FALSE(addrMemoryDebtInfoMap.empty());
}

TEST_F(TestUbseMemController, ProcessDebtInfoForAddr)
{
    NodeMemDebtInfoMap memDebtInfoMap;
    std::string nodeId = "1";
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    nodeMap["1"] = ubse::nodeController::UbseNodeInfo{
        .clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING};
    nodeMap["2"] = ubse::nodeController::UbseNodeInfo{
        .clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING};
    nodeMap["3"] = ubse::nodeController::UbseNodeInfo{
        .clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING};
    std::vector<UbseMemAddrDesc> debtInfos;

    // 场景 1: 节点账本信息为空
    ProcessDebtInfoForAddr(memDebtInfoMap, nodeId, nodeMap, debtInfos);
    EXPECT_TRUE(debtInfos.empty());

    // 场景 2: 节点账本信息不为空
    NodeMemDebtInfo nodeDebtInfo;
    nodeDebtInfo.addrImportObjMap["test"] = UbseMemAddrBorrowImportObj{};
    nodeDebtInfo.addrImportObjMap["test"].status.state =
        ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    nodeDebtInfo.addrImportObjMap["test"].req.exportNodeId = "2";
    nodeDebtInfo.addrImportObjMap["test"].req.importNodeId = "1";
    UbseMemImportResult importResult{};
    nodeDebtInfo.addrImportObjMap["test"].status.importResults.emplace_back(importResult);
    nodeDebtInfo.addrExportObjMap["test_1"] = UbseMemAddrBorrowExportObj{};
    nodeDebtInfo.addrExportObjMap["test_1"].status.state =
        ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    nodeDebtInfo.addrExportObjMap["test_1"].req.exportNodeId = "2";
    nodeDebtInfo.addrExportObjMap["test_1"].req.importNodeId = "1";
    memDebtInfoMap["1"] = nodeDebtInfo;
    ProcessDebtInfoForAddr(memDebtInfoMap, nodeId, nodeMap, debtInfos);
    EXPECT_FALSE(debtInfos.empty());
}

TEST_F(TestUbseMemController, UbseGetAddrMemDebtInfoWithNode)
{
    std::string nodeId = "1";
    std::vector<UbseMemAddrDesc> debtInfos;

    // 场景 1: 参数不合法
    EXPECT_EQ(UbseGetAddrMemDebtInfoWithNode("", debtInfos), UBSE_ERR_INVALID_ARG);

    // 场景 2: 节点不在静态列表中
    MOCKER(IsNodeInStaticList).stubs().will(returnValue(false));
    EXPECT_EQ(UbseGetAddrMemDebtInfoWithNode(nodeId, debtInfos), UBSE_ERR_INVALID_ARG);

    // 场景 3: 获取账本信息失败
    MOCKER(IsNodeInStaticList).reset();
    MOCKER(IsNodeInStaticList).stubs().will(returnValue(true));
    MOCKER(UbseGetMemDebtInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseGetAddrMemDebtInfoWithNode(nodeId, debtInfos), UBSE_ERR_INTERNAL);

    // 场景 4: 获取账本信息成功
    MOCKER(UbseGetMemDebtInfo).reset();
    MOCKER(UbseGetMemDebtInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(CheckReconciliationStatus).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseGetAddrMemDebtInfoWithNode(nodeId, debtInfos), UBSE_OK);
}
} // namespace ubse::mem_controller::ut
