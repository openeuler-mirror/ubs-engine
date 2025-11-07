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

#ifndef UBSE_MEM_CONTROLLER_QUERY_API_H
#define UBSE_MEM_CONTROLLER_QUERY_API_H
#include <string>
#include <vector>

#include "ubse_mem_controller_def.h"
#include "ubse_node_controller.h"

namespace ubse::mem::controller {
/**
 * @brief 查询本节点numa形态远端内存信息
 *
 * @param name [IN] 借用标识, name最大长度48, 不含结尾字符\0; name在节点内保持唯一性
 * @param numa_desc [OUT] 借用形成的远端numa信息
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBSE_ERR_OUT_OF_RANGE:name参数超出范围;
 * UBSE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBSE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBSE_ERR_NOT_EXIST:借用关系不存在;
 * UBSE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBSE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t UbseMemNumaGet(const std::string &name, def::UbseMemNumaDesc &numa_desc);

/**
 * @brief 批量查询本节点numa形态远端内存, 这些借用关系都以指定借用标识为前缀
 *
 * @param numa_desc [OUT] numa内存描述信息数组调用成功时, 调用成功时, 调用方需要使用free接口主动释放内存
 * @param numa_desc_cnt [OUT] numa内存描述信息数组中的元素个数
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBSE_ERR_OUT_OF_RANGE:name参数超出范围;
 * UBSE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBSE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBSE_ERR_NOT_EXIST:借用关系不存在
 * UBSE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBSE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t UbseMemNumaList(std::vector<def::UbseMemNumaDesc> &numaDescs);

/**
 * @brief 查询本节点fd形态远端内存信息
 *
 * @param name [IN] 借用标识, name最大长度48字节, 不含结尾字符\0; name在节点内保持唯一性
 * @param fd_desc [OUT] fd内存信息
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBSE_ERR_OUT_OF_RANGE:name参数超出范围;
 * UBSE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBSE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBSE_ERR_NOT_EXIST:借用关系不存在;
 * UBSE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBSE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t UbseMemFdGet(const std::string &name, def::UbseMemFdDesc &fdDesc);

/**
 * @brief 批量查询本节点fd形态远端内存, 这些借用关系都以指定借用标识为前缀
 *
 * @param fd_descs [OUT] fd内存描述信息数组, 调用成功时，调用方需要使用free接口主动释放内存
 * @param fd_desc_cnt [OUT] fd内存描述信息数组中的元素个数
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBSE_ERR_OUT_OF_RANGE:name前缀参数超出范围;
 * UBSE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBSE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBSE_ERR_NOT_EXIST:借用关系不存在;
 * UBSE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBSE_ERR_INTERNAL:UBSE服务端内部错误
 */
int32_t UbseMemFdList(std::vector<def::UbseMemFdDesc> &fdDescs);

/**
 * @brief 查询指定节点numa信息
 *
 * @param nodeid [IN] 节点标识, nodeid最大长度48字节, 不含结尾字符\0
 * @param node_numa_mem_list [OUT] 节点numa信息数组, 调用方需要使用free接口主动释放内存
 * @param node_numa_mem_cnt [OUT] 节点numa信息个数
 * @return UBS_SUCCESS:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBSE_ERR_OUT_OF_RANGE:nodeid长度超108字节;
 * UBSE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBSE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBSE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBSE_ERR_INTERNAL:UBSE服务端内部错误
 */
void UbseNodeNumaMemGet(const std::string &nodeId, std::vector<ubse::nodeController::UbseNumaInfo> &nodeNumaMemList);
} // namespace ubse::mem::controller
#endif
