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

#include "virt_agent_container_fuzz.h"

#include <mockcpp/mokc.h>
#include <secodeFuzz.h>
#include "securec.h"

#include "mem_container_msg.h"
#include "test/FUZZ/main_test_fuzz.h"
#include "ubs_virt_agent_container.h"

namespace ubse::virt::container {
using namespace ubse::common::def;
using namespace ubse::fuzz;
using namespace vm;

int fuzzCount = GetFuzzCount();
const int8_t S8_MAX = 127;
const int16_t S16_MAX = 32767;
const int32_t S32_MAX = 2147483647;
const int64_t S64_MAX = 9223372036854775807;

const uint16_t U16_MAX = 65535;
const uint32_t U32_MAX = 4294967295;
const uint64_t U64_MAX = 18446744073709551615;

void VirtAgentContainerFuzz::SetUp()
{
    Test::SetUp();
}

void VirtAgentContainerFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(VirtAgentContainerFuzz,
       WaterLineInjectFuzz){DT_FUZZ_START(0, fuzzCount, "WaterLineInjectFuzz", 0){watermark_t param;
param.highWaterMark = *(u16 *)DTSetGetNumberRange(&g_Element[0], 1000, 1000, U16_MAX, "");
param.lowWaterMark = *(u16 *)DTSetGetNumberRange(&g_Element[1], 1, 1, U16_MAX, "");

ubs_container_inject_waterLine(&param);
} // namespace ubse::virt::container
DT_FUZZ_END()
}

TEST_F(VirtAgentContainerFuzz,
       GetContainerPidsFuzz){DT_FUZZ_START(0, fuzzCount, "GetContainerPidsFuzz", 0){container_id_list container_list;
char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "conf");
std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
const std::size_t maxSize = data.size() + 1;
strcpy_s(container_list.containerId[0], maxSize, data.c_str());
container_list.containerIdSize = *(s16 *)DTSetGetNumberRange(&g_Element[1], 1, 0, S16_MAX, "");
container_pid_info *infos = NULL;
uint32_t infoSize = 0;

ubs_container_get_container_pids(&container_list, &infos, &infoSize);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentContainerFuzz,
       ContainerInfoQueryFuzz){DT_FUZZ_START(0, fuzzCount, "ContainerInfoQueryFuzz", 0){pid_param param = {0};
char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
if (rawModuleName) {
    snprintf_s(param.srcNid, sizeof(param.srcNid), sizeof(param.srcNid), "%s", rawModuleName);
} else {
    snprintf_s(param.srcNid, sizeof(param.srcNid), sizeof(param.srcNid), "unknown");
}
param.pids[0] = *(s8 *)DTSetGetNumberRange(&g_Element[1], 1, 0, S8_MAX, "");
param.pids_size = *(s8 *)DTSetGetNumberRange(&g_Element[2], 1, 0, S8_MAX, "");
pid_mem_info *infos;
uint32_t infoSize = 0;

ubs_container_info_query(&param, &infos, &infoSize);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentContainerFuzz, WaterlineMemBorrowFuzz){
    DT_FUZZ_START(0, fuzzCount, "WaterlineMemBorrowFuzz", 0){mem_borrow_request_t mem_borrow_request;
// 获取并设置 srcNid
char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
if (rawModuleName) {
    std::string data(rawModuleName);
    const std::size_t maxSize = sizeof(mem_borrow_request.borrowParam.srcNid);
    strcpy_s(mem_borrow_request.borrowParam.srcNid, maxSize, data.c_str());
} else {
    strcpy_s(mem_borrow_request.borrowParam.srcNid, sizeof(mem_borrow_request.borrowParam.srcNid), "unknown");
}

// 获取并设置 srcLocations
src_location_t src_locations;
src_locations.socketId = *(s16 *)DTSetGetNumberRange(&g_Element[1], 1, 0, S16_MAX, "");
src_locations.numaId = *(s16 *)DTSetGetNumberRange(&g_Element[2], 1, 0, S16_MAX, "");
mem_borrow_request.borrowParam.srcLocations[0] = src_locations;
mem_borrow_request.borrowParam.srcLocationsSize = *(s16 *)DTSetGetNumberRange(&g_Element[3], 1, 0, S16_MAX, "");

// 获取并设置 borrowSizes
mem_borrow_request.borrowSizes[0] = *(u64 *)DTSetGetNumberRange(&g_Element[4], 1, 0, U64_MAX, "");
mem_borrow_request.borrowSizesSize = *(s16 *)DTSetGetNumberRange(&g_Element[5], 1, 0, S16_MAX, "");
watermark_t water_mark;
water_mark.highWaterMark = *(u16 *)DTSetGetNumberRange(&g_Element[6], 1, 0, U16_MAX, "");
water_mark.lowWaterMark = *(u16 *)DTSetGetNumberRange(&g_Element[7], 1, 0, U16_MAX, "");
mem_borrow_request.waterMark = water_mark;

char **borrowIds = nullptr;
uint32_t infoSize = 0;
ubs_virt_agent_waterline_mem_borrow(&mem_borrow_request, &borrowIds, &infoSize);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentContainerFuzz, WaterlineMemMigrateFuzz){
    DT_FUZZ_START(0, fuzzCount, "WaterlineMemMigrateFuzz", 0){mem_migrate_request_t mem_migrate_request;
borrow_param_t borrow_param_test;
char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
const std::size_t maxSize = sizeof(borrow_param_test.srcNid);
strcpy_s(borrow_param_test.srcNid, maxSize, data.c_str());

src_location_t src_locations;
src_locations.socketId = *(s16 *)DTSetGetNumberRange(&g_Element[1], 1, 0, S16_MAX, "");
src_locations.numaId = *(s16 *)DTSetGetNumberRange(&g_Element[2], 1, 0, S16_MAX, "");
borrow_param_test.srcLocations[0] = src_locations;
borrow_param_test.srcLocationsSize = 1;

mem_migrate_request.borrowParam = borrow_param_test;
char *rawModuleName2 = DT_SetGetString(&g_Element[3], 5, 10000, "123");
std::string data2 = rawModuleName2 ? std::string(rawModuleName2) : "unknown";
const std::size_t maxSize2 = SDK_NO_128;
strcpy_s(mem_migrate_request.borrowIds[0], maxSize2, data2.c_str());
mem_migrate_request.borrowIdsSize = 1;

container_param_t container_param;
container_param.pid = *(s16 *)DTSetGetNumberRange(&g_Element[4], 1, 0, S16_MAX, "");
container_param.ratio = *(s16 *)DTSetGetNumberRange(&g_Element[5], 1, 0, S16_MAX, "");
mem_migrate_request.containerParam[0] = container_param;
mem_migrate_request.containerParamSize = 1;

ubs_virt_agent_waterline_mem_migrate(&mem_migrate_request);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentContainerFuzz, WaterlineMemReturnFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "WaterlineMemReturnFuzz", 0)
    {
        return_request_t return_request;
        borrow_param_t borrow_param_test;
        char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
        std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
        const std::size_t maxSize = sizeof(borrow_param_test.srcNid);
        strcpy_s(borrow_param_test.srcNid, maxSize, data.c_str());

        src_location_t src_locations;
        src_locations.socketId = *(s16 *)DTSetGetNumberRange(&g_Element[1], 1, 0, S16_MAX, "");
        src_locations.numaId = *(s16 *)DTSetGetNumberRange(&g_Element[2], 1, 0, S16_MAX, "");
        borrow_param_test.srcLocations[0] = src_locations;
        borrow_param_test.srcLocationsSize = 1;

        return_request.borrowParam = borrow_param_test;
        char *rawModuleName2 = DT_SetGetString(&g_Element[3], 5, 10000, "123");
        std::string data2 = rawModuleName2 ? std::string(rawModuleName2) : "unknown";
        const std::size_t maxSize2 = SDK_NO_128;
        strcpy_s(return_request.borrowIds[0], maxSize2, data2.c_str());
        return_request.borrowIdsSize = 1;
        return_request.pids[0] = *(s32 *)DTSetGetNumberRange(&g_Element[4], 1, 0, S32_MAX, "");
        return_request.pidsSize = 1;

        ubs_virt_agent_waterline_mem_return(&return_request);
    }
    DT_FUZZ_END()
}
} // namespace ubse::virt::container