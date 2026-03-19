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

#ifndef UBS_ENGINE_UBSE_MEM_CONTROLLER_FD_API_H
#define UBS_ENGINE_UBSE_MEM_CONTROLLER_FD_API_H

#include "ubse_mmi_interface.h"
#include "ubse_mem_controller.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;
/* *
 * Fd内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdBorrow(const UbseMemFdBorrowReq &req, UbseMemOperationResp &resp);

/* *
 * Fd内存借用设置导入设备权限
 * @param req [IN] 请求参数
 * @param realRequestNodeId [IN] 从通信链路中获取的请求节点ID
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdPermission(const UbseMemFdPermissionReq &req, const std::string &realRequestNodeId);

/* *
 * Fd类型内存的Export对象回调
 * @param exportObj [IN] Fd类型内存的Export对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdBorrowExportObjCallback(const UbseMemFdBorrowExportObj &exportObj);

/* *
 * Fd类型内存的Import对象回调
 * @param exportObj [IN] Fd类型内存的Import对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdBorrowImportObjCallback(const UbseMemFdBorrowImportObj &importObj);

/* *
 * Fd类型内存的Import对象设置权限回调
 * @param exportObj [IN] Fd类型内存的Import对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdBorrowImportObjForPermissionCallback(const UbseMemFdBorrowImportObj &importObj);

uint32_t UbseMemFdReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp, const std::string &realRequestNodeId);

uint32_t CheckFdResourceState(const std::string &name, const std::string &importNodeId);

uint32_t DeleteFdExport(const UbseMemFdBorrowExportObj &exportObj);

uint32_t AddFdImport(const UbseMemFdBorrowImportObj &importObj);

uint32_t AddFdExport(const UbseMemFdBorrowExportObj &exportObj);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CONTROLLER_FD_API_H
