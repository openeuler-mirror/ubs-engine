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

#ifndef UBS_ENGINE_H
#define UBS_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化ubse客户端
 *
 * @param ubs_engine_uds_path [IN]
 * ubse服务端的uds文件路径；传入空指针或空路径，则采用ubse默认地址/var/run/ubse/ubse.sock ；路径长度限制：遵从linux
 * `sun_path` 的大小是 108 字节 (`#define UNIX_PATH_MAX 108`)，所以路径不能超过 107 个字符（不含结尾的空字符 `\0`）
 * @return UBS_SUCCESS:操作成功;
 * UBS_ENGINE_ERR_OUT_OF_RANGE:参数数据长度超108字节;
 * UBS_ENGINE_ERR_RESOURCE:SDK正在finalize过程中，初始化被拒绝;
 * @note 支持fork子进程场景: 父进程调用ubs_engine_client_finalize后fork，
 * 子进程调用本接口将runtime状态从FINALIZED重新迁移回READY
 */
int32_t ubs_engine_client_initialize(const char* ubs_engine_uds_path);

/**
 * @brief   销毁ubse客户端
 * @note    行为契约:
 * 1. void返回且幂等, finalize完成后重复调用为无操作;
 * 2. 本接口不支持并发调用: 调用方必须自行保证任一时刻仅一个线程调用本接口, finalize进行中的重复调用,会打ERROR日志并立即返回;
 * 3. 禁止在SDK回调(故障通知handler)内调用, 会被拒绝并打ERROR日志;
 * 4. 阻塞等待所有in-flight业务调用完成(排空, 上限与默认业务总超时对齐: 1800秒+60秒余量, 超时打ERROR日志并继续拆除),
 *    随后停止长连接工作线程并清理已注册的故障handler;
 *    线程停止为join语义, 最长受在途用户回调执行时长约束(在途调用阻塞在用户注册的回调内不返回时, 本接口不返回);
 * 5. fork支持: 建议在fork前调用本接口使进程进入静默态, 子进程通过ubs_engine_client_initialize重新拉起;
 * 6. 单进程内仅允许链接一份SDK实现(动态库, 或仅一处链接的静态库); 静态库被多个so分别链接时,
 *    各副本runtime状态独立, finalize的进程级静默语义与故障handler注册表不跨副本生效;
 */
void ubs_engine_client_finalize(void);

#ifdef __cplusplus
}
#endif
#endif // UBS_ENGINE_H
