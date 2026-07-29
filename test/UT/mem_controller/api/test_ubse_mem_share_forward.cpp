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

#include "test_ubse_mem_share_forward.h"

#include <memory>
#include <string>

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "message/ubse_mem_share_delete_import_ledger_req_simpo.h"

namespace ubse::mem::controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::com;
using namespace ubse::context;
using namespace ubse::message;
using namespace ubse::mem::controller::message;

void TestShareForward::SetUp()
{
    Test::SetUp();
}

void TestShareForward::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// SF-01: ForwardDeleteImportLedgerToCascade returns error when comModule is null
TEST_F(TestShareForward, ForwardDeleteImportLedgerToCascade_NullComModule_ReturnsError)
{
    std::shared_ptr<UbseComModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(nullModule));

    auto ret = ForwardDeleteImportLedgerToCascade("fault-node-1", "cascade-node-1");
    EXPECT_NE(ret, UBSE_OK);
}

// SF-02: ForwardDeleteImportLedgerToCascade succeeds when comModule and RpcSend work
TEST_F(TestShareForward, ForwardDeleteImportLedgerToCascade_RpcSendSuccess_ReturnsOk)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(comModule));

    const auto rpcFunc = &UbseComModule::RpcSend<UbseMemShareDeleteImportLedgerReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(rpcFunc)
        .stubs()
        .will(returnValue(UBSE_OK));

    auto ret = ForwardDeleteImportLedgerToCascade("fault-node-2", "cascade-node-2");
    EXPECT_EQ(ret, UBSE_OK);
}

// SF-03: ForwardDeleteImportLedgerToCascade returns error when all RpcSend retries fail
TEST_F(TestShareForward, ForwardDeleteImportLedgerToCascade_RpcSendAlwaysFails_ReturnsError)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(comModule));

    const auto rpcFunc = &UbseComModule::RpcSend<UbseMemShareDeleteImportLedgerReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(rpcFunc)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    auto ret = ForwardDeleteImportLedgerToCascade("fault-node-3", "cascade-node-3");
    EXPECT_NE(ret, UBSE_OK);
}

} // namespace ubse::mem::controller::ut
