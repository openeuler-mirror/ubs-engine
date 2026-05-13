/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MatrixEngine is licensed under Mulan PSL v2.
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

#include "ubse_mem_controller.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
namespace ubse::mem::controller {
uint32_t UbseGetMemDebtInfoFromMaster(const std::string& nodeId,
                                      ubse::adapter_plugins::mmi::NodeMemDebtInfoMap& memDebtInfoMap);
uint32_t GetDebtInfoMapByNodeId(const std::string& nodeId,
                                ubse::adapter_plugins::mmi::NodeMemDebtInfoMap& memDebtInfoMap);

/**
* @brief 查询本节点numa形态远端内存信息
*
* @param name [IN] 借用标识
* @param udsInfo [IN] 权限信息
* @param numaDesc [OUT] 借用形成的远端numa信息
*/
uint32_t UbseMemNumaGet(const std::string& name, def::UbseMemNumaDesc& numaDesc,
                        const def::UbseUdsInfo* udsInfo = nullptr);

/**
* @brief 批量查询本节点numa形态远端内存, 这些借用关系都以指定借用标识为前缀
*
* @param udsInfo [IN] 权限信息
* @param numaDescs [OUT] numa内存描述信息数组
*/
uint32_t UbseMemNumaList(const def::UbseUdsInfo& udsInfo, std::vector<def::UbseMemNumaDesc>& numaDescs);

/**
* @brief 查询本节点fd形态远端内存信息
*
* @param name [IN] 借用标识
* @param udsInfo [IN] 权限信息
* @param fdDesc [OUT] fd内存信息
*/
uint32_t UbseMemFdGet(const std::string& name, def::UbseMemFdDesc& fdDesc, const def::UbseUdsInfo* udsInfo = nullptr);

/**
* @brief 批量查询本节点fd形态远端内存, 这些借用关系都以指定借用标识为前缀
*
* @param udsInfo [IN] 权限信息
* @param fdDescs [OUT] fd内存描述信息数组
*/
uint32_t UbseMemFdList(const def::UbseUdsInfo& udsInfo, std::vector<def::UbseMemFdDesc>& fdDescs);
/* *
* @brief 查询共享内存状态
*
* @param name [IN] 借用标识, name最大长度48字节, 含结尾字符\0; name在节点内保持唯一性
* @param fd_desc [OUT] fd内存信息
* @return UBSE_OK:操作成功;
* UBS_ERR_NULL_POINTER:空指针;
* UBSE_ERR_OUT_OF_RANGE:name参数超出范围;
* UBSE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
* UBSE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
* UBSE_ERR_NOT_EXIST:借用关系不存在;
* UBSE_ERR_TIMEOUT:UBSE服务端处理超时;
* UBSE_ERR_INTERNAL:UBSE服务端内部错误
*/
uint32_t UbseMemShmStatusGet(const std::string& name, def::UbseMemShmMemStatusDesc& shmStatusDesc);
/**
* @brief 查询本节点fd形态远端内存信息
*
* @param name [IN] 借用标识
* @param shmDesc [OUT] fd内存信息
*/
uint32_t UbseMemShmGet(const std::string& name, def::UbseMemShmDesc& shmDesc,
                       const def::UbseUdsInfo* udsInfo = nullptr);

uint32_t UbseMemShmGetByNodeId(const std::string& name, def::UbseMemShmDesc& shmDesc, std::string& srcNodeId);

/**
 * @brief 批量查询shm形态远端内存, 这些借用关系都以指定借用标识为前缀
 * @param request [IN] 请求体
 * @param shmDescs [OUT] shm内存描述信息数组
 */
uint32_t UbseMemShmList(def::UbseMemDebtQueryRequest& request, std::vector<def::UbseMemShmDesc>& shmDescs);

uint32_t UbseNodeInfoGet(const std::string& nodeId, ubse::adapter_plugins::mmi::UbseNodeInfo& ubseNodeInfo);

/**
* @brief 查询addr信息
*
* @param name [IN] 借用标识
* @param importNodeId [IN] 导入节点ID
* @param desc [OUT] 借用形成的远端numa信息
*/
int32_t UbseMemAddrGet(const std::string& name, const std::string& importNodeId, UbseMemAddrDesc& desc);

/**
* @brief 查询指定节点numa形态远端内存信息
*
* @param name [IN] 借用标识
* @param importNodeId [IN] 导入节点ID
* @param numaDesc [OUT] 借用形成的远端numa信息
*/
int32_t UbseMemNumaGetWithImportNode(const std::string& name, const std::string& importNodeId,
                                     UbseMemNumaDesc& numaDesc);

uint32_t UbseMemNodeBorrowInfoQuery(std::vector<def::UbseNodeBorrowInfo>& nodeBorrowInfo);

uint32_t UbseMemIdGetByImportMemId(def::UbseMemIdQueryRequest& request, def::UbseExportMemDesc& exportMemDesc);
} // namespace ubse::mem::controller
#endif
