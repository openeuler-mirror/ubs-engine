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
#include "test_ubse_cli_urma_cmd_reg.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_urma_cmd_reg.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::common::def;
using namespace ubse::serial;
void TestUbseCliUrmaCmdReg::SetUp() {}

void TestUbseCliUrmaCmdReg::TearDown() {}
TEST_F(TestUbseCliUrmaCmdReg, RegisterUrmaModule)
{
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UBSE_CLI_REGISTER_MODULE("CLI_URMA_MODULE", UbseCliRegUrmaModule);
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators.size(), 1);
    UbseCliModuleRegistry::GetInstance().UbseCliCallAllModuleSignUp();
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators.size(), 0);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
}

TEST_F(TestUbseCliUrmaCmdReg, UbseQueryUrmaQosFuncSuccess)
{
    UbseCliRegUrmaModule urma_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_urma_qos_ubse_invoke_call_normal));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "node", "1" } };    
    EXPECT_NO_THROW(UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliUrmaCmdReg, UbseQueryUrmaQosFuncEmpty)
{
    UbseCliRegUrmaModule urma_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_urma_invoke_call_empty));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "node", "1" } };    
    EXPECT_NO_THROW(UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliUrmaCmdReg, UbseQueryUrmaQosFuncFailed)
{
    UbseCliRegUrmaModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "node", "1" } };     
    EXPECT_NO_THROW(UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliUrmaCmdReg, UbseQueryUrmaQosFuncErrorSize)
{
    UbseCliRegUrmaModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "node", "1" } };     
    EXPECT_NO_THROW(UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

}