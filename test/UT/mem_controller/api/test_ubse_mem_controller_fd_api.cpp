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

#include "test_ubse_mem_controller_fd_api.h"

#include <ubse_com_module.h>
#include <ubse_error.h>
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_mem_account.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mmi_module.h"
#include "debt/ubse_mem_debt_ledger.h"
#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "test_ubse_mem_controller_fd_api.h"

namespace ubse::mem_controller::fd::ut {
using namespace ubse::mem::controller;
using namespace ubse::election;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::controller::debt;
using namespace ubse::mem::decoder::utils;
using namespace ubse::mmi;
using namespace ubse::context;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::com;

const std::string NODE_ONE = "1";
const std::string NODE_TWO = "2";

void SendFdExportObjSuccess(std::shared_ptr<com::UbseComModule>& module)
{
    module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
}
void MasterSendFdExpoprtObjError()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
}
void AgentSendFdExportObjError()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
}

void SendFdImportObjSuccess(std::shared_ptr<com::UbseComModule>& module)
{
    module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
}
void AgentSendAddrImportObjError()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
}
void MasterSendAddrImportObjError()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
}
void BuildOperationSuccessMock(std::shared_ptr<com::UbseComModule>& module)
{
    module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
}

void ExportCallbackCommonSetup(election::UbseRoleInfo& currentInfo, election::UbseRoleInfo& masterInfo,
                               std::shared_ptr<com::UbseComModule>& module)
{
    // 当前节点为0号节点
    currentInfo.nodeId = NODE_TWO;
    MOCKER(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    // 模拟主节点为1号节点
    masterInfo.nodeId = NODE_ONE;
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    // 模拟RPC MODULE获取成功
    module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
}

void AddToExportObjMap(const std::string& name, const std::string& nodeId, UbseMemFdBorrowExportObj& fdBorrowExportObj)
{
    auto exportKey = mem::controller::GenerateExportObjKey(name, nodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(nodeId, exportKey,
                                                                                        fdBorrowExportObj);
}
void AddToImportObjMap(const std::string& name, const std::string& nodeId, UbseMemFdBorrowImportObj& fdBorrowImportObj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(nodeId, name,
                                                                                        fdBorrowImportObj);
}

void TestUbseMemControllerFdApi::SetUp()
{
    election::UbseRoleInfo masterInfo;
    masterInfo.nodeId = NODE_ONE;
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    Test::SetUp();
}
void TestUbseMemControllerFdApi::TearDown()
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerFdApi, AgentSendFdExportObjFailedTest)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_TWO;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.requestNodeId = "1";
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;

    election::UbseRoleInfo currentInfo{};
    election::UbseRoleInfo masterInfo{};
    std::shared_ptr<com::UbseComModule> module;
    ExportCallbackCommonSetup(currentInfo, masterInfo, module);
    AgentSendFdExportObjError();
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdReturnSuccessTest)
{
    MOCKER(WaitNodeStateWork).stubs().will(returnValue(UBSE_OK));
    UbseMemFdBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo{};
    numaInfo.nodeId = NODE_ONE;
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.algoResult = exportObj.algoResult;
    std::string name = "test";
    AddToExportObjMap(name, NODE_ONE, exportObj);
    AddToImportObjMap(name, NODE_ONE, importObj);

    UbseMemReturnReq req;
    req.name = name;
    req.importNodeId = NODE_ONE;
    UbseMemOperationResp resp;

    std::shared_ptr<com::UbseComModule> module;
    SendFdImportObjSuccess(module);
    EXPECT_EQ(UBSE_OK, UbseMemFdReturn(req, resp, NODE_ONE));
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdReturnSendFailedTest)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemFdBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo{};
    numaInfo.nodeId = NODE_ONE;
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.algoResult = exportObj.algoResult;
    std::string name = "test";
    AddToExportObjMap(name, NODE_ONE, exportObj);
    AddToImportObjMap(name, NODE_ONE, importObj);

    UbseMemReturnReq req;
    req.name = name;
    req.importNodeId = NODE_ONE;
    UbseMemOperationResp resp;

    std::shared_ptr<com::UbseComModule> module;
    BuildOperationSuccessMock(module);
    EXPECT_EQ(UBSE_OK, UbseMemFdReturn(req, resp, NODE_ONE));
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdReturnNotExistTest)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    std::shared_ptr<com::UbseComModule> module;
    BuildOperationSuccessMock(module);
    EXPECT_EQ(UBSE_OK, UbseMemFdReturn(req, resp, NODE_ONE));
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdReturnCheckPermissionFailedTest)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemFdBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_TWO;
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    std::string name = "test";
    AddToExportObjMap(name, NODE_ONE, exportObj);
    AddToImportObjMap(name, NODE_ONE, importObj);

    UbseMemReturnReq req;
    req.name = name;
    req.importNodeId = NODE_ONE;
    UbseUdsInfo udsInfo{.uid = 1000, .gid = 1000, .pid = 1000, .username = "ubsmd"};
    req.udsInfo = udsInfo;
    UbseMemOperationResp resp;
    std::shared_ptr<com::UbseComModule> module;
    BuildOperationSuccessMock(module);
    EXPECT_EQ(UBSE_OK, UbseMemFdReturn(req, resp, NODE_ONE));
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdReturnImportNotExistTest)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemFdBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_TWO;
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    std::string name = "test";
    AddToExportObjMap(name, NODE_ONE, exportObj);

    UbseMemReturnReq req;
    req.name = name;
    req.importNodeId = NODE_ONE;
    UbseMemOperationResp resp;

    std::shared_ptr<com::UbseComModule> module;
    BuildOperationSuccessMock(module);
    EXPECT_EQ(UBSE_OK, UbseMemFdReturn(req, resp, NODE_ONE));

    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    AddToExportObjMap(name, NODE_ONE, exportObj);
    SendFdExportObjSuccess(module);

    EXPECT_EQ(UBSE_OK, UbseMemFdReturn(req, resp, NODE_ONE));
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdReturnExistTest)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemFdBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_TWO;
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    std::string name = "test";
    AddToExportObjMap(name, NODE_ONE, exportObj);
    AddToImportObjMap(name, NODE_ONE, importObj);

    UbseMemReturnReq req;
    req.name = name;
    req.importNodeId = NODE_ONE;
    UbseMemOperationResp resp;
    std::shared_ptr<com::UbseComModule> module;
    BuildOperationSuccessMock(module);

    EXPECT_EQ(UBSE_OK, UbseMemFdReturn(req, resp, NODE_ONE));

    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    AddToExportObjMap(name, NODE_ONE, exportObj);
    AddToImportObjMap(name, NODE_ONE, importObj);
    EXPECT_EQ(UBSE_OK, UbseMemFdReturn(req, resp, NODE_ONE));

    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    AddToExportObjMap(name, NODE_ONE, exportObj);
    SendFdExportObjSuccess(module);
    EXPECT_EQ(UBSE_OK, UbseMemFdReturn(req, resp, NODE_ONE));
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdPermissionSuccessTest)
{
    UbseMemFdBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    std::string name = "test";
    AddToImportObjMap(name, NODE_ONE, importObj);

    UbseMemFdPermissionReq req;
    req.name = name;
    req.requestNodeId = NODE_ONE;
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    const auto func = &com::UbseComModule::RpcSend<mem::controller::message::UbseMemFdBorrowImportobjSimpoPtr,
                                                   UbseMemOperationRespSimpoPtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseMemFdPermission(req, NODE_ONE));
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdPermissionTest)
{
    std::string name = "fd_test";
    const uint32_t UBS_ENGINE_ERR_NOT_EXIST = 1007;
    const uint32_t UBS_ENGINE_ERR_AUTH_FAILED = 1003;
    const uint32_t UBS_ENGINE_ERR_INTERNAL = 1005;
    UbseMemFdPermissionReq req;
    req.name = name;
    req.requestNodeId = NODE_ONE;
    uint32_t ret = UbseMemFdPermission(req, NODE_ONE);
    EXPECT_EQ(UBS_ENGINE_ERR_NOT_EXIST, ret);

    UbseMemFdBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    AddToImportObjMap(name, NODE_ONE, importObj);

    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    const auto func = &com::UbseComModule::RpcSend<mem::controller::message::UbseMemFdBorrowImportobjSimpoPtr,
                                                   UbseMemOperationRespSimpoPtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    ret = UbseMemFdPermission(req, NODE_ONE);
    EXPECT_EQ(UBS_ENGINE_ERR_INTERNAL, ret);

    UbseUdsInfo udsInfo{.uid = 1000, .gid = 1000, .pid = 1000, .username = "ubsmd"};
    req.udsInfo = udsInfo;
    ret = UbseMemFdPermission(req, NODE_ONE);
    EXPECT_EQ(UBS_ENGINE_ERR_AUTH_FAILED, ret);
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdBorrowImportObjForPermissionCallbackTest)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_TWO;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = NODE_ONE;
    importObj.req.size = 1024;
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseRoleInfo master;
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(master)).will(returnValue(UBSE_OK));
    MOCKER(&context::UbseContext::GetModule<UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdImportExecutor).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdBorrowImportObjForPermissionCallback(importObj), UBSE_OK);
}

TEST_F(TestUbseMemControllerFdApi, FdImportRunningCallback)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_TWO;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = NODE_ONE;
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = NODE_ONE;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&MemDecoderUtils::GetChipAndDieId).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&MemDecoderUtils::GetChipAndDieId).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_ERROR_NULLPTR, ret);
}

TEST_F(TestUbseMemControllerFdApi, UbseMemFdBorrowExportObjCallbackAgentHighSafe)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER(&context::UbseContext::GetModule<UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdExportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(IsHighSafety).stubs().will(returnValue(true));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(UBSE_OK, exportObj.errorCode);
    UbseMemObmmInfo obmmInfo;
    exportObj.status.exportObmmInfo.push_back(obmmInfo);
    ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}
} // namespace ubse::mem_controller::fd::ut