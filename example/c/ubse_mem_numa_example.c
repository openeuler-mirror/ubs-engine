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

#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "ubse/ubs_engine_mem.h"
#include "ubse/ubs_error.h"
#include "ubse_mem_fault_common.h"

void print_mem_numa_desc(const ubs_mem_numa_desc_t* numa_desc)
{
    printf("Memory Resource Descriptor:\n");
    printf("Resource Name: %s\n", numa_desc->name);
    printf("Numaid: %ld \n", numa_desc->numaid);
    printf("ExportNode SlotId: %u\n", numa_desc->export_node.slot_id);
    printf("ImportNode SlotId: %u\n", numa_desc->import_node.slot_id);
    printf("Size: %lu\n", numa_desc->size);
    // 打印 Export Node 的 IP 列表
    printf("Export Node IP List:\n");
    for (int i = 0; i < UBS_TOPO_IPADDR_NUM; i++) {
        if (numa_desc->export_node.ips[i].af == AF_INET) {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &numa_desc->export_node.ips[i].ipv4, ip_str, INET_ADDRSTRLEN);
            printf("  IPv4[%d]: %s\n", i, ip_str);
        } else if (numa_desc->export_node.ips[i].af == AF_INET6) {
            char ip_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &numa_desc->export_node.ips[i].ipv6, ip_str, INET6_ADDRSTRLEN);
            printf("  IPv6[%d]: %s\n", i, ip_str);
        } else if (numa_desc->export_node.ips[i].af == 0) {
            // 未设置的 IP 地址
            printf("  IP[%d]: (unset)\n", i);
        } else {
            printf("  IP[%d]: Unknown address family (%d)\n", i, numa_desc->export_node.ips[i].af);
        }
    }
    // 格式化十六进制打印，每16字节换行
    printf("User Info:\n");
    for (int i = 0; i < UBSE_MAX_USR_INFO_LENGTH; i++) {
        if (i % 16 == 0 && i > 0) {
            printf("\n");
        }
        printf("%02X ", numa_desc->usrInfo[i]);
    }
    printf("\n");
}

void ubse_mem_numa_create_example(void)
{
    // 1. 准备调用参数
    const char* mem_name = "sample_memory";
    const uint64_t mem_size = 200 * 1024 * 1024;   // 200MB
    ubs_mem_distance_t distance = MEM_DISTANCE_L0; // 直连节点
    ubs_mem_numa_desc_t numa_desc = {0};           // 结果存储结构体

    // 2. 调用创建函数
    ubs_error_t result = ubs_mem_numa_create(mem_name, mem_size, distance, &numa_desc);
    // 3. 处理结果
    if (result == UBS_SUCCESS) {
        printf("Memory resource created successfully!\n");
        print_mem_numa_desc(&numa_desc);
    } else {
        // 错误处理
        printf("Error creating memory resource: %s\n", ubs_error_string(result));
    }
}

void ubse_mem_numa_get_example(void)
{
    // 1. 准备调用参数
    const char* mem_name = "sample_memory";
    ubs_mem_numa_desc_t numa_desc = {0}; // 结果存储结构体

    // 2. 调用查询函数
    ubs_error_t result = ubs_mem_numa_get(mem_name, &numa_desc);
    // 3. 处理结果
    if (result == UBS_SUCCESS) {
        printf("Memory resource get successfully!\n");
        print_mem_numa_desc(&numa_desc);
    } else {
        // 错误处理
        printf("Error get memory resource: %s\n", ubs_error_string(result));
    }
}

void ubse_mem_numa_list_example(void)
{
    ubs_mem_numa_desc_t* numa_desc_list = NULL;
    uint32_t numa_desc_cnt = 0;

    printf("Retrieving numa desc list...\n");

    // 调用 API 获取节点列表
    int32_t ret = ubs_mem_numa_list(&numa_desc_list, &numa_desc_cnt);
    if (ret != UBS_SUCCESS) {
        fprintf(stderr, "Failed to get numa desc list: %s (%s)\n", ubs_error_name(ret), ubs_error_string(ret));
        return;
    }

    printf("Successfully retrieved %u numa desc:\n", numa_desc_cnt);

    // 打印所有节点信息
    for (uint32_t i = 0; i < numa_desc_cnt; i++) {
        printf("[%u] ", i + 1);
        print_mem_numa_desc(&numa_desc_list[i]);
    }

    // 释放节点列表内存
    free(numa_desc_list);
}

void ubse_mem_numa_delete_example(void)
{
    // 1. 准备调用参数
    const char* mem_name = "sample_memory";

    // 2. 调用删除函数
    ubs_error_t result = ubs_mem_numa_delete(mem_name);
    // 3. 处理结果
    if (result == UBS_SUCCESS) {
        printf("Memory resource delete successfully!\n");
    } else {
        // 错误处理
        printf("Error delete memory resource: %s\n", ubs_error_string(result));
    }
}

static int32_t numa_fault_handler(const char *name, uint64_t numaid, ubs_mem_fault_type_t type)
{
    ubse_print_fault_info("NUMA", name, numaid, type);
    return 0;
}

static void ubse_mem_numa_fault_register_example(void)
{
    printf("=== ubs_mem_numa_fault_register_example ===\n");

    ubse_setup_signal_handlers();

    printf("[MAIN] Registering numa memory fault handler...\n");
    int32_t ret = ubs_mem_numa_fault_register(numa_fault_handler);
    ubse_start_fault_monitoring("numa", ret);
}

int main(void)
{
    // 测试创建numa内存
    ubse_mem_numa_create_example();

    // 测试查询numa内存
    ubse_mem_numa_get_example();

    // 测试查询numa内存列表
    ubse_mem_numa_list_example();

    // 测试删除numa内存
    ubse_mem_numa_delete_example();

    // 测试查询numa内存
    ubse_mem_numa_get_example();

    // 测试删除numa内存
    ubse_mem_numa_delete_example();

    // 测试numa内存故障上报
    ubse_mem_numa_fault_register_example();
}