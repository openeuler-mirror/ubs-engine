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

#include "virt_agent_case_conf_fuzz.h"

#include <mockcpp/mokc.h>
#include <secodeFuzz.h>
#include "securec.h"

#include "case_conf_msg.h"
#include "test/FUZZ/main_test_fuzz.h"
#include "ubs_virt_agent_case_conf.h"

namespace ubse::virt::caseconf {
using namespace ubse::common::def;
using namespace ubse::fuzz;
using namespace vm;

int fuzzCount = GetFuzzCount();
const int16_t S16_MAX = 32767;
const int32_t S32_MAX = 2147483647;
const int64_t S64_MAX = 9223372036854775807;

const uint16_t U16_MAX = 65535;
const uint32_t U32_MAX = 4294967295;
const uint64_t U64_MAX = 18446744073709551615;

void VirtAgentCaseConfFuzz::SetUp()
{
    Test::SetUp();
}

void VirtAgentCaseConfFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(VirtAgentCaseConfFuzz,
       CaseConfGetFuzz){DT_FUZZ_START(0, fuzzCount, "CaseConfGetFuzz", 0){case_conf_info_t case_conf_info;
// 获取并设置 cur_case
char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
if (rawModuleName) {
    std::string data(rawModuleName);
    const std::size_t maxSize = sizeof(case_conf_info.cur_case);
    strcpy_s(case_conf_info.cur_case, maxSize, data.c_str());
} else {
    strcpy_s(case_conf_info.cur_case, sizeof(case_conf_info.cur_case), "unknown");
}

// 获取并设置 over_commitment_ratio
char *rawModuleName2 = DT_SetGetString(&g_Element[1], 5, 10000, "123");
if (rawModuleName2) {
    std::string data2(rawModuleName2);
    const std::size_t maxSize2 = sizeof(case_conf_info.over_commitment_ratio);
    strcpy_s(case_conf_info.over_commitment_ratio, maxSize2, data2.c_str());
} else {
    strcpy_s(case_conf_info.over_commitment_ratio, sizeof(case_conf_info.over_commitment_ratio), "unknown");
}

// 获取并设置 migrate_waterLine
char *rawModuleName3 = DT_SetGetString(&g_Element[2], 5, 10000, "123");
if (rawModuleName3) {
    std::string data3(rawModuleName3);
    const std::size_t maxSize3 = sizeof(case_conf_info.migrate_waterLine);
    strcpy_s(case_conf_info.migrate_waterLine, maxSize3, data3.c_str());
} else {
    strcpy_s(case_conf_info.migrate_waterLine, sizeof(case_conf_info.migrate_waterLine), "unknown");
}

// 获取并设置 index
case_conf_info.index = *(u64 *)DTSetGetNumberRange(&g_Element[1], 1, 0, U64_MAX, "");

// 获取并设置 host_id
char *rawModuleName4 = DT_SetGetString(&g_Element[3], 5, 10000, "123");
if (rawModuleName4) {
    std::string data4(rawModuleName4);
    const std::size_t maxSize4 = sizeof(case_conf_info.host_id);
    strcpy_s(case_conf_info.host_id, maxSize4, data4.c_str());
} else {
    strcpy_s(case_conf_info.host_id, sizeof(case_conf_info.host_id), "unknown");
}

ubs_virt_agent_case_conf_get(&case_conf_info);
} // namespace ubse::virt::caseconf
DT_FUZZ_END()
}

TEST_F(VirtAgentCaseConfFuzz, CaseConfSetFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "CaseConfSetFuzz", 0)
    {
        char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 100, "123");
        std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
        const std::size_t maxSize = data.size() + 1;

        std::vector<char> param(maxSize);
        strncpy_s(param.data(), maxSize, data.c_str(), data.size());

        case_conf_set_info_t case_conf_set_info{0};

        ubs_virt_agent_case_conf_set(param.data(), &case_conf_set_info);
    }
    DT_FUZZ_END()
}

} // namespace ubse::virt::caseconf