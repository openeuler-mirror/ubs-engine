/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * UbsEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
  *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // getuid, getgid, getpid
#include "ubs_engine_mem.h"
static const char *g_shm_name = "demo_shm";
static const uint64_t g_shm_size = 4 * 1024 * 1024; // 4MB
static ubs_mem_fd_owner_t g_owner;
static ubs_mem_shm_desc_t g_shm_desc;
static void ubs_mem_share_create_example(void)
{
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region;
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;
    ubs_mem_nodes_t *provider = NULL;
    printf("=== ubs_mem_share_create_example ===\n");
    int ret = ubs_mem_shm_create(g_shm_name, g_shm_size, usr_info, 0, &region, provider);
    if (ret != 0) {
        printf("ubs_mem_shm_create failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_create success, name=%s\n", g_shm_name);
    }
}

static void ubs_mem_share_create_with_affinity_example(void)
{
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region;
    region.node_cnt = 4;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;
    region.slot_ids[2] = 3;
    region.slot_ids[3] = 4;

    ubs_mem_nodes_t provider;
    provider.node_cnt = 1;
    provider.slot_ids[0] = 2;
    printf("=== ubs_mem_share_create_example ===\n");
    uint32_t affinity_socket_id = 216; // 这个需要查询CPU拓扑,找到app所在的cpu亲和的socket-id
    printf("socket-id=%d\n", affinity_socket_id);
    int ret =
        ubs_mem_shm_create_with_affinity(g_shm_name, g_shm_size, affinity_socket_id, usr_info, 0, &region, &provider);
    if (ret != 0) {
        printf("ubs_mem_shm_create_with_affinity failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_create_with_affinity success, name=%s\n", g_shm_name);
    }
}

static void ubs_mem_share_create_with_lender_example(void)
{
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region;
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;
    ubs_mem_lender_t lender;
    lender.slot_id = 2;
    lender.numa_id = 1;
    lender.socket_id = UINT32_MAX;
    lender.port_id = UINT32_MAX;
    lender.lender_size = 128 * 1024 * 1024;

    printf("=== ubs_mem_share_create_with_lender_example ===\n");
    int ret = ubs_mem_shm_create_with_lender(g_shm_name, usr_info, 0, &region, &lender);
    if (ret != 0) {
        printf("ubs_mem_shm_create_with_lender failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_create_with_lender success, name=%s\n", g_shm_name);
    }
}

static void ubs_mem_share_attach_example(void)
{
    g_owner.uid = getuid();
    g_owner.gid = getgid();
    g_owner.pid = getpid();
    ubs_mem_shm_desc_t *shm_descs = NULL;

    printf("=== ubs_mem_share_attach_example ===\n");
    int ret = ubs_mem_shm_attach(g_shm_name, &g_owner, 0666, &shm_descs);
    if (ret != 0) {
        printf("ubs_mem_shm_attach failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_attach success, name=%s\n", g_shm_name);
    }
}

static void ubs_mem_share_detach_example(void)
{
    printf("=== ubs_mem_share_detach_example ===\n");
    int ret = ubs_mem_shm_detach(g_shm_name);
    if (ret != 0) {
        printf("ubs_mem_shm_detach failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_detach success, name=%s\n", g_shm_name);
    }
}

static void ubs_mem_share_delete_example(void)
{
    printf("=== ubs_mem_share_delete_example ===\n");
    int ret = ubs_mem_shm_delete(g_shm_name);
    if (ret != 0) {
        printf("ubs_mem_shm_delete failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_delete success, name=%s\n", g_shm_name);
    }
}
static void ubs_mem_share_get_example(void)
{
    ubs_mem_shm_desc_t *shm_descs = NULL;
    printf("=== ubs_mem_share_get_example ===\n");
    const int ret = ubs_mem_shm_get("shm_mem", &shm_descs);
    if (ret != 0) {
        printf("ubs_mem_shm_get failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_get success\n");
        printf("name=%s, size=%lu, import_desc_cnt=%u\n", shm_descs->name, shm_descs->mem_size,
               shm_descs->import_desc_cnt);
        for (int i = 0; i < shm_descs->import_desc_cnt; i++) {
            printf("[%u] shm_descs=%u\n", i, shm_descs->import_desc[i].memid_cnt);
            printf("memid: ");
            for (int j = 0; j < shm_descs->import_desc[i].memid_cnt; j++) {
                printf("%lu ", shm_descs->import_desc[i].memids[j]);
            }
            printf("\n");
            printf("importNode: slot_id: %u, host_name: %s, socket_id: %p",
                   shm_descs->import_desc[i].import_node.slot_id, shm_descs->import_desc[i].import_node.host_name,
                   shm_descs->import_desc[i].import_node.socket_id);
            printf("\nmemid_node: ");
        }
        free(shm_descs);
    }
}
static void ubs_mem_share_list_example(void)
{
    ubs_mem_shm_desc_t *shm_descs = NULL;
    uint32_t shm_desc_cnt = 0;

    printf("=== ubs_mem_share_list_example ===\n");
    const int ret = ubs_mem_shm_list(&shm_descs, &shm_desc_cnt);
    if (ret != 0) {
        printf("ubs_mem_shm_list failed, ret=%d\n", ret);
        return;
    }
    printf("ubs_mem_shm_list success, cnt=%u\n", shm_desc_cnt);
    for (uint32_t i = 0; i < shm_desc_cnt; i++) {
        printf("name=%s, size=%lu, import_desc_cnt=%u\n", shm_descs[i].name, shm_descs[i].mem_size,
               shm_descs[i].import_desc_cnt);
        for (int j = 0; j < shm_descs[i].import_desc_cnt; j++) {
            printf("[%u] shm_descs=%u\n", i, shm_descs[i].import_desc[j].memid_cnt);
            printf("memid: ");
            for (int k = 0; k < shm_descs[i].import_desc[j].memid_cnt; k++) {
                printf("%lu ", shm_descs[i].import_desc[j].memids[k]);
            }
            printf("\n");
            printf("importNode: slot_id: %u, host_name: %s, socket_id: %p",
                   shm_descs[i].import_desc[j].import_node.slot_id, shm_descs[i].import_desc[j].import_node.host_name,
                   shm_descs[i].import_desc[j].import_node.socket_id);
            printf("\nmemid_node: ");
        }
    }
    free(shm_descs);
}
static void ubs_mem_shm_fault_get_example(void)
{
    ubs_mem_memids_fault_t fault_info;
    printf("=== ubs_mem_shm_fault_get_example ===\n");
    const int ret = ubs_mem_shm_fault_get(g_shm_name, &fault_info);
    if (ret != 0) {
        printf("ubs_mem_shm_fault_get failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_fault_get success, fault_cnt=%u\n", fault_info.memid_cnt);
        for (uint32_t i = 0; i < fault_info.memid_cnt; i++) {
            printf("[%u] name=%s, memId:%lu, size=%d\n", i, g_shm_name, fault_info.memids[i],
                   fault_info.memid_status[i]);
        }
    }
}
int main(void)
{
    ubs_mem_share_create_example();
    ubs_mem_share_attach_example();
    ubs_mem_share_detach_example();
    ubs_mem_share_delete_example();

    ubs_mem_share_create_with_lender_example();
    ubs_mem_share_attach_example();
    ubs_mem_share_detach_example();
    ubs_mem_share_delete_example();
}