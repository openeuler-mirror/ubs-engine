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

#include "ubse_base_message.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_mem_api.h"
#include "ubse_mem_buffer_convert.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_dispatcher.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_def.h"
#include "ubse_mem_scheduler_impl.h"

using ubse::mem::scheduler::SchedulerImpl;
#include "ubse_mem_util.h"
#include "ubse_mmi_interface_impl.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"
#include "ubse_topo_util.h"
#include "message/ubse_mem_debt_info_query_req_simpo.h"
#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "src/adapter_plugins/mmi/ubse_mmi_module.h"

namespace ubse::mem_controller::ut {
using namespace ubse::task_executor;
using namespace ubse::context;
using namespace ubse::mem::scheduler;
using namespace ubse::com;
using namespace ubse::election;
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::message;
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::message;
using namespace ubse::mmi;
using namespace ubse::mem::util;
using namespace ubse::timer;
using namespace ubse::utils;
using namespace api::server;

const int BORROW_SIZE = 128;
void TestUbseMemControllerApi::SetUp()
{
    Test::SetUp();
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
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
    EXPECT_NO_THROW(RegisterNodeCtlNotify());

    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));

    EXPECT_NO_THROW(RegisterNodeCtlNotify());
}

TEST_F(TestUbseMemControllerApi, Init)
{
    MOCKER_CPP(&SchedulerImpl::Init).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == ubse::mem::controller::Init());
    EXPECT_TRUE(UBSE_OK == ubse::mem::controller::Init());
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowDispatchCheckParameterFail)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    UbseMemFdBorrowReq req{};
    MOCKER_CPP(&UbseMemCreateReqUnpack).expects(once()).with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);

    req.name = "UbseMemFdBorrowDispatchCheckParameterFail";
    MOCKER_CPP(&UbseMemCreateReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateReqUnpack).expects(once()).with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);

    req.size = 128ULL * 1024ULL * 1024ULL;
    req.distance = ubse::adapter_plugins::mmi::MEM_DISTANCE_L1;
    MOCKER_CPP(&UbseMemCreateReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateReqUnpack).expects(once()).with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);

    req.distance = ubse::adapter_plugins::mmi::MEM_DISTANCE_L0;
    req.lenderLocs = {ubse::adapter_plugins::mmi::UbseNumaLocation{}, ubse::adapter_plugins::mmi::UbseNumaLocation{},
                      ubse::adapter_plugins::mmi::UbseNumaLocation{}};
    MOCKER_CPP(&UbseMemCreateReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateReqUnpack).expects(once()).with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowDispatchRpcFail)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    UbseMemFdBorrowReq req{"UbseMemFdBorrowDispatchRpcFail"};
    req.size = 128ULL * 1024ULL * 1024ULL;
    req.distance = ubse::adapter_plugins::mmi::MEM_DISTANCE_L0;
    req.lenderLocs = {ubse::adapter_plugins::mmi::UbseNumaLocation{}};
    MOCKER_CPP(&UbseMemCreateReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);

    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);

    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR_NULLPTR);

    const auto func1 = &UbseComModule::RpcSend<UbseMemFdBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_ERROR);
    EXPECT_EQ(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowWithLenderDispatch)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    UbseMemFdBorrowReq req{"UbseMemFdBorrowDispatchRpcFail"};
    req.size = 2ULL * 128ULL * 1024ULL * 1024ULL;
    req.distance = ubse::adapter_plugins::mmi::MEM_DISTANCE_L0;
    req.lenderLocs = {ubse::adapter_plugins::mmi::UbseNumaLocation{}, ubse::adapter_plugins::mmi::UbseNumaLocation{}};
    req.lenderSizes = {0};
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemControllerDispatcher::UbseMemFdBorrowWithLenderDispatch(buffer, context));

    req.lenderSizes = {0, 0};
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemControllerDispatcher::UbseMemFdBorrowWithLenderDispatch(buffer, context));

    req.lenderSizes = {128ULL * 1024ULL * 1024ULL, 2 * 128ULL * 1024ULL * 1024ULL};
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemControllerDispatcher::UbseMemFdBorrowWithLenderDispatch(buffer, context));

    req.lenderSizes = {128ULL * 1024ULL * 1024ULL, 128ULL * 1024ULL * 1024ULL};
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).reset();
    MOCKER_CPP(&UbseMemCreateWithLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemControllerDispatcher::UbseMemFdBorrowDispatch(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdBorrowSendFdExportFail)
{
    UbseRoleInfo currentNode;
    currentNode.nodeRole = election::ELECTION_ROLE_MASTER;
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentNode)).will(returnValue(UBSE_OK));
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemFdBorrowReq req{};
    req.name = "test";
    req.requestNodeId = "0";
    req.size = BORROW_SIZE;
    req.udsInfo = udsInfo;
    UbseMemOperationResp resp{};
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>)
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
    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    UbseMemFdBorrowImportObj importObj{.req = req};
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    UbseMemDebtNumaInfo numaInfo{.nodeId = "0", .socketId = 0, .numaId = 0, .size = 0};
    importObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>)
        .stubs()
        .with(outBound(importObj))
        .will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(module));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == mem::controller::UbseMemFdBorrow(req, resp));
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaBorrowImportObjFail)
{
    UbseRoleInfo currentNode;
    currentNode.nodeRole = election::ELECTION_ROLE_MASTER;
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentNode)).will(returnValue(UBSE_OK));
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemNumaBorrowReq req{};
    req.name = "test";
    req.requestNodeId = "0";
    req.size = BORROW_SIZE;
    req.udsInfo = udsInfo;

    UbseMemOperationResp resp{};
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowImportObj>)
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
    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    UbseMemNumaBorrowImportObj importObj{.req = req};
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    UbseMemDebtNumaInfo numaInfo{.nodeId = "0", .socketId = 0, .numaId = 0, .size = 0};
    importObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowImportObj>)
        .stubs()
        .with(outBound(importObj))
        .will(returnValue(UBSE_OK));
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
    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    UbseMemNumaBorrowImportObj importObj{.req = req};
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    UbseMemDebtNumaInfo numaInfo{.nodeId = "0", .socketId = 0, .numaId = 0, .size = 0};
    importObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowImportObj>)
        .stubs()
        .with(outBound(importObj))
        .will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(module));
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
    AddFdExport(exportObj);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
    AddFdExport(exportObj);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    numaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = numaInfos;
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    AddFdImport(importObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    AddFdImport(importObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
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

    MOCKER_CPP(&mmi::UbseMmiModule::UbseMemGetObjData).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(LoadLocalAllObjs(info), UBSE_ERROR);
    EXPECT_EQ(LoadLocalAllObjs(info), UBSE_OK);

    MOCKER_CPP(&UbseContext::GetModule<mmi::UbseMmiModule>).reset();
    MOCKER_CPP(&mmi::UbseMmiModule::UbseMemGetObjData).reset();
    MOCKER_CPP(&UbseContext::GetModule<mmi::UbseMmiModule>).stubs().will(returnValue(module));
    // 模拟 MMI 接口返回的对象数据
    NodeMemDebtInfo nodeMemDebtInfo{};
    UbseMemShareBorrowExportObj ubseMemShareBorrowExportObj{};
    nodeMemDebtInfo.shareExportObjMap.insert({"test", ubseMemShareBorrowExportObj});
    UbseMemShareBorrowImportObj ubseMemShareBorrowImportObj{};
    nodeMemDebtInfo.shareImportObjMap.insert({"test", ubseMemShareBorrowImportObj});

    UbseMemAddrBorrowExportObj ubseMemAddrBorrowExportObj{};
    nodeMemDebtInfo.addrExportObjMap.insert({"test", ubseMemAddrBorrowExportObj});
    UbseMemAddrBorrowImportObj ubseMemAddrBorrowImportObj{};
    nodeMemDebtInfo.addrImportObjMap.insert({"test", ubseMemAddrBorrowImportObj});
    MOCKER_CPP(&mmi::UbseMmiModule::UbseMemGetObjData)
        .stubs()
        .with(outBound(nodeMemDebtInfo))
        .will(returnValue(UBSE_OK));
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
    EXPECT_NE(ret, UBSE_OK);
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_NE(ret, UBSE_OK);
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaExportExpectSuccessMasterCallbackFail1)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowExportObjCallback(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
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
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportExpectDestroyMasterCallbackExportNotExist)
{
    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerApi, FdImportExpectDestroyMasterCallback)
{
    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaBorrowImportObjCallback(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, NumaImportExpectSuccessMasterCallBackFail)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
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
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
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

TEST_F(TestUbseMemControllerApi, GetDebtInfoMapByNodeId)
{
    NodeMemDebtInfoMap memDebtInfoMap{};
    auto ret = GetDebtInfoMapByNodeId("1", memDebtInfoMap);
    EXPECT_NE(ret, UBSE_OK);
    ret = GetDebtInfoMapByNodeId("1", memDebtInfoMap);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    ret = GetDebtInfoMapByNodeId("1", memDebtInfoMap);
    EXPECT_NE(ret, UBSE_OK);
    const auto func1 = &UbseComModule::RpcSend<NodeMemDebtInfoQueryReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    ret = GetDebtInfoMapByNodeId("1", memDebtInfoMap);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaReturnRespHandler)
{
    UbseMemOperationResp resp{};
    resp.name = "test";
    auto ret = UbseMemControllerDispatcher::UbseMemNumaReturnRespHandler(resp);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    ret = UbseMemControllerDispatcher::UbseMemNumaReturnRespHandler(resp);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaReturnRespHandler(resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, GetNodeMemDebtInfoById)
{
    EXPECT_NO_THROW(GetNodeMemDebtInfoById("5"));
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;

    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    exportNumaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos = exportNumaInfos;
    AddNumaImport(importObj);
    EXPECT_NO_THROW(GetNodeMemDebtInfoById("1"));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    AddFdImport(importObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
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
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    AddFdImport(importObj);
    exportObj.req.requestNodeId = "1";
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "1";
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowExportObj>).stubs().will(returnValue(UBSE_OK));
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
    MOCKER(&UbseMmiModule::UbseMemGetObjData).stubs().with(outBound(nodeMemDebtInfo)).will(returnValue(UBSE_OK));
    auto ret = LoadLocalAllObjs(node);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemFdReturnDispatch)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    auto ret = UbseMemControllerDispatcher::UbseMemFdReturnDispatch(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    ret = UbseMemControllerDispatcher::UbseMemFdReturnDispatch(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    UbseMemReturnReq req{};
    MOCKER(UbseMemFdDeleteReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemFdReturnDispatch(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseMemFdDeleteReqUnpack).reset();
    req.name = "test";
    MOCKER(UbseMemFdDeleteReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemFdReturnDispatch(buffer, context);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemReturnReqSimpoPtr, UbseBaseMessagePtr>;
    ret = UbseMemControllerDispatcher::UbseMemFdReturnDispatch(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemFdReturnDispatch(buffer, context);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaCreateHandlerAsync)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    auto ret = UbseMemControllerDispatcher::UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    UbseMemNumaBorrowReq req{};
    MOCKER(UbseMemNumaCreateReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateHandler(buffer, context);
    EXPECT_EQ(ret, UBSE_ERROR);
    MOCKER(UbseMemNumaCreateReqUnpack).reset();
    req.name = "test";
    req.size = 128 * 1024; // 128*1024为测试数据
    MOCKER(UbseMemNumaCreateReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateHandler(buffer, context);
    EXPECT_EQ(ret, UBSE_ERROR);
    UbseRoleInfo currentRole{};
    currentRole.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).reset();
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentRole)).will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateHandler(buffer, context);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    UbseMemControllerDispatcher::UbseMemNumaCreateHandler(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaCreateWithLender)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    auto ret = UbseMemControllerDispatcher::UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    UbseMemNumaBorrowReq req{};
    MOCKER(UbseMemNumaCreateLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_EQ(ret, UBSE_ERROR);
    req.name = "test";
    req.size = 128 * 1024; // 128*1024为测试数据
    UbseRoleInfo currentRole{};
    currentRole.nodeId = "1";
    MOCKER(UbseMemNumaCreateLenderReqUnpack).reset();
    MOCKER(UbseMemNumaCreateLenderReqUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    MOCKER(UbseGetCurrentNodeInfo).reset();
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentRole)).will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateWithLender(buffer, context);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    const auto func1 = &UbseComModule::RpcSend<UbseMemNumaBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaCreateWithLender(buffer, context);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaDelete)
{
    UbseIpcMessage buffer{};
    UbseRequestContext context{};
    auto ret = UbseMemControllerDispatcher::UbseMemNumaDelete(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    ret = UbseMemControllerDispatcher::UbseMemNumaDelete(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    UbseMemControllerDispatcher::UbseMemNumaDelete(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    UbseMemReturnReq req{};
    req.name = "test";
    MOCKER(UbseMemNumaDeleteUnpack).stubs().with(any(), outBound(req)).will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaDelete(buffer, context);
    EXPECT_EQ(ret, UBSE_OK);
    UbseRoleInfo currentRole{};
    currentRole.nodeId = "1";
    MOCKER(UbseGetCurrentNodeInfo).reset();
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentRole)).will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaDelete(buffer, context);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    ret = UbseMemControllerDispatcher::UbseMemNumaDelete(buffer, context);
    EXPECT_NE(ret, UBSE_OK);
    const auto func1 = &UbseComModule::RpcSend<UbseMemReturnReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, UbseMemNumaBorrowRespHandler)
{
    UbseMemOperationResp resp;
    auto ret = UbseMemControllerDispatcher::UbseMemNumaBorrowRespHandler(resp);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemControllerDispatcher::UbseMemNumaBorrowRespHandler(resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, GetCnaInfoForNumaBorrow)
{
    // 准备输入参数
    std::string exportNodeId = "1";
    std::string importNodeId = "2";
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.requestNodeId = importNodeId;
    importObj.req.size = 128;
    importObj.algoResult.exportNumaInfos.push_back({.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128});

    // 模拟拓扑信息
    MOCKER(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(GetCnaInfoForNumaBorrow(exportNodeId, importNodeId, importObj), UBSE_ERROR);
    MOCKER(UbseNodeMemGetTopologyCnaInfo).reset();
    UbseNodeMemCnaInfoOutput cnaOutput;
    cnaOutput.borrowNodeCna = 1;
    cnaOutput.exportNodeCna = 2;
    cnaOutput.borrowSocketId = "0";
    cnaOutput.exportSocketId = "1";
    cnaOutput.portGroupId = "0";
    MOCKER(UbseNodeMemGetTopologyCnaInfo).stubs().with(any(), outBound(cnaOutput)).will(returnValue(UBSE_OK));
    EXPECT_EQ(GetCnaInfoForNumaBorrow(exportNodeId, importNodeId, importObj), UBSE_OK);
    UbseMemObmmInfo obmmInfo;
    importObj.exportObmmInfo.emplace_back(obmmInfo);
    MOCKER(&UbseNodeController::GetEid).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(GetCnaInfoForNumaBorrow(exportNodeId, importNodeId, importObj), UBSE_ERROR);
    MOCKER(&UbseNodeController::GetEid).reset();
    MOCKER(&UbseNodeController::GetEid).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(GetCnaInfoForNumaBorrow(exportNodeId, importNodeId, importObj), UBSE_OK);
    // 模拟lenderPort不为-1
    importObj.req.linkInfo.lenderPort = 1;
    EXPECT_EQ(GetCnaInfoForNumaBorrow(exportNodeId, importNodeId, importObj), UBSE_OK);
    MOCKER(IsSameSocketMultiPortTopo).stubs().will(returnValue(true));
    EXPECT_EQ(GetCnaInfoForNumaBorrow(exportNodeId, importNodeId, importObj), UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, CreateTaskExecutor)
{
    auto ret = CreateTaskExecutor();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerApi, GetCnaInfoWhenImport)
{
    std::string exportNodeId = "1";
    std::string importNodeId = "2";
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test";
    importObj.req.requestNodeId = importNodeId;
    importObj.req.size = 128;
    importObj.algoResult.exportNumaInfos.push_back({.nodeId = exportNodeId, .socketId = 0, .numaId = 0, .size = 128});
    // 模拟拓扑信息
    MOCKER(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(GetCnaInfoWhenImport(exportNodeId, importNodeId, importObj, true), UBSE_ERROR);
    MOCKER(UbseNodeMemGetTopologyCnaInfo).reset();
    MOCKER(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(GetCnaInfoWhenImport(exportNodeId, importNodeId, importObj, true), UBSE_OK);
    // importObj.exportObmmInfo不为空，cnaInput信息无效
    UbseMemObmmInfo obmmInfo;
    importObj.exportObmmInfo.emplace_back(obmmInfo);
    EXPECT_EQ(GetCnaInfoWhenImport(exportNodeId, importNodeId, importObj, true), UBSE_ERROR);
    UbseNodeMemCnaInfoOutput cnaOutput;
    cnaOutput.borrowNodeCna = 1;
    cnaOutput.exportNodeCna = 2;
    cnaOutput.borrowSocketId = "0";
    cnaOutput.exportSocketId = "1";
    cnaOutput.portGroupId = "0";
    MOCKER(UbseNodeMemGetTopologyCnaInfo).reset();
    MOCKER(UbseNodeMemGetTopologyCnaInfo).stubs().with(any(), outBound(cnaOutput)).will(returnValue(UBSE_OK));
    MOCKER(&UbseNodeController::GetEid).reset();
    MOCKER(&UbseNodeController::GetEid).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(GetCnaInfoWhenImport(exportNodeId, importNodeId, importObj, true), UBSE_OK);
}

TEST_F(TestUbseMemControllerApi, ClearNodeMap)
{
    EXPECT_NO_THROW(ClearNodeMap());
    MOCKER(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseElectionModule>()));
    EXPECT_NO_THROW(ClearNodeMap());
    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(true)).then(returnValue(false));
    EXPECT_NO_THROW(ClearNodeMap());
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.importNodeId = "2";
    importObj.req.name = "test";
    AddFdImport(importObj);
    UbseMemFdBorrowExportObj exportObj;
    exportObj.req.importNodeId = "2";
    exportObj.req.name = "test";
    AddFdExport(exportObj);
    std::string currentNodeId = "2";
    MOCKER(&UbseNodeController::GetCurrentNodeId).stubs().will(returnValue(currentNodeId));
    EXPECT_NO_THROW(ClearNodeMap());
    MOCKER(&UbseElectionModule::IsLeader).reset();
    MOCKER(&UbseElectionModule::IsLeader).stubs().will(returnValue(true));
    MOCKER(&UbseElectionModule::GetCurrentNode).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(ClearNodeMap());
}
} // namespace ubse::mem_controller::ut
