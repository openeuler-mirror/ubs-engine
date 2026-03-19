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

#include "ubse_client_fuzz.h"

#include <mockcpp/mokc.h>
#include <secodeFuzz.h>

#include "test/FUZZ/main_test_fuzz.h"
#include "ubs_engine.h"

namespace ubse::fuzz::client {
using namespace ubse::fuzz;
using namespace ubse::common::def;

int fuzzCount = GetFuzzCount();

void UbseClientFuzz::SetUp()
{
    Test::SetUp();
}
void UbseClientFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(UbseClientFuzz, TestClientInitialize)
{
    DT_FUZZ_START(0, fuzzCount, "TestClientInitialize", 0)
    {
        // 生成不同长度的 uds_path
        char *initValue = "var/run/ubse/ubse.sock";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *udsPathFuzz = DTSetGetString(&g_Element[0], length, maxLen, initValue, "uds服务端路径");
        // 初始化客户端
        ubs_engine_client_initialize(udsPathFuzz);
        // 销毁客户端
        ubs_engine_client_finalize();
    }
    DT_FUZZ_END()
}
} // namespace ubse::fuzz::client