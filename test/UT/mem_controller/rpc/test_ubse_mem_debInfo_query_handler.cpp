/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_debInfo_query_handler.h"
#include "ubse_base_message.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mem_debt_info_partial_fetch.h"
#include "ubse_module.h"
#include "debt/ubse_mem_debt_info_query.h"
#include "message/node_mem_debt_info_simpo.h"
#include "message/ubse_mem_controller_def_simpo.h"
#include "message/ubse_mem_debt_info_partial_fetch_req.h"
#include "message/ubse_mem_debt_info_partial_fetch_res.h"
#include "message/ubse_mem_debt_info_query_req_simpo.h"

namespace ubse::mem_controller::ut {
using namespace ubse::context;
using namespace ubse::module;
using namespace ubse::com;
using namespace ubse::mem::controller::rpc;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::controller::debt;

void TestUbseMemDebInfoQueryHandler::SetUp()
{
    Test::SetUp();
}

void TestUbseMemDebInfoQueryHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}
TEST_F(TestUbseMemDebInfoQueryHandler, RegMxeMemGetOptResultHandler)
{
    std::shared_ptr<UbseComModule> comModule = nullptr;
    auto func = &UbseContext::GetModule<UbseComModule>;
    MOCKER(func).stubs().will(returnValue(comModule)).then(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    auto ret = UbseMemDebtInfoQueryHandler::RegUbseMemDebtInfoQueryHandler();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    ret = UbseMemDebtInfoQueryHandler::RegUbseMemDebtInfoQueryHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
    ret = UbseMemDebtInfoQueryHandler::RegUbseMemDebtInfoQueryHandler();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemDebInfoQueryHandler, Handle_Success)
{
    UbseBaseMessagePtr req = Ref<UbseBaseMessage>(new NodeMemDebtInfoSimpo);
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new NodeMemDebtInfoSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoQueryHandler q;
    auto ret = q.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemDebInfoQueryHandler, Handle_Failed1)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new NodeMemDebtInfoSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoQueryHandler q;
    auto ret = q.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, Handle_Failed2)
{
    UbseBaseMessagePtr req = Ref<UbseBaseMessage>(new NodeMemDebtInfoSimpo);
    UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoQueryHandler q;
    auto ret = q.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoFdGetHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemFdDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoFdGetHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoFdListHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemFdDescListSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoFdListHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoNumaGetHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new DefUbseMemNumaDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoNumaGetHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoNumaGetWithImportNodeHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemNumaDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoNumaGetWithImportNodeHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoNumaListHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new DefUbseMemNumaDescListSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoNumaListHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmGetHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemShmDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoShmGetHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmListHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemShmDescListSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoShmListHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmStatusGetHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemShmMemStatusDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoShmStatusGetHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoAddrGetHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemAddrDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoAddrGetHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoPartialFetchHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemDebtInfoPartialFetchRes);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoPartialFetchHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoPartialFetchHandler_Handle_NullRsp)
{
    UbseBaseMessagePtr req = Ref<UbseBaseMessage>(new UbseMemDebtInfoPartialFetchReq);
    UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoPartialFetchHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemIdInfoGetHandler_Handle_NullReq)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = Ref<UbseBaseMessage>(new UbseMemExportMemDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemIdInfoGetHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, GetOpCode)
{
    UbseMemDebtInfoQueryHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBINFO_QUERY));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoFdGetHandler_GetOpCode)
{
    UbseMemDebtInfoFdGetHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_FD_GET));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoFdListHandler_GetOpCode)
{
    UbseMemDebtInfoFdListHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_FD_LIST));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoNumaGetHandler_GetOpCode)
{
    UbseMemDebtInfoNumaGetHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_NUMA_GET));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoNumaGetWithImportNodeHandler_GetOpCode)
{
    UbseMemDebtInfoNumaGetWithImportNodeHandler handler;
    EXPECT_EQ(handler.GetOpCode(),
              static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_NUMA_GET_WITH_IMPORT_NODE));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoNumaListHandler_GetOpCode)
{
    UbseMemDebtInfoNumaListHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_NUMA_LIST));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmGetHandler_GetOpCode)
{
    UbseMemDebtInfoShmGetHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_GET));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmListHandler_GetOpCode)
{
    UbseMemDebtInfoShmListHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_LIST));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmStatusGetHandler_GetOpCode)
{
    UbseMemDebtInfoShmStatusGetHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_STATUS_GET));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoAddrGetHandler_GetOpCode)
{
    UbseMemDebtInfoAddrGetHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_ADDR_GET));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoPartialFetchHandler_GetOpCode)
{
    UbseMemDebtInfoPartialFetchHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_PARTIAL_FETCH));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemNodeBorrowQueryHandler_GetOpCode)
{
    UbseMemNodeBorrowQueryHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_NODE_BORROW_INFO_QUERY));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemIdInfoGetHandler_GetOpCode)
{
    UbseMemIdInfoGetHandler handler;
    EXPECT_EQ(handler.GetOpCode(), static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_ID_DEBINFO_QUERY));
    EXPECT_EQ(handler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY));
    EXPECT_TRUE(handler.NeedReply());
}

TEST_F(TestUbseMemDebInfoQueryHandler, RegUbseMemDebtInfoQueryHandler_HandlerNull)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemDebtInfoQueryHandler::RegUbseMemDebtInfoQueryHandler();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoFdGetHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemFdDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoFdGetHandler handler;
    MOCKER(UbseMemFdGet).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoFdGetHandler_Handle_NullRsp)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoFdGetHandler handler;
    MOCKER(UbseMemFdGet).stubs().will(returnValue(UBSE_OK));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoFdListHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemFdDescListSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoFdListHandler handler;
    MOCKER(UbseMemFdList).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoNumaGetHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new DefUbseMemNumaDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoNumaGetHandler handler;
    MOCKER(UbseMemNumaGet).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoNumaGetWithImportNodeHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemNumaDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoNumaGetWithImportNodeHandler handler;
    MOCKER(UbseMemNumaGetWithImportNode).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoNumaListHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new DefUbseMemNumaDescListSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoNumaListHandler handler;
    MOCKER(UbseMemNumaList).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmGetHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemShmDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoShmGetHandler handler;
    MOCKER(UbseMemShmGet).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmGetHandler_Handle_NullRsp)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoShmGetHandler handler;
    MOCKER(UbseMemShmGet).stubs().will(returnValue(UBSE_OK));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmListHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemShmDescListSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoShmListHandler handler;
    MOCKER(UbseMemShmList).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmStatusGetHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemShmMemStatusDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoShmStatusGetHandler handler;
    MOCKER(UbseMemShmStatusGet).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoShmStatusGetHandler_Handle_NullRsp)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoShmStatusGetHandler handler;
    MOCKER(UbseMemShmStatusGet).stubs().will(returnValue(UBSE_OK));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoAddrGetHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemAddrDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoAddrGetHandler handler;
    MOCKER(UbseMemAddrGet).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoPartialFetchHandler_Handle_EmptyRawData)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtInfoPartialFetchReq);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemDebtInfoPartialFetchRes);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoPartialFetchHandler handler;
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_EMPTY);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoPartialFetchHandler_Handle_ValidateFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtInfoPartialFetchReq);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemDebtInfoPartialFetchRes);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoPartialFetchHandler handler;
    uint8_t data[] = {1, 2, 3};
    req->SetInputRawData(data, sizeof(data));
    MOCKER(ValidateDebtFetchInfo).stubs().will(returnValue(UBSE_ERROR_INVAL));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemDebtInfoPartialFetchHandler_Handle_FetchFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemDebtInfoPartialFetchReq);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemDebtInfoPartialFetchRes);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemDebtInfoPartialFetchHandler handler;
    uint8_t data[] = {1, 2, 3};
    req->SetInputRawData(data, sizeof(data));
    MOCKER(ValidateDebtFetchInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(FetchDebtInfoByTypeAndPage).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemNodeBorrowQueryHandler_Handle_BusinessFail)
{
    UbseBaseMessagePtr req = nullptr;
    auto rsp = Ref<UbseBaseMessage>(new UbseMemNodeBorrowInfoMessage);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemNodeBorrowQueryHandler handler;
    MOCKER(UbseMemNodeBorrowQuery).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemNodeBorrowQueryHandler_Handle_NullRsp)
{
    UbseBaseMessagePtr req = nullptr;
    UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemNodeBorrowQueryHandler handler;
    MOCKER(UbseMemNodeBorrowQuery).stubs().will(returnValue(UBSE_OK));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemIdInfoGetHandler_Handle_BusinessFail)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemIdQueryRequestSimpo);
    auto rsp = Ref<UbseBaseMessage>(new UbseMemExportMemDescSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemIdInfoGetHandler handler;
    MOCKER(UbseMemGetMemIdByImport).stubs().will(returnValue(UBSE_ERROR));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemDebInfoQueryHandler, UbseMemIdInfoGetHandler_Handle_NullRsp)
{
    auto req = Ref<UbseBaseMessage>(new UbseMemIdQueryRequestSimpo);
    UbseBaseMessagePtr rsp = nullptr;
    UbseComBaseMessageHandlerCtxPtr ctx;
    UbseMemIdInfoGetHandler handler;
    MOCKER(UbseMemGetMemIdByImport).stubs().will(returnValue(UBSE_OK));
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}
} // namespace ubse::mem_controller::ut