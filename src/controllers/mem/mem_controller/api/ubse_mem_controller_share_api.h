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
#include <string>
#include <vector>

#include "ubse_mem_controller.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller {
using ubse::adapter_plugins::mmi::UbMemFaultType;
using ubse::adapter_plugins::mmi::UbseMemOperationResp;
using ubse::adapter_plugins::mmi::UbseMemReturnReq;
using ubse::adapter_plugins::mmi::UbseMemShareAttachReq;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemShareDetachReq;
/* *
 * 共享内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareBorrow(const UbseMemShareBorrowReq& req, UbseMemOperationResp& resp);

/* *
 * 挂载内存
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareAttach(const UbseMemShareAttachReq& req, UbseMemOperationResp& resp);

/* *
 * 去挂载内存
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareDetach(const UbseMemShareDetachReq& req, UbseMemOperationResp& resp,
                            const std::string& realRequestNodeId);

/* *
 * Share类型内存的Export对象回调
 * @param exportObj [IN] Share类型内存的Export对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareBorrowExportObjCallback(const UbseMemShareBorrowExportObj& exportObj);

/* *
 * Share类型内存的Import对象回调
 * @param exportObj [IN] Share类型内存的Import对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareBorrowImportObjCallback(const UbseMemShareBorrowImportObj& importObj);

uint32_t UbseMemShareReturn(const UbseMemReturnReq& req, UbseMemOperationResp& resp,
                            const std::string& realRequestNodeId);

uint32_t UpdateFaultShareExportObj(const std::string& nodeId, uint64_t memId, const std::string& memName,
                                   UbMemFaultType type);

uint32_t AddShareImport(const UbseMemShareBorrowImportObj& importObj);

uint32_t AddShareExport(const UbseMemShareBorrowExportObj& exportObj);

uint32_t DeleteShareExport(const UbseMemShareBorrowExportObj& exportObj);

/* *
 * 按name查找当前有效(非DESTROYED)的借用关系: 借用域查询/迁移逐块迁移前置共用。
 * exportObjs 仅收 EXPORT_SUCCESS 的导出对象; importObjs 收非 DESTROYED 的导入对象。
 * @param name [IN] 借用标识
 * @param exportObjs [OUT] 命中的导出对象
 * @param importObjs [OUT] 命中的导入对象
 */
void FindShareBorrowObjByName(const std::string& name, std::vector<UbseMemShareBorrowExportObj>& exportObjs,
                              std::vector<UbseMemShareBorrowImportObj>& importObjs);

/* *
 * 统一决策入口: 特性检查 + (非迁移)名字存在性检查 + 亲和性校验 + SetNodeIndex + ShareAllocate。
 * isMigrate=true: 跳过名字存在性检查(老obj必然存在, 互斥由迁移会话保证), 迁移新导出准备与普通借用共用。
 * 成功时 exportObj.algoResult 已填好决策结果; 失败返回 API 级错误码, 由调用方构建 resp/advice。
 * @param req [IN] 借用请求
 * @param exportObj [OUT] 决策产物
 * @param isMigrate [IN] 是否迁移借用
 * @return 0: 成功; 非0: 失败
 */
uint32_t ShareBorrowSchedule(const UbseMemShareBorrowReq& req, UbseMemShareBorrowExportObj& exportObj, bool isMigrate);

/* *
 * agent侧执行核心：账本登记(EXPORT_RUNNING) + ShmExportExecutor(obmm_export) + 高安签名。
 * 不含任何回发master的逻辑，普通借用异步链与迁移同步链共用；
 * 失败时账本清理由调用方负责(异步链 EraseShareExport，迁移链 master 侧 Rollback)。
 * @param exportObj [IN/OUT] 借出对象(含决策结果与req)
 * @param exportNodeId [IN] 导出节点id
 * @return 0: 成功; 非0: 失败
 */
uint32_t ShareExportExecute(UbseMemShareBorrowExportObj& exportObj, const std::string& exportNodeId);
void ShareExportUpdateState(UbseMemShareBorrowExportObj& exportObj,
                            const ubse::adapter_plugins::mmi::UbseMemState& state);
void EraseShareExport(const UbseMemShareBorrowExportObj& exportObj);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CONTROLLER_SHARE_API_H
