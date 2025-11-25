/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include <malloc.h>
#include <stdio.h>
#include <unistd.h>

#include "ubs_engine.h"

#define UNIT_MB (1024 * 1024)

void print_node_info(const ubs_topo_node_t *node)
{
    printf("Node ID: %d\n", node->slot_id);
    printf("Type: ");
}

void ubse_node_list_example()
{
    ubs_topo_node_t *node_list = NULL;
    uint32_t node_cnt = 0;

    printf("Retrieving local node info...\n");

    // 调用 API 获取节点列表
    int32_t ret = ubs_topo_node_list(&node_list, &node_cnt);
    if (ret != UBS_SUCCESS) {
        fprintf(stderr, "Failed to get node list: %s (%s)\n", ubs_error_name(ret), ubs_error_string(ret));
        return;
    }

    printf("Successfully retrieved %u nodes:\n", node_cnt);

    // 打印所有节点信息
    for (uint32_t i = 0; i < node_cnt; i++) {
        printf("[%u] ", i + 1);
        print_node_info(&node_list[i]);
    }

    // 释放节点列表内存
    free(node_list);
}

void ubse_node_get_example()
{
    ubs_topo_node_t node = {0};

    printf("Retrieving node list...\n");

    // 调用 API 获取节点列表
    int32_t ret = ubs_topo_node_local_get(&node);
    if (ret != UBS_SUCCESS) {
        fprintf(stderr, "Failed to get node list: %s (%s)\n", ubs_error_name(ret), ubs_error_string(ret));
        return;
    }

    printf("Successfully retrieved nodes:\n");
    print_node_info(&node);
}

void ubse_cpu_link_list_example()
{
    ubs_topo_link_t *links = NULL;
    uint32_t link_count = 0;

    printf("Retrieving cpu link list...\n");
    // 调用 API 获取节点列表
    int32_t ret = ubs_topo_link_list(&links, &link_count);
    if (ret != UBS_SUCCESS) {
        fprintf(stderr, "Failed to get cpu link list: %s (%s)\n", ubs_error_name(ret), ubs_error_string(ret));
        return;
    }
    printf("Found %u CPU links:\n", link_count);
    for (uint32_t i = 0; i < link_count; i++) {
        printf("Link %u: %u-socket%u -> %u-socket%u\n", i, links[i].slot_id, links[i].socket_id, links[i].peer_slot_id,
               links[i].peer_socket_id);
    }
    free(links); // 必须释放内存
}

void ubse_node_numa_mem_list_example()
{
    ubs_mem_numastat_t *numa_mem = NULL;
    uint32_t numa_count = 0;

    printf("Retrieving numa list...\n");
    // 调用 API 获取节点列表
    int32_t ret = ubs_mem_numastat_get(1, &numa_mem, &numa_count);
    if (ret != UBS_SUCCESS) {
        fprintf(stderr, "Failed to get numa list: %s (%s)\n", ubs_error_name(ret), ubs_error_string(ret));
        return;
    }
    printf("NUMA memory info for 1:\n");
    for (uint32_t i = 0; i < numa_count; i++) {
        printf("NUMA %u (Socket %u):\n", numa_mem[i].numa_id, numa_mem[i].socket_id);
        printf("  Reserved Ratio: %u%%\n", numa_mem[i].mem_lend_ratio);
        printf("  Total Memory: %.2f MB\n", (double)numa_mem[i].mem_total / UNIT_MB);
        printf("  Free Memory: %.2f MB\n", (double)numa_mem[i].mem_free / UNIT_MB);
        printf("  Huge Pages: %u\n", numa_mem[i].huge_pages_2M);
        printf("  Free Huge Pages: %u\n", numa_mem[i].free_huge_pages_2M);
    }
    free(numa_mem); // 释放分配的内存
}

int main()
{
    // 查询当前节点
    ubse_node_get_example();

    // 查询全量节点
    ubse_node_list_example();

    // 查询拓扑信息
    ubse_cpu_link_list_example();

    // 查询numa信息
    ubse_node_numa_mem_list_example();
}
