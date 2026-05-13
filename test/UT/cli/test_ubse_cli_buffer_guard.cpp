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

#include "test_ubse_cli_buffer_guard.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_buffer_guard.h"

namespace ubse::ut::cli {
using namespace ubse::cli::reg;

void TestUbseCliBufferGuard::SetUp() {}

void TestUbseCliBufferGuard::TearDown() {}

static bool BUFFER_FREE_FLAG = false;

void mock_ubse_api_buffer_free(ubse_api_buffer_t* apiBuffer)
{
    BUFFER_FREE_FLAG = true;
    if (apiBuffer->buffer != nullptr) {
        free(apiBuffer->buffer);
        apiBuffer->buffer = nullptr;
        apiBuffer->length = 0;
    }
}
/**
 * Automatic Memory Release
 */
TEST_F(TestUbseCliBufferGuard, AutoReleaseBuffer)
{
    MOCKER(&ubse_api_buffer_free).stubs().will(invoke(mock_ubse_api_buffer_free));
    {
        ubse_api_buffer_t res_buffer{};
        res_buffer.length = 8;
        res_buffer.buffer = static_cast<uint8_t*>(malloc(8));
        UbseCliBufferGuard ubseCliBufferGuard(res_buffer);
    }
    MOCKER(&ubse_api_buffer_free).reset();
    EXPECT_TRUE(BUFFER_FREE_FLAG);
}
} // namespace ubse::ut::cli
