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

#ifndef UBS_ENGINE_UBSE_MEM_CONTROLLER_SHARE_API_H
#define UBS_ENGINE_UBSE_MEM_CONTROLLER_SHARE_API_H
#include "ubse_mmi_interface.h"
#include "ubse_mem_controller.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;
/* *
 * 共享内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareBorrow(const UbseMemShareBorrowReq &req, UbseMemOperationResp &resp);

/* *
 * 挂载内存
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareAttach(const UbseMemShareAttachReq &req, UbseMemOperationResp &resp);

/* *
 * 去挂载内存
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareDetach(const UbseMemShareDetachReq &req, UbseMemOperationResp &resp, const std::string &realRequestNodeId);

/* *
 * Share类型内存的Export对象回调
 * @param exportObj [IN] Share类型内存的Export对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareBorrowExportObjCallback(const UbseMemShareBorrowExportObj &exportObj);

/* *
 * Share类型内存的Import对象回调
 * @param exportObj [IN] Share类型内存的Import对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareBorrowImportObjCallback(const UbseMemShareBorrowImportObj &importObj);

uint32_t UbseMemShareReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp, const std::string &realRequestNodeId);

uint32_t UpdateFaultShareExportObj(const std::string &nodeId, uint64_t memId, const std::string &memName,
                                   UbMemFaultType type);

uint32_t AddShareImport(const UbseMemShareBorrowImportObj &importObj);

uint32_t AddShareExport(const UbseMemShareBorrowExportObj &exportObj);

uint32_t DeleteShareExport(const UbseMemShareBorrowExportObj &exportObj);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CONTROLLER_SHARE_API_H
