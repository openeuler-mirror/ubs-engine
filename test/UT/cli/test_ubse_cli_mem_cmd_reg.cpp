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

#include "test_ubse_cli_mem_cmd_reg.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "test_mock_invoke.h"
#include "ubse_cli_mem_cmd_reg.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_serial_util.h"
namespace ubse::ut::cli {
using namespace ubse::cli::framework;
using namespace ubse::serial;
using namespace ubse::cli::reg;
void TestUbseCliMemCmdReg::SetUp() {}

void TestUbseCliMemCmdReg::TearDown() {}

TEST_F(TestUbseCliMemCmdReg, RegisterMemModule)
{
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UBSE_CLI_REGISTER_MODULE("CLI_MEM_MODULE", UbseCliRegMemModule);
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators.size(), 1);
    UbseCliModuleRegistry::GetInstance().UbseCliCallAllModuleSignUp();
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators.size(), 0);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemQueryEmptyParams)
{
    UbseCliRegMemModule mem_module;
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc({})->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemQueryErrorParams)
{
    UbseCliRegMemModule mem_module;
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "type", "error" } };
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsInvokeFailed)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "type", "borrow_detail" } };
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsInvokeErrorSize)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "type", "borrow_detail" } };
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsInvokeEmpty)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "type", "borrow_detail" } };
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsInvokeNormal)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_borrow_details_ubse_invoke_call_normal));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "type", "borrow_detail" } };
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeBorrowInvokeFailed)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "type", "node_borrow" } };
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeBorrowInvokeErrorSize)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "type", "node_borrow" } };
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeBorrowInvokeEmpty)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "type", "node_borrow" } };
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeBorrowInvokeNormal)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_node_borrow_ubse_invoke_call_normal));
    std::map<std::basic_string<char>, std::basic_string<char>> params{ { "type", "node_borrow" } };
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
TEST_F(TestUbseCliMemCmdReg, CheckMemoryInvokeFailed)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, CheckMemoryInvokeErrorSize)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, CheckMemoryInvokeEmpty)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, CheckMemoryInvokeNormal)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_check_memory_ubse_invoke_call_normal));
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
} // namespace ubse::ut::cli
