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

void print_mem_fd_desc(const ubs_mem_fd_desc_t* fd_desc)
{
    printf("Memory Resource Descriptor:\n");
    printf("Resource Name: %s\n", fd_desc->name);
    printf("Total Size: %lu bytes\n", fd_desc->mem_size);
    printf("Unit Size: %zu bytes\n", fd_desc->unit_size);
    printf("Memory IDs: [");
    for (uint32_t i = 0; i < fd_desc->memid_cnt; i++) {
        printf("%lu", fd_desc->memids[i]);
        if (i < fd_desc->memid_cnt - 1)
            printf(", ");
    }
    printf("]\n");
}

void ubse_mem_fd_create_example(void)
{
    // 1. 准备调用参数
    const char* mem_name = "sample_memory";
    const uint64_t mem_size = 128 * 1024 * 1024; // 200MB
    ubs_mem_fd_owner_t uds_info = {.uid = getuid(), .gid = getgid(), .pid = getpid()};
    ubs_mem_distance_t distance = MEM_DISTANCE_L0; // 直连节点
    ubs_mem_fd_desc_t fd_desc = {0};               // 结果存储结构体

    // 2. 调用创建函数
    ubs_error_t result = ubs_mem_fd_create(mem_name, mem_size, &uds_info, 0660, distance, &fd_desc);
    // 3. 处理结果
    if (result == UBS_SUCCESS) {
        printf("Memory resource created successfully!\n");
        print_mem_fd_desc(&fd_desc);
    } else {
        // 错误处理
        printf("Error creating memory resource: %s\n", ubs_error_string(result));
    }
}

void ubse_mem_fd_get_example(void)
{
    // 1. 准备调用参数
    const char* mem_name = "sample_memory";
    ubs_mem_fd_desc_t fd_desc = {0}; // 结果存储结构体

    // 2. 调用查询函数
    ubs_error_t result = ubs_mem_fd_get(mem_name, &fd_desc);
    // 3. 处理结果
    if (result == UBS_SUCCESS) {
        printf("Memory resource get successfully!\n");
        print_mem_fd_desc(&fd_desc);
    } else {
        // 错误处理
        printf("Error get memory resource: %s\n", ubs_error_string(result));
    }
}

void ubse_mem_fd_list_example(void)
{
    ubs_mem_fd_desc_t* fd_desc_list = NULL;
    uint32_t fd_desc_cnt = 0;

    printf("Retrieving fd desc list...\n");

    // 调用 API 获取节点列表
    int32_t ret = ubs_mem_fd_list(&fd_desc_list, &fd_desc_cnt);
    if (ret != UBS_SUCCESS) {
        fprintf(stderr, "Failed to get fd desc list: %s (%s)\n", ubs_error_name(ret), ubs_error_string(ret));
        return;
    }

    printf("Successfully retrieved %u fd desc:\n", fd_desc_cnt);

    // 打印所有节点信息
    for (uint32_t i = 0; i < fd_desc_cnt; i++) {
        printf("[%u] ", i + 1);
        print_mem_fd_desc(&fd_desc_list[i]);
    }

    // 释放节点列表内存
    free(fd_desc_list);
}

void ubse_mem_fd_delete_example(void)
{
    // 1. 准备调用参数
    const char* mem_name = "sample_memory";

    // 2. 调用删除函数
    ubs_error_t result = ubs_mem_fd_delete(mem_name);
    // 3. 处理结果
    if (result == UBS_SUCCESS) {
        printf("Memory resource delete successfully!\n");
    } else {
        // 错误处理
        printf("Error delete memory resource: %s\n", ubs_error_string(result));
    }
}

static int32_t fd_fault_handler(const char *name, uint64_t memid, ubs_mem_fault_type_t type)
{
    ubse_print_fault_info("FD", name, memid, type);
    return 0;
}

static void ubse_mem_fd_fault_register_example(void)
{
    printf("=== ubs_mem_fd_fault_register_example ===\n");

    ubse_setup_signal_handlers();

    printf("[MAIN] Registering fd memory fault handler...\n");
    int32_t ret = ubs_mem_fd_fault_register(fd_fault_handler);
    ubse_start_fault_monitoring("fd", ret);
}

int main(void)
{
    // 测试创建fd内存
    ubse_mem_fd_create_example();

    // 测试查询fd内存
    ubse_mem_fd_get_example();

    // 测试查询当前节点fd内存列表
    ubse_mem_fd_list_example();

    // 测试删除fd内存
    ubse_mem_fd_delete_example();

    // 测试查询fd内存
    ubse_mem_fd_get_example();

    // 测试删除fd内存
    ubse_mem_fd_delete_example();

    // 测试fd内存故障上报
    ubse_mem_fd_fault_register_example();
}