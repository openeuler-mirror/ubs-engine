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

#include "test_ubse_mem_controller_api.h"

#include <mockcpp/mokc.h>
#include <netinet/in.h>

#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "message/ubse_mem_debtInfo_query_req_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "src/res_plugins/mmi/ubse_mmi_module.h"
#include "ubs_error.h"
#include "ubse_base_message.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_mem_api.h"
#include "ubse_mem_api_convert.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_scheduler.h"
#include "ubse_node_controller.h"
#include "ubse_node_topology.h"
#include "ubse_thread_pool_module.h"

namespace ubse::mem_controller::ut {
using namespace ubse::task_executor;
using namespace ubse::context;
using namespace ubse::mem::scheduler;
using namespace ubse::com;
using namespace ubse::election;
using namespace ubse::nodeController;
using namespace ubse::mem::obj;
using namespace ubse::message;
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::message;
using namespace ubse::mmi;
const int BORROW_SIZE = 128;
void TestUbseMemControllerApi::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerApi, RegisterNodeCtlNotify)
{
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_TRUE(UBS_ENGINE_ERR_INTERNAL == RegisterNodeCtlNotify());

    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));

    EXPECT_TRUE(UBSE_ERROR == RegisterNodeCtlNotify());
    EXPECT_TRUE(UBS_SUCCESS == RegisterNodeCtlNotify());
}

TEST_F(TestUbseMemControllerApi, Init)
{
    std::shared_ptr<UbseTaskExecutorModule> nullModule = nullptr;
    std::shared_ptr<UbseTaskExecutorModule> module = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_TRUE(UBSE_ERROR_NULLPTR == ubse::mem::controller::Init());
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == ubse::mem::controller::Init());
    MOCKER_CPP(ubse::mem::scheduler::Init).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == ubse::mem::controller::Init());
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&usbe::mem::api::UbseMemApi::Register).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == ubse::mem::controller::Init());
    EXPECT_TRUE(UBSE_OK == ubse::mem::controller::Init());
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowDispatchCheckParameterFail)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    UbseMemFdBorrowReq req{};
    MOCKER_CPP(&UbseMemCreateReqUnpack).expects(once()).with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), static_cast<int>(IPC_ERROR_INVALID_ARGUMENT));

    req.name = "UbseMemFdBorrowDispatchCheckParameterFail";
    MOCKER_CPP(&UbseMemCreateReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateReqUnpack).expects(once()).with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), static_cast<int>(IPC_ERROR_INVALID_ARGUMENT));

    req.size = 128ULL * 1024ULL * 1024ULL;
    req.distance = MEM_DISTANCE_L1;
    MOCKER_CPP(&UbseMemCreateReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateReqUnpack).expects(once()).with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), static_cast<int>(IPC_ERROR_INVALID_ARGUMENT));

    req.distance = MEM_DISTANCE_L0;
    req.lenderLocs = {mem::obj::UbseNumaLocation{}, mem::obj::UbseNumaLocation{}, mem::obj::UbseNumaLocation{}};
    MOCKER_CPP(&UbseMemCreateReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateReqUnpack).expects(once()).with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), static_cast<int>(IPC_ERROR_INVALID_ARGUMENT));
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowDispatchRpcFail)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    UbseMemFdBorrowReq req{"UbseMemFdBorrowDispatchRpcFail"};
    req.size = 128ULL * 1024ULL * 1024ULL;
    req.distance = MEM_DISTANCE_L0;
    req.lenderLocs = {mem::obj::UbseNumaLocation{}};
    MOCKER_CPP(&UbseMemCreateReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);

    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR_NULLPTR);

    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowWithLenderDispatch)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    UbseMemFdBorrowReq req{"UbseMemFdBorrowDispatchRpcFail"};
    req.size = 2ULL * 128ULL * 1024ULL * 1024ULL;
    req.distance = MEM_DISTANCE_L0;
    req.lenderLocs = {mem::obj::UbseNumaLocation{}, mem::obj::UbseNumaLocation{}};
    req.lenderSizes = {0};
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_TRUE(IPC_ERROR_INVALID_ARGUMENT == UbseMemFdBorrowWithLenderDispatch(buffer, context));

    req.lenderSizes = {0, 0};
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_TRUE(IPC_ERROR_INVALID_ARGUMENT == UbseMemFdBorrowWithLenderDispatch(buffer, context));

    req.lenderSizes = {128ULL * 1024ULL * 1024ULL, 2 * 128ULL * 1024ULL * 1024ULL};
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_TRUE(IPC_ERROR_INVALID_ARGUMENT == UbseMemFdBorrowWithLenderDispatch(buffer, context));

    req.lenderSizes = {128ULL * 1024ULL * 1024ULL, 128ULL * 1024ULL * 1024ULL};
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowSendFdExportFail)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemFdBorrowReq req{};
    req.name = "test";
    req.requestNodeId = "0";
    req.size = BORROW_SIZE;
    req.udsInfo = udsInfo;
    UbseMemOperationResp resp{};
    MOCKER_CPP(UbseMemFdImportObjStateChangeHandler)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_TRUE(UBSE_ERROR_NULLPTR == mem::controller::UbseMemFdBorrow(req, resp));
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrow)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemFdBorrowReq req{};
    req.name = "test";
    req.requestNodeId = "0";
    req.size = BORROW_SIZE;
    req.udsInfo = udsInfo;
    UbseMemOperationResp resp{};
    UbseMemFdBorrowImportObj importObj{.req = req};
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    UbseMemDebtNumaInfo numaInfo{.nodeId = "0", .socketId = 0, .numaId = 0, .size = 0};
    importObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    MOCKER_CPP(&UbseMemFdImportObjStateChangeHandler).stubs().with(outBound(importObj)).will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(module));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == mem::controller::UbseMemFdBorrow(req, resp));
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaBorrowImportObjFail)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemNumaBorrowReq req{};
    req.name = "test";
    req.requestNodeId = "0";
    req.size = BORROW_SIZE;
    req.udsInfo = udsInfo;

    UbseMemOperationResp resp{};
    MOCKER_CPP(UbseMemNumaImportObjStateChangeHandler)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_TRUE(UBSE_ERROR_NULLPTR == mem::controller::UbseMemNumaBorrow(req, resp));
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaBorrowSendNumaExportObjFail)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemNumaBorrowReq req{};
    req.name = "test";
    req.requestNodeId = "0";
    req.size = BORROW_SIZE;
    req.udsInfo = udsInfo;
    UbseMemOperationResp resp{};
    UbseMemNumaBorrowImportObj importObj{.req = req};
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    UbseMemDebtNumaInfo numaInfo{.nodeId = "0", .socketId = 0, .numaId = 0, .size = 0};
    importObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    MOCKER_CPP(&UbseMemNumaImportObjStateChangeHandler).stubs().with(outBound(importObj)).will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(module));
    EXPECT_TRUE(UBSE_ERROR == mem::controller::UbseMemNumaBorrow(req, resp));
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaBorrow)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemNumaBorrowReq req{};
    req.name = "test";
    req.requestNodeId = "0";
    req.size = BORROW_SIZE;
    req.udsInfo = udsInfo;
    UbseMemOperationResp resp{};
    UbseMemNumaBorrowImportObj importObj{.req = req};
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    UbseMemDebtNumaInfo numaInfo{.nodeId = "0", .socketId = 0, .numaId = 0, .size = 0};
    importObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    MOCKER_CPP(&UbseMemNumaImportObjStateChangeHandler).stubs().with(outBound(importObj)).will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(module));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_OK == mem::controller::UbseMemNumaBorrow(req, resp));
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowExportObjCallbackAgentRunning)
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
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_ERROR_NULLPTR, ret);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowExportObjCallbackAgentRunningSuccess)
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
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowExportObjCallbackAgentDestroying)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    MOCKER(UbseMemFdExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    AddFdExport(exportObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_ERROR_NULLPTR, ret);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowExportObjCallbackAgentDestroyingSuccess)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    MOCKER(UbseMemFdExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    AddFdExport(exportObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdExportMasterCallbackSuccess)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    MOCKER(UbseMemFdExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    AddFdExport(exportObj);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    MOCKER(UbseMemFdImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    AddFdImport(importObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    MOCKER(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdExportMasterCallbackDestoryedFaild)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    MOCKER(UbseMemFdExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    AddFdExport(exportObj);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    MOCKER(UbseMemFdImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    AddFdImport(importObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(UbseMemFdExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowExportObjCallback)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    AddFdExport(exportObj);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    MOCKER(UbseMemFdImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    AddFdImport(importObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(UbseMemFdExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, LoadLocalAllObjs)
{
    nodeController::UbseNodeInfo info{};
    info.localState = UbseNodeLocalState::UBSE_NODE_READY;
    info.nodeId = "LoadLocalAllObjs";
    EXPECT_EQ(LoadLocalAllObjs(info), UBSE_OK);

    info.localState = UbseNodeLocalState::UBSE_NODE_RESTORE;
    std::shared_ptr<mmi::UbseMmiModule> nullModule = nullptr;
    std::shared_ptr<mmi::UbseMmiModule> module = std::make_shared<mmi::UbseMmiModule>();
    MOCKER_CPP(&UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(LoadLocalAllObjs(info), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&mmi::UbseMmiModule::UbseMemGetObjData).stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(LoadLocalAllObjs(info), UBSE_ERROR);
    EXPECT_EQ(LoadLocalAllObjs(info), UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportRunningCallbackCreateSuccess)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportRunningCallbackMmiNull)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportDestroyingCallbackDestroySuccess1)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemNumaUnExportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportDestroyingCallbackDestroySuccess2)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    AddNumaExport(exportObj);
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportDestroyingCallbackDestroyFail)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    AddNumaExport(exportObj);
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemNumaUnExportExecutor).stubs().will(returnValue(UBSE_ERROR));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportMasterCallbackImportNoExist)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test1";
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportExpectSuccessMasterCallbackGetCnaFail)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    numaInfo.socketId = 0;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    std::vector<UbseMemObmmInfo> exportObmmInfos;
    UbseMemNumaBorrowImportObj importObj;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.req.name = "test";
    AddNumaImport(importObj);
    MOCKER(&GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    const auto func2 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemNumaExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}
TEST_F(TestUbseMemControllerApi, NumaExportExpectSuccessMasterCallbackSuccess)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    numaInfo.socketId = 0;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    std::vector<UbseMemObmmInfo> exportObmmInfos;
    UbseMemNumaBorrowImportObj importObj;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.req.name = "test";
    AddNumaImport(importObj);
    MOCKER(&GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    const auto func2 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemNumaExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportExpectSuccessMasterCallbackFail)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    numaInfo.socketId = 0;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    std::vector<UbseMemObmmInfo> exportObmmInfos;
    UbseMemNumaBorrowImportObj importObj;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.req.name = "test";
    AddNumaImport(importObj);
    MOCKER(&GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    const auto func2 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    const auto func3 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    MOCKER(func3).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemNumaExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportExpectSuccessMasterCallbackFail1)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    numaInfo.socketId = 0;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    std::vector<UbseMemObmmInfo> exportObmmInfos;
    UbseMemNumaBorrowImportObj importObj;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.req.name = "test";
    AddNumaImport(importObj);
    MOCKER(&GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    const auto func2 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    const auto func3 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    MOCKER(func3).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemNumaExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportExpectDestroyMasterCallbackDestroySuccess)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    numaInfo.socketId = 0;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    std::vector<UbseMemObmmInfo> exportObmmInfos;
    UbseMemNumaBorrowImportObj importObj;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.req.name = "test";
    AddNumaImport(importObj);
    MOCKER(UbseMemNumaExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApi, NumaExportExpectDestroyMasterCallbackDestroyFail)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    numaInfo.socketId = 0;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    std::vector<UbseMemObmmInfo> exportObmmInfos;
    UbseMemNumaBorrowImportObj importObj;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.req.name = "test";
    AddNumaImport(importObj);
    MOCKER(UbseMemNumaExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, FdExportRunningAgentCallbackExportFail)
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
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdExportExecutor).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdExportRunningAgentCallbackExportSuccess)
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
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdExportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdExportRunningAgentCallbackDestroyWait)
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
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    auto copy = exportObj;
    copy.status.state = UBSE_MEM_EXPORT_DESTROYING_WAIT;
    AddFdExport(copy);
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdExportExecutor).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportRunningCallback)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdImportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportRunningCallbackImportError)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdImportExecutor).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportRunningCallbackMmiNull)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportDestroyingAgentCallback)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    AddFdImport(importObj);
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdImportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportDestroyingAgentCallbackNotExist)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdImportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportDestroyingAgentCallbackMmiNull)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseMmiModule::UbseMemFdImportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportDestroyingAgentCallbackError)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    AddFdImport(importObj);
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemFdUnImportExecutor).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportExpectSuccessMasterCallback)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "2";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportExpectSuccessMasterCallbackSendFail)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "2";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportExpectSuccessMasterCallbackFail)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "2";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemFdImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportExpectDestroyMasterCallbackExportNotExist)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "2";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemFdImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportExpectDestroyMasterCallback)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemFdBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req.name = "test";
    AddFdExport(exportObj);
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "2";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemFdImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, NumaExportRunningCallbackExportFail)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemNumaExportExecutor).stubs().will(returnValue(UBSE_ERROR));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportRunningCallbackExportSuccess)
{
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "0";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.name = "test";
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;

    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemNumaExportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportRunningAgentCallbackMmiNull)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportRunningAgentCallbackImportFail)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemNumaImportExecutor).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportRunningAgentCallbackImportSuccess)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    MOCKER(&UbseMmiModule::UbseMemNumaImportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportDestroyingAgentCallbackMmiNull)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    AddNumaImport(importObj);
    MOCKER(&UbseMmiModule::UbseMemNumaImportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportDestroyingAgentCallbackDestoryFail)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    AddNumaImport(importObj);
    MOCKER(&UbseMmiModule::UbseMemNumaUnImportExecutor).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportDestroyingAgentCallbackDestorySuccess)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    AddNumaImport(importObj);
    MOCKER(&UbseMmiModule::UbseMemNumaUnImportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportExpectSuccessMasterCallBack)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "0";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemNumaImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportExpectSuccessMasterCallBackFail)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "0";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = importObj.req;
    AddNumaExport(exportObj);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemNumaImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportExpectDestroyedMasterCallBack)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "0";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = importObj.req;
    AddNumaExport(exportObj);
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func2 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportExpectDestroyedMasterCallBackFail)
{
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "0";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = importObj.req;
    AddNumaExport(exportObj);
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func2 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemReturn)
{
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    req.name = "test";
    req.requestNodeId = "1";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;
    std::vector<UbseMemDebtNumaInfo> importNumaInfos;
    numaInfo.nodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importNumaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = exportNumaInfos;
    importObj.algoResult.exportNumaInfos = exportNumaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = importObj.req;
    AddNumaExport(exportObj);
    AddNumaImport(importObj);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func2 = &UbseComModule::RpcSend<UbseMemNumaBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemReturn(req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemReturnSingleExport)
{
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    req.name = "test1";
    req.requestNodeId = "1";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;
    std::vector<UbseMemDebtNumaInfo> importNumaInfos;
    numaInfo.nodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importNumaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test1";
    importObj.req.importNodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = exportNumaInfos;
    importObj.algoResult.exportNumaInfos = exportNumaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = importObj.req;
    AddNumaExport(exportObj);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func2 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemReturn(req, resp);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApi, UbseMemReturnSingleExportDestoryed)
{
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    req.name = "test";
    req.requestNodeId = "1";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;
    std::vector<UbseMemDebtNumaInfo> importNumaInfos;
    numaInfo.nodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importNumaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = exportNumaInfos;
    importObj.algoResult.exportNumaInfos = exportNumaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = importObj.req;
    AddNumaImport(importObj);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func2 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemReturn(req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, DeleteFdExport)
{
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    req.name = "test";
    req.requestNodeId = "1";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;
    std::vector<UbseMemDebtNumaInfo> importNumaInfos;
    numaInfo.nodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importNumaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = exportNumaInfos;
    importObj.algoResult.exportNumaInfos = exportNumaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = importObj.req;
    UbseMemFdBorrowExportObj fdExportObj;
    fdExportObj.algoResult = importObj.algoResult;
    fdExportObj.req = importObj.req;
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = DeleteNumaExport(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
    ret = DeleteFdExport(fdExportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, GetNdeoMemDebtInfoMap)
{
    NodeMemDebtInfoMap memDebtInfoMap{};
    auto ret = GetNdeoMemDebtInfoMap("1", memDebtInfoMap);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    ret = GetNdeoMemDebtInfoMap("1", memDebtInfoMap);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    ret = GetNdeoMemDebtInfoMap("1", memDebtInfoMap);
    EXPECT_NE(ret, UBSE_OK);
    const auto func1 = &UbseComModule::RpcSend<NodeMemDebtInfoQueryReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    ret = GetNdeoMemDebtInfoMap("1", memDebtInfoMap);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaReturnRespHandler)
{
    UbseMemOperationResp resp{};
    resp.name = "test";
    auto ret = UbseMemNumaReturnRespHandler(resp);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs().will(returnValue(std::make_shared<UbseApiServerModule>()));
    ret = UbseMemNumaReturnRespHandler(resp);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemNumaReturnRespHandler(resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, GetNodeMemDebtInfoById)
{
    NodeMemDebtInfo info;
    auto ret = GetNodeMemDebtInfoById("5", info);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;

    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    exportNumaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = exportNumaInfos;
    AddNumaImport(importObj);
    ret = GetNodeMemDebtInfoById("1", info);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, FdExportMasterCallbackGetCnaFail)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    AddFdExport(exportObj);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    MOCKER(UbseMemFdImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    AddFdImport(importObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(UbseMemFdExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdExportMasterCallbackExportFail)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "0";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    AddFdExport(exportObj);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    MOCKER(UbseMemFdImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    AddFdImport(importObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(UbseMemFdExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, LoadObjState)
{
    ubse::nodeController::UbseNodeInfo node;
    node.localState = UbseNodeLocalState::UBSE_NODE_RESTORE;
    MOCKER(&UbseContext::GetModule<UbseMmiModule>).stubs().will(returnValue(std::make_shared<UbseMmiModule>()));
    NodeMemDebtInfo nodeMemDebtInfo{};
    UbseMemFdBorrowExportObj fdExportObj{};
    UbseMemNumaBorrowExportObj numaExportObj{};
    UbseMemFdBorrowImportObj fdImportObj{};
    UbseMemNumaBorrowImportObj numaImportObj{};
    nodeMemDebtInfo.fdExportObjMap["test"] = fdExportObj;
    nodeMemDebtInfo.numaExportObjMap["test"] = numaExportObj;
    nodeMemDebtInfo.fdImportObjMap["test"] = fdImportObj;
    nodeMemDebtInfo.numaImportObjMap["test"] = numaImportObj;
    MOCKER(&UbseMmiModule::UbseMemGetObjData)
        .stubs().with(outBound(nodeMemDebtInfo)).will(returnValue(UBSE_OK));
    auto ret = LoadLocalAllObjs(node);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdReturnDispatch)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    auto ret = UbseMemFdReturnDispatch(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemFdReturnDispatch(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    UbseMemReturnReq req{};
    MOCKER(UbseMemFdDeleteReqUnpack).stubs()
        .with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemFdReturnDispatch(buffer, context);
    EXPECT_EQ(ret, IPC_ERROR_INVALID_ARGUMENT);
    MOCKER(UbseMemFdDeleteReqUnpack).reset();
    req.name = "test";
    MOCKER(UbseMemFdDeleteReqUnpack).stubs()
        .with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemFdReturnDispatch(buffer, context);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemReturnReqSimpoPtr, UbseBaseMessagePtr>;
    ret = UbseMemFdReturnDispatch(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemFdReturnDispatch(buffer, context);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaCreateHandlerAsync)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    auto ret = UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    UbseMemNumaBorrowReq req{};
    MOCKER(UbseMemNumaCreateReqUnpack).stubs()
        .with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateHandler(buffer, context);
    EXPECT_EQ(ret, IPC_ERROR_INVALID_ARGUMENT);
    MOCKER(UbseMemNumaCreateReqUnpack).reset();
    req.name = "test";
    req.size = 128 * 1024; // 128*1024为测试数据
    MOCKER(UbseMemNumaCreateReqUnpack).stubs()
        .with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateHandler(buffer, context);
    EXPECT_EQ(ret, UBSE_ERROR);
    UbseRoleInfo currentRole{};
    currentRole.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).reset();
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentRole)).will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateHandler(buffer, context);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    ret = UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaCreateWithLender)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    auto ret = UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    UbseMemNumaBorrowReq req{};
    MOCKER(UbseMemNumaCreateLenderReqUnpack).stubs()
        .with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_EQ(ret, IPC_ERROR_INVALID_ARGUMENT);
    req.name = "test";
    req.size = 128 * 1024; // 128*1024为测试数据
    UbseRoleInfo currentRole{};
    currentRole.nodeId = "1";
    MOCKER(UbseMemNumaCreateLenderReqUnpack).reset();
    MOCKER(UbseMemNumaCreateLenderReqUnpack).stubs()
        .with(any(), outBound(req)).will(returnValue(UBSE_OK));
    MOCKER(UbseGetCurrentNodeInfo).reset();
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentRole)).will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateWithLender(buffer, context);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    ret = UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaDelete)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    auto ret = UbseMemNumaDelete(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemNumaDelete(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    UbseMemNumaDelete(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    UbseMemReturnReq req{};
    req.name = "test";
    MOCKER(UbseMemNumaDeleteUnPack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemNumaDelete(buffer, context);
    EXPECT_EQ(ret, UBSE_OK);
    UbseRoleInfo currentRole{};
    currentRole.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).reset();
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentRole)).will(returnValue(UBSE_OK));
    ret = UbseMemNumaDelete(buffer, context);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    ret = UbseMemNumaDelete(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    const auto func1 = &UbseComModule::RpcSend<UbseMemReturnReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemReturnFd)
{
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    req.name = "test";
    req.requestNodeId = "1";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;
    std::vector<UbseMemDebtNumaInfo> importNumaInfos;
    numaInfo.nodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importNumaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = exportNumaInfos;
    importObj.algoResult.exportNumaInfos = exportNumaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemFdBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = importObj.req;
    AddFdExport(exportObj);
    AddFdImport(importObj);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func2 = &UbseComModule::RpcSend<UbseMemFdBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemReturn(req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemReturnFdSingleExportDestoryed)
{
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    req.name = "test";
    req.requestNodeId = "1";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;
    std::vector<UbseMemDebtNumaInfo> importNumaInfos;
    numaInfo.nodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importNumaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "2";
    exportNumaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = exportNumaInfos;
    importObj.algoResult.exportNumaInfos = exportNumaInfos;
    UbseMemImportResult result;
    result.memId = 1;
    result.numaId = 1;
    importObj.status.importResults.emplace_back(result);
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemFdBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = importObj.req;
    AddFdImport(importObj);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func2 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemReturn(req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaBorrowRespHandler)
{
    UbseMemOperationResp resp;
    auto ret = UbseMemNumaBorrowRespHandler(resp);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs().will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(UbseMemNumaCreateResponsePack).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemNumaBorrowRespHandler(resp);
    EXPECT_EQ(ret, UBSE_OK);
}

}  // namespace ubse::mem_controller::ut