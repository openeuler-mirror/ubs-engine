/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_cli_mem_attach.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_mem_attach.h"
#include "ubse_cli_mem_query.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_mem_controller.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"

namespace ubse::cli::reg {
class UbseCliMemAttach::UbseCliMemAttachImpl {
public:
    UbseCliMemAttachImpl() {}
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliAttachMem(const std::string& name);

private:
    UbseCliMemQuery query;
};
} // namespace ubse::cli::reg

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::serial;
using namespace ubse::mem::controller;

void TestUbseCliMemAttach::SetUp() {}
void TestUbseCliMemAttach::TearDown() {}

TEST_F(TestUbseCliMemAttach, AttachMemInvokeFailed)
{
    UbseCliMemAttach attacher;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(attacher.UbseCliAttachMem("test")->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemAttach, AttachMemInvokeNormal)
{
    UbseCliMemAttach attacher;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_shm_ubse_invoke_call_normal));
    EXPECT_NO_THROW(attacher.UbseCliAttachMem("test")->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemAttach, AttachMemInvokeEmpty)
{
    UbseCliMemAttach attacher;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    EXPECT_NO_THROW(attacher.UbseCliAttachMem("test")->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
} // namespace ubse::ut::cli
