/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_rpc.h"

#include <mockcpp/mockcpp.hpp>

#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "ubse_api_server_module.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mem_rpc.h"
#include "ubse_mem_rpc_to_controller.h"
#include "ubse_mem_utils.h"
#include "ubse_thread_pool.h"

namespace ubse::mem_controller::ut {
using namespace task_executor;
using namespace ubse::message;
using namespace ubse::mem::controller;
using namespace mem::controller::message;

void TestUbseMemRpc::SetUp()
{
    Test::SetUp();
}
void TestUbseMemRpc::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowMessageHandler)
{
    UbseMemFdBorrowMessageHandler ubseMemFdBorrowMessageHandler{};
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&mem::utils::GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemFdBorrowReqSimpo();
    EXPECT_NE(req, nullptr);
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    EXPECT_EQ(ubseMemFdBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemFdBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowMessageHandlerGetOpCode)
{
    UbseMemFdBorrowMessageHandler ubseMemFdBorrowMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowMessageHandler.GetOpCode(), static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowMessageHandlerGetModuleCode)
{
    UbseMemFdBorrowMessageHandler ubseMemFdBorrowMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowMessageHandler.GetOpCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemNumaBorrowReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    MOCKER_CPP(&DoNumaBorrowAsync).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    UbseMemNumaBorrowMessageHandler ubseMemNumaBorrowMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemNumaBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowMessageHandlerGetOpCode)
{
    UbseMemNumaBorrowMessageHandler ubseMemNumaBorrowMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowMessageHandler.GetOpCode(), static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowMessageHandlerGetModuleCode)
{
    UbseMemNumaBorrowMessageHandler ubseMemNumaBorrowMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowMessageHandler.GetOpCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemReturnMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemReturnReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    MOCKER_CPP(&DoReturnAsync).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    UbseMemReturnMessageHandler ubseMemReturnMessageHandler{};
    EXPECT_EQ(ubseMemReturnMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemReturnMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemReturnMessageHandlerGetOpCode)
{
    UbseMemReturnMessageHandler ubseMemReturnMessageHandler{};
    EXPECT_EQ(ubseMemReturnMessageHandler.GetOpCode(), static_cast<uint16_t>(UbseOpCode::UBSE_MEM_RETURN));
}

TEST_F(TestUbseMemRpc, UbseMemReturnMessageHandlerGetModuleCode)
{
    UbseMemReturnMessageHandler ubseMemReturnMessageHandler{};
    EXPECT_EQ(ubseMemReturnMessageHandler.GetOpCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RETURN));
}

TEST_F(TestUbseMemRpc, Serialize)
{
    UbseMemCallbackMessage ubseMemCallbackMessage{};
    EXPECT_EQ(ubseMemCallbackMessage.Serialize(), UBSE_OK);
    ubseMemCallbackMessage.data = "Serialize";
    EXPECT_EQ(ubseMemCallbackMessage.Serialize(), UBSE_OK);
}

TEST_F(TestUbseMemRpc, Deserialize)
{
    UbseMemCallbackMessage ubseMemCallbackMessage{};
    EXPECT_EQ(ubseMemCallbackMessage.Deserialize(), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowExportObjCallbackMessageHandler)
{
    UbseMemFdBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemFdBorrowExportobjSimpo();
    EXPECT_NE(ptr, nullptr);
    UbseMemFdBorrowExportObj exportObj{};
    exportObj.algoResult.exportNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    ptr->SetUbseMemFdBorrowExportobj(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemFdBorrowExportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&mem::utils::GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));

    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    UbseMemFdBorrowExportObjCallbackMessageHandler handler{};
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_OK);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_MASTER_EMPTY_VECTOR_ERROR);
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowExportObjCallbackMessageHandlerGetOpCode)
{
    UbseMemFdBorrowExportObjCallbackMessageHandler ubseMemFdBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowExportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowExportObjCallbackMessageHandlerGetModuleCode)
{
    UbseMemFdBorrowExportObjCallbackMessageHandler ubseMemFdBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowExportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowImportObjCallbackMessageHandler)
{
    UbseMemFdBorrowImportObjCallbackMessageHandler ubseMemFdBorrowImportObjCallbackMessageHandler{};
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&mem::utils::GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemFdBorrowImportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    EXPECT_EQ(ubseMemFdBorrowImportObjCallbackMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemFdBorrowImportObjCallbackMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowImportObjCallbackMessageHandlerGetOpCode)
{
    UbseMemFdBorrowImportObjCallbackMessageHandler ubseMemFdBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowImportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowImportObjCallbackMessageHandlerGetModuleCode)
{
    UbseMemFdBorrowImportObjCallbackMessageHandler ubseMemFdBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowImportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowExportObjCallbackMessageHandler)
{
    UbseMemNumaBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemNumaBorrowExportobjSimpo();
    EXPECT_NE(ptr, nullptr);
    UbseMemNumaBorrowExportObj exportObj{};
    exportObj.algoResult.exportNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    ptr->SetUbseMemNumaBorrowExportobj(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemFdBorrowExportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&mem::utils::GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));

    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    UbseMemNumaBorrowExportObjCallbackMessageHandler handler{};
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_OK);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_MASTER_EMPTY_VECTOR_ERROR);
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowExportObjCallbackMessageHandlerGetOpCode)
{
    UbseMemNumaBorrowExportObjCallbackMessageHandler ubseMemNumaBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowExportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowExportObjCallbackMessageHandlerGetModuleCode)
{
    UbseMemNumaBorrowExportObjCallbackMessageHandler ubseMemNumaBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowExportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowImportObjCallbackMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemNumaBorrowImportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&mem::utils::GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    UbseMemNumaBorrowImportObjCallbackMessageHandler ubseMemNumaBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowImportObjCallbackMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemNumaBorrowImportObjCallbackMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowImportObjCallbackMessageHandlerGetOpCode)
{
    UbseMemNumaBorrowImportObjCallbackMessageHandler ubseMemNumaBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowImportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowImportObjCallbackMessageHandlerGetModuleCode)
{
    UbseMemNumaBorrowImportObjCallbackMessageHandler ubseMemNumaBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowImportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowRespMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemOperationRespSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::shared_ptr<api::server::UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<api::server::UbseApiServerModule> module = std::make_shared<api::server::UbseApiServerModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<api::server::UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));

    MOCKER_CPP(&api::server::UbseApiServerModule::SendResponse)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));

    UbseMemFdBorrowRespMessageHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR_NULLPTR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowRespMessageHandlerGetOpCode)
{
    UbseMemFdBorrowRespMessageHandler ubseMemFdBorrowRespMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowRespMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowRespMessageHandlerGetModuleCode)
{
    UbseMemFdBorrowRespMessageHandler ubseMemFdBorrowRespMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowRespMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER));
}

TEST_F(TestUbseMemRpc, UbseMemFdReturnRespMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemOperationRespSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::shared_ptr<api::server::UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<api::server::UbseApiServerModule> module = std::make_shared<api::server::UbseApiServerModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<api::server::UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));

    MOCKER_CPP(&api::server::UbseApiServerModule::SendResponse)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));

    UbseMemFdReturnRespMessageHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR_NULLPTR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemFdReturnRespMessageHandlerGetOpCode)
{
    UbseMemFdReturnRespMessageHandler ubseMemFdReturnRespMessageHandler{};
    EXPECT_EQ(ubseMemFdReturnRespMessageHandler.GetOpCode(), static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_RETURN));
}

TEST_F(TestUbseMemRpc, UbseMemFdReturnRespMessageHandlerGetModuleCode)
{
    UbseMemFdReturnRespMessageHandler ubseMemFdReturnRespMessageHandler{};
    EXPECT_EQ(ubseMemFdReturnRespMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER));
}

TEST_F(TestUbseMemRpc, RegHandlerFailed)
{
    UbseMemControllerModule ubseMemControllerModule{};
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    const auto getModuleFunc = &UbseContext::GetModule<UbseComModule>;
    MOCKER(getModuleFunc).stubs().will(returnValue(nullModule)).then(returnValue(module));

    const auto returnReqSimpoRegFunc = &UbseComModule::RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>;
    MOCKER(returnReqSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));

    const auto operationRespSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(operationRespSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK))
        .then(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE));

    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_ERROR);
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);
}

TEST_F(TestUbseMemRpc, RegFdHandlerFailed)
{
    UbseMemControllerModule ubseMemControllerModule{};
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    const auto getModuleFunc = &UbseContext::GetModule<UbseComModule>;
    MOCKER(getModuleFunc).stubs().will(returnValue(module)).then(returnValue(nullModule)).then(returnValue(module));

    const auto returnReqSimpoRegFunc = &UbseComModule::RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>;
    MOCKER(returnReqSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto operationRespSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(operationRespSimpoRegFunc).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_ERROR);

    const auto fdBorrowReqSimpoRegFunc = &UbseComModule::RegRpcService<UbseMemFdBorrowReqSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowReqSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);

    const auto fdBorrowExportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemFdBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowExportobjSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);

    const auto fdBorrowImportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemFdBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowImportobjSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);

    MOCKER(operationRespSimpoRegFunc).reset();
    MOCKER(operationRespSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_OK))
        .then(returnValue(UBSE_OK))
        .then(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);
}

TEST_F(TestUbseMemRpc, RegNumaHandlerFailed)
{
    UbseMemControllerModule ubseMemControllerModule{};
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    const auto getModuleFunc = &UbseContext::GetModule<UbseComModule>;
    MOCKER(getModuleFunc).stubs().will(returnValue(module)).then(returnValue(module))
        .then(returnValue(nullModule)).then(returnValue(module));

    const auto returnReqSimpoRegFunc = &UbseComModule::RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>;
    MOCKER(returnReqSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto operationRespSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(operationRespSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto fdBorrowReqSimpoRegFunc = &UbseComModule::RegRpcService<UbseMemFdBorrowReqSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowReqSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto fdBorrowExportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemFdBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowExportobjSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto fdBorrowImportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemFdBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowImportobjSimpoRegFunc).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_ERROR);

    const auto numaBorrowReqSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemNumaBorrowReqSimpo, UbseMemCallbackMessage>;
    MOCKER(numaBorrowReqSimpoRegFunc).stubs().will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);

    const auto numaBorrowExportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemNumaBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(numaBorrowExportobjSimpoRegFunc).stubs().will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);

    const auto numaBorrowImportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemNumaBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(numaBorrowImportobjSimpoRegFunc).stubs().will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);

    MOCKER(operationRespSimpoRegFunc).reset();
    MOCKER(operationRespSimpoRegFunc).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_OK))
        .then(returnValue(UBSE_OK)).then(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE));
    EXPECT_EQ(ubseMemControllerModule.Start(), UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE);
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowRespMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemOperationRespSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    MOCKER_CPP(&DoNumaBorrowRespAsync).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    UbseMemNumaBorrowRespMessageHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowRespMessageHandlerGetOpCode)
{
    UbseMemNumaBorrowRespMessageHandler handler{};
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowRespMessageHandlerGetModuleCode)
{
    UbseMemNumaBorrowRespMessageHandler handler{};
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER));
}

TEST_F(TestUbseMemRpc, UbseMemNumaReturnRespMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemOperationRespSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    MOCKER_CPP(&DoReturnRespAsync).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    UbseMemNumaReturnRespMessageHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemNumaReturnRespMessageHandlerGetOpCode)
{
    UbseMemNumaReturnRespMessageHandler handler{};
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_RETURN));
}

TEST_F(TestUbseMemRpc, UbseMemNumaReturnRespMessageHandlerGetModuleCode)
{
    UbseMemNumaReturnRespMessageHandler handler{};
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER));
}
}
