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

#include "virt_agent_log_fuzz.h"
#include <mockcpp/mokc.h>
#include <secodeFuzz.h>
#include "securec.h"

#include "test/FUZZ/main_test_fuzz.h"
#include "ubs_virt_agent_log.h"

namespace ubse::virt::log {
using namespace ubse::common::def;
using namespace ubse::fuzz;


int fuzzCount = GetFuzzCount();
const int16_t S16_MAX = 32767;
const int32_t S32_MAX = 2147483647;
const int64_t S64_MAX = 9223372036854775807;

const uint16_t U16_MAX = 65535;
const uint32_t U32_MAX = 4294967295;
const uint64_t U64_MAX = 18446744073709551615;

void VirtAgentLogFuzz::SetUp()
{
    Test::SetUp();
}

void VirtAgentLogFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

void test_log_handler(uint32_t level, const char *message)
{
    // 处理日志消息
    switch (level) {
        case 1:
            std::cout << "DEBUG: " << message << std::endl;
            break;
        case 2:
            std::cout << "INFO: " << message << std::endl;
            break;
        case 3:
            std::cout << "WARNING: " << message << std::endl;
            break;
        case 4:
            std::cout << "ERROR: " << message << std::endl;
            break;
        default:
            std::cout << "UNKNOWN: " << message << std::endl;
            break;
    }
}

TEST_F(VirtAgentLogFuzz, LogCallBackRegisterFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "LogCallBackRegisterFuzz", 0)
    {
        ubs_virt_agent_log_callback_register(test_log_handler);
    }
    DT_FUZZ_END()
}

}  // namespace ubse::virt::log
