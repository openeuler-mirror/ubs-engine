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

#include "ubs_engine.h"

void print_mem_numa_desc(const ubs_mem_numa_desc_t *numa_desc)
{
    printf("Memory Resource Descriptor:\n");
    printf("Resource Name: %s\n", numa_desc->name);
    printf("Numaid: %ld \n", numa_desc->numaid);
}

void ubse_mem_numa_create_example()
{
    // 1. 准备调用参数
    const char *mem_name = "sample_memory";
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

void ubse_mem_numa_get_example()
{
    // 1. 准备调用参数
    const char *mem_name = "sample_memory";
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

void ubse_mem_numa_list_example()
{
    ubs_mem_numa_desc_t *numa_desc_list = NULL;
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

void ubse_mem_numa_delete_example()
{
    // 1. 准备调用参数
    const char *mem_name = "sample_memory";

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

int main()
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
}