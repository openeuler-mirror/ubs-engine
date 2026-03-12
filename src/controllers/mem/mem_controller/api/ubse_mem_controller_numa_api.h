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

#ifndef UBS_ENGINE_UBSE_MEM_CONTROLLER_NUMA_API_H
#define UBS_ENGINE_UBSE_MEM_CONTROLLER_NUMA_API_H
#include "ubse_mmi_interface.h"
#include "ubse_mem_controller.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;
/* *
 * Numa内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemNumaBorrow(const UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp);

/* *
 * Numa类型内存的Export对象回调
 * @param exportObj [IN] Numa类型内存的Export对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemNumaBorrowExportObjCallback(const UbseMemNumaBorrowExportObj &exportObj);

/* *
 * Numa类型内存的Import对象回调
 * @param exportObj [IN] Numa类型内存的Import对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemNumaBorrowImportObjCallback(const UbseMemNumaBorrowImportObj &importObj);

uint32_t UbseMemNumaReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp, const std::string &realRequestNodeId);

uint32_t CheckNumaResourceState(const std::string &name, const std::string &importNodeId);

uint32_t DeleteNumaExport(const UbseMemNumaBorrowExportObj &exportObj);

uint32_t AddNumaImport(const UbseMemNumaBorrowImportObj &importObj);

uint32_t AddNumaExport(const UbseMemNumaBorrowExportObj &exportObj);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CONTROLLER_NUMA_API_H
