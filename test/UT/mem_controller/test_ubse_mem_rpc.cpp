#include "migrate/ubse_mem_migrate_internal.h"
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
#include <optional>

#include "ubse_api_server_module.h"
#include "ubse_com_module.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_mem_api.h"
#include "ubse_mem_async_processor.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_fd_api.h"
#include "ubse_mem_controller_module.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_debt_info_query_handler.h"
#include "ubse_mem_decoder_utils.h"
#include "ubse_mem_migrate_cancel_handler.h"
#include "ubse_mem_migrate_export_handler.h"
#include "ubse_mem_migrate_get_handler.h"
#include "ubse_mem_migrate_session.h"
#include "ubse_mem_migrate_switch_handler.h"
#include "ubse_mem_rpc_processor.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mem_util.h"
#include "ubse_mmi_module.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "debt/ubse_mem_debt_ledger.h"
#include "message/ubse_mem_simpo_types.h"
#include "migrate/ubse_mem_controller_migrate_api.h"
#include "migrate/ubse_mem_migrate_helper.h"

namespace ubse::mem_controller::ut {
using namespace task_executor;
using namespace ubse::message;
using namespace ubse::mem::controller;
using namespace ubse::mem::api;
using namespace mem::controller::message;
using namespace ubse::mem::util;
using namespace ubse::com;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::context;

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
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    EXPECT_EQ(ubseMemNumaBorrowMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

TEST_F(TestUbseMemRpc, UbseMemAddrBorrowMessageHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    EXPECT_EQ(ubseMemAddrBorrowMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemShareBorrowMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShareBorrowMessageHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    EXPECT_EQ(ubseMemShareBorrowMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemShareAttachMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShareAttachMessageHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    EXPECT_EQ(ubseMemShareAttachMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemShareDetachMessageHandler
TEST_F(TestUbseMemRpc, UbseMemShareDetachMessageHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareDetachReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow) UbseComBaseMessageHandlerCtx("test", 0, 0, "1");
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
    EXPECT_EQ(ubseMemShareDetachMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW));
}

// UbseMemFdReturnHandler
TEST_F(TestUbseMemRpc, UbseMemFdReturnHandler)
{
    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemReturnReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow) UbseComBaseMessageHandlerCtx("test", 0, 0, "1");
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
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemReturnReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow) UbseComBaseMessageHandlerCtx("test", 0, 0, "1");

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
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemReturnReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow) UbseComBaseMessageHandlerCtx("test", 0, 0, "1");

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
    ptr->SetUbseMesgInfo(exportObj);

    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow) UbseComBaseMessageHandlerCtx("test", 0, 0, "1");
    EXPECT_NE(ctx, nullptr);

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));

    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemFdBorrowImportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    auto ctx = new (std::nothrow) UbseComBaseMessageHandlerCtx("test", 0, 0, "1");
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
    ptr->SetUbseMesgInfo(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemNumaBorrowExportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));

    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    ptr->SetUbseMesgInfo(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareBorrowExportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    ptr->SetUbseMesgInfo(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShareBorrowExportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    ptr->SetUbseMesgInfo(exportObj);

    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemAddrBorrowImportobjSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemCallbackMessage();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};

    std::string name = "ubseMemController";
    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(nullPtr)).then(returnValue(ubseTaskExecutorPtr));
    bool (UbseTaskExecutor::*func)(const std::function<void()>& task) = &UbseTaskExecutor::Execute;
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
    EXPECT_EQ(ubseMemFdBorrowRespMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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
    EXPECT_EQ(ubseMemFdReturnRespMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
}

TEST_F(TestUbseMemRpc, RegHandlerFailed)
{
    UbseMemControllerModule ubseMemControllerModule{};
    const auto regMemControllerHandlerFunc = &ubse::mem::controller::RegUbseMemControllerHandler;
    MOCKER_CPP(regMemControllerHandlerFunc).stubs();
    const auto memApiRegisterFunc = &UbseMemApi::Register;
    MOCKER_CPP(memApiRegisterFunc).stubs().will(returnValue(UBSE_OK));
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
    const auto regMemControllerHandlerFunc = &ubse::mem::controller::RegUbseMemControllerHandler;
    MOCKER_CPP(regMemControllerHandlerFunc).stubs();
    const auto memApiRegisterFunc = &UbseMemApi::Register;
    MOCKER_CPP(memApiRegisterFunc).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    const auto getModuleFunc = &UbseContext::GetModule<UbseComModule>;
    MOCKER(getModuleFunc).stubs().will(returnValue(module)).then(returnValue(nullModule)).then(returnValue(module));

    const auto returnReqSimpoRegFunc = &UbseComModule::RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>;
    MOCKER(returnReqSimpoRegFunc).stubs().will(returnValue(UBSE_OK));

    const auto operationRespSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>;
    MOCKER(operationRespSimpoRegFunc).stubs().will(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

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
    const auto regMemControllerHandlerFunc = &ubse::mem::controller::RegUbseMemControllerHandler;
    MOCKER_CPP(regMemControllerHandlerFunc).stubs();
    const auto memApiRegisterFunc = &UbseMemApi::Register;
    MOCKER_CPP(memApiRegisterFunc).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    const auto getModuleFunc = &UbseContext::GetModule<UbseComModule>;
    MOCKER(getModuleFunc)
        .stubs()
        .will(returnValue(module))
        .then(returnValue(module))
        .then(returnValue(nullModule))
        .then(returnValue(module));

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
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    const auto numaBorrowReqSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemNumaBorrowReqSimpo, UbseMemCallbackMessage>;
    MOCKER(numaBorrowReqSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    const auto numaBorrowExportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemNumaBorrowExportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(numaBorrowExportobjSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    const auto numaBorrowImportobjSimpoRegFunc =
        &UbseComModule::RegRpcService<UbseMemNumaBorrowImportobjSimpo, UbseMemCallbackMessage>;
    MOCKER(numaBorrowImportobjSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE))
        .then(returnValue(UBSE_OK));
    EXPECT_NE(ubseMemControllerModule.Start(), UBSE_OK);

    MOCKER(operationRespSimpoRegFunc).reset();
    MOCKER(operationRespSimpoRegFunc)
        .stubs()
        .will(returnValue(UBSE_OK))
        .then(returnValue(UBSE_OK))
        .then(returnValue(UBSE_OK))
        .then(returnValue(UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE));
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
    EXPECT_EQ(ubseMemShmCreateRespMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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
    EXPECT_EQ(ubseMemShmAttachRespMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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
    EXPECT_EQ(ubseMemShmDetachRespMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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
    EXPECT_EQ(ubseMemReturnRespHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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
    EXPECT_EQ(ubseMemFdPermissionHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP));
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

    // 迁移请求和业务结果均只同步确认入队，异步结果使用独立 opcode。
    const auto func5 = &UbseComModule::RegRpcService<UbseMemMigrateSwitchBeginImportReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func5).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func6 = &UbseComModule::RegRpcService<UbseMemMigrateBeginExportReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func6).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func7 = &UbseComModule::RegRpcService<UbseMemMigrateEndExportReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func7).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func8 = &UbseComModule::RegRpcService<UbseMemMigrateDataCopyReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func8).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func9 = &UbseComModule::RegRpcService<UbseMemMigrateSwitchEndImportReqSimpo, UbseMemCallbackMessage>;
    MOCKER(func9).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func10 = &UbseComModule::RegRpcService<UbseMemMigrateOpRespSimpo, UbseMemCallbackMessage>;
    MOCKER(func10).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(MemScheduleHandler::RegisterShareBorrowHandlers(comModule), UBSE_ERROR);

    const auto func11 = &UbseComModule::RegRpcService<UbseMemMigrateBeginExportRespSimpo, UbseMemCallbackMessage>;
    MOCKER(func11).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
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
    fdPermissionReq.requestId = 1;
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

// 迁移 agent：资源只来自账本，当前块上下文保留待提交资源和结束决定。
namespace {
using namespace ubse::mem::controller::rpc;
using namespace ubse::mem::controller::debt;
const std::string MIG_SWITCH_NODE = "13";
const std::string MIG_SWITCH_DST_NODE = "14";
std::string MIG_SWITCH_NAME;
uint64_t g_switchRequestId = 50000;
constexpr uint64_t MIG_SWITCH_OLD_IMPORT_MEMID = 3001;

void SwitchTestMockSet()
{
    MIG_SWITCH_NAME = "migrate_switch_test_mem_" + std::to_string(++g_switchRequestId);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    MOCKER(&GetCurNodeId).stubs().will(returnValue(MIG_SWITCH_NODE));
}

void MigrateControllerMockSet()
{
    MOCKER(&ubse::election::UbseGetMasterNodeId).stubs().with(outBound(std::string("1"))).will(returnValue(UBSE_OK));
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mem::decoder::utils::MemDecoderUtils::GetChipAndDieId).stubs().will(returnValue(UBSE_OK));
}

UbseMemShareBorrowImportObj BuildSwitchTestImportObj(uint32_t count = 2)
{
    UbseMemShareBorrowImportObj obj{};
    obj.req.name = MIG_SWITCH_NAME;
    obj.req.baseNodeId = "11";
    obj.importNodeId = MIG_SWITCH_NODE;
    obj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    obj.algoResult.exportNumaInfos = {{"11", 0, 0, count * 128ULL}};
    for (uint32_t b = 0; b < count; ++b) {
        obj.status.importResults.push_back({3001 + b, -1});
        obj.status.decoderResult.push_back({0, 0x7000 + b, 0x7100 + b});
        UbseMemObmmInfo block{};
        block.memId = 1001 + b;
        block.desc.length = 128;
        obj.exportObmmInfo.push_back(block);
        obj.migrateInfos.push_back(MakeShmMigrateInfo(0, b, 0, false));
    }
    return obj;
}

void PutSwitchTestImportObj(const UbseMemShareBorrowImportObj& obj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(obj.importNodeId,
                                                                                           obj.req.name, obj);
}

auto GetSwitchTestImportObj()
{
    return UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(MIG_SWITCH_NODE,
                                                                                                  MIG_SWITCH_NAME);
}

UbseMemMigrateRpcInfo AgentRpc(uint32_t b = 0)
{
    return {MIG_SWITCH_NAME, b, MIG_SWITCH_NODE, 4001 + b, MakeShmMigrateInfo(1, b, 11, true), false};
}

UbseMemMigrateSwitchBeginImportReq AgentBegin(uint32_t b = 0)
{
    UbseMemMigrateSwitchBeginImportReq req{};
    req.rpcInfo = AgentRpc(b);
    req.oldExportMemId = 1001 + b;
    req.targetBlock.memId = req.rpcInfo.exportMemId;
    req.targetBlock.memIdStatus = UB_MEM_SWITCHING;
    req.targetBlock.desc.addr = 0x200000;
    req.targetBlock.desc.length = 128;
    req.targetBlock.desc.scna = 5;
    req.targetBlock.desc.dcna = 7;
    req.targetLayout.exportNumaInfos = {{MIG_SWITCH_DST_NODE, 1, 2, 256}};
    req.targetLayout.importNumaInfos = {{MIG_SWITCH_NODE, 1, 2, 256}};
    req.targetLayout.attachSocketId = 1;
    return req;
}

void PutPendingImport(uint32_t b = 0)
{
    auto obj = BuildSwitchTestImportObj();
    if (b != 0) {
        obj.migrateInfos[b - 1] = MakeShmMigrateInfo(1, b - 1, 0, false);
    }
    const auto req = AgentBegin(b);
    obj.switchContext = migrate_detail::MakeSwitchContext(req.rpcInfo);
    obj.switchContext->target = UbseMemMigrateBlockData{
        req.targetBlock, ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult{0, 0x8000, 0x9000},
        req.targetLayout};
    obj.exportObmmInfo[b].memIdStatus = UB_MEM_SWITCHING;
    PutSwitchTestImportObj(obj);
}

void MockDecoderDelete()
{
    auto& mti = ubse::adapter_plugins::mti::UbseMtiInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(mti, &ubse::adapter_plugins::mti::UbseMtiInterface::DeleteDecoderEntry)
        .stubs()
        .will(returnValue(UBSE_OK));
}

UbseResult CreateOneDecoder(const std::pair<uint32_t, uint32_t>&, UbseMemShareBorrowImportObj& work)
{
    EXPECT_EQ(1U, work.exportObmmInfo.size());
    EXPECT_LE(work.req.trustRingData.lendSignedDatas.size(), 1U);
    work.status.decoderResult = {{0, 0x8000, 0x9000}};
    return UBSE_OK;
}

UbseResult CreateDistinctBlockDecoder(const std::pair<uint32_t, uint32_t>&, UbseMemShareBorrowImportObj& work)
{
    EXPECT_EQ(1U, work.exportObmmInfo.size());
    const auto b = work.exportObmmInfo.front().memId - 4001;
    EXPECT_LT(b, 2U);
    EXPECT_EQ(0x200000U + b * 0x1000, work.exportObmmInfo.front().desc.addr);
    EXPECT_EQ(MIG_SWITCH_DST_NODE, work.algoResult.exportNumaInfos.front().nodeId);
    EXPECT_EQ(MIG_SWITCH_NODE, work.algoResult.importNumaInfos.front().nodeId);
    EXPECT_EQ(99, work.algoResult.exportNumaInfos.front().socketId);
    EXPECT_EQ(1U, work.algoResult.attachSocketId);
    EXPECT_EQ(5U, work.exportObmmInfo.front().desc.scna);
    EXPECT_EQ(7U, work.exportObmmInfo.front().desc.dcna);
    work.status.decoderResult = {{1, 0x8000 + b * 0x1000, 0x9000 + b}};
    return UBSE_OK;
}

UbseResult CheckDistinctBlockSwitch(mmi::UbseMmiModule*, const UbseMemShmSwitchObj& obj)
{
    const auto b = obj.oldImportMemId - 3001;
    EXPECT_LT(b, 2U);
    EXPECT_EQ(MIG_SWITCH_NAME, obj.name);
    EXPECT_EQ(0x8000U + b * 0x1000, obj.newPa);
    EXPECT_EQ(5U, obj.newScna);
    EXPECT_EQ(7U, obj.newDcna);
    return UBSE_OK;
}

UbseResult CheckLocalDecoderLocation(const std::pair<uint32_t, uint32_t>& chipDie,
                                     const std::vector<UbseMemObmmInfo>& blocks,
                                     const mem::decoder::utils::ImportDecoderParam& param, UbseMemImportStatus& status)
{
    EXPECT_EQ(2U, chipDie.first);
    EXPECT_EQ(3U, chipDie.second);
    EXPECT_EQ(1U, blocks.size());
    EXPECT_EQ(4001U, blocks.front().memId);
    EXPECT_EQ(0U, param.handle);
    EXPECT_EQ(mem::decoder::utils::DecoderBorrowType::SHARE, param.type);
    status.decoderResult = {{1, 0x8000, 0x9000}};
    return UBSE_OK;
}
} // namespace

TEST_F(TestUbseMemRpc, SharedDecoderUsesSuppliedLocalDecoderLocation)
{
    auto work = BuildSwitchTestImportObj(1);
    work.algoResult.exportNumaInfos.front().socketId = 99;
    work.exportObmmInfo.front().memId = 4001;
    MOCKER(&mem::decoder::utils::MemDecoderUtils::GetChipAndDieId)
        .expects(once())
        .with(eq(99U), any())
        .will(returnValue(UBSE_OK));
    MOCKER(&IsHighSafety).stubs().will(returnValue(false));
    MOCKER(&ImportToAddDecoderEntry).expects(once()).will(invoke(&CheckLocalDecoderLocation));
    EXPECT_EQ(UBSE_OK, RealImportDecoder({2, 3}, work));
    ASSERT_EQ(1U, work.status.decoderResult.size());
    EXPECT_EQ(0x9000U, work.status.decoderResult.front().handle);
}

TEST_F(TestUbseMemRpc, MigrateDecoderMatchesEachBlockAndPeer)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutSwitchTestImportObj(BuildSwitchTestImportObj());
    MOCKER(&RealImportDecoder).expects(exactly(2)).will(invoke(&CreateDistinctBlockDecoder));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchBeginImportExecutor)
        .expects(exactly(2))
        .will(invoke(&CheckDistinctBlockSwitch));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor).expects(exactly(2)).will(returnValue(UBSE_OK));
    MockDecoderDelete();
    auto& service = UbseMemShmMigrateService::GetInstance();
    for (uint32_t b = 0; b < 2; ++b) {
        auto req = AgentBegin(b);
        req.targetBlock.desc.addr += b * 0x1000;
        req.targetBlock.desc.marId = 1;
        req.targetLayout.exportNumaInfos.front().socketId = 99;
        UbseMemMigrateOpResp result{};
        ASSERT_EQ(UBSE_OK, service.SwitchBeginImport(req, "1", result));
        ASSERT_TRUE(result.block && result.block->decoder);
        EXPECT_EQ(req.targetBlock.memId, result.block->exportInfo.memId);
        EXPECT_EQ(0x9000U + b, result.block->decoder->handle);
        ASSERT_EQ(UBSE_OK, service.SwitchEndImport({1, req.rpcInfo}, "1", result));
    }
    const auto obj = GetSwitchTestImportObj();
    for (uint32_t b = 0; b < 2; ++b) {
        EXPECT_EQ(3001U + b, obj->status.importResults[b].memId);
        EXPECT_EQ(4001U + b, obj->exportObmmInfo[b].memId);
        EXPECT_EQ(0x9000U + b, obj->status.decoderResult[b].handle);
        EXPECT_EQ(0x8000U + b * 0x1000, obj->status.decoderResult[b].hpa);
    }
    EXPECT_EQ(MIG_SWITCH_DST_NODE, obj->req.baseNodeId);
}

TEST_F(TestUbseMemRpc, MigrateBeginStoresPendingAndKeepsOldBlock)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    const auto old = BuildSwitchTestImportObj();
    PutSwitchTestImportObj(old);
    MOCKER(&RealImportDecoder).expects(once()).will(invoke(&CreateOneDecoder));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchBeginImportExecutor).expects(once()).will(returnValue(UBSE_OK));
    UbseMemMigrateOpResp response{};
    auto& service = UbseMemShmMigrateService::GetInstance();
    const auto req = AgentBegin();
    ASSERT_EQ(UBSE_OK, service.SwitchBeginImport(req, "1", response));
    ASSERT_TRUE(response.block);
    ASSERT_TRUE(response.block->decoder);
    EXPECT_FALSE(response.committed);
    EXPECT_EQ(req.targetBlock.memId, response.block->exportInfo.memId);
    EXPECT_EQ(req.targetBlock.desc.addr, response.block->exportInfo.desc.addr);
    EXPECT_EQ(req.targetBlock.desc.length, response.block->exportInfo.desc.length);
    EXPECT_EQ(0x8000, response.block->decoder->hpa);
    const auto ledger = GetSwitchTestImportObj();
    ASSERT_NE(nullptr, ledger);
    ASSERT_TRUE(ledger->switchContext);
    ASSERT_TRUE(ledger->switchContext->target);
    EXPECT_FALSE(ledger->switchContext->endDecision.has_value());
    EXPECT_EQ(req.rpcInfo.blockIndex, ledger->switchContext->blockIndex);
    EXPECT_EQ(req.targetBlock.memId, ledger->switchContext->target->exportInfo.memId);
    EXPECT_EQ(old.status.importResults[0].memId, ledger->status.importResults[0].memId);
    EXPECT_EQ(old.migrateInfos[0], ledger->migrateInfos[0]);
    EXPECT_EQ(old.exportObmmInfo[0].memId, ledger->exportObmmInfo[0].memId);
    EXPECT_EQ(old.status.decoderResult[0].handle, ledger->status.decoderResult[0].handle);
    EXPECT_EQ(UB_MEM_SWITCHING, ledger->exportObmmInfo[0].memIdStatus);
    EXPECT_EQ(old.exportObmmInfo[1].memId, ledger->exportObmmInfo[1].memId);
    response = {};
    ASSERT_EQ(UBSE_OK, service.SwitchBeginImport(req, "1", response)); // 不重复分配 decoder/begin
    ASSERT_TRUE(response.block);
    EXPECT_EQ(req.targetBlock.memId, response.block->exportInfo.memId);
    EXPECT_EQ(ledger, GetSwitchTestImportObj());
}

TEST_F(TestUbseMemRpc, MigrateBeginFailureRetainsNewDecoderForRollback)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutSwitchTestImportObj(BuildSwitchTestImportObj());
    MOCKER(&RealImportDecoder).expects(once()).will(invoke(&CreateOneDecoder));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchBeginImportExecutor).expects(once()).will(returnValue(UBSE_ERROR));
    UbseMemMigrateOpResp response{};
    auto& service = UbseMemShmMigrateService::GetInstance();
    const auto req = AgentBegin();
    EXPECT_EQ(UBSE_ERROR, service.SwitchBeginImport(req, "1", response));
    auto ledger = GetSwitchTestImportObj();
    ASSERT_TRUE(ledger->switchContext->target->decoder);
    EXPECT_EQ(0x9000, ledger->switchContext->target->decoder->handle);
    EXPECT_EQ(false, ledger->switchContext->endDecision);
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.SwitchBeginImport(req, "1", response));
    MockDecoderDelete();
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor).expects(once()).will(returnValue(UBSE_OK));
    auto info = req.rpcInfo;
    info.rollback = true;
    EXPECT_EQ(UBSE_OK, service.SwitchEndImport({0, info}, "1", response));
    ledger = GetSwitchTestImportObj();
    EXPECT_EQ(1001, ledger->exportObmmInfo[0].memId);
    EXPECT_EQ(UB_MEM_HEALTHY, ledger->exportObmmInfo[0].memIdStatus);
    EXPECT_FALSE(ledger->switchContext->target);
}

TEST_F(TestUbseMemRpc, MigrateDuplicateBeginDoesNotReallocateIncompletePendingBlock)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutPendingImport();
    ASSERT_TRUE(migrate_detail::UpdateMigrateImport(MIG_SWITCH_NODE, MIG_SWITCH_NAME, [](auto& obj) {
        obj.switchContext->target->decoder.reset();
        return true;
    }));
    MOCKER(&RealImportDecoder).expects(never());
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchBeginImportExecutor).expects(never());
    UbseMemMigrateOpResp response{};
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED,
              UbseMemShmMigrateService::GetInstance().SwitchBeginImport(AgentBegin(), "1", response));
    EXPECT_FALSE(response.block);
    const auto ledger = GetSwitchTestImportObj();
    EXPECT_EQ(1001, ledger->exportObmmInfo[0].memId);
    ASSERT_TRUE(ledger->switchContext->target);
    EXPECT_FALSE(ledger->switchContext->target->decoder);
}

TEST_F(TestUbseMemRpc, MigrateCopyRepeatsActualWorkAndUsesLocalLedger)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutPendingImport();
    MOCKER(&mmi::UbseMmiModule::UbseMemShmMigrateCopyExecutor)
        .expects(exactly(2))
        .with(eq(uint64_t(3001)), eq(uint64_t(128)), eq(false))
        .will(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    EXPECT_EQ(UBSE_OK, service.DataCopy({AgentRpc()}, "1"));
    EXPECT_EQ(UBSE_OK, service.DataCopy({AgentRpc()}, "1"));
}

TEST_F(TestUbseMemRpc, MigrateCopyRejectsWrongControllerAndMissingBegin)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutSwitchTestImportObj(BuildSwitchTestImportObj());
    MOCKER(&mmi::UbseMmiModule::UbseMemShmMigrateCopyExecutor).expects(never());
    auto& service = UbseMemShmMigrateService::GetInstance();
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.DataCopy({AgentRpc()}, "2"));
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.DataCopy({AgentRpc()}, "1"));
}

TEST_F(TestUbseMemRpc, MigrateCopyFailureClosesBlockForRollback)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutPendingImport();
    MOCKER(&mmi::UbseMmiModule::UbseMemShmMigrateCopyExecutor).expects(once()).will(returnValue(UBSE_ERROR));
    auto& service = UbseMemShmMigrateService::GetInstance();
    EXPECT_EQ(UBSE_ERROR, service.DataCopy({AgentRpc()}, "1"));
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.DataCopy({AgentRpc()}, "1"));
    const auto ledger = GetSwitchTestImportObj();
    ASSERT_NE(nullptr, ledger);
    ASSERT_TRUE(ledger->switchContext);
    EXPECT_EQ(false, ledger->switchContext->endDecision);
    EXPECT_EQ(1001U, ledger->exportObmmInfo[0].memId);
    EXPECT_EQ(3001U, ledger->status.importResults[0].memId);
    EXPECT_EQ(UB_MEM_SWITCHING, ledger->exportObmmInfo[0].memIdStatus);
    ASSERT_TRUE(ledger->switchContext->target);
    EXPECT_EQ(4001U, ledger->switchContext->target->exportInfo.memId);
}

TEST_F(TestUbseMemRpc, MigrateCommitChangesOnlyCurrentBlockAndDuplicateUsesLedger)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutPendingImport();
    MockDecoderDelete();
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor).expects(once()).will(returnValue(UBSE_OK));
    const auto old = GetSwitchTestImportObj();
    auto& service = UbseMemShmMigrateService::GetInstance();
    UbseMemMigrateOpResp result{};
    EXPECT_EQ(UBSE_OK, service.SwitchEndImport({1, AgentRpc()}, "1", result));
    EXPECT_TRUE(result.committed);
    ASSERT_TRUE(result.block);
    EXPECT_EQ(4001U, result.block->exportInfo.memId);
    EXPECT_EQ(128U, result.block->exportInfo.desc.length);
    ASSERT_TRUE(result.block->decoder);
    EXPECT_EQ(0x9000U, result.block->decoder->handle);
    auto committed = GetSwitchTestImportObj();
    ASSERT_NE(nullptr, committed);
    ASSERT_TRUE(committed->switchContext);
    EXPECT_EQ(true, committed->switchContext->endDecision);
    EXPECT_FALSE(committed->switchContext->target);
    EXPECT_EQ(UB_MEM_HEALTHY, committed->exportObmmInfo[0].memIdStatus);
    EXPECT_EQ(old->status.importResults[0].memId, committed->status.importResults[0].memId);
    EXPECT_EQ(4001, committed->exportObmmInfo[0].memId);
    EXPECT_EQ(0x9000, committed->status.decoderResult[0].handle);
    EXPECT_EQ(old->status.decoderResult[1].handle, committed->status.decoderResult[1].handle);
    EXPECT_EQ(old->exportObmmInfo[1].memId, committed->exportObmmInfo[1].memId);
    EXPECT_EQ(ShmMigrateImportInfo(AgentRpc().migrateInfo), committed->migrateInfos[0]);
    EXPECT_EQ(old->migrateInfos[1], committed->migrateInfos[1]);
    EXPECT_EQ("11", committed->req.baseNodeId);
    EXPECT_EQ(UBSE_OK, service.SwitchEndImport({1, AgentRpc()}, "1", result));
    EXPECT_TRUE(result.committed);
    ASSERT_TRUE(result.block);
    EXPECT_EQ(committed->exportObmmInfo[0].memId, result.block->exportInfo.memId);
    EXPECT_EQ(committed, GetSwitchTestImportObj()); // 重复完成请求不再重写正式账本。
}

TEST_F(TestUbseMemRpc, MigrateAbortBeforeBeginRejectsLateBeginAndCopy)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutSwitchTestImportObj(BuildSwitchTestImportObj());
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchBeginImportExecutor).expects(never());
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor).expects(never());
    auto& service = UbseMemShmMigrateService::GetInstance();
    auto info = AgentRpc();
    info.rollback = true;
    UbseMemMigrateOpResp result{};
    EXPECT_EQ(UBSE_OK, service.SwitchEndImport({0, info}, "1", result));
    EXPECT_EQ(UBSE_OK, service.SwitchEndImport({0, info}, "1", result));
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.SwitchBeginImport(AgentBegin(), "1", result));
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.DataCopy({AgentRpc()}, "1"));
}

TEST_F(TestUbseMemRpc, MigrateEndAgainDoesNotCacheFailureOrChangeDirection)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutPendingImport();
    MockDecoderDelete();
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor)
        .expects(exactly(2))
        .will(returnValue(UBSE_ERROR_AGAIN))
        .then(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    UbseMemMigrateOpResp result{};
    EXPECT_EQ(UBSE_ERROR_AGAIN, service.SwitchEndImport({1, AgentRpc()}, "1", result));
    EXPECT_EQ(UB_MEM_SWITCHING, GetSwitchTestImportObj()->exportObmmInfo[0].memIdStatus);
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.DataCopy({AgentRpc()}, "1"));
    EXPECT_EQ(UBSE_OK, service.SwitchEndImport({1, AgentRpc()}, "1", result));
    auto abort = AgentRpc();
    abort.rollback = true;
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.SwitchEndImport({0, abort}, "1", result));
}

TEST_F(TestUbseMemRpc, MigrateEndFailureDoesNotInventTerminalLedger)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutPendingImport();
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor).stubs().will(returnValue(UBSE_MMI_OBMM_OP_FAILED));
    UbseMemMigrateOpResp result{};
    EXPECT_EQ(UBSE_MMI_OBMM_OP_FAILED,
              UbseMemShmMigrateService::GetInstance().SwitchEndImport({1, AgentRpc()}, "1", result));
    EXPECT_FALSE(result.committed);
    EXPECT_EQ(UB_MEM_SWITCHING, GetSwitchTestImportObj()->exportObmmInfo[0].memIdStatus);
    EXPECT_EQ(1001, GetSwitchTestImportObj()->exportObmmInfo[0].memId);
}

TEST_F(TestUbseMemRpc, MigrateRejectedRollbackReportsOnlyMatchingCommittedBlock)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutPendingImport();
    MockDecoderDelete();
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor).expects(once()).will(returnValue(UBSE_OK));
    auto& service = UbseMemShmMigrateService::GetInstance();
    auto info = AgentRpc();
    UbseMemMigrateOpResp committed{};
    ASSERT_EQ(UBSE_OK, service.SwitchEndImport({1, info}, "1", committed));
    const auto local = GetSwitchTestImportObj();
    info.rollback = true;
    UbseMemMigrateOpResp rejected{};
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.SwitchEndImport({0, info}, "1", rejected));
    EXPECT_TRUE(rejected.committed);
    ASSERT_TRUE(rejected.block);
    EXPECT_EQ(info.exportMemId, rejected.block->exportInfo.memId);
    ASSERT_TRUE(rejected.block->decoder);
    EXPECT_EQ(local->status.decoderResult[0].handle, rejected.block->decoder->handle);
    EXPECT_EQ(local, GetSwitchTestImportObj());

    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.DataCopy({AgentRpc()}, "1"));

    // 其他迁移轮次不能借用当前块的提交事实。
    auto unrelatedInfo = info;
    unrelatedInfo.migrateInfo = MakeShmMigrateInfo(3, 0, 11, true);
    UbseMemMigrateOpResp unrelated{};
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.SwitchEndImport({0, unrelatedInfo}, "1", unrelated));
    EXPECT_FALSE(unrelated.committed);
    EXPECT_FALSE(unrelated.block);
    EXPECT_EQ(local, GetSwitchTestImportObj());

    // 提交后出现故障仍禁止回退，且不能通过重复end抹掉故障。
    ASSERT_TRUE(migrate_detail::UpdateMigrateImport(MIG_SWITCH_NODE, MIG_SWITCH_NAME, [](auto& obj) {
        obj.exportObmmInfo[0].memIdStatus = UB_MEM_READ_DATA_ERR;
        return true;
    }));
    const auto faulted = GetSwitchTestImportObj();
    UbseMemMigrateOpResp faultResult{};
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.SwitchEndImport({0, info}, "1", faultResult));
    EXPECT_TRUE(faultResult.committed);
    info.rollback = false;
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.SwitchEndImport({1, info}, "1", faultResult));
    EXPECT_EQ(faulted, GetSwitchTestImportObj());
}

TEST_F(TestUbseMemRpc, MigrateCommittedDecoderCleanupDelegatesToCycleCheck)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutPendingImport();
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor).expects(once()).will(returnValue(UBSE_OK));
    auto& mti = ubse::adapter_plugins::mti::UbseMtiInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(mti, &ubse::adapter_plugins::mti::UbseMtiInterface::DeleteDecoderEntry)
        .expects(once())
        .will(returnValue(UBSE_ERROR));
    auto& service = UbseMemShmMigrateService::GetInstance();
    UbseMemMigrateOpResp result{};
    EXPECT_EQ(UBSE_OK, service.SwitchEndImport({1, AgentRpc()}, "1", result));
    EXPECT_TRUE(result.committed);
    const auto obj = GetSwitchTestImportObj();
    EXPECT_EQ(4001, obj->exportObmmInfo[0].memId);
    EXPECT_FALSE(obj->switchContext->target);
    EXPECT_EQ(true, obj->switchContext->endDecision);
    EXPECT_EQ(UBSE_OK, obj->status.errCode);
    EXPECT_EQ(UBSE_OK, service.SwitchEndImport({1, AgentRpc()}, "1", result));
}

TEST_F(TestUbseMemRpc, MigrateLateCommittedEndDoesNotReplaceNextPendingBlock)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    PutPendingImport(1);
    migrate_detail::UpdateMigrateImport(MIG_SWITCH_NODE, MIG_SWITCH_NAME, [](auto& obj) {
        obj.exportObmmInfo[0].memId = AgentRpc().exportMemId;
        return true;
    });
    UbseMemMigrateOpResp result{};
    EXPECT_EQ(UBSE_OK, UbseMemShmMigrateService::GetInstance().SwitchEndImport({1, AgentRpc()}, "1", result));
    ASSERT_TRUE(GetSwitchTestImportObj()->switchContext);
    EXPECT_EQ(1U, GetSwitchTestImportObj()->switchContext->blockIndex);
    EXPECT_TRUE(GetSwitchTestImportObj()->switchContext->target);
    EXPECT_FALSE(GetSwitchTestImportObj()->switchContext->endDecision);
}

TEST_F(TestUbseMemRpc, MigrateResultMergeUpdatesOnlyReportingNodeAndCurrentIndex)
{
    SwitchTestMockSet();
    auto old = BuildSwitchTestImportObj();
    PutSwitchTestImportObj(old);
    old.importNodeId = "15";
    PutSwitchTestImportObj(old);
    const auto req = AgentBegin();
    UbseMemMigrateOpResp result{};
    result.rpcInfo = AgentRpc();
    result.block = UbseMemMigrateBlockData{req.targetBlock,
                                           ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult{0, 0x8000, 0x9000},
                                           req.targetLayout};
    EXPECT_TRUE(migrate_detail::MergeMigrateImportResult(result, true));
    EXPECT_EQ(1001, GetSwitchTestImportObj()->exportObmmInfo[0].memId);
    result.committed = true;
    auto incomplete = result;
    incomplete.rpcInfo.rollback = true;
    incomplete.block->decoder.reset();
    EXPECT_FALSE(migrate_detail::MergeMigrateImportResult(incomplete, false));
    EXPECT_EQ(1001, GetSwitchTestImportObj()->exportObmmInfo[0].memId);
    EXPECT_TRUE(migrate_detail::MergeMigrateImportResult(result, false));
    EXPECT_EQ(4001, GetSwitchTestImportObj()->exportObmmInfo[0].memId);
    EXPECT_EQ(1002, GetSwitchTestImportObj()->exportObmmInfo[1].memId);
    const auto other =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource("15", MIG_SWITCH_NAME);
    EXPECT_EQ(1001, other->exportObmmInfo[0].memId);
    EXPECT_FALSE(migrate_detail::UpdateMigrateImport(MIG_SWITCH_NODE, MIG_SWITCH_NAME, [](auto&) { return false; }));
}

TEST_F(TestUbseMemRpc, MigrateContextAndSingleBlockSerializeRoundtrip)
{
    SwitchTestMockSet();
    PutPendingImport();
    auto original = *GetSwitchTestImportObj();
    original.switchContext->endDecision = false;
    ubse::simpo::util::UbseAutoSimpo<UbseMemShareBorrowImportObj> message;
    message.SetUbseMesgInfo(original);
    ASSERT_EQ(UBSE_OK, message.Serialize());
    message.SetInputRawData(message.SerializedData(), message.SerializedDataSize());
    ASSERT_EQ(UBSE_OK, message.Deserialize());
    const auto& result = message.GetUbseMesgInfo();
    ASSERT_TRUE(result.switchContext);
    EXPECT_EQ(false, result.switchContext->endDecision);
    EXPECT_EQ(0x9000, result.switchContext->target->decoder->handle);
    original.switchContext.reset();
    message.SetUbseMesgInfo(original);
    ASSERT_EQ(UBSE_OK, message.Serialize());
    message.SetInputRawData(message.SerializedData(), message.SerializedDataSize());
    ASSERT_EQ(UBSE_OK, message.Deserialize());
    EXPECT_FALSE(message.GetUbseMesgInfo().switchContext);
}

namespace {
UbseResult MappedBeginBlock(mmi::UbseMmiModule*, const UbseMemShmSwitchExportObj& device, ubse_mem_obmm_mem_desc& desc)
{
    EXPECT_EQ(1U, device.exportObj.algoResult.exportNumaInfos.size());
    EXPECT_EQ(256U, device.exportObj.algoResult.exportNumaInfos.front().size);
    desc.addr = 0x200000 + device.blockIndex * 128;
    desc.length = device.numaSizes.front();
    desc.tokenid = 17;
    return UBSE_OK;
}

UbseMemShareBorrowExportObj BuildMappedSwitchTarget(const UbseMemShareBorrowImportObj& old)
{
    UbseMemShareBorrowExportObj target{};
    target.req.name = MIG_SWITCH_NAME;
    target.algoResult.exportNumaInfos = {{MIG_SWITCH_NODE, 0, 0, 256}};
    target.status.state = UBSE_MEM_EXPORT_RUNNING;
    for (uint32_t index = 0; index < old.status.importResults.size(); ++index) {
        target.status.exportObmmInfo.push_back({old.status.importResults[index].memId, {}, UB_MEM_HEALTHY});
        target.migrateInfos.push_back(MakeShmMigrateInfo(1, index, 11, true));
    }
    return target;
}

void ExpectMappedSwitchStatus(std::optional<uint32_t> currentBlock)
{
    const auto target = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(
        MIG_SWITCH_NODE, MIG_SWITCH_NAME);
    ASSERT_NE(nullptr, target);
    const auto local = GetSwitchTestImportObj();
    ASSERT_NE(nullptr, local);
    ASSERT_LE(target->status.exportObmmInfo.size(), local->exportObmmInfo.size());
    for (uint32_t index = 0; index < target->status.exportObmmInfo.size(); ++index) {
        const auto expected = currentBlock == index ? UB_MEM_SWITCHING : UB_MEM_HEALTHY;
        EXPECT_EQ(expected, target->status.exportObmmInfo[index].memIdStatus) << "blockIndex=" << index;
        EXPECT_EQ(expected, local->exportObmmInfo[index].memIdStatus) << "blockIndex=" << index;
    }
}
} // namespace

TEST_F(TestUbseMemRpc, MigrateBPreservesTemplateAndConvertsLastBlockWithoutFinalize)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    MockDecoderDelete();
    const auto old = BuildSwitchTestImportObj();
    PutSwitchTestImportObj(old);
    MOCKER(&nodeController::UbseNodeController::GetLocalEidBySocket).stubs().will(returnValue(UBSE_OK));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchBeginExportExecutor)
        .expects(exactly(2))
        .will(invoke(&MappedBeginBlock));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor).expects(exactly(2)).will(returnValue(UBSE_OK));
    const auto target = BuildMappedSwitchTarget(old);
    auto& service = UbseMemShmMigrateService::GetInstance();
    for (uint32_t b = 0; b < 2; ++b) {
        auto info = AgentRpc(b);
        info.exportMemId = old.status.importResults[b].memId;
        UbseMemMigrateBeginExportReq req{target, info};
        UbseMemMigrateBeginExportResp begin{};
        ASSERT_EQ(UBSE_OK, service.SwitchBeginExport(req, "1", begin));
        EXPECT_EQ(info.exportMemId, begin.block.memId);
        EXPECT_EQ(UBSE_OK, service.SwitchBeginExport(req, "1", begin));
        ExpectMappedSwitchStatus(b);
        UbseMemMigrateOpResp end{};
        ASSERT_EQ(UBSE_OK, service.SwitchEndExport({1, info}, "1", end));
        EXPECT_TRUE(end.committed);
        ASSERT_TRUE(end.block);
        EXPECT_EQ(info.exportMemId, end.block->exportInfo.memId);
        EXPECT_EQ(begin.block.desc.addr, end.block->exportInfo.desc.addr);
        EXPECT_EQ(begin.block.desc.length, end.block->exportInfo.desc.length);
        const auto committedImport = GetSwitchTestImportObj();
        const auto committedExport =
            UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(MIG_SWITCH_NODE,
                                                                                                   MIG_SWITCH_NAME);
        EXPECT_EQ(UBSE_OK, service.SwitchEndExport({1, info}, "1", end));
        ASSERT_TRUE(end.block);
        EXPECT_EQ(info.exportMemId, end.block->exportInfo.memId);
        EXPECT_EQ(committedImport, GetSwitchTestImportObj());
        EXPECT_EQ(committedExport,
                  UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(
                      MIG_SWITCH_NODE, MIG_SWITCH_NAME));
        auto abort = info;
        abort.rollback = true;
        EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, service.SwitchEndExport({0, abort}, "1", end));
        const auto local = GetSwitchTestImportObj();
        EXPECT_EQ(info.exportMemId, local->exportObmmInfo[b].memId);
        EXPECT_EQ(ShmMigrateImportInfo(info.migrateInfo), local->migrateInfos[b]);
        EXPECT_EQ(old.status.importResults[b].memId, local->status.importResults[b].memId);
        EXPECT_EQ(b == 0, local->realExe);
        ExpectMappedSwitchStatus(std::nullopt);
    }
    const auto exp = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(
        MIG_SWITCH_NODE, MIG_SWITCH_NAME);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, exp->status.state);
    EXPECT_EQ(MIG_SWITCH_NODE, GetSwitchTestImportObj()->req.baseNodeId);
    EXPECT_EQ(256U, exp->algoResult.exportNumaInfos.front().size);
}

namespace {
void CheckMappedRollback(uint32_t committedBlocks)
{
    SwitchTestMockSet();
    MigrateControllerMockSet();
    MockDecoderDelete();
    const auto old = BuildSwitchTestImportObj();
    PutSwitchTestImportObj(old);
    MOCKER(&nodeController::UbseNodeController::GetLocalEidBySocket).stubs().will(returnValue(UBSE_OK));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchBeginExportExecutor)
        .expects(exactly(committedBlocks + 1))
        .will(invoke(&MappedBeginBlock));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmSwitchEndExecutor)
        .expects(exactly(committedBlocks + 1))
        .will(returnValue(UBSE_OK));
    const auto target = BuildMappedSwitchTarget(old);
    auto& service = UbseMemShmMigrateService::GetInstance();
    for (uint32_t index = 0; index <= committedBlocks; ++index) {
        auto info = AgentRpc(index);
        info.exportMemId = old.status.importResults[index].memId;
        UbseMemMigrateBeginExportResp begin{};
        ASSERT_EQ(UBSE_OK, service.SwitchBeginExport({target, info}, "1", begin));
        ExpectMappedSwitchStatus(index);
        info.rollback = index == committedBlocks;
        UbseMemMigrateOpResp end{};
        ASSERT_EQ(UBSE_OK, service.SwitchEndExport({info.rollback ? 0U : 1U, info}, "1", end));
        EXPECT_EQ(!info.rollback, end.committed);
    }
    const auto local = GetSwitchTestImportObj();
    ASSERT_NE(nullptr, local);
    EXPECT_TRUE(local->realExe);
    EXPECT_EQ("11", local->req.baseNodeId);
    for (uint32_t index = 0; index < local->exportObmmInfo.size(); ++index) {
        EXPECT_EQ(UB_MEM_HEALTHY, local->exportObmmInfo[index].memIdStatus);
        EXPECT_EQ(index < committedBlocks ? old.status.importResults[index].memId : old.exportObmmInfo[index].memId,
                  local->exportObmmInfo[index].memId);
        EXPECT_EQ(index < committedBlocks ? ShmMigrateImportInfo(target.migrateInfos[index]) : old.migrateInfos[index],
                  local->migrateInfos[index]);
    }
    const auto exp = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(
        MIG_SWITCH_NODE, MIG_SWITCH_NAME);
    if (committedBlocks == 0) {
        EXPECT_EQ(nullptr, exp);
        return;
    }
    ASSERT_NE(nullptr, exp);
    ExpectMappedSwitchStatus(std::nullopt);
    EXPECT_NE(0U, exp->status.exportObmmInfo.front().desc.addr);
    EXPECT_EQ(old.status.importResults.front().memId, exp->status.exportObmmInfo.front().memId);
    EXPECT_EQ(0U, exp->status.exportObmmInfo[committedBlocks].desc.addr);
    EXPECT_EQ(0U, exp->status.exportObmmInfo[committedBlocks].desc.length);
}
} // namespace

TEST_F(TestUbseMemRpc, MigrateBFirstRollbackRemovesTemplateWithHealthyFutureBlocks)
{
    CheckMappedRollback(0);
}

TEST_F(TestUbseMemRpc, MigrateBLaterRollbackPreservesCommittedBlocksAndClearsCurrentStatus)
{
    CheckMappedRollback(1);
}

TEST_F(TestUbseMemRpc, UbseMemShmMigrateGetHandler_GetCodes)
{
    UbseMemShmMigrateGetHandler handler{};
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_SHM_MIGRATE_GET_REQ));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemRpc, UbseMemShmMigrateGetMessage_SerializeRoundtrip)
{
    UbseMemShmMigrateGetReq reqData{};
    reqData.name = "migrate_get_test_mem";
    reqData.caller = {1001, 1001, 1, "creator"};
    reqData.reserved = 0;
    UbseMemShmMigrateGetReqMessage reqMessage(reqData);
    EXPECT_EQ(reqMessage.Serialize(), UBSE_OK);
    reqMessage.SetInputRawData(reqMessage.SerializedData(), reqMessage.SerializedDataSize());
    EXPECT_EQ(reqMessage.Deserialize(), UBSE_OK);
    EXPECT_EQ(reqMessage.getReq.name, "migrate_get_test_mem");
    EXPECT_EQ(1001, reqMessage.getReq.caller.uid);
    EXPECT_EQ("creator", reqMessage.getReq.caller.username);

    UbseMemShmMigrateGetResp respData{};
    respData.errCode = 0;
    respData.name = "migrate_get_test_mem";
    respData.srcNodeId = "3";
    respData.dstNodeId = "4";
    respData.state = static_cast<uint32_t>(UBS_MEM_MIGRATE_STATE_DATA_COPYING);
    respData.totalMemids = 4;
    respData.migratedMemids = 2;
    respData.totalSize = 8192;
    respData.failReason = MIGRATE_FAIL_COPY;
    respData.errMsg = "data copy failed after retries";
    UbseMemShmMigrateGetRespMessage rspMessage(respData);
    EXPECT_EQ(rspMessage.Serialize(), UBSE_OK);
    rspMessage.SetInputRawData(rspMessage.SerializedData(), rspMessage.SerializedDataSize());
    EXPECT_EQ(rspMessage.Deserialize(), UBSE_OK);
    EXPECT_EQ(rspMessage.getResp.name, "migrate_get_test_mem");
    EXPECT_EQ(rspMessage.getResp.srcNodeId, "3");
    EXPECT_EQ(rspMessage.getResp.dstNodeId, "4");
    EXPECT_EQ(rspMessage.getResp.state, static_cast<uint32_t>(UBS_MEM_MIGRATE_STATE_DATA_COPYING));
    EXPECT_EQ(rspMessage.getResp.totalMemids, 4U);
    EXPECT_EQ(rspMessage.getResp.migratedMemids, 2U);
    EXPECT_EQ(rspMessage.getResp.totalSize, 8192ULL);
    EXPECT_EQ(rspMessage.getResp.failReason, static_cast<uint32_t>(MIGRATE_FAIL_COPY));
    EXPECT_EQ(rspMessage.getResp.errMsg, "data copy failed after retries");
}

/*
 * 用例描述：migrate_get handler 无会话(无活跃/无终态留存)
 * 预期结果：Handle返回UBSE_OK(框架应答口径), rsp载荷errCode=UBSE_ERR_NOT_EXIST
 */
TEST_F(TestUbseMemRpc, UbseMemShmMigrateGetHandler_NotExist)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    UbseMemShmMigrateGetHandler handler{};
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShmMigrateGetReqMessage(UbseMemShmMigrateGetReq{});
    UbseBaseMessage::DeConvert<UbseMemShmMigrateGetReqMessage>(req)->getReq.name = "no_such_mem";
    UbseBaseMessage::DeConvert<UbseMemShmMigrateGetReqMessage>(req)->getReq.caller = {0, 0, 0, "creator"};
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemShmMigrateGetRespMessage();
    UbseComBaseMessageHandlerCtxPtr ctx{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
    auto resp = UbseBaseMessage::DeConvert<UbseMemShmMigrateGetRespMessage>(rsp);
    EXPECT_EQ(resp->getResp.errCode, static_cast<uint32_t>(UBSE_ERR_NOT_EXIST));
}

/*
 * 用例描述：migrate_get handler 活跃会话命中, 进度字段按会话派生
 * 预期结果：errCode=0; 槽位/state/块计数与 session 一致; FAILED 才有 errMsg
 */
TEST_F(TestUbseMemRpc, UbseMemShmMigrateGetHandler_ActiveSession)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    const std::string name = "migrate_get_active_mem";
    auto [session, isNew] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        name, "3", "4", 2, UbseMemMigrateTargetKind::NOT_IMPORT_NODE, 3072);
    ASSERT_TRUE(isNew);
    auto& copyStartManager = UbseMemShmMigrateSessionManager::GetInstance();
    copyStartManager.UpdateSessionState(name, UBS_MEM_MIGRATE_STATE_EXPORTING);
    EXPECT_EQ(UbseMemShmMigrateSessionManager::CopyStartResult::STARTED,
              copyStartManager.TryStartCopy(name, copyStartManager.GetSession(name)));
    ASSERT_TRUE(copyStartManager.MarkBlockCompleted(name, session, 0));

    UbseMemShmMigrateGetHandler handler{};
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemShmMigrateGetReqMessage(UbseMemShmMigrateGetReq{});
    UbseBaseMessage::DeConvert<UbseMemShmMigrateGetReqMessage>(req)->getReq.name = name;
    UbseBaseMessage::DeConvert<UbseMemShmMigrateGetReqMessage>(req)->getReq.caller = {0, 0, 0, "creator"};
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemShmMigrateGetRespMessage();
    UbseComBaseMessageHandlerCtxPtr ctx{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
    auto resp = UbseBaseMessage::DeConvert<UbseMemShmMigrateGetRespMessage>(rsp);
    EXPECT_EQ(resp->getResp.errCode, 0U);
    EXPECT_EQ(resp->getResp.name, name);
    EXPECT_EQ(resp->getResp.srcNodeId, "3");
    EXPECT_EQ(resp->getResp.dstNodeId, "4");
    EXPECT_EQ(resp->getResp.state, static_cast<uint32_t>(UBS_MEM_MIGRATE_STATE_DATA_COPYING));
    EXPECT_EQ(resp->getResp.totalMemids, 2U);    // 受理时确定总块数
    EXPECT_EQ(resp->getResp.migratedMemids, 1U); // 串行完成计数
    EXPECT_EQ(resp->getResp.totalSize, 3072ULL); // 受理时记录的总大小
    EXPECT_EQ(resp->getResp.failReason, 0U);
    EXPECT_TRUE(resp->getResp.errMsg.empty());
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
}

// ==== migrate_cancel 取消 handler(0x001A)语义 UT ====

TEST_F(TestUbseMemRpc, UbseMemShmMigrateCancelHandler_GetCodes)
{
    UbseMemShmMigrateCancelHandler handler{};
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_SHM_MIGRATE_CANCEL_REQ));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemRpc, UbseMemMigrateCancelMessage_SerializeRoundtrip)
{
    UbseMemMigrateCancelReq reqData{};
    reqData.name = "migrate_cancel_test_mem";
    reqData.reserved = 0;
    UbseMemMigrateCancelReqMessage reqMessage(reqData);
    EXPECT_EQ(reqMessage.Serialize(), UBSE_OK);
    reqMessage.SetInputRawData(reqMessage.SerializedData(), reqMessage.SerializedDataSize());
    EXPECT_EQ(reqMessage.Deserialize(), UBSE_OK);
    EXPECT_EQ(reqMessage.cancelReq.name, "migrate_cancel_test_mem");

    UbseMemMigrateCancelResp respData{};
    respData.errCode = UBSE_ERR_NOT_EXIST;
    respData.reserved = 0;
    UbseMemMigrateCancelRespMessage rspMessage(respData);
    EXPECT_EQ(rspMessage.Serialize(), UBSE_OK);
    rspMessage.SetInputRawData(rspMessage.SerializedData(), rspMessage.SerializedDataSize());
    EXPECT_EQ(rspMessage.Deserialize(), UBSE_OK);
    EXPECT_EQ(rspMessage.cancelResp.errCode, static_cast<uint32_t>(UBSE_ERR_NOT_EXIST));
}

/*
 * 用例描述：migrate_cancel handler 无会话(无活跃/无终态留存)
 * 预期结果：Handle返回UBSE_OK(框架应答口径), rsp载荷errCode=UBSE_ERR_NOT_EXIST
 */
TEST_F(TestUbseMemRpc, UbseMemShmMigrateCancelHandler_NotExist)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    UbseMemShmMigrateCancelHandler handler{};
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemMigrateCancelReqMessage(UbseMemMigrateCancelReq{});
    UbseBaseMessage::DeConvert<UbseMemMigrateCancelReqMessage>(req)->cancelReq.name = "no_such_mem";
    UbseBaseMessage::DeConvert<UbseMemMigrateCancelReqMessage>(req)->cancelReq.caller = {0, 0, 0, "creator"};
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemMigrateCancelRespMessage();
    UbseComBaseMessageHandlerCtxPtr ctx{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
    auto resp = UbseBaseMessage::DeConvert<UbseMemMigrateCancelRespMessage>(rsp);
    EXPECT_EQ(resp->cancelResp.errCode, static_cast<uint32_t>(UBSE_ERR_NOT_EXIST));
}

/*
 * 用例描述：migrate_cancel handler 取消受理(导出前取消检查点 前置) + 幂等 + 终态拒绝
 * 预期结果：INIT 会话受理成功置取消标志 且置 cancelRequested; 重复取消幂等返 0;
 * 进入逐块迁移 后再取消返 UBSE_ERR_MIGRATE_NOT_ALLOWED
 */
TEST_F(TestUbseMemRpc, UbseMemShmMigrateCancelHandler_AcceptAndIdempotent)
{
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    const std::string name = "migrate_cancel_active_mem";
    auto [session, isNew] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(name, "3", "4", 4);
    ASSERT_TRUE(isNew);
    (void)session; // 会话以 name 寻址, 快照指针不参与断言

    UbseMemShmMigrateCancelHandler handler{};
    const UbseBaseMessagePtr req = new (std::nothrow) UbseMemMigrateCancelReqMessage(UbseMemMigrateCancelReq{});
    UbseBaseMessage::DeConvert<UbseMemMigrateCancelReqMessage>(req)->cancelReq.name = name;
    UbseBaseMessage::DeConvert<UbseMemMigrateCancelReqMessage>(req)->cancelReq.caller = {0, 0, 0, "creator"};
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseMemMigrateCancelRespMessage();
    UbseComBaseMessageHandlerCtxPtr ctx{};
    EXPECT_EQ(handler.Handle(req, rsp, ctx), UBSE_OK);
    auto resp = UbseBaseMessage::DeConvert<UbseMemMigrateCancelRespMessage>(rsp);
    EXPECT_EQ(resp->cancelResp.errCode, 0U);
    auto accepted = UbseMemShmMigrateSessionManager::GetInstance().GetSession(name);
    ASSERT_NE(accepted, nullptr);
    EXPECT_TRUE(accepted->cancelRequested);
    EXPECT_TRUE(accepted->cancelRequested);

    const UbseBaseMessagePtr rsp2 = new (std::nothrow) UbseMemMigrateCancelRespMessage();
    EXPECT_EQ(handler.Handle(req, rsp2, ctx), UBSE_OK);
    auto resp2 = UbseBaseMessage::DeConvert<UbseMemMigrateCancelRespMessage>(rsp2);
    EXPECT_EQ(resp2->cancelResp.errCode, 0U);

    // 进入DATA_COPYING后的取消拒绝独立验证；已受理取消不会被Consume强行推进。
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
    UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(name, "3", "4", 4);
    auto& copyStartManager = UbseMemShmMigrateSessionManager::GetInstance();
    copyStartManager.UpdateSessionState(name, UBS_MEM_MIGRATE_STATE_EXPORTING);
    ASSERT_EQ(UbseMemShmMigrateSessionManager::CopyStartResult::STARTED,
              copyStartManager.TryStartCopy(name, copyStartManager.GetSession(name)));
    const UbseBaseMessagePtr rsp3 = new (std::nothrow) UbseMemMigrateCancelRespMessage();
    EXPECT_EQ(handler.Handle(req, rsp3, ctx), UBSE_OK);
    auto resp3 = UbseBaseMessage::DeConvert<UbseMemMigrateCancelRespMessage>(rsp3);
    EXPECT_EQ(resp3->cancelResp.errCode, static_cast<uint32_t>(UBSE_ERR_MIGRATE_NOT_ALLOWED));
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessions();
}
namespace {
std::vector<std::function<void()>> g_migrateHandlerWork;
bool g_migrateResultReported = false;
bool g_migrateResultApplied = false;

bool HoldMigrateHandlerWork(UbseTaskExecutor*, const std::function<void()>& work)
{
    g_migrateHandlerWork.push_back(work);
    return true;
}

void MigrateHandlerExecutorMockSet()
{
    g_migrateHandlerWork.clear();
    auto executor = UbseTaskExecutor::Create("migrateHandlerUT", 1, 16);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(executor));
    bool (UbseTaskExecutor::*execute)(const std::function<void()>&) = &UbseTaskExecutor::Execute;
    MOCKER(execute).stubs().will(invoke(&HoldMigrateHandlerWork));
}

UbseMemMigrateRpcInfo HandlerTestRpcInfo()
{
    UbseMemMigrateRpcInfo info{};
    info.name = "migrate-handler-test";
    info.srcNodeId = "13";
    return info;
}

UbseResult FakeNewExportOperation(UbseMemShmMigrateService*, const UbseMemShareBorrowExportObj& req,
                                  UbseMemShareBorrowExportObj& result)
{
    result = req;
    return UBSE_OK;
}

UbseResult CaptureNewExportReport(UbseComModule*, const SendParam& param, UbseMemShareBorrowExportobjSimpoPtr& request,
                                  UbseBaseMessagePtr&, const bool)
{
    EXPECT_EQ("1", param.GetRemoteId());
    EXPECT_EQ(static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_NEW_EXPORT_RESULT),
              param.GetOpCode());
    const auto& result = request->GetUbseMesgInfo();
    EXPECT_EQ(g_switchRequestId, result.req.requestId);
    g_migrateResultReported = true;
    return UBSE_OK;
}

void CaptureNewExportResultApply(UbseMemShmMigrateService*, const UbseMemShareBorrowExportObj& result)
{
    EXPECT_EQ(MIG_SWITCH_NAME, result.req.name);
    g_migrateResultApplied = true;
}

} // namespace

TEST_F(TestUbseMemRpc, UbseMemMigrateNewExportHandler_QueueThenReport)
{
    SwitchTestMockSet();
    MigrateHandlerExecutorMockSet();
    g_migrateResultReported = false;
    MOCKER_CPP(&UbseMemShmMigrateService::Export).stubs().will(invoke(&FakeNewExportOperation));
    MOCKER(&ubse::election::UbseGetMasterNodeId).stubs().with(outBound(std::string("1"))).will(returnValue(UBSE_OK));
    MOCKER(&GetWaitTimeOut).stubs().will(returnValue(3U));
    MOCKER(&context::UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseComModule>()));
    const auto send = &UbseComModule::RpcSend<UbseMemShareBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(send).stubs().will(invoke(&CaptureNewExportReport));
    UbseMemShareBorrowExportObj data{};
    data.req.name = MIG_SWITCH_NAME;
    data.req.requestNodeId = "1";
    data.req.requestId = g_switchRequestId;
    data.algoResult.exportNumaInfos.push_back({MIG_SWITCH_NODE, 0, -1, 0, 0, 0});
    // 正常导出不需要阶段epoch或节点结果下标。
    UbseBaseMessagePtr req = new UbseMemShareBorrowExportobjSimpo();
    UbseBaseMessage::DeConvert<UbseMemShareBorrowExportobjSimpo>(req)->SetUbseMesgInfo(data);
    UbseBaseMessagePtr ack = new UbseMemCallbackMessage();
    UbseMemMigrateExportHandler handler;
    EXPECT_EQ(UBSE_OK, handler.Handle(req, ack, {}));
    EXPECT_EQ("sync_success", UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(ack)->data);
    ASSERT_EQ(1U, g_migrateHandlerWork.size());
    EXPECT_FALSE(g_migrateResultReported); // 入队ACK不等待业务执行。
    auto work = std::move(g_migrateHandlerWork.front());
    g_migrateHandlerWork.clear();
    work();
    EXPECT_TRUE(g_migrateResultReported);
}

TEST_F(TestUbseMemRpc, UbseMemMigrateNewExportResultHandler_QueueBeforeApply)
{
    SwitchTestMockSet();
    MigrateHandlerExecutorMockSet();
    g_migrateResultApplied = false;
    MOCKER_CPP(&UbseMemShmMigrateService::NewExportResult).stubs().will(invoke(&CaptureNewExportResultApply));
    UbseMemShareBorrowExportObj data{};
    data.req.name = MIG_SWITCH_NAME;
    data.req.requestNodeId = "1";
    data.req.requestId = g_switchRequestId;
    data.algoResult.exportNumaInfos.push_back({MIG_SWITCH_NODE, 0, -1, 0, 0, 0});
    UbseBaseMessagePtr req = new UbseMemShareBorrowExportobjSimpo();
    UbseBaseMessage::DeConvert<UbseMemShareBorrowExportobjSimpo>(req)->SetUbseMesgInfo(data);
    UbseBaseMessagePtr ack = new UbseMemCallbackMessage();
    UbseMemMigrateNewExportResultHandler handler;
    EXPECT_EQ(UBSE_OK, handler.Handle(req, ack, {}));
    EXPECT_EQ("sync_success", UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(ack)->data);
    ASSERT_EQ(1U, g_migrateHandlerWork.size());
    EXPECT_FALSE(g_migrateResultApplied);
    auto work = std::move(g_migrateHandlerWork.front());
    g_migrateHandlerWork.clear();
    work();
    EXPECT_TRUE(g_migrateResultApplied);
}

TEST_F(TestUbseMemRpc, UbseMemMigrateSwitchBeginImportHandler_QueueAck)
{
    SwitchTestMockSet();
    MigrateHandlerExecutorMockSet();
    UbseMemMigrateSwitchBeginImportReq data{};
    data.rpcInfo = HandlerTestRpcInfo();
    data.targetBlock = AgentBegin().targetBlock;
    data.oldExportMemId = MIG_SWITCH_OLD_IMPORT_MEMID;
    UbseBaseMessagePtr req = new UbseMemMigrateSwitchBeginImportReqSimpo();
    UbseBaseMessage::DeConvert<UbseMemMigrateSwitchBeginImportReqSimpo>(req)->SetUbseMesgInfo(data);
    UbseBaseMessagePtr ack = new UbseMemCallbackMessage();
    UbseMemMigrateSwitchBeginImportHandler handler;
    UbseComBaseMessageHandlerCtx peer{"", 0, 0, "1"};
    EXPECT_EQ(UBSE_OK, handler.Handle(req, ack, &peer));
    EXPECT_EQ("sync_success", UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(ack)->data);
    ASSERT_EQ(1U, g_migrateHandlerWork.size());
    g_migrateHandlerWork.clear();
}

TEST_F(TestUbseMemRpc, UbseMemMigrateSwitchBeginImportHandler_RejectEnqueue)
{
    SwitchTestMockSet();
    auto executor = UbseTaskExecutor::Create("migrateQueueFailureUT", 1, 16);
    MOCKER_CPP(&GetExecutor).stubs().will(returnValue(executor));
    bool (UbseTaskExecutor::*execute)(const std::function<void()>&) = &UbseTaskExecutor::Execute;
    MOCKER(execute).stubs().will(returnValue(false));
    UbseMemMigrateSwitchBeginImportReq data{};
    data.rpcInfo = HandlerTestRpcInfo();
    data.targetBlock = AgentBegin().targetBlock;
    data.oldExportMemId = MIG_SWITCH_OLD_IMPORT_MEMID;
    UbseBaseMessagePtr req = new UbseMemMigrateSwitchBeginImportReqSimpo();
    UbseBaseMessage::DeConvert<UbseMemMigrateSwitchBeginImportReqSimpo>(req)->SetUbseMesgInfo(data);
    UbseBaseMessagePtr ack = new UbseMemCallbackMessage();
    UbseMemMigrateSwitchBeginImportHandler handler;
    UbseComBaseMessageHandlerCtx peer{"", 0, 0, "1"};
    EXPECT_EQ(UBSE_ERROR, handler.Handle(req, ack, &peer));
    EXPECT_TRUE(UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(ack)->data.empty());
}

namespace {
uint32_t g_agentMasterQueries = 0;
uint32_t g_agentReportSends = 0;
uint32_t g_agentRetrySleeps = 0;
bool g_agentLookupFails = false;
bool g_agentFirstLookupFails = false;
bool g_agentSendFails = false;
bool g_agentFirstSendFails = false;
std::vector<std::string> g_agentReportTargets;

uint32_t RetryTestMasterLookup(std::string& masterSlotId)
{
    ++g_agentMasterQueries;
    if (g_agentLookupFails || (g_agentFirstLookupFails && g_agentMasterQueries == 1)) {
        return UBSE_ERR_NOT_EXIST;
    }
    masterSlotId = std::to_string(10 + g_agentMasterQueries);
    return UBSE_OK;
}

unsigned int RetryTestAgentSleep(unsigned int seconds)
{
    EXPECT_EQ(SEND_RETRY_DURATION, seconds);
    ++g_agentRetrySleeps;
    return 0;
}

UbseResult RetryTestReportSend(UbseComModule*, const SendParam& param, UbseMemShareBorrowExportobjSimpoPtr& request,
                               UbseBaseMessagePtr&, const bool)
{
    ++g_agentReportSends;
    g_agentReportTargets.push_back(param.GetRemoteId());
    EXPECT_EQ(g_switchRequestId, request->GetUbseMesgInfo().req.requestId);
    return g_agentSendFails || (g_agentFirstSendFails && g_agentReportSends == 1) ? UBSE_ERROR : UBSE_OK;
}

UbseResult RetryTestBlockReportSend(UbseComModule*, const SendParam& param, UbseMemMigrateOpRespSimpoPtr& request,
                                    UbseBaseMessagePtr&, const bool)
{
    ++g_agentReportSends;
    g_agentReportTargets.push_back(param.GetRemoteId());
    EXPECT_EQ(UBSE_OK, request->GetUbseMesgInfo().errCode);
    EXPECT_EQ(MIG_SWITCH_NAME, request->GetUbseMesgInfo().rpcInfo.name);
    EXPECT_EQ(static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_COPY_RESULT), param.GetOpCode());
    return g_agentReportSends == 1 ? UBSE_ERROR : UBSE_OK;
}

void AgentRetryMockSet()
{
    MOCKER_CPP(&UbseMemShmMigrateService::Export).stubs().will(invoke(&FakeNewExportOperation));
    g_agentMasterQueries = 0;
    g_agentReportSends = 0;
    g_agentRetrySleeps = 0;
    g_agentLookupFails = false;
    g_agentFirstLookupFails = false;
    g_agentSendFails = false;
    g_agentFirstSendFails = false;
    g_agentReportTargets.clear();
    MOCKER(&context::UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseComModule>()));
    MOCKER(&ubse::election::UbseGetMasterNodeId).stubs().will(invoke(&RetryTestMasterLookup));
    MOCKER(&GetWaitTimeOut).stubs().will(returnValue(3U * SEND_RETRY_DURATION));
    MOCKER(&::sleep).stubs().will(invoke(&RetryTestAgentSleep));
    const auto send = &UbseComModule::RpcSend<UbseMemShareBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(send).stubs().will(invoke(&RetryTestReportSend));
}

UbseMemShareBorrowExportObj AgentRetryRequest()
{
    UbseMemShareBorrowExportObj req{};
    req.req.name = MIG_SWITCH_NAME;
    req.req.requestId = g_switchRequestId;
    req.algoResult.exportNumaInfos.push_back({MIG_SWITCH_NODE, 0, -1, 0, 0, 0});
    return req;
}
} // namespace

TEST_F(TestUbseMemRpc, MigrateBlockResultRetryStaysWithRequestController)
{
    SwitchTestMockSet();
    PutPendingImport();
    g_agentReportSends = 0;
    g_agentReportTargets.clear();
    MOCKER(&ubse::election::UbseGetMasterNodeId)
        .expects(once())
        .with(outBound(std::string("1")))
        .will(returnValue(UBSE_OK));
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmMigrateCopyExecutor).expects(once()).will(returnValue(UBSE_OK));
    MOCKER(&context::UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseComModule>()));
    MOCKER(&GetWaitTimeOut).stubs().will(returnValue(3U * SEND_RETRY_DURATION));
    MOCKER(&::sleep).expects(once()).will(returnValue(0U));
    const auto send = &UbseComModule::RpcSend<UbseMemMigrateOpRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(send).stubs().will(invoke(&RetryTestBlockReportSend));
    UbseMemShmMigrateService::GetInstance().ProcessCopyRequest({AgentRpc()}, "1");
    EXPECT_EQ((std::vector<std::string>{"1", "1"}), g_agentReportTargets);
}

TEST_F(TestUbseMemRpc, UbseMemMigrateAgentReportRefreshesMasterOnEachRetry)
{
    SwitchTestMockSet();
    AgentRetryMockSet();
    g_agentFirstLookupFails = true;
    g_agentFirstSendFails = true;
    UbseMemShmMigrateService::GetInstance().ProcessExportRequest(AgentRetryRequest());
    EXPECT_EQ(3U, g_agentMasterQueries);
    EXPECT_EQ(2U, g_agentReportSends);
    EXPECT_EQ(2U, g_agentRetrySleeps);
    EXPECT_EQ((std::vector<std::string>{"12", "13"}), g_agentReportTargets);
}

TEST_F(TestUbseMemRpc, UbseMemMigrateAgentReportLookupRetryExhausted)
{
    SwitchTestMockSet();
    AgentRetryMockSet();
    g_agentLookupFails = true;
    UbseMemShmMigrateService::GetInstance().ProcessExportRequest(AgentRetryRequest());
    EXPECT_EQ(3U, g_agentMasterQueries);
    EXPECT_EQ(0U, g_agentReportSends);
    EXPECT_EQ(3U, g_agentRetrySleeps);
}

TEST_F(TestUbseMemRpc, UbseMemMigrateAgentReportSendRetryExhausted)
{
    SwitchTestMockSet();
    AgentRetryMockSet();
    g_agentSendFails = true;
    UbseMemShmMigrateService::GetInstance().ProcessExportRequest(AgentRetryRequest());
    EXPECT_EQ(3U, g_agentMasterQueries);
    EXPECT_EQ(3U, g_agentReportSends);
    EXPECT_EQ(3U, g_agentRetrySleeps);
    EXPECT_EQ((std::vector<std::string>{"11", "12", "13"}), g_agentReportTargets);
}

TEST_F(TestUbseMemRpc, UbseMemMigrateAgentReportStopsAfterSuccess)
{
    SwitchTestMockSet();
    AgentRetryMockSet();
    UbseMemShmMigrateService::GetInstance().ProcessExportRequest(AgentRetryRequest());
    EXPECT_EQ(1U, g_agentMasterQueries);
    EXPECT_EQ(1U, g_agentReportSends);
    EXPECT_EQ(0U, g_agentRetrySleeps);
}

TEST_F(TestUbseMemRpc, UbseMemMigrateBeginImportResultHandlersHaveDistinctOperations)
{
    UbseMemMigrateBeginImportResultHandler importHandler;
    UbseMemMigrateBeginExportResultHandler exportHandler;
    EXPECT_EQ(static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_IMPORT_RESULT),
              importHandler.GetOpCode());
    EXPECT_EQ(static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_EXPORT_RESULT),
              exportHandler.GetOpCode());
    EXPECT_NE(importHandler.GetOpCode(), exportHandler.GetOpCode());
}

TEST_F(TestUbseMemRpc, UbseMemMigrateBeginExportResultHandler_QueueAck)
{
    SwitchTestMockSet();
    MigrateHandlerExecutorMockSet();
    UbseMemMigrateBeginExportResp data{};
    data.rpcInfo = HandlerTestRpcInfo();
    UbseBaseMessagePtr req = new UbseMemMigrateBeginExportRespSimpo();
    UbseBaseMessage::DeConvert<UbseMemMigrateBeginExportRespSimpo>(req)->SetUbseMesgInfo(data);
    UbseBaseMessagePtr ack = new UbseMemCallbackMessage();
    UbseMemMigrateBeginExportResultHandler handler;
    UbseComBaseMessageHandlerCtx peer{"", 0, 0, "13"};
    EXPECT_EQ(UBSE_OK, handler.Handle(req, ack, &peer));
    EXPECT_EQ("sync_success", UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(ack)->data);
    ASSERT_EQ(1U, g_migrateHandlerWork.size());
    g_migrateHandlerWork.clear();
}

TEST_F(TestUbseMemRpc, UbseMemMigrateMappedCopyStartHonorsCancellationAndSessionIdentity)
{
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    mgr.ClearSessions();
    const std::string name = "mapped-copy-cancel-gate";
    auto [session, created] = mgr.GetOrCreateSession(name, "3", "4", 2);
    ASSERT_TRUE(created);
    mgr.UpdateSessionState(name, UBS_MEM_MIGRATE_STATE_EXPORTING);
    ASSERT_EQ(UBSE_OK, mgr.RequestMigrateCancel(name));
    using Decision = UbseMemShmMigrateSessionManager::CopyStartResult;
    EXPECT_EQ(Decision::CANCELED, mgr.TryStartCopy(name, session));
    EXPECT_EQ(UBS_MEM_MIGRATE_STATE_EXPORTING, session->state);
    mgr.ClearSessions();
    auto [replacement, replaced] = mgr.GetOrCreateSession(name, "3", "4", 2);
    ASSERT_TRUE(replaced);
    mgr.UpdateSessionState(name, UBS_MEM_MIGRATE_STATE_EXPORTING);
    EXPECT_EQ(Decision::STALE, mgr.TryStartCopy(name, session));
    EXPECT_EQ(Decision::STARTED, mgr.TryStartCopy(name, replacement));
    EXPECT_EQ(UBSE_ERR_MIGRATE_NOT_ALLOWED, mgr.RequestMigrateCancel(name));
    EXPECT_EQ(Decision::STALE, mgr.TryStartCopy(name, replacement));
    mgr.ClearSessions();
}

TEST_F(TestUbseMemRpc, UbseMemMigrateProgressRejectsStaleAndOutOfOrderCompletion)
{
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    mgr.ClearSessions();
    const std::string name = "serial-progress";
    auto [session, created] = mgr.GetOrCreateSession(name, "3", "4", 2);
    ASSERT_TRUE(created);
    EXPECT_FALSE(session->completedBlocks.has_value());
    EXPECT_FALSE(mgr.MarkBlockCompleted(name, session, 0));
    mgr.UpdateSessionState(name, UBS_MEM_MIGRATE_STATE_EXPORTING);
    ASSERT_EQ(UbseMemShmMigrateSessionManager::CopyStartResult::STARTED, mgr.TryStartCopy(name, session));
    EXPECT_FALSE(mgr.MarkBlockCompleted(name, session, 1));
    EXPECT_FALSE(mgr.MarkBlockCompleted(name, session, 2));
    ASSERT_TRUE(mgr.MarkBlockCompleted(name, session, 0));
    EXPECT_TRUE(mgr.MarkBlockCompleted(name, session, 0));
    EXPECT_EQ(1U, session->completedBlocks.value());
    mgr.FinishSession(name, UBS_MEM_MIGRATE_STATE_FAILED);
    EXPECT_FALSE(mgr.MarkBlockCompleted(name, session, 1));
    EXPECT_TRUE(session->completedBlocks.has_value());
    auto [replacement, replaced] = mgr.GetOrCreateSession(name, "3", "4", 2);
    ASSERT_TRUE(replaced);
    EXPECT_FALSE(replacement->completedBlocks.has_value());
    mgr.UpdateSessionState(name, UBS_MEM_MIGRATE_STATE_EXPORTING);
    ASSERT_EQ(UbseMemShmMigrateSessionManager::CopyStartResult::STARTED, mgr.TryStartCopy(name, replacement));
    EXPECT_FALSE(mgr.MarkBlockCompleted(name, session, 0));
    EXPECT_EQ(0U, replacement->completedBlocks.value());
    EXPECT_TRUE(mgr.MarkBlockCompleted(name, replacement, 0));
    EXPECT_TRUE(mgr.MarkBlockCompleted(name, replacement, 1));
    EXPECT_EQ(2U, replacement->completedBlocks.value());
    mgr.ClearSessions();
}

TEST_F(TestUbseMemRpc, UbseMemMigrateOpResult_SerializeRoundtrip)
{
    UbseMemMigrateOpResp data{};
    data.errCode = UBSE_ERROR;
    data.committed = true;
    data.rpcInfo = HandlerTestRpcInfo();
    data.block = UbseMemMigrateBlockData{};
    data.block->exportInfo.memId = 1234;
    UbseMemMigrateOpRespSimpo message;
    message.SetUbseMesgInfo(data);
    ASSERT_EQ(UBSE_OK, message.Serialize());
    message.SetInputRawData(message.SerializedData(), message.SerializedDataSize());
    ASSERT_EQ(UBSE_OK, message.Deserialize());
    EXPECT_EQ(data.errCode, message.GetUbseMesgInfo().errCode);
    EXPECT_EQ(data.rpcInfo.name, message.GetUbseMesgInfo().rpcInfo.name);
    ASSERT_TRUE(message.GetUbseMesgInfo().block);
    EXPECT_EQ(1234U, message.GetUbseMesgInfo().block->exportInfo.memId);
    EXPECT_TRUE(message.GetUbseMesgInfo().committed);
}

TEST_F(TestUbseMemRpc, UbseMemMigrateBeginExportResult_SerializeRoundtrip)
{
    UbseMemMigrateBeginExportResp data{};
    data.errCode = UBSE_ERROR;
    data.rpcInfo.name = "migrate-handler-test";
    data.rpcInfo.srcNodeId = "13";
    data.rpcInfo.blockIndex = 3;
    data.rpcInfo.exportMemId = 1234;
    data.rpcInfo.migrateInfo = 0x11003;
    data.rpcInfo.rollback = true;
    data.block.memId = 1234;
    data.block.desc.length = 128;
    UbseMemMigrateBeginExportRespSimpo message;
    message.SetUbseMesgInfo(data);
    ASSERT_EQ(UBSE_OK, message.Serialize());
    message.SetInputRawData(message.SerializedData(), message.SerializedDataSize());
    ASSERT_EQ(UBSE_OK, message.Deserialize());
    const auto result = message.GetUbseMesgInfo();
    EXPECT_EQ(data.errCode, result.errCode);
    EXPECT_EQ(data.block.memId, result.block.memId);
    EXPECT_EQ(data.block.desc.length, result.block.desc.length);
    EXPECT_EQ(data.rpcInfo.name, result.rpcInfo.name);
    EXPECT_EQ(data.rpcInfo.srcNodeId, result.rpcInfo.srcNodeId);
    EXPECT_EQ(data.rpcInfo.blockIndex, result.rpcInfo.blockIndex);
    EXPECT_EQ(data.rpcInfo.exportMemId, result.rpcInfo.exportMemId);
    EXPECT_EQ(data.rpcInfo.migrateInfo, result.rpcInfo.migrateInfo);
    EXPECT_EQ(data.rpcInfo.rollback, result.rpcInfo.rollback);
}

TEST_F(TestUbseMemRpc, MigrationMasterUserPermission)
{
    using ubse::config::UbseConfModule;
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    mgr.ClearSessions();
    const std::string name = "master_user_permission";
    UbseUdsInfo owner{1001, 1001, 1, "creator"};
    mgr.GetOrCreateSession(name, "1", "2", 1, UbseMemMigrateTargetKind::NOT_IMPORT_NODE, 4096, owner);
    auto& service = UbseMemShmMigrateService::GetInstance();
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, owner));
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, UbseUdsInfo{2001, 2001, 1, "creator"}));
    mgr.FinishSession(name, UBS_MEM_MIGRATE_STATE_SUCCESS, UBSE_OK);
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, owner));
    EXPECT_EQ(UBSE_ERR_AUTH_FAILED, service.CheckUserPermission(name, UbseUdsInfo{}));
    auto conf = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    std::string users = " operator , automation ";
    MOCKER(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("auth.shm.migrate")), eq(std::string("allowedUsers")), outBound(users))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, UbseUdsInfo{1002, 1002, 1, "operator"}));
    EXPECT_EQ(UBSE_ERR_AUTH_FAILED, service.CheckUserPermission(name, UbseUdsInfo{1003, 1003, 1, "oper"}));
    EXPECT_EQ(UBSE_ERR_AUTH_FAILED, service.CheckUserPermission(name, UbseUdsInfo{1003, 1003, 1, "Operator"}));
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, UbseUdsInfo{0, 0, 1, "root"}));
    mgr.ClearSessions();
}

TEST_F(TestUbseMemRpc, MigrationMasterBuiltinUserPermissionWithoutConfig)
{
    using ubse::config::UbseConfModule;
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    mgr.ClearSessions();
    const std::string name = "configured_root_permission";
    mgr.GetOrCreateSession(name, "1", "2", 1, UbseMemMigrateTargetKind::NOT_IMPORT_NODE, 4096,
                           UbseUdsInfo{1001, 1001, 1, "creator"});
    MOCKER(&UbseConfModule::GetConf<std::string>).expects(never());
    auto& service = UbseMemShmMigrateService::GetInstance();
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, UbseUdsInfo{0, 0, 1, "root"}));
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, UbseUdsInfo{0, 0, 1, "root"}, true));
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, UbseUdsInfo{1000, 1000, 1, "ubse"}));
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, UbseUdsInfo{1000, 1000, 1, "ubse"}, true));
    mgr.ClearSessions();
}

TEST_F(TestUbseMemRpc, MigrationMasterDoesNotReadLegacyConfig)
{
    using ubse::config::UbseConfModule;
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    mgr.ClearSessions();
    const std::string name = "legacy_migration_permission";
    UbseUdsInfo owner{1001, 1001, 1, "creator"};
    mgr.GetOrCreateSession(name, "1", "2", 1, UbseMemMigrateTargetKind::NOT_IMPORT_NODE, 4096, owner);
    auto conf = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    MOCKER(&UbseConfModule::GetConf<std::string>)
        .expects(never())
        .with(eq(std::string("ubse.memory")), eq(std::string("shm.migrate.allowedUsers")), any());
    MOCKER(&UbseConfModule::GetConf<std::string>)
        .expects(once())
        .with(eq(std::string("auth.shm.migrate")), eq(std::string("allowedUsers")), any())
        .will(returnValue(UBSE_ERROR));
    auto& service = UbseMemShmMigrateService::GetInstance();
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission(name, owner));
    EXPECT_EQ(UBSE_ERR_AUTH_FAILED, service.CheckUserPermission(name, UbseUdsInfo{1002, 1002, 1, "operator"}));
    mgr.ClearSessions();
}

TEST_F(TestUbseMemRpc, MigrationMasterMissingConfigDeniesNonCreator)
{
    using ubse::config::UbseConfModule;
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    mgr.ClearSessions();
    UbseUdsInfo owner{1001, 1001, 1, ""};
    mgr.GetOrCreateSession("owner_uid_fallback", "1", "2", 1, UbseMemMigrateTargetKind::NOT_IMPORT_NODE, 4096, owner);
    std::shared_ptr<UbseConfModule> missing;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(missing));
    auto& service = UbseMemShmMigrateService::GetInstance();
    EXPECT_EQ(UBSE_OK, service.CheckUserPermission("owner_uid_fallback", UbseUdsInfo{1001, 1001, 1, "creator"}, true));
    EXPECT_EQ(UBSE_ERR_AUTH_FAILED,
              service.CheckUserPermission("owner_uid_fallback", UbseUdsInfo{1002, 1002, 1, "other"}, true));
    mgr.ClearSessions();
}

} // namespace ubse::mem_controller::ut
