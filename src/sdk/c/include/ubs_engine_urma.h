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

typedef enum {
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
uint32_t ubs_urma_dev_get(ubs_urma_dev_t **urma_devices, uint32_t *urma_cnt);

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
uint32_t ubs_urma_dev_alloc(const char *name, ubs_urma_dev_info_t *dev_info);

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
uint32_t ubs_urma_dev_free(const char *name);

/**
 * @brief 设置指定URMA设备的QoS带宽
 * @param name [IN] URMA设备名称
 * @param minBandWidth [IN] 最小带宽，单位Mbps,只支持8的倍数
 * @param maxBandWidth [IN] 最大带宽，单位Mbps,只支持8的倍数
 * @return UBS_SUCCESS:操作成功，输入非8倍数会以8的倍数生效;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:参数超出范围
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 * UBS_ENGINE_ERR_NOT_EXIST: urma设备不存在
 * UBS_ERR_NOT_SUPPORTED： 不支持的操作，仅独享设备支持设置
 */
uint32_t ubs_urma_bandwidth_set(const char *name, uint32_t minBandWidth, uint32_t maxBandWidth);

/**
 * @brief 查询指定URMA设备的QoS带宽
 * @param name [IN] URMA设备名称
 * @param minBandWidth [OUT] 最小带宽，单位Mbps
 * @param maxBandWidth [OUT] 最大带宽，单位Mbps
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误
 * UBS_ENGINE_ERR_NOT_EXIST: urma设备不存在
 */
uint32_t ubs_urma_bandwidth_get(const char *name, uint32_t *minBandWidth, uint32_t *maxBandWidth);

/**
 * @brief 取消指定URMA设备的QoS带宽设置
 * @param name [IN] URMA设备名称
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBS_ENGINE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBS_ENGINE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBS_ENGINE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBS_ENGINE_ERR_INTERNAL:UBSE服务端内部错误；
 * UBS_ENGINE_ERR_NOT_EXIST: urma设备不存在；
 * UBS_ERR_NOT_SUPPORTED： 不支持的操作，仅独享设备支持设置
 */
uint32_t ubs_urma_bandwidth_reset(const char *name);

#ifdef __cplusplus
}
#endif
#endif // UBS_URMA_CONTROLLER_H