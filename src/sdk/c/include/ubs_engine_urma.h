/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_URMA_CONTROLLER_H
#define UBS_URMA_CONTROLLER_H

#include <stdint.h>
#include "ubs_error.h"
#ifdef __cplusplus
extern "C" {
#endif

#define UBS_URMA_NAME_MAX 32        // 包含结束符长度
#define UBS_MAX_URMA_PATH_LENGTH 64 // 包含结束符长度
#define UBS_VFE_PATH_NUM 2
#define UBS_URMA_QOS_CONFIG_MIN_COUNT 1  // QoS配置最小数量
#define UBS_URMA_QOS_CONFIG_MAX_COUNT 2  // QoS配置最大数量
#define UBS_URMA_QOS_PRIORITY_MAX 1      // QoS优先级最大值

typedef enum
{
    UNIQUE = 0, // 独占类型
    SHARED      // 共享类型
} ubs_urma_type;

typedef struct {
    char name[UBS_URMA_NAME_MAX];
    uint32_t healthy; // 0表示端口可用，1表示端口不可用
    uint64_t hw_res_id;
} ubs_urma_dev_t;

typedef struct {
    char bonding_path[UBS_MAX_URMA_PATH_LENGTH];
    char bonding_eid[UBS_MAX_URMA_PATH_LENGTH];
    char vfe_path[UBS_VFE_PATH_NUM][UBS_MAX_URMA_PATH_LENGTH];
} ubs_urma_dev_info_t;

typedef struct {
    uint32_t priority; // 优先级，值为0或1
    uint32_t bandwidth; // 带宽，单位Gbps
} ubs_urma_qos_config_t;

/**
 * @brief 查询指定类型的urma设备信息
 *
 * @param urma_devices [OUT] urma设备信息列表，调用者需要释放该内存
 * @param urma_cnt [OUT] urma设备信息数量
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
uint32_t ubs_urma_dev_get(ubs_urma_dev_t** urma_devices, uint32_t* urma_cnt);

/**
 * @brief 分配指定的urma设备
 *
 * @param name [IN] urma设备名称
 * @param dev_info [IN/OUT] 分配urma设备的路径，需要调用方申请内存
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 * UBS_ENGINE_ERR_NOT_EXIST: urma设备不存在
 */
uint32_t ubs_urma_dev_alloc(const char* name, ubs_urma_dev_info_t* dev_info);

/**
 * @brief 释放指定的urma设备
 *
 * @param name [IN] urma设备名称
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 * UBS_ENGINE_ERR_NOT_EXIST: urma设备不存在
 */
uint32_t ubs_urma_dev_free(const char* name);

/**
 * @brief 设置URMA设备QoS配置（优先级和带宽）
 *
 * @param configs [IN] QoS配置数组指针
 * @param count [IN] 数组元素个数，有效范围1-2
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_INVALID_PARAM:参数错误或优先级不正确
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
uint32_t ubs_urma_qos_create(const ubs_urma_qos_config_t *configs, uint32_t count);

/**
 * @brief 清理所有QoS配置
 *
 * @return UBS_SUCCESS:操作成功;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 */
uint32_t ubs_urma_qos_delete(void);

#ifdef __cplusplus
}
#endif
#endif // UBS_URMA_CONTROLLER_H