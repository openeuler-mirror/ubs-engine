/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_controller_rpc_register.h"
#include "message/node_mem_debt_info_simpo.h"
#include "message/node_mem_debtInfo_query_req_simpo.h"
#include "message/ubse_mem_controller_def_simpo.h"
#include "message/ubse_mem_debt_info_partial_fetch_req.h"
#include "message/ubse_mem_debt_info_partial_fetch_res.h"
#include "message/ubse_mem_update_obj_state.simpo.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_error.h"

namespace ubse::mem_controller::ut {
using namespace ubse::context;
using namespace ubse::module;
using namespace ubse::com;
using namespace ubse::mem::controller::rpc;
using namespace ubse::mem::controller::message;

void TestUbseMemControllerRpcRegister::SetUp()
{
    Test::SetUp();
}

void TestUbseMemControllerRpcRegister::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_ComModuleNull)
{
    std::shared_ptr<UbseComModule> nullModule;
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule));
    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_C)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_Success)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>;
    MOCKER(funcShmGet).stubs().will(returnValue(UBSE_OK));

    const auto funcShmList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescListSimpo>;
    MOCKER(funcShmList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmStatus = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmMemStatusDescSimpo>;
    MOCKER(funcShmStatus).stubs().will(returnValue(UBSE_OK));

    const auto funcAddrGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemAddrDescSimpo>;
    MOCKER(funcAddrGet).stubs().will(returnValue(UBSE_OK));

    const auto funcPartialFetch = &UbseComModule::RegRpcService<UbseMemDebtInfoPartialFetchReq, UbseMemDebtInfoPartialFetchRes>;
    MOCKER(funcPartialFetch).stubs().will(returnValue(UBSE_OK));

    const auto funcNodeBorrow = &UbseComModule::RegRpcService<UbseMemNodeBorrowInfoReqMessage, UbseMemNodeBorrowInfoMessage>;
    MOCKER(funcNodeBorrow).stubs().will(returnValue(UBSE_OK));

    const auto funcMemId = &UbseComModule::RegRpcService<UbseMemIdQueryRequestSimpo, UbseMemExportMemDescSimpo>;
    MOCKER(funcMemId).stubs().will(returnValue(UBSE_OK));

    const auto funcAgentUpdate = &UbseComModule::RegRpcService<UbseMemUpdateObjState, UbseMemUpdateObjStateReply>;
    MOCKER(funcAgentUpdate).stubs().will(returnValue(UBSE_OK));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_DebtInfoQueryFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_FdGetFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_FdListFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_NumaGetFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_NumaGetWithImportFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_NumaListFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_ShmGetFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>;
    MOCKER(funcShmGet).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_ShmListFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>;
    MOCKER(funcShmGet).stubs().will(returnValue(UBSE_OK));

    const auto funcShmList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescListSimpo>;
    MOCKER(funcShmList).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_ShmStatusFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>;
    MOCKER(funcShmGet).stubs().will(returnValue(UBSE_OK));

    const auto funcShmList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescListSimpo>;
    MOCKER(funcShmList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmStatus = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmMemStatusDescSimpo>;
    MOCKER(funcShmStatus).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_AddrGetFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>;
    MOCKER(funcShmGet).stubs().will(returnValue(UBSE_OK));

    const auto funcShmList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescListSimpo>;
    MOCKER(funcShmList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmStatus = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmMemStatusDescSimpo>;
    MOCKER(funcShmStatus).stubs().will(returnValue(UBSE_OK));

    const auto funcAddrGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemAddrDescSimpo>;
    MOCKER(funcAddrGet).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_PartialFetchFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>;
    MOCKER(funcShmGet).stubs().will(returnValue(UBSE_OK));

    const auto funcShmList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescListSimpo>;
    MOCKER(funcShmList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmStatus = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmMemStatusDescSimpo>;
    MOCKER(funcShmStatus).stubs().will(returnValue(UBSE_OK));

    const auto funcAddrGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemAddrDescSimpo>;
    MOCKER(funcAddrGet).stubs().will(returnValue(UBSE_OK));

    const auto funcPartialFetch = &UbseComModule::RegRpcService<UbseMemDebtInfoPartialFetchReq, UbseMemDebtInfoPartialFetchRes>;
    MOCKER(funcPartialFetch).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_NodeBorrowFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>;
    MOCKER(funcShmGet).stubs().will(returnValue(UBSE_OK));

    const auto funcShmList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescListSimpo>;
    MOCKER(funcShmList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmStatus = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmMemStatusDescSimpo>;
    MOCKER(funcShmStatus).stubs().will(returnValue(UBSE_OK));

    const auto funcAddrGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemAddrDescSimpo>;
    MOCKER(funcAddrGet).stubs().will(returnValue(UBSE_OK));

    const auto funcPartialFetch = &UbseComModule::RegRpcService<UbseMemDebtInfoPartialFetchReq, UbseMemDebtInfoPartialFetchRes>;
    MOCKER(funcPartialFetch).stubs().will(returnValue(UBSE_OK));

    const auto funcNodeBorrow = &UbseComModule::RegRpcService<UbseMemNodeBorrowInfoReqMessage, UbseMemNodeBorrowInfoMessage>;
    MOCKER(funcNodeBorrow).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_MemIdFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>;
    MOCKER(funcShmGet).stubs().will(returnValue(UBSE_OK));

    const auto funcShmList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescListSimpo>;
    MOCKER(funcShmList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmStatus = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmMemStatusDescSimpo>;
    MOCKER(funcShmStatus).stubs().will(returnValue(UBSE_OK));

    const auto funcAddrGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemAddrDescSimpo>;
    MOCKER(funcAddrGet).stubs().will(returnValue(UBSE_OK));

    const auto funcPartialFetch = &UbseComModule::RegRpcService<UbseMemDebtInfoPartialFetchReq, UbseMemDebtInfoPartialFetchRes>;
    MOCKER(funcPartialFetch).stubs().will(returnValue(UBSE_OK));

    const auto funcNodeBorrow = &UbseComModule::RegRpcService<UbseMemNodeBorrowInfoReqMessage, UbseMemNodeBorrowInfoMessage>;
    MOCKER(funcNodeBorrow).stubs().will(returnValue(UBSE_OK));

    const auto funcMemId = &UbseComModule::RegRpcService<UbseMemIdQueryRequestSimpo, UbseMemExportMemDescSimpo>;
    MOCKER(funcMemId).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerRpcRegister, RegMemControllerHandler_AgentUpdateFail)
{
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));

    const auto funcDebtInfoQuery = &UbseComModule::RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>;
    MOCKER(funcDebtInfoQuery).stubs().will(returnValue(UBSE_OK));

    const auto funcFdGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescSimpo>;
    MOCKER(funcFdGet).stubs().will(returnValue(UBSE_OK));

    const auto funcFdList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemFdDescListSimpo>;
    MOCKER(funcFdList).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescSimpo>;
    MOCKER(funcNumaGet).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaGetWithImport = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemNumaDescSimpo>;
    MOCKER(funcNumaGetWithImport).stubs().will(returnValue(UBSE_OK));

    const auto funcNumaList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, DefUbseMemNumaDescListSimpo>;
    MOCKER(funcNumaList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescSimpo>;
    MOCKER(funcShmGet).stubs().will(returnValue(UBSE_OK));

    const auto funcShmList = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmDescListSimpo>;
    MOCKER(funcShmList).stubs().will(returnValue(UBSE_OK));

    const auto funcShmStatus = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemShmMemStatusDescSimpo>;
    MOCKER(funcShmStatus).stubs().will(returnValue(UBSE_OK));

    const auto funcAddrGet = &UbseComModule::RegRpcService<UbseMemDebtQueryRequestSimpo, UbseMemAddrDescSimpo>;
    MOCKER(funcAddrGet).stubs().will(returnValue(UBSE_OK));

    const auto funcPartialFetch = &UbseComModule::RegRpcService<UbseMemDebtInfoPartialFetchReq, UbseMemDebtInfoPartialFetchRes>;
    MOCKER(funcPartialFetch).stubs().will(returnValue(UBSE_OK));

    const auto funcNodeBorrow = &UbseComModule::RegRpcService<UbseMemNodeBorrowInfoReqMessage, UbseMemNodeBorrowInfoMessage>;
    MOCKER(funcNodeBorrow).stubs().will(returnValue(UBSE_OK));

    const auto funcMemId = &UbseComModule::RegRpcService<UbseMemIdQueryRequestSimpo, UbseMemExportMemDescSimpo>;
    MOCKER(funcMemId).stubs().will(returnValue(UBSE_OK));

    const auto funcAgentUpdate = &UbseComModule::RegRpcService<UbseMemUpdateObjState, UbseMemUpdateObjStateReply>;
    MOCKER(funcAgentUpdate).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RegMemControllerHandler();
    EXPECT_EQ(ret, UBSE_ERROR);
}
} // namespace ubse::mem_controller::ut