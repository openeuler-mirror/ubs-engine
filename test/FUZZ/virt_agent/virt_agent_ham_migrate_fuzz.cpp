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

#include "virt_agent_ham_migrate_fuzz.h"

#include <mockcpp/mokc.h>
#include <secodeFuzz.h>
#include "securec.h"

#include "test/FUZZ/main_test_fuzz.h"
#include "ubs_virt_agent_ham_migrate.h"

namespace ubse::virt::hammigrate {
using namespace ubse::common::def;
using namespace ubse::fuzz;


int fuzzCount = GetFuzzCount();
const int8_t S8_MAX = 127;
const int16_t S16_MAX = 32767;
const int32_t S32_MAX = 2147483647;
const int64_t S64_MAX = 9223372036854775807;

const uint16_t U8_MAX = 255;
const uint16_t U16_MAX = 65535;
const uint32_t U32_MAX = 4294967295;
const uint64_t U64_MAX = 18446744073709551615;

void VirtAgentHamMigrateFuzz::SetUp()
{
    Test::SetUp();
}

void VirtAgentHamMigrateFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(VirtAgentHamMigrateFuzz, MakeMigrateDecisionFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MakeMigrateDecisionFuzz", 0)
    {
        uint32_t infoSize = *(u32 *)DTSetGetNumberRange(&g_Element[0], 1, 0, U32_MAX, "");
        char *rawModuleName = DT_SetGetString(&g_Element[1], 5, 10000, "123");
        std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
        const std::size_t maxSize = data.size() + 1;
        std::vector<char> param(maxSize);
        strncpy_s(param.data(), maxSize, data.c_str(), data.size());

        char *rawModuleName2 = DT_SetGetString(&g_Element[2], 5, 10000, "123");
        std::string data2 = rawModuleName ? std::string(rawModuleName2) : "unknown";
        const std::size_t maxSize2 = data2.size() + 1;
        std::vector<char> uuid(maxSize2);
        strncpy_s(uuid.data(), maxSize2, data2.c_str(), data2.size());

        uint32_t destNumaId = *(u32 *)DTSetGetNumberRange(&g_Element[3], 1, 0, U32_MAX, "");
        uint32_t migrateStrategy = *(u32 *)DTSetGetNumberRange(&g_Element[4], 1, 0, U32_MAX, "");

        ubs_virt_agent_make_migrate_decision(infoSize, uuid.data(), param.data(), destNumaId, &migrateStrategy);
    }
    DT_FUZZ_END()
}

TEST_F(VirtAgentHamMigrateFuzz, StartIpcClientFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "StartIpcClientFuzz", 0)
    {
        uint16_t timeout = *(u16 *)DTSetGetNumberRange(&g_Element[0], 1, 0, U16_MAX, "");

        ubs_virt_agent_set_timeout(timeout, 1);
    }
    DT_FUZZ_END()
}

TEST_F(VirtAgentHamMigrateFuzz, SyncSendForHamFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "SyncSendForHamFuzz", 0)
    {
        VirtAgentByteBuffer param;
        char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
        std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
        param.len = data.size();
        param.data = new uint8_t[data.size()];
        memcpy_s(param.data, data.size(), data.c_str(), data.size());

        VirtAgentByteBuffer result = {nullptr, 0};

        ubs_sync_send_msg(&param, &result, 1);

        if (param.data) {
            delete[] param.data;
            param.data = nullptr;
            param.len = 0;
        }

        if (result.data) {
            free(result.data);
            result.data = nullptr;
            result.len = 0;
        }
    }
    DT_FUZZ_END()
}

TEST_F(VirtAgentHamMigrateFuzz, AsyncSendForHamFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "ASyncSendForHamFuzz", 0)
    {
        VirtAgentByteBuffer param;
        char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
        std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
        const std::size_t maxSize = data.size() + 1;
        param.len = data.size() + 1;
        param.data = (uint8_t *)malloc(maxSize);
        strncpy_s((char *)param.data, maxSize, data.c_str(), maxSize);
        VirtAgentCallbackDef result;

        ubs_async_send_msg(&param, &result, 1);

        if (param.data) {
            free(param.data);
            param.data = nullptr;
            param.len = 0;
        }
    }
    DT_FUZZ_END()
}

}  // namespace ubse::virt::hammigrate
