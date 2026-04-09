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
#include <numa.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <securec.h>
#include "ubse/ubs_engine_mem.h"
#include "ubse/ubs_error.h"

// 定义结构体
typedef struct {
    void* ptr;                  // numa分配的内存指针
    size_t size;                // 分配的内存大小
    ubs_mem_numa_desc_t* desc;  // NUMA内存描述符
} Numa_handler;

/* 分配 NUMA 内存 */
void do_numa_allocate(Numa_handler* handler, ubs_mem_numa_desc_t* numa_desc, size_t size)
{
    size_t alloc_size = size;
    void* ptr = numa_alloc_onnode(size, (int)numa_desc->numaid);
    if (ptr == NULL) {
        printf("memory is failed to be allocated yet on numa %ld\n", numa_desc->numaid);
        alloc_size = 0;
    }
    handler->ptr = ptr;
    handler->size = alloc_size;
    handler->desc = numa_desc;
    printf("Memory allocated on NUMA %ld\n", numa_desc->numaid);
}

/* 释放 NUMA 内存 */
void do_numa_free(Numa_handler* handler)
{
    if (handler->ptr == NULL) {
        return;
    }
    numa_free(handler->ptr, handler->size);
    handler->ptr = NULL;
    handler->size = 0;
    handler->desc = NULL;
}

// 读取 NUMA 内存数据
char* do_read_memory(void* ptr, size_t offset, size_t length)
{
    if (ptr == NULL) {
        printf("Memory pointer is NULL.\n");
        return NULL;
    }
    char* read_buf = ptr + offset;
    read_buf[length - 1] = '\0';
    printf("Read from memory: %s\n", read_buf);
    return read_buf;
}

/* 写入 NUMA 内存数据 */
int do_write_memory(void* ptr, size_t offset, const char* data, size_t length)
{
    if (ptr == NULL) {
        printf("Memory pointer is NULL.\n");
        return -1;
    }
    char* dest = (char*) ptr + offset;
    errno_t ret = memcpy_s(dest, length, data, length);
    if (ret != EOK) {
        printf("memcpy_s failed with error code: %d\n", ret);
        return -1;
    }
    printf("Written to memory: %s\n", (const char*)data);
    return 0;
}

/* 打印 NUMA 内存描述符信息 */
void do_numa_print(const ubs_mem_numa_desc_t* numa_desc)
{
    printf("Memory Resource Descriptor:\n");
    printf("Resource Name: %s\n", numa_desc->name);
    printf("Numaid: %ld \n", numa_desc->numaid);
}

/* 创建 NUMA 内存资源借用 */
void do_numa_create(const char* mem_name, const uint64_t mem_size, ubs_mem_numa_desc_t* numa_desc)
{
    ubs_mem_distance_t distance = MEM_DISTANCE_L0;

    printf("Memory resource created size:%lu!\n", mem_size);
    ubs_error_t result = ubs_mem_numa_create(mem_name, mem_size, distance, numa_desc);
    if (result == UBS_SUCCESS) {
        printf("Memory resource created successfully!\n");
        do_numa_print(numa_desc);
    } else {
        printf("Error creating memory resource: %s\n", ubs_error_string(result));
    }
}

/* 获取 NUMA 内存资源借用 */
ubs_mem_numa_desc_t do_numa_get(const char* mem_name)
{
    ubs_mem_numa_desc_t numa_desc = {0};
    ubs_error_t result = ubs_mem_numa_get(mem_name, &numa_desc);
    if (result == UBS_SUCCESS) {
        printf("Memory resource get successfully!\n");
        do_numa_print(&numa_desc);
    } else {
        printf("Error get memory resource: %s\n", ubs_error_string(result));
    }
    return numa_desc;
}

/* 删除 NUMA 内存资源借用 */
void do_numa_delete(const char* mem_name)
{
    ubs_error_t result = ubs_mem_numa_delete(mem_name);
    if (result == UBS_SUCCESS) {
        printf("Memory resource delete successfully!\n");
    } else {
        printf("Error delete memory resource: %s\n", ubs_error_string(result));
    }
}

int main(int argc, char* argv[])
{
    const char* default_mem_name = "default_memory";
    const char* mem_name = default_mem_name;
    if (argc >= 2) {
        mem_name = argv[1];
    } else {
        printf("No memory name provided. Using default: %s\n", default_mem_name);
    }

    Numa_handler handler;
    ubs_mem_numa_desc_t numa_desc = {0};
    const uint64_t mem_size = 128 * 1024 * 1024;  // 128MB

    // 测试创建numa内存
    printf("Testing ubse_mem_numa_create...\n");
    do_numa_create(mem_name, mem_size, &numa_desc);

    // 测试获取numa内存
    printf("\nTesting ubse_mem_fd_get...\n");
    do_numa_get(mem_name);

    const uint64_t alloc_size = 1 * 1024 * 1024;  // 1MB
    // 测试分配内存
    do_numa_allocate(&handler, &numa_desc, alloc_size);
    if (handler.ptr == NULL) {
        printf("Failed to allocate memory on NUMA node.\n");
        return -1;
    }

    // 测试读写操作
    printf("\nTesting write and read ...\n");
    const char* hello = "Hello World!";
    size_t hello_len = strlen(hello) + 1;
    do_write_memory(handler.ptr, 0, hello, hello_len);
    do_read_memory(handler.ptr, 0, hello_len);

    // 归还内存
    do_numa_free(&handler);

    // 测试删除numa内存
    printf("\nTesting ubse_mem_numa_delete...\n");
    do_numa_delete(mem_name);

    return 0;
}
