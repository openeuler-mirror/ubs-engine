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

#include "test_ubse_mem_debt_info_query.h"

#include <mockcpp/mockcpp.hpp>

#include "message/ubse_mem_debtInfo_query_req_simpo.h"
#include "ubse_com_module.h"
#include "ubse_election.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_resource.h"

namespace ubse::mem_controller::ut {
using namespace com;
using namespace ubse::resource::mem;

void TestUbseMemDebtInfoQuery::SetUp()
{
    Test::SetUp();
}

void TestUbseMemDebtInfoQuery::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemDebtInfoQuery, UbseGetMemDebtInfo)
{
    std::string nodeId = "1";
    NodeMemDebtInfoMap memDebtInfoMap{};
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    const auto getModuleFunc = &context::UbseContext::GetModule<UbseComModule>;
    MOCKER(getModuleFunc).stubs().will(returnValue(nullModule)).then(returnValue(module));

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::NodeMemDebtInfoQueryReqSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    EXPECT_EQ(UbseGetMemDebtInfo(nodeId, memDebtInfoMap), UBSE_ERROR);
    EXPECT_EQ(UbseGetMemDebtInfo(nodeId, memDebtInfoMap), UBSE_ERROR_MODULE_LOAD_FAILED);
    EXPECT_EQ(UbseGetMemDebtInfo(nodeId, memDebtInfoMap), UBSE_ERROR);
    EXPECT_EQ(UbseGetMemDebtInfo(nodeId, memDebtInfoMap), UBSE_OK);
}

TEST_F(TestUbseMemDebtInfoQuery, UbseGetLocalNodeMemDebtInfo)
{
    election::UbseRoleInfo currentNodeInfo{};
    currentNodeInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo)
        .stubs()
        .with(outBound(currentNodeInfo))
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    MOCKER_CPP(&mem::controller::GetNodeMemDebtInfoById)
        .stubs()
        .will(returnValue(UBSE_ERROR_NULLPTR))
        .then(returnValue(UBSE_OK));
    NodeMemDebtInfo nodeMemDebtInfo{};
    EXPECT_EQ(UbseGetLocalNodeMemDebtInfo(nodeMemDebtInfo), UBSE_ERROR);
    EXPECT_EQ(UbseGetLocalNodeMemDebtInfo(nodeMemDebtInfo), UBSE_ERROR_NULLPTR);
    EXPECT_EQ(UbseGetLocalNodeMemDebtInfo(nodeMemDebtInfo), UBSE_OK);
}
}
