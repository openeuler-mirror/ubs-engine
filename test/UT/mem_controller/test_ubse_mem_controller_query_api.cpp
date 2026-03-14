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

#include "test_ubse_mem_controller_query_api.h"

#include <mockcpp/mockcpp.hpp>

#include "debt/ubse_mem_debt_info_query.h"
#include "message/ubse_mem_controller_def_simpo.h"
#include "ubse_com_module.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_node_controller_query_api.h"
#include "ubse_ipc_common.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::mem::def;
using namespace mem::controller::message;
using namespace ubse::com;

void TestUbseMemControllerQueryApi::SetUp()
{
    election::UbseRoleInfo currentInfo("1", election::ELECTION_ROLE_MASTER, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    Test::SetUp();
}
void TestUbseMemControllerQueryApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemFdGet)
{
    std::string name = "UbseMemFdGet";
    mem::def::UbseMemFdDesc fdDesc;
    EXPECT_EQ(UbseMemFdGet(name, fdDesc), UBSE_ERR_NOT_EXIST);

    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    UbseMemFdBorrowImportObj importObj{};
    UbseUdsInfo udsInfo{.uid = 1, .gid = 1, .pid = 1, .username = "ubse"};
    importObj.req.udsInfo = udsInfo;
    debtInfoMap["1"].fdImportObjMap[name] = importObj;
    debtInfoMap["1"].fdImportObjMap[name].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    nodeMemDebtInfoMap = debtInfoMap;
    UbseUdsInfo currentUdsInfo{.uid = 1, .gid = 1, .pid = 1, .username = "invalid"};
    EXPECT_EQ(UbseMemFdGet(name, fdDesc, &currentUdsInfo), UBSE_ERR_AUTH_FAILED);
    MOCKER_CPP(UbseQueryResult).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdGet(name, fdDesc), UBSE_OK);
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdGet(name, fdDesc), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemFdGet(name, fdDesc), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdGet(name, fdDesc), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemFdList)
{
    std::vector<mem::def::UbseMemFdDesc> fdDescs{};
    UbseUdsInfo udsInfo{.uid = 1, .gid = 1, .pid = 1, .username = "ubse"};
    std::string name = "UbseMemFdList";

    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.udsInfo = udsInfo;
    debtInfoMap["1"].fdImportObjMap[name] = importObj;
    debtInfoMap["1"].fdImportObjMap[name].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    nodeMemDebtInfoMap = debtInfoMap;
    MOCKER_CPP(UbseQueryResult).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdList(udsInfo, fdDescs), UBSE_OK);
    // 模拟获取UbseGetMasterInfo失败
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemFdList(udsInfo, fdDescs), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟获取UbseGetCurrentNodeInfo失败
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemFdList(udsInfo, fdDescs), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdList(udsInfo, fdDescs), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemFdList(udsInfo, fdDescs), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdList(udsInfo, fdDescs), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemNumaGet)
{
    std::string name = "UbseMemNumaGet";
    mem::def::UbseMemNumaDesc numaDesc;
    EXPECT_EQ(UbseMemNumaGet(name, numaDesc), UBSE_ERR_NOT_EXIST);

    UbseUdsInfo udsInfo{.uid = 1, .gid = 1, .pid = 1, .username = "ubse"};
    UbseMemNumaBorrowImportObj importObj{};
    importObj.req.udsInfo = udsInfo;

    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    debtInfoMap["1"].numaImportObjMap[name] = importObj;
    debtInfoMap["1"].numaImportObjMap[name].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    nodeMemDebtInfoMap = debtInfoMap;
    UbseUdsInfo currentUdsInfo{.uid = 1, .gid = 1, .pid = 1, .username = "invalid"};
    EXPECT_EQ(UbseMemNumaGet(name, numaDesc, &currentUdsInfo), UBSE_ERR_AUTH_FAILED);

    MOCKER_CPP(UbseQueryResult).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaGet(name, numaDesc), UBSE_OK);
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaGet(name, numaDesc), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemNumaGet(name, numaDesc), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaGet(name, numaDesc), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemNumaList)
{
    std::string name = "UbseMemNumaList";
    UbseUdsInfo udsInfo{.uid = 1, .gid = 1, .pid = 1, .username = "ubse"};

    std::vector<mem::def::UbseMemNumaDesc> numaDescs{};

    UbseMemNumaBorrowImportObj importObj{};
    importObj.req.udsInfo = udsInfo;

    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    debtInfoMap["1"].numaImportObjMap[name] = importObj;
    debtInfoMap["1"].numaImportObjMap[name].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    nodeMemDebtInfoMap = debtInfoMap;
    MOCKER_CPP(UbseQueryResult).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaList(udsInfo, numaDescs), UBSE_OK);

    // 模拟获取UbseGetMasterInfo失败
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemNumaList(udsInfo, numaDescs), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟获取UbseGetCurrentNodeInfo失败
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemNumaList(udsInfo, numaDescs), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaList(udsInfo, numaDescs), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemNumaList(udsInfo, numaDescs), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaList(udsInfo, numaDescs), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemShmList)
{
    std::vector<mem::def::UbseMemShmDesc> shmDescs{};
    UbseMemDebtQueryRequest request{};

    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    debtInfoMap["1"].shareExportObjMap["name"] = UbseMemShareBorrowExportObj{};
    debtInfoMap["1"].shareExportObjMap["name"].algoResult.exportNumaInfos.push_back(UbseMemDebtNumaInfo{.nodeId = "1"});
    debtInfoMap["1"].shareImportObjMap["name"].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    nodeMemDebtInfoMap = debtInfoMap;
    MOCKER_CPP(UbseQueryResult).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmList(request, shmDescs), UBSE_OK);
    // 模拟获取UbseGetMasterInfo失败
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmList(request, shmDescs), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟获取UbseGetCurrentNodeInfo失败
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmList(request, shmDescs), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmList(request, shmDescs), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmList(request, shmDescs), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmList(request, shmDescs), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemShmStatusGet)
{
    UbseMemShmMemStatusDesc shmDescs;
    std::string name = "name";
    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    UbseMemShareBorrowExportObj ubseMemShareBorrowExportObj;
    UbseMemObmmInfo obmmInfo1;
    obmmInfo1.memIdStatus = UB_MEM_HEALTHY;
    UbseMemObmmInfo obmmInfo2;
    obmmInfo2.memIdStatus = MAR_ILLEGAL_ACCESS_ERR;
    ubseMemShareBorrowExportObj.status.exportObmmInfo.push_back(obmmInfo1);
    ubseMemShareBorrowExportObj.status.exportObmmInfo.push_back(obmmInfo2);
    debtInfoMap["1"].shareExportObjMap[name] = ubseMemShareBorrowExportObj;
    debtInfoMap["1"].shareExportObjMap[name].algoResult.exportNumaInfos.push_back(UbseMemDebtNumaInfo{.nodeId = "1"});
    nodeMemDebtInfoMap = debtInfoMap;
    MOCKER_CPP(UbseQueryResult).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmStatusGet(name, shmDescs), UBSE_OK);

    // 模拟获取UbseGetMasterInfo失败
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmStatusGet(name, shmDescs), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟获取UbseGetCurrentNodeInfo失败
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmStatusGet(name, shmDescs), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟localNodeId != masterNodeId
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmStatusGet(name, shmDescs), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmStatusGet(name, shmDescs), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmStatusGet(name, shmDescs), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemShmGetByNodeId)
{
    std::string name = "name";

    UbseMemShmDesc shmDescs;
    std::string srcNode = "2";
    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    UbseMemShareBorrowExportObj obj;
    obj.req.name = "name";
    debtInfoMap["1"].shareExportObjMap[name] = obj;
    debtInfoMap["1"].shareExportObjMap[name].algoResult.exportNumaInfos.push_back(UbseMemDebtNumaInfo{.nodeId = "1"});
    debtInfoMap["1"].shareImportObjMap[name].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    nodeMemDebtInfoMap = debtInfoMap;

    MOCKER_CPP(UbseQueryResult).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmGetByNodeId(name, shmDescs, srcNode), UBSE_OK);
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmGetByNodeId(name, shmDescs, srcNode), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmGetByNodeId(name, shmDescs, srcNode), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmGetByNodeId(name, shmDescs, srcNode), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemShmGet)
{
    std::string name = "name";
    UbseMemShmDesc shmDescs;
    UbseUdsInfo udsInfo{.uid = 1, .gid = 1, .pid = 1, .username = "ubse"};
    std::string srcNode = "2";
    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.udsInfo = udsInfo;
    debtInfoMap["1"].shareExportObjMap[name] = exportObj;
    debtInfoMap["1"].shareExportObjMap[name].algoResult.exportNumaInfos.push_back(UbseMemDebtNumaInfo{.nodeId = "1"});
    debtInfoMap["1"].shareImportObjMap[name].status.importResults.emplace_back(UbseMemImportResult{});
    nodeMemDebtInfoMap = debtInfoMap;

    MOCKER_CPP(UbseQueryResult).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmGet(name, shmDescs, &udsInfo), UBSE_OK);
    // 模拟获取UbseGetMasterInfo失败
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmGet(name, shmDescs, &udsInfo), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟获取UbseGetCurrentNodeInfo失败
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmGet(name, shmDescs, &udsInfo), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmGet(name, shmDescs, &udsInfo), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemShmGet(name, shmDescs, &udsInfo), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemShmGet(name, shmDescs, &udsInfo), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemNodeBorrowInfoQueryWhenLocalNotMaster)
{
    GlobalMockObject::reset();
    std::vector<UbseNodeBorrowInfo> nodeBorrowInfo;
    // 模拟GetMasterAndLocalNodeId成功
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));

    // 模拟RpcSend成功
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(comModule));
    UbseMemNodeBorrowInfoMessagePtr responsePtr = new (std::nothrow) UbseMemNodeBorrowInfoMessage();
    responsePtr->nodeBorrowInfos_.push_back(UbseNodeBorrowInfo());
    const auto func = &UbseComModule::RpcSend<UbseMemNodeBorrowInfoReqMessagePtr, UbseMemNodeBorrowInfoMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));

    // 第一次com模块不存在
    uint32_t ret = UbseMemNodeBorrowInfoQuery(nodeBorrowInfo);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);

    // 第二次成功
    ret = UbseMemNodeBorrowInfoQuery(nodeBorrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemNodeBorrowInfoQueryWhenGetNodeIdFailed)
{
    GlobalMockObject::reset();
    std::vector<UbseNodeBorrowInfo> nodeBorrowInfo;

    // 模拟GetMasterAndLocalNodeId失败
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));

    // 调用函数
    uint32_t ret = UbseMemNodeBorrowInfoQuery(nodeBorrowInfo);

    // 验证结果
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemNodeBorrowInfoQueryWhenLocalIsMaster)
{
    GlobalMockObject::reset();
    std::vector<UbseNodeBorrowInfo> nodeBorrowInfo;
    // 模拟GetMasterAndLocalNodeId成功
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseNodeBorrowInfo> nodeBorrowQueryRet{};
    nodeBorrowQueryRet.push_back(UbseNodeBorrowInfo{1, "node1", 2, "node2", 128}); // 节点1向节点2借出内存128M
    MOCKER_CPP(debt::UbseMemNodeBorrowQuery).stubs().with(outBound(nodeBorrowQueryRet)).will(returnValue(UBSE_OK));
    // 调用函数
    uint32_t ret = UbseMemNodeBorrowInfoQuery(nodeBorrowInfo);
    // 验证结果
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(nodeBorrowInfo.size(), nodeBorrowQueryRet.size());
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemNodeBorrowInfoQueryWhenRpcSendFailed)
{
    GlobalMockObject::reset();
    std::vector<UbseNodeBorrowInfo> nodeBorrowInfo;
    // 模拟GetMasterAndLocalNodeId成功
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));

    // 模拟RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>).stubs().then(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemNodeBorrowInfoReqMessagePtr, UbseMemNodeBorrowInfoMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));

    // 调用函数
    uint32_t ret = UbseMemNodeBorrowInfoQuery(nodeBorrowInfo);

    // 验证结果
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerQueryApi, UbseNodeInfoGet)
{
    ubse::adapter_plugins::mmi::UbseNodeInfo ubseNodeInfo;
    std::string nodeId = "1";

    // 模拟节点ID为空
    EXPECT_EQ(UbseNodeInfoGet("", ubseNodeInfo), UBSE_OK);

    // 模拟节点ID转换失败
    EXPECT_EQ(UbseNodeInfoGet("invalid", ubseNodeInfo), UBSE_ERROR);

    // 模拟节点ID未找到
    EXPECT_EQ(UbseNodeInfoGet(nodeId, ubseNodeInfo), UBSE_ERROR);

    // 模拟节点ID找到且状态正常
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap;
    ubse::nodeController::UbseNodeInfo nodeInfo;
    nodeInfo.localState = ubse::nodeController::UbseNodeLocalState::UBSE_NODE_READY;
    nodeInfoMap.insert({nodeId, nodeInfo});
    MOCKER_CPP(&nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    EXPECT_EQ(UbseNodeInfoGet(nodeId, ubseNodeInfo), UBSE_OK);
    EXPECT_EQ(ubseNodeInfo.status, UBSE_NODE_STATUS_NORMAL);

    // 模拟节点ID找到且状态异常
    MOCKER_CPP(&nodeController::UbseNodeController::GetAllNodes).reset();
    nodeInfo.localState = ubse::nodeController::UbseNodeLocalState::UBSE_NODE_RESTORE;
    nodeInfoMap.clear();
    nodeInfoMap.insert({nodeId, nodeInfo});
    MOCKER_CPP(&nodeController::UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    EXPECT_EQ(UbseNodeInfoGet(nodeId, ubseNodeInfo), UBSE_OK);
    EXPECT_EQ(ubseNodeInfo.status, UBSE_NODE_STATUS_ABNORMAL);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemAddrGet)
{
    UbseMemAddrDesc desc;
    std::string name = "test_name";
    std::string importNodeId = "1";
    // 模拟本地节点是主节点且查询成功
    MOCKER_CPP(debt::UbseMemAddrGet).reset();
    MOCKER_CPP(debt::UbseMemAddrGet).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemAddrGet(name, importNodeId, desc), UBSE_OK);
    // 模拟获取UbseGetMasterInfo失败
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemAddrGet(name, importNodeId, desc), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟获取UbseGetCurrentNodeInfo失败
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemAddrGet(name, importNodeId, desc), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemAddrGet(name, importNodeId, desc), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemAddrGet(name, importNodeId, desc), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemAddrGet(name, importNodeId, desc), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemNumaGetWithImportNode)
{
    ubse::mem::controller::UbseMemNumaDesc numaDesc;
    std::string name = "test_name";
    std::string importNodeId = "1";
    // 模拟本地节点是主节点且查询成功
    MOCKER_CPP(debt::UbseMemNumaGetWithImportNode).reset();
    MOCKER_CPP(debt::UbseMemNumaGetWithImportNode).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaGetWithImportNode(name, importNodeId, numaDesc), UBSE_OK);
    // 模拟获取UbseGetMasterInfo失败
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    election::UbseRoleInfo masterRoleInfo("1", election::ELECTION_ROLE_MASTER);
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemNumaGetWithImportNode(name, importNodeId, numaDesc), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟获取UbseGetCurrentNodeInfo失败
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    election::UbseRoleInfo currentRoleInfo("2", election::ELECTION_ROLE_AGENT);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemNumaGetWithImportNode(name, importNodeId, numaDesc), UBSE_ERR_DAEMON_UNREACHABLE);
    // 模拟localNodeId != masterNodeId
    MOCKER_CPP(election::UbseGetMasterInfo).reset();
    MOCKER_CPP(election::UbseGetMasterInfo).stubs().with(outBound(masterRoleInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).reset();
    MOCKER_CPP(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaGetWithImportNode(name, importNodeId, numaDesc), UBSE_ERROR_MODULE_LOAD_FAILED);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend失败
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseMemDebtQueryRequestSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemNumaGetWithImportNode(name, importNodeId, numaDesc), UBSE_ERROR);
    // 模拟SendQueryToMasterIfNotMaster中RpcSend成功
    MOCKER_CPP(func).reset();
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemNumaGetWithImportNode(name, importNodeId, numaDesc), UBSE_OK);
}
} // namespace ubse::mem_controller::ut