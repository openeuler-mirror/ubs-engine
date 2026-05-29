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

#include "test_ubse_cli_mem_create.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_mem_create.h"
#include "ubse_cli_mem_query.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_mem_controller.h"
#include "ubse_mmi_def.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"

namespace ubse::cli::reg {
class UbseCliMemCreate::UbseCliMemCreateImpl {
public:
    UbseCliMemCreateImpl() {}

    template <typename T, uint16_t ModuleCode, uint16_t Opcode>
    bool UbseCliMemCreateRequest(UbseSerialization& ubse_req_serial, T& container);

    template <uint16_t ModuleCode, uint16_t Opcode>
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliCreateMemImpl(UbseSerialization& ubse_req_serial,
                                                                       const std::string& name);

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliCreateShareMemImpl(const std::string& name, size_t size,
                                                                            const std::vector<uint32_t>& region);

private:
    std::string errorMsg_{};
    uint32_t callErrorCode_{UBSE_OK};
    using queryFuncType =
        std::function<std::shared_ptr<framework::UbseCliResultEcho>(const std::string&, UbseCliMemQuery::WaitType)>;
    std::map<uint16_t, queryFuncType> queryFuncMap_;
    UbseCliMemQuery query;
};
} // namespace ubse::cli::reg

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::serial;
using namespace ubse::mem::controller;

void TestUbseCliMemCreate::SetUp() {}
void TestUbseCliMemCreate::TearDown() {}

TEST_F(TestUbseCliMemCreate, CreateNumaMemInvokeFailed)
{
    UbseCliMemCreate creator;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(creator.UbseCliCreateNumaMem("test", 128 * 1024 * 1024, "1/2/3-4/5/6")->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCreate, CreateNumaMemInvokeNormal)
{
    UbseCliMemCreate creator;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_mem_operation_resp_ubse_invoke_call_normal));
    EXPECT_NO_THROW(creator.UbseCliCreateNumaMem("test", 128 * 1024 * 1024, "1/2/3-4/5/6")->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCreate, CreateFdMemInvokeFailed)
{
    UbseCliMemCreate creator;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(creator.UbseCliCreateFdMem("test", 256 * 1024 * 1024)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCreate, CreateFdMemInvokeNormal)
{
    UbseCliMemCreate creator;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_mem_operation_resp_ubse_invoke_call_normal));
    EXPECT_NO_THROW(creator.UbseCliCreateFdMem("test", 256 * 1024 * 1024)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCreate, CreateShareMemInvokeFailed)
{
    UbseCliMemCreate creator;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::vector<uint32_t> region{1, 2};
    EXPECT_NO_THROW(creator.UbseCliCreateShareMem("test", 512 * 1024 * 1024, region)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCreate, CreateShareMemInvokeNormal)
{
    UbseCliMemCreate creator;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_shm_ubse_invoke_call_normal));
    std::vector<uint32_t> region{1, 2};
    EXPECT_NO_THROW(creator.UbseCliCreateShareMem("test", 512 * 1024 * 1024, region)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCreate, CreateShareMemInvokeEmpty)
{
    UbseCliMemCreate creator;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    std::vector<uint32_t> region{1, 2};
    EXPECT_NO_THROW(creator.UbseCliCreateShareMem("test", 512 * 1024 * 1024, region)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
} // namespace ubse::ut::cli
