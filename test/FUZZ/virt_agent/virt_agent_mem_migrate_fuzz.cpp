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

#include "virt_agent_mem_migrate_fuzz.h"

#include <mockcpp/mokc.h>
#include <secodeFuzz.h>
#include "securec.h"

#include "test/FUZZ/main_test_fuzz.h"
#include "ubs_virt_agent_mem_migrate.h"

namespace ubse::virt::migrate {
using namespace ubse::common::def;
using namespace ubse::fuzz;

int fuzzCount = GetFuzzCount();
const int16_t S16_MAX = 32767;
const int32_t S32_MAX = 2147483647;
const int64_t S64_MAX = 9223372036854775807;

const uint16_t U16_MAX = 65535;
const uint32_t U32_MAX = 4294967295;
const uint64_t U64_MAX = 18446744073709551615;

void VirtAgentMemMigrateFuzz::SetUp()
{
    Test::SetUp();
}

void VirtAgentMemMigrateFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(VirtAgentMemMigrateFuzz, UpdatePageFlowAndStatusFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "UpdatePageFlowAndStatusFuzz", 0)
    {
        char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
        std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
        const std::size_t maxSize = data.size() + 1;
        std::vector<char> param(maxSize);
        strncpy_s(param.data(), maxSize, data.c_str(), data.size());

        char *rawModuleName2 = DT_SetGetString(&g_Element[1], 5, 10000, "123");
        std::string data2 = rawModuleName2 ? std::string(rawModuleName2) : "unknown";
        const std::size_t maxSize2 = data2.size() + 1;
        std::vector<char> uuid(maxSize2);
        strncpy_s(uuid.data(), maxSize2, data2.c_str(), data2.size());

        update_page_flow_and_status(param.data(), uuid.data());
    }
    DT_FUZZ_END()
}

} // namespace ubse::virt::migrate
