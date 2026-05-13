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

#include "test_ubse_election_pkt_handler.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_election_pkt_simpo.h"
#include "ubse_election_reply_pkt_simpo.h"

namespace ubse::ut::election {
using namespace ubse::election::message;
using namespace ubse::context;
void TestUbseElectionPktHandler::SetUp()
{
    Test::SetUp();
    electionPktHandler = UbseElectionPktHandler();
    electionPkt = {1, "1", "2", 2, 1, 1, {"3", "4"}, 3, 4};
    UbseElectionPktSimpoPtr inputElectionPktSimpo = new UbseElectionPktSimpo(electionPkt);
    inputElectionPktSimpo->Serialize();
    request =
        new UbseElectionPktSimpo(inputElectionPktSimpo->SerializedData(), inputElectionPktSimpo->SerializedDataSize());
    response = new UbseElectionPktSimpo();
}

void TestUbseElectionPktHandler::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseElectionPktHandler, RegElectionPktHandler_ShouldReturnError_WhenUbseComModuleIsNull)
{
    UbseContext::GetInstance().GetModule<UbseComModule>() = nullptr;
    UbseResult result = UbseElectionPktHandler::RegElectionPktHandler();
    EXPECT_EQ(result, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseElectionPktHandler, RegElectionPktHandler_ShouldReturnError_WhenRegRpcServiceFailed)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    UbseResult (UbseComModule::*func)(UbseComBaseMessageHandlerPtr&) =
        &ubse::com::UbseComModule::RegRpcService<UbseElectionPktSimpo, UbseElectionReplyPktSimpo>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    UbseResult result = UbseElectionPktHandler::RegElectionPktHandler();
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST_F(TestUbseElectionPktHandler, RegElectionPktHandlerSuccess)
{
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    UbseResult (UbseComModule::*func)(UbseComBaseMessageHandlerPtr&) =
        &ubse::com::UbseComModule::RegRpcService<UbseElectionPktSimpo, UbseElectionReplyPktSimpo>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    UbseResult result = UbseElectionPktHandler::RegElectionPktHandler();
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestUbseElectionPktHandler, Handle)
{
    UbseElectionPktHandler handler;
    UbseBaseMessagePtr req = new (std::nothrow) UbseElectionPktSimpo();
    UbseBaseMessagePtr rsp = new (std::nothrow)(UbseElectionReplyPktSimpo);
    UbseComBaseMessageHandlerCtxPtr ctx{};
    auto ret = handler.Handle(req, rsp, ctx);
    EXPECT_EQ(UBSE_OK, ret);
}
} // namespace ubse::ut::election
