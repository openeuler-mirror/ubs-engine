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

#include "test_ubse_mem_rpc.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_debt_info_query_handler.h"
#include "message/ubse_mem_addr_borrow_exportobj_simpo.h"
#include "message/ubse_mem_addr_borrow_importobj_simpo.h"
#include "message/ubse_mem_addr_borrow_req_simpo.h"
#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "message/ubse_mem_share_attach_req_simpo.h"
#include "message/ubse_mem_share_borrow_exportobj_simpo.h"
#include "message/ubse_mem_share_borrow_importobj_simpo.h"
#include "message/ubse_mem_share_borrow_req_simpo.h"
#include "message/ubse_mem_share_detach_req_simpo.h"
#include "ubse_api_server_module.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_mem_async_processor.h"
#include "ubse_mem_controller_fd_api.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mem_rpc_processor.h"
#include "ubse_mem_util.h"
#include "ubse_thread_pool.h"

namespace ubse::mem_controller::ut {
using namespace task_executor;
using namespace ubse::message;
using namespace ubse::mem::controller;
using namespace mem::controller::message;
using namespace ubse::mem::util;

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
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
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

TEST_F(TestUbseMemRpc, UbseMemFdBorrowMessageHandler_GetOpCode)
{
    UbseMemFdBorrowMessageHandler ubseMemFdBorrowMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowMessageHandler.GetOpCode(),
              static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowMessageHandler_GetModuleCode)
{
    UbseMemFdBorrowMessageHandler ubseMemFdBorrowMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowMessageHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemNumaBorrowReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    UbseMemNumaBorrowMessageHandler ubseMemNumaBorrowMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemNumaBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowMessageHandler_GetOpCode)
{
    UbseMemNumaBorrowMessageHandler ubseMemNumaBorrowMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowMessageHandler.GetOpCode(),
              static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowMessageHandler_GetModuleCode)
{
    UbseMemNumaBorrowMessageHandler ubseMemNumaBorrowMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowMessageHandler.GetModuleCode(),
              static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemAddrBorrowMessageHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemAddrBorrowReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    UbseMemAddrBorrowMessageHandler ubseMemAddrBorrowMessageHandler{};
    EXPECT_EQ(ubseMemAddrBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemAddrBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemAddrBorrowMessageHandler_GetOpCode)
{
    UbseMemAddrBorrowMessageHandler ubseMemAddrBorrowMessageHandler{};
    EXPECT_EQ(ubseMemAddrBorrowMessageHandler.GetOpCode(),
              static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_ADDR_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemAddrBorrowMessageHandler_GetModuleCode)
{
    UbseMemAddrBorrowMessageHandler ubseMemAddrBorrowMessageHandler{};
    EXPECT_EQ(ubseMemAddrBorrowMessageHandler.GetModuleCode(),
              static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemShareBorrowMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShareBorrowMessageHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareBorrowReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    UbseMemShareBorrowMessageHandler ubseMemShareBorrowMessageHandler{};
    EXPECT_EQ(ubseMemShareBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemShareBorrowMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemShareBorrowMessageHandler_GetOpCode)
{
    UbseMemShareBorrowMessageHandler ubseMemShareBorrowMessageHandler{};
    EXPECT_EQ(ubseMemShareBorrowMessageHandler.GetOpCode(),
              static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemShareBorrowMessageHandler_GetModuleCode)
{
    UbseMemShareBorrowMessageHandler ubseMemShareBorrowMessageHandler{};
    EXPECT_EQ(ubseMemShareBorrowMessageHandler.GetModuleCode(),
              static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemShareAttachMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShareAttachMessageHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareAttachReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    UbseMemShareAttachMessageHandler ubseMemShareAttachMessageHandler{};
    EXPECT_EQ(ubseMemShareAttachMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemShareAttachMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemShareAttachMessageHandler_GetOpCode)
{
    UbseMemShareAttachMessageHandler ubseMemShareAttachMessageHandler{};
    EXPECT_EQ(ubseMemShareAttachMessageHandler.GetOpCode(),
              static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_ATTACH));
}

TEST_F(TestUbseMemRpc, UbseMemShareAttachMessageHandler_GetModuleCode)
{
    UbseMemShareAttachMessageHandler ubseMemShareAttachMessageHandler{};
    EXPECT_EQ(ubseMemShareAttachMessageHandler.GetModuleCode(),
              static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemShareDetachMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShareDetachMessageHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareDetachReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow)
        UbseComBaseMessageHandlerCtx("test", 0, 0, "1");
    EXPECT_NE(ctx, nullptr);
    UbseMemShareDetachMessageHandler ubseMemShareDetachMessageHandler{};
    EXPECT_EQ(ubseMemShareDetachMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemShareDetachMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
    delete ctx;
}

TEST_F(TestUbseMemRpc, UbseMemShareDetachMessageHandler_GetOpCode)
{
    UbseMemShareDetachMessageHandler ubseMemShareDetachMessageHandler{};
    EXPECT_EQ(ubseMemShareDetachMessageHandler.GetOpCode(),
              static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DETACH));
}

TEST_F(TestUbseMemRpc, UbseMemShareDetachMessageHandler_GetModuleCode)
{
    UbseMemShareDetachMessageHandler ubseMemShareDetachMessageHandler{};
    EXPECT_EQ(ubseMemShareDetachMessageHandler.GetModuleCode(),
              static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemFdReturnHandler
TEST_F(TestUbseMemRpc, UbseMemFdReturnHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemReturnReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow)
        UbseComBaseMessageHandlerCtx("test", 0, 0, "1");
    EXPECT_NE(ctx, nullptr);

    MOCKER_CPP(&DoReturnAsync).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    UbseMemFdReturnHandler ubseMemFdReturnHandler{};
    EXPECT_EQ(ubseMemFdReturnHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemFdReturnHandler.Handle(req, rsp, ctx), UBSE_OK);
    delete ctx;
}

TEST_F(TestUbseMemRpc, UbseMemFdReturnHandlerGetOpCode)
{
    UbseMemFdReturnHandler ubseMemFdReturnHandler{};
    EXPECT_EQ(ubseMemFdReturnHandler.GetOpCode(), static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_RETURN));
}

TEST_F(TestUbseMemRpc, UbseMemFdReturnHandlerGetModuleCode)
{
    UbseMemFdReturnHandler ubseMemFdReturnHandler{};
    EXPECT_EQ(ubseMemFdReturnHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
}

// UbseMemNumaReturnHandler
TEST_F(TestUbseMemRpc, UbseMemNumaReturnHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemReturnReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow)
        UbseComBaseMessageHandlerCtx("test", 0, 0, "1");

    MOCKER_CPP(&DoReturnAsync).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    UbseMemNumaReturnHandler ubseMemNumaReturnHandler{};
    EXPECT_EQ(ubseMemNumaReturnHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemNumaReturnHandler.Handle(req, rsp, ctx), UBSE_OK);
    delete ctx;
}

TEST_F(TestUbseMemRpc, UbseMemNumaReturnHandlerGetOpCode)
{
    UbseMemNumaReturnHandler ubseMemNumaReturnHandler{};
    EXPECT_EQ(ubseMemNumaReturnHandler.GetOpCode(), static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN));
}

TEST_F(TestUbseMemRpc, UbseMemNumaReturnHandlerGetModuleCode)
{
    UbseMemNumaReturnHandler ubseMemNumaReturnHandler{};
    EXPECT_EQ(ubseMemNumaReturnHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
}

// UbseMemShareReturnHandler
TEST_F(TestUbseMemRpc, UbseMemShareReturnHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemReturnReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow)
        UbseComBaseMessageHandlerCtx("test", 0, 0, "1");

    MOCKER_CPP(&DoReturnAsync).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    UbseMemShareReturnHandler ubseMemShareReturnHandler{};
    EXPECT_EQ(ubseMemShareReturnHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemShareReturnHandler.Handle(req, rsp, ctx), UBSE_OK);
    delete ctx;
}

TEST_F(TestUbseMemRpc, UbseMemShareReturnHandlerGetOpCode)
{
    UbseMemShareReturnHandler ubseMemShareReturnHandler{};
    EXPECT_EQ(ubseMemShareReturnHandler.GetOpCode(),
              static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_RETURN));
}

TEST_F(TestUbseMemRpc, UbseMemShareReturnHandlerGetModuleCode)
{
    UbseMemShareReturnHandler ubseMemShareReturnHandler{};
    EXPECT_EQ(ubseMemShareReturnHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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

    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow)
        UbseComBaseMessageHandlerCtx("test", 0, 0, "1");
    EXPECT_NE(ctx, nullptr);

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));

    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    MOCKER(&MemoryBorrowRpcObjCheck<UbseMemFdBorrowExportObj>).stubs().will(returnValue(UBSE_OK));

    UbseMemFdBorrowExportObjCallbackMessageHandler handler{};
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_OK);
    delete ctx;
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowExportObjCallbackMessageHandlerGetOpCode)
{
    UbseMemFdBorrowExportObjCallbackMessageHandler ubseMemFdBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowExportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowExportObjCallbackMessageHandlerGetModuleCode)
{
    UbseMemFdBorrowExportObjCallbackMessageHandler ubseMemFdBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowExportObjCallbackMessageHandler.GetModuleCode(),
              static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowImportObjCallbackMessageHandler)
{
    UbseMemFdBorrowImportObjCallbackMessageHandler ubseMemFdBorrowImportObjCallbackMessageHandler{};
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemFdBorrowImportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow)
        UbseComBaseMessageHandlerCtx("test", 0, 0, "1");
    MOCKER(&MemoryBorrowRpcObjCheck<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(ubseMemFdBorrowImportObjCallbackMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemFdBorrowImportObjCallbackMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
    delete ctx;
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowImportObjCallbackMessageHandlerGetOpCode)
{
    UbseMemFdBorrowImportObjCallbackMessageHandler ubseMemFdBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowImportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowImportObjCallbackMessageHandlerGetModuleCode)
{
    UbseMemFdBorrowImportObjCallbackMessageHandler ubseMemFdBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowImportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler
TEST_F(TestUbseMemRpc, UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemFdBorrowImportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemOperationRespSimpo();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    MOCKER_CPP(&DoReturnAsync).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler_GetOpCode)
{
    UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler
        ubseMemFdBorrowImportObjForPermissionCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowImportObjForPermissionCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_FOR_PERMISSION_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowImportObjCallbackMessageHandler_GetModuleCode)
{
    UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler
        ubseMemFdBorrowImportObjForPermissionCallbackMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowImportObjForPermissionCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowExportObjCallbackMessageHandler)
{
    UbseMemNumaBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemNumaBorrowExportobjSimpo();
    EXPECT_NE(ptr, nullptr);
    UbseMemNumaBorrowExportObj exportObj{};
    exportObj.algoResult.exportNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    ptr->SetUbseMemNumaBorrowExportobj(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemNumaBorrowExportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));

    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    MOCKER(&MemoryBorrowRpcObjCheck<UbseMemNumaBorrowExportObj>).stubs().will(returnValue(UBSE_OK));

    UbseMemNumaBorrowExportObjCallbackMessageHandler handler{};
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowExportObjCallbackMessageHandlerGetOpCode)
{
    UbseMemNumaBorrowExportObjCallbackMessageHandler ubseMemNumaBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowExportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowExportObjCallbackMessageHandlerGetModuleCode)
{
    UbseMemNumaBorrowExportObjCallbackMessageHandler ubseMemNumaBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowExportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
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
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    MOCKER(&MemoryBorrowRpcObjCheck<UbseMemNumaBorrowImportObj>).stubs().will(returnValue(UBSE_OK));

    UbseMemNumaBorrowImportObjCallbackMessageHandler ubseMemNumaBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowImportObjCallbackMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(ubseMemNumaBorrowImportObjCallbackMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowImportObjCallbackMessageHandlerGetOpCode)
{
    UbseMemNumaBorrowImportObjCallbackMessageHandler ubseMemNumaBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowImportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowImportObjCallbackMessageHandlerGetModuleCode)
{
    UbseMemNumaBorrowImportObjCallbackMessageHandler ubseMemNumaBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemNumaBorrowImportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemShareBorrowExportObjCallbackMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShareBorrowExportObjCallbackMessageHandler)
{
    UbseMemShareBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemShareBorrowExportobjSimpo();
    EXPECT_NE(ptr, nullptr);
    UbseMemShareBorrowExportObj exportObj{};
    exportObj.algoResult.exportNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    ptr->SetUbseMemShareBorrowExportobj(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareBorrowExportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    MOCKER(&ShareBorrowRpcObjCheck<UbseMemShareBorrowExportObj>).stubs().will(returnValue(UBSE_OK));

    UbseMemShareBorrowExportObjCallbackMessageHandler handler{};
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemShareBorrowExportObjCallbackMessageHandler_GetOpCode)
{
    UbseMemShareBorrowExportObjCallbackMessageHandler ubseMemShareBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemShareBorrowExportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_EXPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemShareBorrowExportObjCallbackMessageHandler_GetModuleCode)
{
    UbseMemShareBorrowExportObjCallbackMessageHandler ubseMemShareBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemShareBorrowExportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemShareBorrowImportObjCallbackMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShareBorrowImportObjCallbackMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareBorrowImportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    MOCKER(&ShareBorrowRpcObjCheck<UbseMemShareBorrowImportObj>).stubs().will(returnValue(UBSE_OK));

    UbseMemShareBorrowImportObjCallbackMessageHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemShareBorrowImportObjCallbackMessageHandler_GetOpCode)
{
    UbseMemShareBorrowImportObjCallbackMessageHandler ubseMemShareBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemShareBorrowImportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_IMPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemShareBorrowImportObjCallbackMessageHandler_GetModuleCode)
{
    UbseMemShareBorrowImportObjCallbackMessageHandler ubseMemShareBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemShareBorrowImportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemAddrBorrowExportObjCallbackMessageHandler
TEST_F(TestUbseMemRpc, UbseMemAddrBorrowExportObjCallbackMessageHandler)
{
    UbseMemAddrBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemAddrBorrowExportobjSimpo();
    EXPECT_NE(ptr, nullptr);
    UbseMemAddrBorrowExportObj exportObj{};
    exportObj.algoResult.exportNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    ptr->SetUbseMemAddrBorrowExportobj(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareBorrowExportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    MOCKER(&MemoryBorrowRpcObjCheck<UbseMemAddrBorrowExportObj>).stubs().will(returnValue(UBSE_OK));

    UbseMemAddrBorrowExportObjCallbackMessageHandler handler{};
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemAddrBorrowExportObjCallbackMessageHandler_GetOpCode)
{
    UbseMemAddrBorrowExportObjCallbackMessageHandler ubseMemAddrBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemAddrBorrowExportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_ADDR_BORROW_EXPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemAddrBorrowExportObjCallbackMessageHandler_GetModuleCode)
{
    UbseMemAddrBorrowExportObjCallbackMessageHandler ubseMemAddrBorrowExportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemAddrBorrowExportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemAddrBorrowImportObjCallbackMessageHandler
TEST_F(TestUbseMemRpc, UbseMemAddrBorrowImportObjCallbackMessageHandler)
{
    UbseMemAddrBorrowImportobjSimpoPtr ptr = new (std::nothrow) UbseMemAddrBorrowImportobjSimpo();
    EXPECT_NE(ptr, nullptr);
    UbseMemAddrBorrowImportObj exportObj{};
    exportObj.algoResult.exportNumaInfos.emplace_back(UbseMemDebtNumaInfo{});
    ptr->SetUbseMemAddrBorrowImportobj(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemAddrBorrowImportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()> &task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    MOCKER(&MemoryBorrowRpcObjCheck<UbseMemAddrBorrowImportObj>).stubs().will(returnValue(UBSE_OK));

    UbseMemAddrBorrowImportObjCallbackMessageHandler handler{};
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(ptr.Get(), rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemAddrBorrowImportObjCallbackMessageHandler_GetOpCode)
{
    UbseMemAddrBorrowImportObjCallbackMessageHandler ubseMemAddrBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemAddrBorrowImportObjCallbackMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_ADDR_BORROW_IMPORT_OBJ_CALLBACK));
}

TEST_F(TestUbseMemRpc, UbseMemAddrBorrowImportObjCallbackMessageHandler_GetModuleCode)
{
    UbseMemAddrBorrowImportObjCallbackMessageHandler ubseMemAddrBorrowImportObjCallbackMessageHandler{};
    EXPECT_EQ(ubseMemAddrBorrowImportObjCallbackMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
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
        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_BORROW_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemFdBorrowRespMessageHandlerGetModuleCode)
{
    UbseMemFdBorrowRespMessageHandler ubseMemFdBorrowRespMessageHandler{};
    EXPECT_EQ(ubseMemFdBorrowRespMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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
    EXPECT_EQ(ubseMemFdReturnRespMessageHandler.GetOpCode(),
              static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_RETURN_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemFdReturnRespMessageHandlerGetModuleCode)
{
    UbseMemFdReturnRespMessageHandler ubseMemFdReturnRespMessageHandler{};
    EXPECT_EQ(ubseMemFdReturnRespMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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

    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);
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
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    const auto fdBorrowExportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemFdBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowExportobjSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    const auto fdBorrowImportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemFdBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowImportobjSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    MOCKER(operationRespSimpoRegFunc).reset();
    MOCKER(operationRespSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_OK))
        .then(returnValue(UBSE_OK))
        .then(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);
}

TEST_F(TestUbseMemRpc, RegNumaHandlerFailed)
{
    UbseMemControllerModule ubseMemControllerModule{};
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    const auto getModuleFunc = &UbseContext::GetModule<UbseComModule>;
    MOCKER(getModuleFunc).stubs().will(returnValue(module)).then(returnValue(module))
        .then(returnValue(nullModule)).then(returnValue(module));

    const auto returnReqSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>;
    MOCKER(returnReqSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto operationRespSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(operationRespSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto fdBorrowReqSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemFdBorrowReqSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowReqSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto fdBorrowExportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemFdBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowExportobjSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto fdBorrowImportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemFdBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(fdBorrowImportobjSimpoRegFunc).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    const auto numaBorrowReqSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemNumaBorrowReqSimpo, UbseMemCallbackMessage>;
    MOCKER(numaBorrowReqSimpoRegFunc).stubs().will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    const auto numaBorrowExportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemNumaBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(numaBorrowExportobjSimpoRegFunc).stubs().will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    const auto numaBorrowImportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemNumaBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(numaBorrowImportobjSimpoRegFunc).stubs().will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    MOCKER(operationRespSimpoRegFunc).reset();
    MOCKER(operationRespSimpoRegFunc).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_OK))
        .then(returnValue(UBSE_OK)).then(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);
}

// UbseMemShmCreateRespMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShmCreateRespMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareBorrowImportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    MOCKER_CPP(&AsyncMemShmBorrowRespProcessor).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    UbseMemShmCreateRespMessageHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemShmCreateRespMessageHandler_GetOpCode)
{
    UbseMemShmCreateRespMessageHandler ubseMemShmCreateRespMessageHandler{};
    EXPECT_EQ(ubseMemShmCreateRespMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_BORROW_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemShmCreateRespMessageHandler_GetModuleCode)
{
    UbseMemShmCreateRespMessageHandler ubseMemShmCreateRespMessageHandler{};
    EXPECT_EQ(ubseMemShmCreateRespMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
}

// UbseMemShmAttachRespMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShmAttachRespMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemOperationRespSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    MOCKER_CPP(&AsyncMemShmAttachRespProcessor).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    UbseMemShmAttachRespMessageHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemShmAttachRespMessageHandler_GetOpCode)
{
    UbseMemShmAttachRespMessageHandler ubseMemShmAttachRespMessageHandler{};
    EXPECT_EQ(ubseMemShmAttachRespMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_ATTACH_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemShmAttachRespMessageHandler_GetModuleCode)
{
    UbseMemShmAttachRespMessageHandler ubseMemShmAttachRespMessageHandler{};
    EXPECT_EQ(ubseMemShmAttachRespMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
}

// UbseMemShmDetachRespMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShmDetachRespMessageHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemOperationRespSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    MOCKER_CPP(&AsyncMemShmDetachRespProcessor).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    UbseMemShmDetachRespMessageHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemShmDetachRespMessageHandler_GetOpCode)
{
    UbseMemShmDetachRespMessageHandler ubseMemShmDetachRespMessageHandler{};
    EXPECT_EQ(ubseMemShmDetachRespMessageHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_DETACH_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemShmDetachRespMessageHandler_GetModuleCode)
{
    UbseMemShmDetachRespMessageHandler ubseMemShmDetachRespMessageHandler{};
    EXPECT_EQ(ubseMemShmDetachRespMessageHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
}

// UbseMemReturnRespHandler
TEST_F(TestUbseMemRpc, UbseMemReturnRespHandler)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemOperationRespSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    MOCKER_CPP(&AsyncMemCommonReturnRespProcessor).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    UbseMemReturnRespHandler handler{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_ERROR);
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemReturnRespHandler_GetOpCode)
{
    UbseMemReturnRespHandler ubseMemReturnRespHandler{};
    EXPECT_EQ(ubseMemReturnRespHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_COMMON_RETURN_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemReturnRespHandler_GetModuleCode)
{
    UbseMemReturnRespHandler ubseMemReturnRespHandler{};
    EXPECT_EQ(ubseMemReturnRespHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemFdPermissionHandler_GetOpCode)
{
    UbseMemFdPermissionHandler ubseMemFdPermissionHandler{};
    EXPECT_EQ(ubseMemFdPermissionHandler.GetOpCode(),
        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_PERMISSION));
}

TEST_F(TestUbseMemRpc, UbseMemFdPermissionHandler_GetModuleCode)
{
    UbseMemFdPermissionHandler ubseMemFdPermissionHandler{};
    EXPECT_EQ(ubseMemFdPermissionHandler.GetModuleCode(),
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_BORROW_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemNumaBorrowRespMessageHandlerGetModuleCode)
{
    UbseMemNumaBorrowRespMessageHandler handler{};
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN_RESP));
}

TEST_F(TestUbseMemRpc, UbseMemNumaReturnRespMessageHandlerGetModuleCode)
{
    UbseMemNumaReturnRespMessageHandler handler{};
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
}

TEST_F(TestUbseMemRpc, RegisterNumaBorrowHandlers)
{
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MemScheduleHandler::RegisterNumaBorrowHandlers(comModule);
    const auto func = &UbseComModule::RegRpcService<UbseMemNumaBorrowReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterNumaBorrowHandlers(comModule), UBSE_ERROR);

    const auto func1 = &UbseComModule::RegRpcService<UbseMemNumaBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterNumaBorrowHandlers(comModule), UBSE_ERROR);

    const auto func3 = &UbseComModule::RegRpcService<UbseMemNumaBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(func3).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterNumaBorrowHandlers(comModule), UBSE_ERROR);

    const auto func2 = &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(func2).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterNumaBorrowHandlers(comModule), UBSE_ERROR);

    EXPECT_EQ(MemScheduleHandler::RegisterNumaBorrowHandlers(comModule), UBSE_OK);
}

TEST_F(TestUbseMemRpc, RegisterAddrBorrowHandlers)
{
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MemScheduleHandler::RegisterNumaBorrowHandlers(comModule);
    const auto func1 = &UbseComModule::RegRpcService<UbseMemAddrBorrowReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterAddrBorrowHandlers(comModule), UBSE_ERROR);

    const auto func = &UbseComModule::RegRpcService<UbseMemAddrBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterAddrBorrowHandlers(comModule), UBSE_ERROR);

    const auto func2 = &UbseComModule::RegRpcService<UbseMemAddrBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(func2).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterAddrBorrowHandlers(comModule), UBSE_ERROR);

    EXPECT_EQ(MemScheduleHandler::RegisterAddrBorrowHandlers(comModule), UBSE_OK);
}

TEST_F(TestUbseMemRpc, RegisterShareBorrowHandlers)
{
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MemScheduleHandler::RegisterNumaBorrowHandlers(comModule);
    const auto func = &UbseComModule::RegRpcService<UbseMemShareBorrowReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func1 = &UbseComModule::RegRpcService<UbseMemShareAttachReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func2 = &UbseComModule::RegRpcService<UbseMemShareDetachReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func2).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func3 = &UbseComModule::RegRpcService<UbseMemShareBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(func3).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func4 = &UbseComModule::RegRpcService<UbseMemShareBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(func4).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_OK);
}

TEST_F(TestUbseMemRpc, RegisterReturnHandler)
{
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MemScheduleHandler::RegisterNumaBorrowHandlers(comModule);
    const auto func = &UbseComModule::RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterReturnHandler(comModule), UBSE_ERROR);

    const auto func1 = &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterReturnHandler(comModule), UBSE_ERROR);

    EXPECT_EQ(MemScheduleHandler::RegisterReturnHandler(comModule), UBSE_OK);
}

TEST_F(TestUbseMemRpc, RegisterShmCreateRespHandlers)
{
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MemScheduleHandler::RegisterNumaBorrowHandlers(comModule);
    const auto func = &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShmCreateRespHandlers(comModule), UBSE_ERROR);

    EXPECT_EQ(MemScheduleHandler::RegisterShmCreateRespHandlers(comModule), UBSE_OK);
}

TEST_F(TestUbseMemRpc, RegisterShmAttachRespHandlers)
{
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MemScheduleHandler::RegisterNumaBorrowHandlers(comModule);
    const auto func = &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShmAttachRespHandlers(comModule), UBSE_ERROR);

    EXPECT_EQ(MemScheduleHandler::RegisterShmAttachRespHandlers(comModule), UBSE_OK);
}

TEST_F(TestUbseMemRpc, RegisterShmDetachRespHandlers)
{
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MemScheduleHandler::RegisterNumaBorrowHandlers(comModule);
    const auto func = &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShmDetachRespHandlers(comModule), UBSE_ERROR);

    EXPECT_EQ(MemScheduleHandler::RegisterShmDetachRespHandlers(comModule), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemFdPermissionReqMessage)
{
    UbseMemFdPermissionReq fdPermissionReq{};
    fdPermissionReq.name = "test";
    fdPermissionReq.requestNodeId = "1";
    fdPermissionReq.requestId  = 1;
    UbseMemFdPermissionReqMessage message(fdPermissionReq);
    EXPECT_EQ(message.Serialize(), UBSE_OK);

    message.SetInputRawData(message.SerializedData(), message.SerializedDataSize());
    EXPECT_EQ(message.Deserialize(), UBSE_OK);
}

TEST_F(TestUbseMemRpc, UbseMemFdPermissionRespMessage)
{
    UbseMemFdPermissionResp resp;
    resp.result = 0;
    resp.requestId = 1;
    UbseMemFdPermissionRespMessage message(resp);
    EXPECT_EQ(message.Serialize(), UBSE_OK);

    message.SetInputRawData(message.SerializedData(), message.SerializedDataSize());
    EXPECT_EQ(message.Deserialize(), UBSE_OK);
}
}
