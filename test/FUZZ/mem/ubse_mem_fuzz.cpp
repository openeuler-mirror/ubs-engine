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

#include "ubse_mem_fuzz.h"

#include <mockcpp/mokc.h>
#include <secodeFuzz.h>
#include "securec.h"

#include "test/FUZZ/main_test_fuzz.h"
#include "ubs_engine_mem.h"

namespace ubse::fuzz::mem {
using namespace ubse::common::def;
using namespace ubse::fuzz;

int fuzzCount = GetFuzzCount();

void UbseMemFuzz::SetUp()
{
    Test::SetUp();
}

void UbseMemFuzz::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(UbseMemFuzz, MemFdCreateFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemFdCreateFuzz", 0)
    {
        char *initValue = "testName";  // 生成随机的借用标识
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        // 生成随机的借用大小
        uint64_t size = *(s64 *)DTSetGetNumberRange(&g_Element[1], 128 * 1024 * 1024, 0, 256 * 1024 * 1024 * 1024, "");
        ubs_mem_fd_owner_t owner{};
        ubs_mem_fd_desc_t fd_desc = {};  // 结果存储结构体
        // 生成随机的使用方进程信息
        owner.uid = *(s32 *)DTSetGetNumberRange(&g_Element[2], 1000, 1, 65535, "");
        owner.gid = *(s32 *)DTSetGetNumberRange(&g_Element[3], 1000, 1, 65535, "");
        owner.pid = *(s32 *)DTSetGetNumberRange(&g_Element[4], 1000, 1, 65535, "");
        // 生成随机的内存访问距离
        ubs_mem_distance_t distance =
            static_cast<ubs_mem_distance_t>(*(s32 *)DTSetGetNumberRange(&g_Element[5], 0, 0, 100, ""));
        mode_t mode = static_cast<mode_t>(*(s32 *)DTSetGetNumberRange(&g_Element[6], 0, 0, 0xFFFFFFFF, ""));
        // 调用创建函数
        ubs_mem_fd_create(fuzzName, size, &owner, mode, distance, &fd_desc);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemFdCreateWithLenderFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemFdCreateWithLenderFuzz", 0)
    {  // 生成随机的借用标识
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;

        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_fd_owner_t owner{};

        // 生成随机的使用方进程信息
        owner.uid = *(s32 *)DTSetGetNumberRange(&g_Element[1], 1000, 1, 65535, "");
        owner.gid = *(s32 *)DTSetGetNumberRange(&g_Element[2], 1000, 1, 65535, "");
        owner.pid = *(s32 *)DTSetGetNumberRange(&g_Element[3], 1000, 1, 65535, "");
        // 生成随机的内存访问距离
        ubs_mem_distance_t distance =
            static_cast<ubs_mem_distance_t>(*(s32 *)DTSetGetNumberRange(&g_Element[4], 0, 0, 100, ""));

        mode_t mode = static_cast<mode_t>(*(s32 *)DTSetGetNumberRange(&g_Element[5], 0, 0, 0xFFFFFFFF, ""));
        ubs_mem_lender_t lender;
        lender.slot_id = *(s32 *)DTSetGetNumberRange(&g_Element[6], 1000, 1, 65535, "");
        lender.socket_id = *(s32 *)DTSetGetNumberRange(&g_Element[7], 1000, 1, 65535, "");
        lender.lender_size = *(s32 *)DTSetGetNumberRange(&g_Element[8], 1000, 1, 1000000000000, "");
        // 调用创建函数
        ubs_mem_fd_desc_t fd_desc = {};  // 结果存储结构体
        ubs_mem_fd_create_with_lender(fuzzName, &owner, mode, &lender, distance, &fd_desc);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemFdCreateWithCandidateFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemFdCreateWithCandidateFuzz", 0)
    {  // 生成随机的借用标识
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;

        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_fd_owner_t owner{};

        // 生成随机的使用方进程信息
        owner.uid = *(s32 *)DTSetGetNumberRange(&g_Element[1], 1000, 1, 65535, "");
        owner.gid = *(s32 *)DTSetGetNumberRange(&g_Element[2], 1000, 1, 65535, "");
        owner.pid = *(s32 *)DTSetGetNumberRange(&g_Element[3], 1000, 1, 65535, "");
        // 生成随机的内存访问距离
        mode_t mode = static_cast<mode_t>(*(s32 *)DTSetGetNumberRange(&g_Element[4], 0, 0, 0xFFFFFFFF, ""));

        uint32_t slot_cnt = *(s32 *)DTSetGetNumberRange(&g_Element[5], 1000, 1, 400, "");
        // 生成随机的借用大小
        uint64_t size = *(s64 *)DTSetGetNumberRange(&g_Element[6], 128 * 1024 * 1024, 0, 256 * 1024 * 1024 * 1024, "");
        uint32_t slot_ids[slot_cnt];
        for (int i = 0; i < slot_cnt; i++) {
            slot_ids[i] = *(s32 *)DTSetGetNumberRange(&g_Element[7], 1000, 1, 400, "");
        }
        // 调用创建函数
        ubs_mem_fd_desc_t fd_desc = {};
        ubs_mem_fd_create_with_candidate(fuzzName, size, &owner, mode, slot_ids, slot_cnt, &fd_desc);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemFdPermissionFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemFdPermissionFuzz", 0)
    {  // 生成随机的借用标识
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_fd_owner_t owner{};
        // 生成随机的使用方进程信息
        owner.uid = *(s32 *)DTSetGetNumberRange(&g_Element[1], 1000, 1, 65535, "");
        owner.gid = *(s32 *)DTSetGetNumberRange(&g_Element[2], 1000, 1, 65535, "");
        owner.pid = *(s32 *)DTSetGetNumberRange(&g_Element[3], 1000, 1, 65535, "");
        mode_t mode = static_cast<mode_t>(*(s32 *)DTSetGetNumberRange(&g_Element[4], 0, 0, 0xFFFFFFFF, ""));
        ubs_mem_fd_permission(fuzzName, &owner, mode);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemFdGetFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemFdGetFuzz", 0)
    {  // 生成随机的借用标识
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_fd_desc_t fdDesc;
        ubs_mem_fd_get(fuzzName, &fdDesc);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemFdDeleteFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemFdDeleteFuzz", 0)
    {  // 生成随机的借用标识
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_fd_delete(fuzzName);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemNumaCreateFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemNumaCreateFuzz", 0)
    {
        char *initValue = "testName";  // 生成随机的借用标识
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        // 生成随机的借用大小
        uint64_t size = *(s64 *)DTSetGetNumberRange(&g_Element[1], 128 * 1024 * 1024, 0, 256 * 1024 * 1024 * 1024, "");
        ubs_mem_numa_desc_t numa_desc = {};  // 结果存储结构体
        // 生成随机的内存访问距离
        ubs_mem_distance_t distance =
            static_cast<ubs_mem_distance_t>(*(s32 *)DTSetGetNumberRange(&g_Element[2], 0, 0, 100, ""));
        // 调用创建函数
        ubs_mem_numa_create(fuzzName, size, distance, &numa_desc);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemNumaCreateWithLenderFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemNumaCreateWithLenderFuzz", 0)
    {
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        uint32_t lender_cnt = *(s64 *)DTSetGetNumberRange(&g_Element[1], 1000, 1, 400, "");
        ubs_mem_lender_t lenders[lender_cnt];
        for (int i = 0; i < lender_cnt; i++) {
            ubs_mem_lender_t lender;
            lender.slot_id = *(s32 *)DTSetGetNumberRange(&g_Element[2], 1000, 1, 65535, "");
            lender.socket_id = *(s32 *)DTSetGetNumberRange(&g_Element[3], 1000, 1, 65535, "");
            lender.lender_size = *(s32 *)DTSetGetNumberRange(&g_Element[4], 1000, 1, 1000000000000, "");
        }
        ubs_mem_numa_desc_t numa_desc = {};  // 结果存储结构体
        ubs_mem_numa_create_with_lender(fuzzName, lenders, lender_cnt, &numa_desc);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemNumaCreateWithCandidateFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemNumaCreateWithCandidateFuzz", 0)
    {  // 生成随机的借用标识
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        uint32_t slot_cnt = *(s32 *)DTSetGetNumberRange(&g_Element[1], 1000, 1, 400, "");
        // 生成随机的借用大小
        uint64_t size = *(s64 *)DTSetGetNumberRange(&g_Element[2], 128 * 1024 * 1024, 0, 256 * 1024 * 1024 * 1024, "");
        uint32_t slot_ids[slot_cnt];
        for (int i = 0; i < slot_cnt; i++) {
            slot_ids[i] = *(s32 *)DTSetGetNumberRange(&g_Element[3], 1000, 1, 400, "");
        }
        // 调用创建函数
        ubs_mem_numa_desc_t numa_desc = {};
        ubs_mem_numa_create_with_candidate(fuzzName, size, slot_ids, slot_cnt, &numa_desc);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemNumaGetFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemNumaGetFuzz", 0)
    {  // 生成随机的借用标识
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_numa_desc_t numaDesc;
        ubs_mem_numa_get(fuzzName, &numaDesc);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemNumaDeleteFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemNumaDeleteFuzz", 0)
    {  // 生成随机的借用标识
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_numa_delete(fuzzName);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemShmCreateFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemShmCreateFuzz", 0)
    {
        char *initValue = "testName";  // 生成随机的借用标识
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        // 生成随机的借用大小
        uint64_t size = *(s64 *)DTSetGetNumberRange(&g_Element[1], 128 * 1024 * 1024, 0, 256 * 1024 * 1024 * 1024, "");
        uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        uint64_t flag = *(s64 *)DTSetGetNumberRange(&g_Element[2], 1, 0, 128, "");
        ubs_mem_nodes_t region;
        region.node_cnt = *(s64 *)DTSetGetNumberRange(&g_Element[3], 1, 0, UBS_MEM_MAX_SLOT_NUM, "");
        for (int i = 0; i < region.node_cnt; i++) {
            region.slot_ids[i] =
                *(s64 *)DTSetGetNumberRange(&g_Element[4], 1, 0, 500, "假设当前是384集群,500已经超过了384");
        }
        ubs_mem_nodes_t provider;
        provider.node_cnt = *(s64 *)DTSetGetNumberRange(&g_Element[5], 1, 0, UBS_MEM_MAX_SLOT_NUM, "");
        for (int i = 0; i < region.node_cnt; i++) {
            provider.slot_ids[i] =
                *(s64 *)DTSetGetNumberRange(&g_Element[6], 1, 0, 500, "假设当前是384集群,500已经超过了384");
        }
        ubs_mem_shm_create(fuzzName, size, usr_info, flag, &region, &provider);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemShmCreateWithAffinityFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemShmCreateWithAffinityFuzz", 0)
    {
        char *initValue = "testName";  // 生成随机的借用标识
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        // 生成随机的借用大小
        uint64_t size = *(s64 *)DTSetGetNumberRange(&g_Element[1], 128 * 1024 * 1024, 0, 256 * 1024 * 1024 * 1024, "");
        uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        uint64_t flag = *(s64 *)DTSetGetNumberRange(&g_Element[2], 1, 0, 128, "");
        ubs_mem_nodes_t region;
        region.node_cnt = *(s64 *)DTSetGetNumberRange(&g_Element[3], 1, 0, UBS_MEM_MAX_SLOT_NUM, "");
        for (int i = 0; i < region.node_cnt; i++) {
            region.slot_ids[i] =
                *(s64 *)DTSetGetNumberRange(&g_Element[4], 1, 0, 500, "假设当前是384集群,500已经超过了384");
        }
        ubs_mem_nodes_t provider;
        provider.node_cnt = *(s64 *)DTSetGetNumberRange(&g_Element[5], 1, 0, UBS_MEM_MAX_SLOT_NUM, "");
        for (int i = 0; i < region.node_cnt; i++) {
            provider.slot_ids[i] =
                *(s64 *)DTSetGetNumberRange(&g_Element[6], 1, 0, 500, "假设当前是384集群,500已经超过了384");
        }
        uint32_t socketId = *(s32 *)DTSetGetNumberRange(&g_Element[5], 1, 0, 1000, "用户随便输入了1000个CPU");
        ubs_mem_shm_create_with_affinity(fuzzName, size, socketId, usr_info, flag, &region, &provider);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemShmAttachFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemShmAttachFuzz", 0)
    {
        char *initValue = "testName";  // 生成随机的借用标识
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        // 生成随机的借用大小
        ubs_mem_fd_owner_t owner{};
        // 生成随机的使用方进程信息
        owner.uid = *(s32 *)DTSetGetNumberRange(&g_Element[1], 1000, 1, 65535, "");
        owner.gid = *(s32 *)DTSetGetNumberRange(&g_Element[2], 1000, 1, 65535, "");
        owner.pid = *(s32 *)DTSetGetNumberRange(&g_Element[3], 1000, 1, 65535, "");
        mode_t mode = static_cast<mode_t>(*(s32 *)DTSetGetNumberRange(&g_Element[4], 0, 0, 0xFFFFFFFF, ""));
        ubs_mem_shm_desc_t *shm_descs = NULL;
        ubs_mem_shm_attach(fuzzName, &owner, mode, &shm_descs);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemShmGetFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemShmGetFuzz", 0)
    {
        char *initValue = "testName";  // 生成随机的借用标识
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_shm_desc_t *shm_descs = NULL;
        ubs_mem_shm_get(fuzzName, &shm_descs);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemShmListWithPrefixFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemShmGetFuzz", 0)
    {
        char *initValue = "testName";  // 生成随机的借用标识
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_shm_desc_t *shm_descs = NULL;
        uint32_t cnt;
        ubs_mem_shm_list_with_prefix(fuzzName, &shm_descs, &cnt);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemShmDetachFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemShmGetFuzz", 0)
    {
        char *initValue = "testName";  // 生成随机的借用标识
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_shm_detach(fuzzName);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemShmDeleteFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemShmDeleteFuzz", 0)
    {
        char *initValue = "testName";
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_shm_delete(fuzzName);
    }
    DT_FUZZ_END()
}

TEST_F(UbseMemFuzz, MemShmFaultGetFuzz)
{
    DT_FUZZ_START(0, fuzzCount, "MemShmFaultGetFuzz", 0)
    {
        char *initValue = "testName";  // 生成随机的借用标识
        int length = strlen(initValue) + 1;
        int maxLen = 1000;
        char *fuzzName = DTSetGetString(&g_Element[0], length, maxLen, initValue, "");
        ubs_mem_memids_fault_t *fault = NULL;
        ubs_mem_shm_fault_get(fuzzName, fault);
    }
    DT_FUZZ_END()
}
}  // namespace ubse::fuzz::mem