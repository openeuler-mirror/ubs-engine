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

#include "test_ubse_mem_controller_dispatcher.h"
#include "ubse_api_server_module.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_mem_async_processor.h"
#include "ubse_mem_buffer_convert.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mem_controller_dispatcher.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_node_api_convert.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::nodeController;
using namespace api::server;
using namespace ubse::context;
using namespace ubse::mem::def;
using namespace ubse::mem::controller::message;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::controller;
using namespace ubse::com;
using namespace ubse::node::api;

void TestUbseMemControllerDispatcher::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerDispatcher::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerDispatcher, RegisterFdSdkDispatcher)
{
    EXPECT_EQ(UbseMemControllerDispatcher::RegisterFdSdkDispatcher(), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    EXPECT_EQ(UbseMemControllerDispatcher::RegisterFdSdkDispatcher(), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, RegisterNumaSdkDispatcher)
{
    EXPECT_EQ(UbseMemControllerDispatcher::RegisterNumaSdkDispatcher(), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    EXPECT_EQ(UbseMemControllerDispatcher::RegisterNumaSdkDispatcher(), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, RegisterSdkDispatcher)
{
    EXPECT_EQ(UbseMemControllerDispatcher::RegisterSdkDispatcher(), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(UbseMemControllerDispatcher::RegisterShmSdkDispatcher).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemControllerDispatcher::RegisterNumaSdkDispatcher).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemControllerDispatcher::RegisterFdSdkDispatcher).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseMemControllerDispatcher::RegisterCliDispatcher).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemControllerDispatcher::RegisterSdkDispatcher(), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, RegisterShmSdkDispatcher)
{
    EXPECT_EQ(UbseMemControllerDispatcher::RegisterShmSdkDispatcher(), UBSE_ERROR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    EXPECT_EQ(UbseMemControllerDispatcher::RegisterShmSdkDispatcher(), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, BufferToShmBorrowReq)
{
    UbseIpcMessage buffer;
    UbseMemShareBorrowReqSimpoPtr reqSimpoPtr{};
    std::string requestNodeId = "1";
    UbseRequestContext context;
    UbseMemShmDispatcher shmDispatcher{};
    shmDispatcher.name = "test";
    shmDispatcher.size = 1024;
    shmDispatcher.usrInfo[0] = 1;
    UbseMemShmRegion shmRegion;
    shmRegion.nodeCnt = 1;
    shmRegion.slotIds[0] = 1;
    shmDispatcher.flag = 1;
    shmDispatcher.shmRegion = shmRegion;
    MOCKER(UbseMemShmCreateReqUnpack).stubs().with(any(), outBound(shmDispatcher)).will(returnValue(UBSE_OK));
    UbseMemControllerDispatcher dispatcher;
    MOCKER(&ubse::mem::controller::UbseNodeInfoGet).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.BufferToShmBorrowReq(buffer, reqSimpoPtr, requestNodeId, context, false), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, BufferToShmAttachReq)
{
    UbseMemControllerDispatcher dispatcher;
    UbseIpcMessage buffer;
    UbseMemShareAttachReqSimpoPtr reqSimpo;
    std::string requestNodeId = "1";
    UbseRequestContext context;
    UbseMemShareAttachReq shareAttachReq{};
    shareAttachReq.importNodeId = "1";
    shareAttachReq.name = "test";
    MOCKER(UbseMemShmAttachReqUnpack).stubs().with(any(), outBound(shareAttachReq)).will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.BufferToShmAttachReq(buffer, reqSimpo, requestNodeId, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, BufferToShmStatusGetReq)
{
    UbseMemControllerDispatcher dispatcher;
    MOCKER(UbseMemShmtatusGetReqUnPack).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_ERROR));
    UbseIpcMessage buffer;
    std::string name;
    EXPECT_EQ(dispatcher.BufferToShmStatusGetReq(buffer, name), UBSE_OK);
    EXPECT_EQ(dispatcher.BufferToShmStatusGetReq(buffer, name), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerDispatcher, BufferToShmGetReq)
{
    UbseMemControllerDispatcher dispatcher;
    MOCKER(UbseMemShmGetReqUnpack).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_ERROR));
    UbseIpcMessage buffer;
    std::string name;
    EXPECT_EQ(dispatcher.BufferToShmGetReq(buffer, name), UBSE_OK);
    EXPECT_EQ(dispatcher.BufferToShmGetReq(buffer, name), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerDispatcher, BufferToShmDetachReq)
{
    UbseIpcMessage buffer;
    UbseMemShareDetachReqSimpoPtr reqSimpo;
    std::string requestNodeId;
    UbseRequestContext context;
    UbseMemShareDetachReq shareDetachReq{};
    MOCKER(UbseMemShmDetachReqUnpack)
        .stubs()
        .with(any(), outBound(shareDetachReq))
        .will(returnValue(UBSE_OK))
        .then(returnValue(UBSE_ERROR));
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.BufferToShmDetachReq(buffer, reqSimpo, requestNodeId, context), UBSE_OK);
    EXPECT_EQ(dispatcher.BufferToShmDetachReq(buffer, reqSimpo, requestNodeId, context), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerDispatcher, BufferToShmReturnReq)
{
    UbseIpcMessage buffer;
    UbseMemReturnReqSimpoPtr reqSimpo;
    std::string requestNodeId;
    UbseRequestContext context;
    UbseMemReturnReq shareRetrunReq;
    UbseMemControllerDispatcher dispatcher;
    MOCKER(UbseMemShmDeleteReqUnpack)
        .stubs()
        .with(any(), outBound(shareRetrunReq))
        .will(returnValue(UBSE_OK))
        .then(returnValue(UBSE_ERROR));
    EXPECT_EQ(dispatcher.BufferToShmReturnReq(buffer, reqSimpo, requestNodeId, context), UBSE_OK);
    EXPECT_EQ(dispatcher.BufferToShmReturnReq(buffer, reqSimpo, requestNodeId, context), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmBorrowRespDispatcher)
{
    UbseMemOperationResp resp;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemShmBorrowRespDispatcher(resp), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    EXPECT_NE(dispatcher.MemShmBorrowRespDispatcher(resp), UBSE_OK);
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmBorrowRespDispatcher(resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmAttachRespDispatcher)
{
    UbseMemOperationResp resp;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemShmAttachRespDispatcher(resp), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    EXPECT_NE(dispatcher.MemShmAttachRespDispatcher(resp), UBSE_OK);
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmAttachRespDispatcher(resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, MemReturnRespDispatcher)
{
    UbseMemOperationResp resp;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemReturnRespDispatcher(resp), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    EXPECT_NE(dispatcher.MemReturnRespDispatcher(resp), UBSE_OK);
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemReturnRespDispatcher(resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmCreateDispatcher)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemShmCreateDispatcher(buffer, context), UBSE_ERR_DAEMON_UNREACHABLE);
    std::string localNodeId = "1";
    std::string masterNodeId = "1";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    MOCKER(&UbseMemControllerDispatcher::BufferToShmBorrowReq)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmCreateDispatcher(buffer, context), UBSE_ERROR);
    EXPECT_EQ(dispatcher.MemShmCreateDispatcher(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(AsyncMemShmBorrowProcessor).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmCreateDispatcher(buffer, context), UBSE_OK);
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId).reset();
    masterNodeId = "2";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmCreateDispatcher(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    EXPECT_NE(dispatcher.MemShmCreateDispatcher(buffer, context), UBSE_OK);
    const auto func = &UbseComModule::RpcSend<UbseMemShareBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmCreateDispatcher(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmCreateDispatcherWithAffinity)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemShmCreateDispatcherWithAffinity(buffer, context), UBSE_ERR_DAEMON_UNREACHABLE);
    std::string localNodeId = "1";
    std::string masterNodeId = "1";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    UbseMemShareBorrowReq req{};
    req.requestNodeId = "1";
    req.name = "test";
    UbseMemShareBorrowReqSimpoPtr ptr = new UbseMemShareBorrowReqSimpo();
    ptr->SetUbseMemShareBorrowReq(req);
    MOCKER(&UbseMemControllerDispatcher::BufferToShmBorrowReq)
        .stubs()
        .with(any(), outBound(ptr), any(), any(), any())
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmCreateDispatcherWithAffinity(buffer, context), UBSE_ERROR);
    EXPECT_EQ(dispatcher.MemShmCreateDispatcherWithAffinity(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(AsyncMemShmBorrowProcessor).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmCreateDispatcherWithAffinity(buffer, context), UBSE_OK);
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId).reset();
    masterNodeId = "2";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmCreateDispatcherWithAffinity(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    EXPECT_NE(dispatcher.MemShmCreateDispatcherWithAffinity(buffer, context), UBSE_OK);
    const auto func = &UbseComModule::RpcSend<UbseMemShareBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmCreateDispatcherWithAffinity(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmAttachDispatcher)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemShmAttachDispatcher(buffer, context), UBSE_ERR_DAEMON_UNREACHABLE);
    std::string localNodeId = "1";
    std::string masterNodeId = "1";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    MOCKER(&UbseMemControllerDispatcher::BufferToShmAttachReq)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmAttachDispatcher(buffer, context), UBSE_ERROR);
    EXPECT_EQ(dispatcher.MemShmAttachDispatcher(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(AsyncMemShmAttachProcessor).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmAttachDispatcher(buffer, context), UBSE_OK);
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId).reset();
    masterNodeId = "2";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmAttachDispatcher(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    EXPECT_NE(dispatcher.MemShmAttachDispatcher(buffer, context), UBSE_OK);
    const auto func = &UbseComModule::RpcSend<UbseMemShareAttachReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmAttachDispatcher(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmMemFaultGet)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemShmMemFaultGet(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(&UbseMemControllerDispatcher::BufferToShmStatusGetReq)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmMemFaultGet(buffer, context), UBSE_OK);
    MOCKER(UbseMemShmStatusGet).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmMemFaultGet(buffer, context), UBSE_OK);
    MOCKER(UbseMemShmMemFaultGetResponsePack).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(dispatcher.MemShmMemFaultGet(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmGetDispatcher)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemShmGetDispatcher(buffer, context), UBSE_ERROR_NULLPTR);
    std::string localNodeId = "1";
    std::string masterNodeId = "1";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmGetDispatcher(buffer, context), UBSE_OK);
    MOCKER(&UbseMemControllerDispatcher::BufferToShmGetReq)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmGetDispatcher(buffer, context), UBSE_OK);
    MOCKER(&UbseMemShmGet).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmGetDispatcher(buffer, context), UBSE_OK);
    MOCKER(UbseMemShmGetResponsePack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmGetDispatcher(buffer, context), UBSE_OK);
    EXPECT_EQ(dispatcher.MemShmGetDispatcher(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmListDispatcher)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemShmListDispatcher(buffer, context), UBSE_ERROR_NULLPTR);
    std::string localNodeId = "1";
    std::string masterNodeId = "1";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmListDispatcher(buffer, context), UBSE_OK);
    MOCKER(UbseMemShmList).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmListDispatcher(buffer, context), UBSE_OK);
    MOCKER(UbseMemShmListResponsePack).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(dispatcher.MemShmListDispatcher(buffer, context), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmDetachDispatcher)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.MemShmDetachDispatcher(buffer, context), UBSE_ERROR);
    std::string localNodeId = "1";
    std::string masterNodeId = "1";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    MOCKER(&UbseMemControllerDispatcher::BufferToShmDetachReq)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmDetachDispatcher(buffer, context), UBSE_ERROR);
    EXPECT_EQ(dispatcher.MemShmDetachDispatcher(buffer, context), UBSE_ERROR);
    MOCKER(AsyncMemShmDetachProcessor).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmDetachDispatcher(buffer, context), UBSE_OK);
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId).reset();
    masterNodeId = "2";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmDetachDispatcher(buffer, context), UBSE_ERROR);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func = &UbseComModule::RpcSend<UbseMemShareDetachReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmDetachDispatcher(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, MemShmReturnDispatcher)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_NE(dispatcher.MemShmReturnDispatcher(buffer, context), UBSE_OK);
    std::string localNodeId = "1";
    std::string masterNodeId = "1";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    MOCKER(&UbseMemControllerDispatcher::BufferToShmReturnReq)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmReturnDispatcher(buffer, context), UBSE_ERROR);
    EXPECT_EQ(dispatcher.MemShmReturnDispatcher(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(AsyncMemShmReturnProcessor).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmReturnDispatcher(buffer, context), UBSE_OK);
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId).reset();
    masterNodeId = "2";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId).reset();
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    EXPECT_NE(dispatcher.MemShmReturnDispatcher(buffer, context), UBSE_OK);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    EXPECT_NE(dispatcher.MemShmReturnDispatcher(buffer, context), UBSE_OK);
    const auto func = &UbseComModule::RpcSend<UbseMemReturnReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.MemShmReturnDispatcher(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, UbseMemFdPermissionDispatch)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    EXPECT_EQ(dispatcher.UbseMemFdPermissionDispatch(buffer, context), UBSE_ERROR);
    std::string localNodeId = "1";
    std::string masterNodeId = "1";
    MOCKER(&UbseMemControllerDispatcher::GetMasterAndLocalNodeId)
        .stubs()
        .with(outBound(localNodeId), outBound(masterNodeId))
        .will(returnValue(UBSE_OK));
    MOCKER(UbseMemFdPermissionReqUnpack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.UbseMemFdPermissionDispatch(buffer, context), UBSE_ERROR);
    EXPECT_EQ(dispatcher.UbseMemFdPermissionDispatch(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func = &UbseComModule::RpcSend<UbseBaseMessagePtr, UbseBaseMessagePtr>;
    MOCKER(func).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(dispatcher.UbseMemFdPermissionDispatch(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.UbseMemFdPermissionDispatch(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, UbseMemFdGetDispatch)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    MOCKER(UbseMemNameUnpack).stubs().will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::controller::UbseMemFdGet).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.UbseMemFdGetDispatch(buffer, context), UBSE_ERROR);
    MOCKER(UbseMemFdDescPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.UbseMemFdGetDispatch(buffer, context), UBSE_ERROR);
    EXPECT_EQ(dispatcher.UbseMemFdGetDispatch(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(dispatcher.UbseMemFdGetDispatch(buffer, context), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerDispatcher, UbseMemNumaBorrowWithCandidate)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    MOCKER(UbseMemNumaCreateWithCandidateReqUnpack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.UbseMemNumaBorrowWithCandidate(buffer, context), UBSE_ERROR);
    MOCKER(UbseMemControllerDispatcher::UbseMemNumaBorrowWithCandidate).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.UbseMemNumaBorrowWithCandidate(buffer, context), UBSE_OK);
}

TEST_F(TestUbseMemControllerDispatcher, UbseMemNumaGetDispatch)
{
    UbseIpcMessage buffer;
    UbseRequestContext context;
    UbseMemControllerDispatcher dispatcher;
    MOCKER(UbseMemNameUnpack).stubs().will(returnValue(UBSE_OK));
    MOCKER(ubse::mem::controller::UbseMemNumaGet).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.UbseMemNumaGetDispatch(buffer, context), UBSE_ERROR);
    MOCKER(UbseMemNumaDescPack).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(dispatcher.UbseMemNumaGetDispatch(buffer, context), UBSE_ERROR);
    EXPECT_EQ(dispatcher.UbseMemNumaGetDispatch(buffer, context), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseApiServerModule>()));
    MOCKER(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(dispatcher.UbseMemNumaGetDispatch(buffer, context), UBSE_ERROR);
}

// Test for UbseMemShmGetMemIdByImportDispatch - normal case
TEST_F(TestUbseMemControllerDispatcher, UbseMemShmGetMemIdByImportDispatch_NormalCase)
{
    UbseIpcMessage buffer{};
    buffer.buffer = reinterpret_cast<uint8_t*>(malloc(1024)); // 提供非空buffer
    buffer.length = 1024;

    UbseRequestContext context{};
    context.requestId = 12345;

    ubse::mem::def::UbseMemIdQueryRequest request{};
    MOCKER(UbseMemGetMemIdByImportReqUnpack).stubs().with(any(), outBound(request)).will(returnValue(UBSE_OK));

    ubse::election::UbseRoleInfo currentRoleInfo{};
    currentRoleInfo.nodeId = "1";
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_OK));

    ubse::mem::def::UbseExportMemDesc memDesc{};
    memDesc.exportSlotId = 2;
    memDesc.exportMemId = 0x1234567890ABCDEF;
    MOCKER(UbseMemIdGetByImportMemId).stubs().with(any(), outBound(memDesc)).will(returnValue(UBSE_OK));

    UbseIpcMessage resp{};
    MOCKER(UbseMemGetMemIdByImportResponsePack).stubs().with(any(), outBound(resp)).will(returnValue(UBSE_OK));

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));

    MOCKER_CPP(&UbseApiServerModule::SendResponse)
        .stubs()
        .with(UBSE_OK, context.requestId, any())
        .will(returnValue(UBSE_OK));

    uint32_t ret = UbseMemControllerDispatcher::UbseMemShmGetMemIdByImportDispatch(buffer, context);
    EXPECT_EQ(ret, UBSE_OK);

    SafeDeleteArray(resp.buffer);
    free(buffer.buffer);
}

// Test for UbseMemFdGetMemIdByImportDispatch - normal case
TEST_F(TestUbseMemControllerDispatcher, UbseMemFdGetMemIdByImportDispatch_NormalCase)
{
    UbseIpcMessage buffer{};
    buffer.buffer = reinterpret_cast<uint8_t*>(malloc(1024)); // 提供非空buffer
    buffer.length = 1024;

    UbseRequestContext context{};
    context.requestId = 12345;

    ubse::mem::def::UbseMemIdQueryRequest request{};
    MOCKER(UbseMemGetMemIdByImportReqUnpack).stubs().with(any(), outBound(request)).will(returnValue(UBSE_OK));

    ubse::election::UbseRoleInfo currentRoleInfo{};
    currentRoleInfo.nodeId = "1";
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_OK));

    ubse::mem::def::UbseExportMemDesc memDesc{};
    memDesc.exportSlotId = 2;
    memDesc.exportMemId = 0x1234567890ABCDEF;
    MOCKER(UbseMemIdGetByImportMemId).stubs().with(any(), outBound(memDesc)).will(returnValue(UBSE_OK));

    UbseIpcMessage resp{};
    MOCKER(UbseMemGetMemIdByImportResponsePack).stubs().with(any(), outBound(resp)).will(returnValue(UBSE_OK));

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));

    MOCKER_CPP(&UbseApiServerModule::SendResponse)
        .stubs()
        .with(UBSE_OK, context.requestId, any())
        .will(returnValue(UBSE_OK));

    uint32_t ret = UbseMemControllerDispatcher::UbseMemFdGetMemIdByImportDispatch(buffer, context);
    EXPECT_EQ(ret, UBSE_OK);

    SafeDeleteArray(resp.buffer);
    free(buffer.buffer);
}

// Test for UbseMemNumaGetMemIdByImportDispatch - normal case
TEST_F(TestUbseMemControllerDispatcher, UbseMemNumaGetMemIdByImportDispatch_NormalCase)
{
    UbseIpcMessage buffer{};
    buffer.buffer = reinterpret_cast<uint8_t*>(malloc(1024)); // 提供非空buffer
    buffer.length = 1024;

    UbseRequestContext context{};
    context.requestId = 12345;

    ubse::mem::def::UbseMemIdQueryRequest request{};
    MOCKER(UbseMemGetMemIdByImportReqUnpack).stubs().with(any(), outBound(request)).will(returnValue(UBSE_OK));

    ubse::election::UbseRoleInfo currentRoleInfo{};
    currentRoleInfo.nodeId = "1";
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentRoleInfo))
        .will(returnValue(UBSE_OK));

    ubse::mem::def::UbseExportMemDesc memDesc{};
    memDesc.exportSlotId = 2;
    memDesc.exportMemId = 0x1234567890ABCDEF;
    MOCKER(UbseMemIdGetByImportMemId).stubs().with(any(), outBound(memDesc)).will(returnValue(UBSE_OK));

    UbseIpcMessage resp{};
    MOCKER(UbseMemGetMemIdByImportResponsePack).stubs().with(any(), outBound(resp)).will(returnValue(UBSE_OK));

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));

    MOCKER_CPP(&UbseApiServerModule::SendResponse)
        .stubs()
        .with(UBSE_OK, context.requestId, any())
        .will(returnValue(UBSE_OK));
    uint32_t ret = UbseMemControllerDispatcher::UbseMemNumaGetMemIdByImportDispatch(buffer, context);
    EXPECT_EQ(ret, UBSE_OK);
    SafeDeleteArray(resp.buffer);
    free(buffer.buffer);
}

// Test for UbseMemShmGetMemIdByImportDispatch - null buffer
TEST_F(TestUbseMemControllerDispatcher, UbseMemShmGetMemIdByImportDispatch_NullBuffer)
{
    UbseIpcMessage buffer{};
    buffer.buffer = nullptr;
    buffer.length = 0;
    UbseRequestContext context{};
    uint32_t ret = UbseMemControllerDispatcher::UbseMemShmGetMemIdByImportDispatch(buffer, context);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerDispatcher, UbseMemShmGetMemIdByImportDispatch_UnpackFailure)
{
    UbseIpcMessage buffer{};
    buffer.buffer = reinterpret_cast<uint8_t*>(malloc(1024));
    buffer.length = 1024;
    UbseRequestContext context{};
    MOCKER(UbseMemGetMemIdByImportReqUnpack).stubs().will(returnValue(UBSE_ERROR_DESERIALIZE_FAILED));
    uint32_t ret = UbseMemControllerDispatcher::UbseMemShmGetMemIdByImportDispatch(buffer, context);
    EXPECT_EQ(ret, UBSE_ERROR_DESERIALIZE_FAILED);
    free(buffer.buffer);
}
} // namespace ubse::mem_controller::ut