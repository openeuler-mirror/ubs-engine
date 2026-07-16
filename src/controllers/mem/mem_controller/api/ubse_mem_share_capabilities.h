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

#ifndef UBS_ENGINE_UBSE_MEM_SHARE_CAPABILITIES_H
#define UBS_ENGINE_UBSE_MEM_SHARE_CAPABILITIES_H

#include <cstdint>
#include <string>
#include <vector>

#include "ubse_error.h"
#include "ubse_mem_share_store.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller {
using namespace ubse::common::def;
using ubse::adapter_plugins::mmi::UbseMemShareAttachReq;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemReturnReq;
using ubse::adapter_plugins::mmi::UbseMemShareDetachReq;

// ==================== 家族一：逻辑能力（拓扑无关，纯业务，无网络） ====================
//
// 仅依赖入参与注入的 IShareStore；不含响应构造与 advice（编排层负责）。

// borrow 请求归一：补全 shmRegion 节点列表
void NormalizeShareRegion(UbseMemShareBorrowReq &req);

// borrow 亲和参数校验
bool ValidateAffinityParams(const UbseMemShareBorrowReq &req);

// borrow 查重：命中同名活跃 export/import 返回 true
bool CheckBorrowDuplicate(const std::string &name, IShareStore &store);

// 依据 shmRegion 节点列表设置节点索引
uint32_t SetNodeIndex(UbseMemShareBorrowReq &req);

// borrow 分配：填充 exportObj.algoResult
UbseResult ShareAllocate(const UbseMemShareBorrowReq &req, UbseMemShareBorrowExportObj &exportObj);

// ---- attach 逻辑能力 ----

// 校验导入节点是否落在 export 的 shmRegion 节点列表内
bool CheckRegions(const UbseMemShareAttachReq &req, const UbseMemShareBorrowExportObj &exportObj);

// 在既有 import 列表中查找匹配的活跃 importObj
bool ExistImportObj(const std::string &name, const std::string &nodeId,
                    const std::vector<UbseMemShareBorrowImportObj> &existImportObjs,
                    UbseMemShareBorrowImportObj &importObj);

// 依据 attachReq 组装 importObj 字段（纯逻辑，不落存储；存储由编排层显式调用 store）
void ConstructShareImportObj(UbseMemShareBorrowImportObj &importObj, const UbseMemShareAttachReq &req);

} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_SHARE_CAPABILITIES_H
