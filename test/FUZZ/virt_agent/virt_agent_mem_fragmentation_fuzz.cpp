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

#include "virt_agent_mem_fragmentation_fuzz.h"

#include <mockcpp/mokc.h>
#include <secodeFuzz.h>
#include "securec.h"

#include "test/FUZZ/main_test_fuzz.h"
#include "ubs_virt_agent_mem_fragmentation.h"

namespace ubse::virt::fragmentation {
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

void VirtAgentMemFragmentationFuzz::SetUp()
{
    Test::SetUp();
}

void VirtAgentMemFragmentationFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(VirtAgentMemFragmentationFuzz, MemFragmentationNodeInfoFuzz){
    DT_FUZZ_START(0, fuzzCount, "MemFragmentationNodeInfoFuzz", 0){numa_info_t * node_list;
uint32_t node_cnt = 0;

ubs_virt_agent_mem_fragmentation_node_info(&node_list, &node_cnt);
} // namespace ubse::virt::fragmentation
DT_FUZZ_END()
}

TEST_F(VirtAgentMemFragmentationFuzz, MemFragmentationVmInfoFuzz){
    DT_FUZZ_START(0, fuzzCount, "MemFragmentationVmInfoFuzz", 0){vm_domain_info_t * vm_info_list{};
uint32_t vm_info_cnt = 0;

ubs_virt_agent_mem_fragmentation_vm_info(&vm_info_list, &vm_info_cnt);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentMemFragmentationFuzz,
       FragmentationNodeAntiAffinityFuzz){DT_FUZZ_START(0, fuzzCount, "FragmentationNodeAntiAffinityFuzz", 0){
    char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";

char *values[] = {const_cast<char *>(data.c_str())};
const std::size_t maxSize = VIRT_MAX_NODE_ID_LENGTH;

NodeAntiDictionary dict{};
strcpy_s(dict.entries[0].key, maxSize, data.c_str());
strcpy_s(dict.entries[0].value[0], maxSize, data.c_str());
dict.entries[0].value_count = 1;
dict.entry_count = 1;
ubs_virt_agent_mem_fragmentation_node_anti_affinity(&dict);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentMemFragmentationFuzz, MemBorrowStrategyFuzz){
    DT_FUZZ_START(0, fuzzCount, "MemBorrowStrategyFuzz", 0){src_memory_borrow_param borrow_param;

char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
const std::size_t maxSize = data.size() + 1;
strcpy_s(borrow_param.src_nid, maxSize, data.c_str());
borrow_param.src_socket_id = *(s16 *)DTSetGetNumberRange(&g_Element[1], 1, 0, S16_MAX, "");
borrow_param.src_numa_id = *(s16 *)DTSetGetNumberRange(&g_Element[2], 1, 0, S16_MAX, "");
borrow_param.borrow_size = *(u64 *)DTSetGetNumberRange(&g_Element[3], 1, 0, U64_MAX, "");

borrow_strategy_c borrow_strategy;
ubs_virt_agent_mem_borrow_strategy(&borrow_param, &borrow_strategy);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentMemFragmentationFuzz,
       MemBorrowExecuteFuzz){DT_FUZZ_START(0, fuzzCount, "MemBorrowExecuteFuzz", 0){borrow_strategy_c borrow_strategy;
char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
const std::size_t maxSize = VIRT_MAX_NODE_ID_LENGTH;
strcpy_s(borrow_strategy.src_host_id, maxSize, data.c_str());

borrow_strategy.src_socket_id = *(s16 *)DTSetGetNumberRange(&g_Element[1], 1, 0, S16_MAX, "");
borrow_strategy.src_numa_id = *(s16 *)DTSetGetNumberRange(&g_Element[2], 1, 0, S16_MAX, "");
borrow_strategy.borrow_size = *(u64 *)DTSetGetNumberRange(&g_Element[3], 1, 0, U64_MAX, "");

dst_numa_info_c dst_numa_info;
strcpy_s(dst_numa_info.host_id, maxSize, data.c_str());
dst_numa_info.socket_id = *(u16 *)DTSetGetNumberRange(&g_Element[4], 1, 0, U16_MAX, "");
dst_numa_info.numa_nums = *(u16 *)DTSetGetNumberRange(&g_Element[5], 1, 0, U16_MAX, "");
dst_numa_info.numa_ids[0] = *(s16 *)DTSetGetNumberRange(&g_Element[6], 1, 0, S16_MAX, "");
dst_numa_info.mem_sizes[0] = *(u64 *)DTSetGetNumberRange(&g_Element[7], 1, 0, U64_MAX, "");
borrow_strategy.dest_numa_infos[0] = dst_numa_info;
borrow_strategy.dest_numa_infos_size = 1;

char **borrow_ids = nullptr;
uint16_t *present_numa_ids = nullptr;
uint32_t borrow_ids_size = 0;
uint32_t present_numa_ids_size = 0;

ubs_virt_agent_mem_borrow_execute(&borrow_strategy, &borrow_ids, &present_numa_ids, &borrow_ids_size,
                                  &present_numa_ids_size);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentMemFragmentationFuzz, MemMigrateStrategyFuzz){
    DT_FUZZ_START(0, fuzzCount, "MemMigrateStrategyFuzz", 0){MemMigrateStrategySrcParam strategy_src_param;

strategy_src_param.borrowSize = *(u64 *)DTSetGetNumberRange(&g_Element[0], 1, 0, U64_MAX, "");
char *rawModuleName = DT_SetGetString(&g_Element[1], 5, 10000, "123");
std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
const std::size_t maxSize = VIRT_MAX_NODE_ID_LENGTH;
strcpy_s(strategy_src_param.borrowInNode, maxSize, data.c_str());

strategy_src_param.vmInfoListSize = 1;
strategy_src_param.vmInfoList[0].pid = *(s16 *)DTSetGetNumberRange(&g_Element[2], 1, 0, S16_MAX, "");
strategy_src_param.vmInfoList[0].ratio = *(u16 *)DTSetGetNumberRange(&g_Element[3], 1, 0, U16_MAX, "");

MemMigrateStrategy strategy{};
ubs_virt_agent_mem_migrate_strategy(&strategy_src_param, &strategy);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentMemFragmentationFuzz, MemMigrateExecuteFuzz){
    DT_FUZZ_START(0, fuzzCount, "MemMigrateExecuteFuzz", 0){MemMigrateExecuteSrcParam param = {0};

char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
std::string data = rawModuleName ? std::string(rawModuleName) : "unknown";
strcpy_s(param.borrowInNode, VIRT_MAX_NODE_ID_LENGTH, data.c_str());
param.borrowIdsCount = 2;
strcpy_s(param.borrowIds[0], MAX_BORROW_ID_LENGTH, data.c_str());
strcpy_s(param.borrowIds[1], MAX_BORROW_ID_LENGTH, data.c_str());

param.vmInfoListSize = 2;
param.vmInfoList[0].destNumaId = *(u16 *)DTSetGetNumberRange(&g_Element[1], 1, 0, U16_MAX, "");
param.vmInfoList[0].memSize = *(u64 *)DTSetGetNumberRange(&g_Element[2], 1, 0, U64_MAX, "");
param.vmInfoList[0].pid = *(u16 *)DTSetGetNumberRange(&g_Element[3], 1, 0, U16_MAX, "");
param.vmInfoList[1].destNumaId = *(u16 *)DTSetGetNumberRange(&g_Element[4], 1, 0, U16_MAX, "");
param.vmInfoList[1].memSize = *(u64 *)DTSetGetNumberRange(&g_Element[5], 1, 0, U64_MAX, "");
param.vmInfoList[1].pid = *(u16 *)DTSetGetNumberRange(&g_Element[6], 1, 0, U16_MAX, "");
param.waitingTime = *(u64 *)DTSetGetNumberRange(&g_Element[7], 1, 0, U64_MAX, "");

ubs_virt_agent_mem_migrate_execute(&param);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentMemFragmentationFuzz, AgentMemReturnFuzz){DT_FUZZ_START(0, fuzzCount, "AgentMemReturnFuzz", 0){
    const char *node_id = DT_SetGetString(&g_Element[0], 5, 10000, "123");

ubs_virt_agent_mem_return(node_id);
}
DT_FUZZ_END()
}

TEST_F(VirtAgentMemFragmentationFuzz, AgentMemRollbackFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "AgentMemRollbackFuzz", 0)
    {
        char *rawModuleName = DT_SetGetString(&g_Element[0], 5, 10000, "123");
        std::string data = rawModuleName ? std::string(rawModuleName) : "default";
        const std::size_t maxSize = data.size() + 1;
        RollbackSrcParam srcParam{};
        strcpy_s(srcParam.node_id, maxSize, data.c_str());
        strcpy_s(srcParam.borrow_id_list[0], maxSize, data.c_str());
        strcpy_s(srcParam.borrow_id_list[1], maxSize, data.c_str());
        srcParam.borrow_id_size = 2;
        ubs_virt_agent_mem_rollback(&srcParam);
    }
    DT_FUZZ_END()
}

} // namespace ubse::virt::fragmentation