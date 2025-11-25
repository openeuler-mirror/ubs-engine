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

#ifndef UBS_ENGINE_MEM_H
#define UBS_ENGINE_MEM_H

#include <stdint.h>
#include <sys/types.h>

#include "ubs_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UBS_MEM_MAX_MEMID_NUM 2048
#define UBS_MEM_MAX_NAME_LENGTH 48
#define UBS_MEM_MIN_SIZE (128ULL * 1024ULL * 1024ULL)
#define UBS_MEM_MAX_SIZE (256ULL * 1024ULL * 1024ULL * 1024ULL)

typedef enum {
    NUMA_LOCAL, // 本地numa
    NUMA_REMOTE // 远端numa, 当前不支持
} ubs_mem_numa_type_t;

typedef struct {
    uint32_t slot_id;              // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socket_id;            // socket id
    uint32_t numa_id;              // 节点中的numa id
    ubs_mem_numa_type_t numa_type; // numa类型
    uint32_t mem_lend_ratio;       // 池化内存借出比例上限
    uint64_t mem_total;            // 内存总量, 单位字节
    uint64_t mem_free;             // 内存空闲量, 单位字节
    uint32_t huge_pages_2M;        // 2M大页数量
    uint32_t free_huge_pages_2M;   // 2M大页空闲数量
} ubs_mem_numastat_t;

/**
 * @brief 查询指定节点numa信息，当前只支持本地内存，后续会增加远端numa
 *
 * @param slot_id [IN] 节点id
 * @param numa_mems [OUT] 节点numa信息数组, 调用方需要使用free接口主动释放内存
 * @param numa_mem_cnt [OUT] 节点numa信息个数
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:node_id长度超108字节;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t ubs_mem_numastat_get(uint32_t slot_id, ubs_mem_numastat_t **numa_mems, uint32_t *numa_mem_cnt);

// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的的运行用户的groupid
    pid_t pid; // 属主进程的的运行用户的pid, 指定pid，pid消失时, ubse自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;

typedef enum {
    MEM_DISTANCE_L0, // L0对应直连节点
    MEM_DISTANCE_L1, // L1对应通过1跳节点, 暂不支持
    MEM_DISTANCE_L2  // L2对应过超过1跳节点 , 暂不支持
} ubs_mem_distance_t;

typedef struct {
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socket_id;   // socket id
    uint64_t numa_id;     // 节点中的numa id
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围[128*1024*1024, 256*1024*1024*1024]
} ubs_mem_lender_t;

#define UBS_MEM_DEV_NAME_PREFIX "obmm_shmdev"
#define UBS_MEM_DEV_PATH "/dev/" UBS_MEM_DEV_NAME_PREFIX
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                     // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM]; // 内存块标识信息，FD的文件形成规则：UBS_MEM_DEV_PATH + memid
    uint64_t mem_size;                      // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                       // 芯片表项拆分粒度, 单位Byte
} ubs_mem_fd_desc_t;

/**
 * @brief  在本节点创建fd形态的远端内存，该资源的管理权限属于调用者
 * 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；
 *
 * @param name [IN] 借用标识, name最大长度48字节, 含结尾字符\0; name在节点内保持唯一性
 * @param size [IN] 借用大小, 单位Byte, 取值范围[128*1024*1024, 256*1024*1024*1024]
 * @param owner [IN] 内存资源使用者属主信息, 可选参数, Null不关注该字段
 * @param mode [IN] 内存资源使用者访问权限, 可选参数, 0则不关注该字段
 * @param distance [IN] 内存访问距离
 * @param fd_desc [OUT] 内存描述信息
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:name或者size参数超出范围;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_EXISTED:借用关系已存在;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t ubs_mem_fd_create(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner, mode_t mode,
    ubs_mem_distance_t distance, ubs_mem_fd_desc_t *fd_desc);

/**
 * @brief 查询本节点fd形态远端内存; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同；
 * 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；
 *
 * @param name [IN] 借用标识, name最大长度48字节, 含结尾字符\0; name在节点内保持唯一性
 * @param fd_desc [OUT] fd内存信息
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:name参数超出范围;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_NOT_EXIST:借用关系不存在;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t ubs_mem_fd_get(const char *name, ubs_mem_fd_desc_t *fd_desc);

/**
 * @brief 查询本节点所有fd形态远端内存; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同；
 * 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；
 *
 * @param fd_descs [OUT] fd内存描述信息数组, 调用成功时，调用方需要使用free接口主动释放内存
 * @param fd_desc_cnt [OUT] fd内存描述信息数组中的元素个数
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:name前缀参数超出范围;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_NOT_EXIST:借用关系不存在;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t ubs_mem_fd_list(ubs_mem_fd_desc_t **fd_descs, uint32_t *fd_desc_cnt);

/**
 * @brief 删除本节点指定fd远端内存; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同；
 * 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；
 *
 * @param name  [IN] 借用标识, name最大长度48字节, 含结尾字符\0; name在节点内保持唯一性
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:name参数超出范围;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_NOT_EXIST:借用关系不存在;
 * UBS_ENGINE_ERR_IMPORT_ABSENT:IMPORT不在位, 无法删除;
 * UBS_ENGINE_ERR_CREATING:正在创建过程中;
 * UBS_ENGINE_ERR_DELETING:正在删除过程中;
 * UBS_ENGINE_ERR_UNIMPORT_SUCCESS:UNIMPORT已经成功, unexport失败, 资源没有释放完全, 后续对账能自动回收;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t ubs_mem_fd_delete(const char *name);

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH]; // 借用标识
    int64_t numaid;                     // 形成远端numa对应的numaid
} ubs_mem_numa_desc_t;

/**
 * @brief 在本节点创建numa形态的远端内存，该资源的管理权限属于调用者
 * 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；
 *
 * @param name [IN] 借用标识, name最大长度48, 含结尾字符\0; name在节点内保持唯一性
 * @param size [IN] 借用大小, 单位Byte, 取值范围[128*1024*1024, 256*1024*1024*1024]
 * @param distance [IN] 内存访问跳数
 * @param numa_desc [OUT] 借用形成的远端numa信息
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:name或者size参数超出范围;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_EXISTED:借用关系已存在;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t ubs_mem_numa_create(const char *name, uint64_t size, ubs_mem_distance_t distance,
    ubs_mem_numa_desc_t *numa_desc);

/**
 * @brief 查询本节点numa形态远端内存信息; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同；
 * 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；
 *
 * @param name [IN] 借用标识, name最大长度48, 含结尾字符\0; name在节点内保持唯一性
 * @param numa_desc [OUT] 借用形成的远端numa信息
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:name参数超出范围;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_NOT_EXIST:借用关系不存在;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t ubs_mem_numa_get(const char *name, ubs_mem_numa_desc_t *numa_desc);

/**
 * @brief 查询本节点所有numa形态远端内存; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同；
 * 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；
 *
 * @param numa_desc [OUT] numa内存描述信息数组调用成功时, 调用成功时, 调用方需要使用free接口主动释放内存
 * @param numa_desc_cnt [OUT] numa内存描述信息数组中的元素个数
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:name参数超出范围;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_NOT_EXIST:借用关系不存在
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t ubs_mem_numa_list(ubs_mem_numa_desc_t **numa_descs, uint32_t *numa_desc_cnt);

/**
 * @brief 删除本节点指定numa远端内存; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同
 * 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；
 *
 * @param name  [IN] 借用标识, name最大长度48字节, 含结尾字符\0; name在节点内保持唯一性
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:name参数超出范围;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_NOT_EXIST:借用关系不存在;
 * UBS_ENGINE_ERR_IMPORT_ABSENT:IMPORT不在位, 无法删除;
 * UBS_ENGINE_ERR_CREATING:正在创建过程中;
 * UBS_ENGINE_ERR_DELETING:正在删除过程中;
 * UBS_ENGINE_ERR_UNIMPORT_SUCCESS:UNIMPORT已经成功, unexport失败, 资源没有释放完全, 后续对账能自动回收;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t ubs_mem_numa_delete(const char *name);
#ifdef __cplusplus
}
#endif
#endif // UBS_ENGINE_MEM_H
