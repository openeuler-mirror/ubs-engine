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

#include "ubse_logger_fuzz.h"

#include <mockcpp/mokc.h>
#include <secodeFuzz.h>

#include "test/FUZZ/main_test_fuzz.h"
#include "ubse_logger.h"

namespace ubse::fuzz::log {
using namespace ubse::log;
using namespace ubse::fuzz;
int g_fuzzCount = GetFuzzCount();

void UbseLoggerFuzz::SetUp()
{
    Test::SetUp();
}

void UbseLoggerFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(UbseLoggerFuzz, TestUbseLogOutput)
{
    DT_FUZZ_START(0, g_fuzzCount, "TestUbseLogOutput", 0)
    {
        char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "conf");
        std::string moduleName = rawModuleName ? std::string(rawModuleName) : "unknown";

        auto levelData = DT_SetGetNumberRange(&g_Element[1], 0, 0, 4);
        uint32_t levelValue = levelData ? *reinterpret_cast<uint32_t *>(levelData) : 0;
        auto level = static_cast<::ubse::log::UbseLogLevel>(levelValue);

        char *rawMsg = DT_SetGetString(&g_Element[2], 8, 10000, "message");
        std::string msg = rawMsg ? std::string(rawMsg) : "default message";

        UbseLogOutput(moduleName.c_str(), level, msg.c_str());
    }
    DT_FUZZ_END()
}
} // namespace ubse::fuzz::log
