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

#include "test_ubse_cli_mem_query.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_mem_query.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_mem_controller.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"

namespace ubse::cli::reg {
class UbseCliMemQuery::UbseCliMemQueryImpl {
public:
    template <typename T, uint16_t ModuleCode, uint16_t OpCode>
    bool UbseCliMemQueryRequest(const std::string& name, T& container);

    template <typename InfoType, uint16_t ModuleCode, uint16_t OpCode>
    std::shared_ptr<framework::UbseCliResultEcho> HandleQueryError(const std::string& name);

    template <typename InfoType, uint16_t ModuleCode, uint16_t OpCode>
    std::shared_ptr<framework::UbseCliResultEcho> HandleNonWaitCreatingState(const std::string& name);

    bool ShmMemoryGet(const std::string& name, UbseMemShmInfo& shmDesc);

private:
    uint32_t errorCode_{UBSE_OK};
    std::string errorMsg_{};
};
} // namespace ubse::cli::reg

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::serial;
using namespace ubse::mem::controller;

void TestUbseCliMemQuery::SetUp() {}
void TestUbseCliMemQuery::TearDown() {}

TEST_F(TestUbseCliMemQuery, GetNumaMemByNameInvokeFailed)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(
        query.UbseCliGetNumaMemByName("test", UbseCliMemQuery::WaitType::QUERY_ONLY)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetNumaMemByNameInvokeEmpty)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    EXPECT_NO_THROW(
        query.UbseCliGetNumaMemByName("test", UbseCliMemQuery::WaitType::QUERY_ONLY)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetNumaMemByNameInvokeNormal)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_numa_ubse_invoke_call_normal));
    EXPECT_NO_THROW(
        query.UbseCliGetNumaMemByName("test", UbseCliMemQuery::WaitType::QUERY_ONLY)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetFdMemByNameInvokeFailed)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(query.UbseCliGetFdMemByName("test", UbseCliMemQuery::WaitType::QUERY_ONLY)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetFdMemByNameInvokeNormal)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_fd_ubse_invoke_call_normal));
    EXPECT_NO_THROW(query.UbseCliGetFdMemByName("test", UbseCliMemQuery::WaitType::QUERY_ONLY)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetShmMemByNameInvokeFailed)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(query.UbseCliGetShmMemByName("test", UbseCliShmOperation::ATTACH)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetShmMemByNameInvokeEmpty)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    EXPECT_NO_THROW(query.UbseCliGetShmMemByName("test", UbseCliShmOperation::ATTACH)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetShmMemByNameInvokeNormal)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_shm_ubse_invoke_call_normal));
    EXPECT_NO_THROW(query.UbseCliGetShmMemByName("test", UbseCliShmOperation::ATTACH)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetShmMemByNameWithCreateOperation)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_shm_ubse_invoke_call_normal));
    EXPECT_NO_THROW(query.UbseCliGetShmMemByName("test", UbseCliShmOperation::CREATE)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetShmMemByNameWithDeleteOperation)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_shm_ubse_invoke_call_normal));
    EXPECT_NO_THROW(query.UbseCliGetShmMemByName("test", UbseCliShmOperation::DELETE)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemQuery, GetShmMemByNameWithDetachOperation)
{
    UbseCliMemQuery query;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_shm_ubse_invoke_call_normal));
    EXPECT_NO_THROW(query.UbseCliGetShmMemByName("test", UbseCliShmOperation::DETACH)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
} // namespace ubse::ut::cli
