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

#include "test_ubse_cli_node_cmd_reg.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_node_cmd_reg.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::common::def;
using namespace ubse::serial;
void TestUbseCliNodeCmdReg::SetUp() {}

void TestUbseCliNodeCmdReg::TearDown() {}

TEST_F(TestUbseCliNodeCmdReg, RegisterNodeModule)
{
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UBSE_CLI_REGISTER_MODULE("CLI_NODE_MODULE", UbseCliRegNodeModule);
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators.size(), 1);
    UbseCliModuleRegistry::GetInstance().UbseCliCallAllModuleSignUp();
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators.size(), 0);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
}

TEST_F(TestUbseCliNodeCmdReg, TopoInvokeFailed)
{
    UbseCliRegNodeModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseQueryUbseTopologyFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, TopoInvokeErrorSize)
{
    UbseCliRegNodeModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseQueryUbseTopologyFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, TopoInvokeEmpty)
{
    UbseCliRegNodeModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseQueryUbseTopologyFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, TopoInvokeSuccess)
{
    UbseCliRegNodeModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_topo_ubse_invoke_call_normal));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseQueryUbseTopologyFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
TEST_F(TestUbseCliNodeCmdReg, ClusterInvokeFailed)
{
    UbseCliRegNodeModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseQueryClusterInfoFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, ClusterInvokeErrorSize)
{
    UbseCliRegNodeModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseQueryClusterInfoFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, ClusterInvokeEmpty)
{
    UbseCliRegNodeModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseQueryClusterInfoFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, ClusterInvokeSuccess)
{
    UbseCliRegNodeModule node_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_cluster_ubse_invoke_call_normal));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseQueryClusterInfoFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
} // namespace ubse::ut::cli
