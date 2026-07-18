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

#ifndef UBS_ENGINE_UBSE_MEM_SHARE_FORWARD_H
#define UBS_ENGINE_UBSE_MEM_SHARE_FORWARD_H

#include <string>

#include "ubse_common_def.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller {
using namespace ubse::common::def;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemShareAttachReq;
using ubse::adapter_plugins::mmi::UbseMemReturnReq;
using ubse::adapter_plugins::mmi::UbseMemShareDetachReq;

// ==================== 转发语义原子 ====================
//
// 每个原子只负责"把某对象/请求发到给定目标节点"：内部选定 opCode、组装 simpo、
// 走统一的重试发送底座。原子不解析目标节点、不构造失败响应——这些编排在 share_api。

// ---- 下发到执行者（agent） ----
UbseResult ForwardExportObjToExecutor(const UbseMemShareBorrowExportObj &exportObj, const std::string &executorNodeId);
UbseResult ForwardImportObjToExecutor(const UbseMemShareBorrowImportObj &importObj, const std::string &executorNodeId);

// ---- 全局主下发到 cascade master（内部按 agent 节点解析 cascade master） ----
UbseResult ForwardExportObjToCascade(const UbseMemShareBorrowExportObj &exportObj);
UbseResult ForwardImportObjToCascade(const UbseMemShareBorrowImportObj &importObj);
UbseResult ForwardDetachReqToCascade(const UbseMemShareDetachReq &req);
UbseResult ForwardDeleteToCascade(const UbseMemShareBorrowExportObj &exportObj);

// ---- 执行者（agent）上报本地主 ----
UbseResult ReportExportObjToLocalMaster(const UbseMemShareBorrowExportObj &exportObj);
UbseResult ReportImportObjToLocalMaster(const UbseMemShareBorrowImportObj &importObj);

// ---- cascade master 请求上转到全局主（内部解析全局主节点） ----
UbseResult ForwardBorrowReqToGlobal(const UbseMemShareBorrowReq &req);
UbseResult ForwardAttachReqToGlobal(const UbseMemShareAttachReq &req);
UbseResult ForwardReturnReqToGlobal(const UbseMemReturnReq &req);

// ---- cascade master 回调上报到全局主（内部解析全局主节点） ----
UbseResult ForwardExportCallbackToGlobal(const UbseMemShareBorrowExportObj &exportObj);
UbseResult ForwardImportCallbackToGlobal(const UbseMemShareBorrowImportObj &importObj);
UbseResult ForwardDeleteCallbackToGlobal(const UbseMemShareBorrowExportObj &exportObj);
UbseResult ForwardDetachCallbackToGlobal(const UbseMemShareBorrowImportObj &importObj);

// ---- detach 三级链 ----
UbseResult ForwardDetachReqToPd(const UbseMemShareDetachReq &req);
UbseResult ForwardDetachReqToGlobal(const UbseMemShareDetachReq &req);

} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_SHARE_FORWARD_H
