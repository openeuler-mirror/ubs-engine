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

#include <fcntl.h>
#include <securec.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "ubse/ubs_engine_mem.h"
#include "ubse/ubs_error.h"

#define DEVICE_PATH "/dev/obmm_shmdev"

typedef struct {
    int fd; // 存储与设备文件关联的文件描述符，对应设备路径为DEVICE_PATH + mem_id(如/dev/obmm_shmdev123)，-1表示无效
    void* mmap_obj;  // 指向内存映射区域的指针, 可以直接访问映射的内存区域, NULL表示未映射
    size_t mem_size; // 记录映射的内存区域大小, 与size参数对应
} Fd_handler;

/* 打印 fd 借用描述符相关信息 */
void do_fd_print(const ubs_mem_fd_desc_t* fd_desc)
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

/* 打开设备文件并进行内存映射 */
void do_fd_mmap(Fd_handler* handler, uint64_t mem_id, size_t size)
{
    if (handler->fd != -1) {
        close(handler->fd);
        handler->fd = -1;
    }

    // 打开新设备
    char device_path[50];
    int ret = snprintf_s(device_path, sizeof(device_path), sizeof(device_path) - 1, "%s%lu", DEVICE_PATH, mem_id);
    if (ret == -1) {
        perror("Failed to format device path (truncated or error)");
        return;
    }
    handler->fd = open(device_path, O_RDWR);
    if (handler->fd == -1) {
        perror("Failed to open device");
        return;
    }

    // 内存映射
    handler->mmap_obj = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, handler->fd, 0);
    if (handler->mmap_obj == MAP_FAILED) {
        perror("Failed to mmap");
        close(handler->fd);
        handler->fd = -1;
        return;
    }
    handler->mem_size = size;
    printf("Device opened: %s, mapped size: %zu bytes\n", device_path, size);
}

/* 向映射的内存区域写入数据 */
void do_fd_write(Fd_handler* handler, size_t offset, const char* str, size_t str_len)
{
    if (!handler->mmap_obj) {
        printf("The device is not mapped yet.\n");
        return;
    }

    if (offset + str_len > handler->mem_size) {
        printf("Write range exceeds mapped area (0-%zu)\n", handler->mem_size - 1);
        return;
    }

    char* dest = (char*)handler->mmap_obj + offset;
    size_t available = handler->mem_size - offset;
    errno_t ret = memcpy_s(dest, available, str, str_len);
    if (ret != EOK) {
        printf("memcpy_s failed: %d\n", ret);
        return;
    }
    printf("Written to memory: %s\n", (const char*)str);
}

/* 从映射的内存区域读取数据 */
void do_fd_read(Fd_handler* handler, size_t offset, size_t str_len)
{
    if (!handler->mmap_obj) {
        printf("The device is not mapped yet.\n");
        return;
    }

    if (offset + str_len > handler->mem_size) {
        printf("Read range exceeds mapped area (0-%zu)\n", handler->mem_size - 1);
        return;
    }

    char* data = (char*)handler->mmap_obj + offset;
    printf("Read from memory: %s\n", data);
}

/* 解除内存映射并关闭设备文件 */
void do_fd_close(Fd_handler* handler)
{
    if (handler->mmap_obj) {
        int res = munmap(handler->mmap_obj, handler->mem_size);
        if (res != 0) {
            perror("Failed to munmap");
            return;
        }
        handler->mmap_obj = NULL;
    }
    if (handler->fd != -1) {
        close(handler->fd);
        handler->fd = -1;
    }
    printf("Device closed.\n");
}

/* 创建 fd 内存 */
void do_fd_create(const char* mem_name, const uint64_t mem_size, ubs_mem_fd_desc_t* fd_desc)
{
    ubs_mem_fd_owner_t uds_info = {.uid = getuid(), .gid = getgid(), .pid = getpid()};
    mode_t mode = 0777;
    ubs_mem_distance_t distance = MEM_DISTANCE_L0;

    ubs_error_t result = ubs_mem_fd_create(mem_name, mem_size, &uds_info, mode, distance, fd_desc);
    if (result == UBS_SUCCESS) {
        printf("Memory resource created successfully!\n");
        do_fd_print(fd_desc);
    } else {
        printf("Error creating memory resource: %s\n", ubs_error_string(result));
    }
}

/* 获取 fd 内存信息 */
ubs_mem_fd_desc_t do_fd_get(const char* mem_name)
{
    ubs_mem_fd_desc_t fd_desc = {0};

    ubs_error_t result = ubs_mem_fd_get(mem_name, &fd_desc);
    if (result == UBS_SUCCESS) {
        printf("Memory resource get successfully!\n");
        do_fd_print(&fd_desc);
    } else {
        printf("Error get memory resource: %s\n", ubs_error_string(result));
    }
    return fd_desc;
}

/* 删除 fd 内存 */
void do_fd_delete(const char* mem_name)
{
    ubs_error_t result = ubs_mem_fd_delete(mem_name);
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

    Fd_handler handler = {-1, NULL, 0};
    ubs_mem_fd_desc_t fd_desc = {0};
    const uint64_t mem_size = 128 * 1024 * 1024; // 128MB

    // 测试创建fd内存
    printf("Testing ubse_mem_fd_create...\n");
    do_fd_create(mem_name, mem_size, &fd_desc);

    // 测试查询fd内存
    printf("\nTesting ubse_mem_fd_get...\n");
    do_fd_get(mem_name);

    // 测试映射fd内存
    do_fd_mmap(&handler, fd_desc.memids[0], fd_desc.unit_size);

    // 测试读写操作
    printf("\nTesting write and read...\n");
    const char* hello = "Hello World!";
    size_t hello_len = strlen(hello) + 1;
    do_fd_write(&handler, 0, hello, hello_len);
    // 读取数据
    do_fd_read(&handler, 0, hello_len);

    // 测试关闭设备
    do_fd_close(&handler);

    // 测试删除fd内存
    printf("\nTesting ubse_mem_fd_delete...\n");
    do_fd_delete(mem_name);

    return 0;
}
