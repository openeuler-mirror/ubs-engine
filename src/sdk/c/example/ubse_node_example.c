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

#include <arpa/inet.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>

#include "ubs_engine.h"

#define UNIT_MB (1024 * 1024)

void print_node_info(const ubs_topo_node_t *node)
{
    printf("Node ID: %d\n", node->slot_id);
    printf("Socket Id: %d, %d\n", node->socket_id[0], node->socket_id[1]);
    // 打印NUMA IDs
    printf("NUMA IDs:\n");
    for (int i = 0; i < UBS_TOPO_SOCKET_NUM; i++) {
        printf("  Socket %d: ", node->socket_id[i]);
        for (int j = 0; j < UBS_TOPO_NUMA_NUM; j++) {
            printf("%d", node->numa_ids[i][j]);
            if (j < UBS_TOPO_NUMA_NUM - 1) {
                printf(", ");
            }
        }
        printf("\n");
    }
    // 打印IP地址
    printf("IP Addresses:\n");
    for (int i = 0; i < UBS_TOPO_IPADDR_NUM; i++) {
        if (node->ips[i].af == AF_INET) {
            // IPv4地址
            printf("  IP %d: ", i);
            char ipv4_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(node->ips[i].ipv4), ipv4_str, INET_ADDRSTRLEN);
            printf("IPv4 - %s\n", ipv4_str);
        } else if (node->ips[i].af == AF_INET6) {
            // IPv6地址
            printf("  IP %d: ", i);
            char ipv6_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(node->ips[i].ipv6), ipv6_str, INET6_ADDRSTRLEN);
            printf("IPv6 - %s\n", ipv6_str);
        } else if (node->ips[i].af != 0) {
            printf("  IP %d: ", i);
            printf("Unknown address family (%d)\n", node->ips[i].af);
        }
    }
    printf("Hostname: %s\n", node->host_name);
}

void ubse_node_list_example(void)
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

void ubse_node_get_example(void)
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

void ubse_cpu_link_list_example(void)
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

void ubse_node_numa_mem_list_example(void)
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

int main(void)
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
