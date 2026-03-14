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

#include "ubse_conf_fuzz.h"

#include <mockcpp/mokc.h>
#include <secodeFuzz.h>

#include "test/FUZZ/main_test_fuzz.h"
#include "ubse_conf.h"
#include "ubse_conf_common_def.h"

namespace ubse::fuzz::config {
using namespace ubse::config;
using namespace ubse::fuzz;

int g_fuzzCount = GetFuzzCount();

void UbseConfFuzz::SetUp()
{
    Test::SetUp();
}

void UbseConfFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(UbseConfFuzz, TestUbseGetUInt)
{
    printf("");
    DT_FUZZ_START(0, g_fuzzCount, "TestUbseGetUInt", 0)
    {
        char *sectionFuzz = DT_SetGetString(&g_Element[0], 9, CONFIG_MAX_LINES + 1, "ubse.log");

        char *keyFUzz = DT_SetGetString(&g_Element[1], 17, CONFIG_MAX_LINES + 1, "log.max.fileSize");

        uint32_t valueFuzz = *reinterpret_cast<uint32_t *>(DT_SetGetU32(&g_Element[2], 20));

        UbseGetUInt(sectionFuzz, keyFUzz, valueFuzz);
    } // namespace ubse::fuzz::config
    DT_FUZZ_END()
}

TEST_F(UbseConfFuzz, TestUbseGetFloat)
{
    printf("");
    DT_FUZZ_START(0, g_fuzzCount, "TestUbseGetFloat", 0)
    {
        char *sectionFuzz = DT_SetGetString(&g_Element[0], 9, CONFIG_MAX_LINES + 1, "ubse.log");

        char *keyFUzz = DT_SetGetString(&g_Element[1], 17, CONFIG_MAX_LINES + 1, "log.max.fileSize");

        float valueFuzz = *reinterpret_cast<float *>(DT_SetGetU32(&g_Element[2], 20));

        UbseGetFloat(sectionFuzz, keyFUzz, valueFuzz);
    } // namespace ubse::fuzz::config
    DT_FUZZ_END()
}

TEST_F(UbseConfFuzz, TestUbseGetStr)
{
    printf("");
    DT_FUZZ_START(0, g_fuzzCount, "TestUbseGetStr", 0)
    {
        char *sectionFuzz = DT_SetGetString(&g_Element[0], 9, CONFIG_MAX_LINES + 1, "ubse.log");

        char *keyFUzz = DT_SetGetString(&g_Element[1], 10, CONFIG_MAX_LINES + 1, "log.level");

        char *valueFuzz = DT_SetGetString(&g_Element[2], 5, CONFIG_MAX_LINES + 1, "INFO");
        std::string valueStr = valueFuzz;

        UbseGetStr(sectionFuzz, keyFUzz, valueStr);
    } // namespace ubse::fuzz::config
    DT_FUZZ_END()
}

TEST_F(UbseConfFuzz, TestUbseGetBool)
{
    printf("");
    DT_FUZZ_START(0, g_fuzzCount, "TestUbseGetBool", 0)
    {
        char *sectionFuzz = DT_SetGetString(&g_Element[0], 9, CONFIG_MAX_LINES + 1, "ubse.log");

        char *keyFUzz = DT_SetGetString(&g_Element[1], 17, CONFIG_MAX_LINES + 1, "log.max.fileSize");

        bool valueFuzz = *reinterpret_cast<uint32_t *>(DT_SetGetU32(&g_Element[2], 20)) % 2 == 0;

        UbseGetBool(sectionFuzz, keyFUzz, valueFuzz);
    }
    DT_FUZZ_END()
}
} // namespace ubse::fuzz::config
