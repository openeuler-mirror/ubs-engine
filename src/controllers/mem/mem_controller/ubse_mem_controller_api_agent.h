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

#ifndef UBSE_MEM_CONTROLLER_API_AGENT_H
#define UBSE_MEM_CONTROLLER_API_AGENT_H

#include <chrono>
#include "ubse_api_server.h"
#include "ubse_election.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::agent {
using ::api::server::UbseIpcMessage;
using ::api::server::UbseRequestContext;
using ubse::adapter_plugins::mmi::MemOperationType;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemOperationResp;
using ubse::adapter_plugins::mmi::UbseMemReturnReq;
using ubse::adapter_plugins::mmi::UbseMemShareAttachReq;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemShareDetachReq;
using ubse::common::def::UbseResult;

const uint32_t MAX_TIMEOUT_SECONDS = 3600; // 1小时

uint32_t Init();

uint32_t UbseMemNumaBorrow(UbseMemNumaBorrowReq& req, UbseMemOperationResp& resp);

/* *
 * Fd内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdBorrow(UbseMemFdBorrowReq& req, UbseMemOperationResp& resp);

/* *
 * 地址内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemAddrBorrow(UbseMemAddrBorrowReq& req, UbseMemOperationResp& resp);

/* *
 * 共享内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemShareBorrow(UbseMemShareBorrowReq& req, UbseMemOperationResp& resp);

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
uint32_t UbseMemShareDetach(const UbseMemShareDetachReq& req, UbseMemOperationResp& resp);

/* *
 * 内存归还，适用于所有借用
 * @param req [IN] 请求参数
 * @param type [IN] 归还类型
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemReturn(const UbseMemReturnReq& req, const MemOperationType& type, UbseMemOperationResp& resp);

UbseResult DealLinkInfo(const std::string& linkInfo, UbseMemNumaBorrowReq& numaBorrowReq,
                        const election::UbseRoleInfo& currentNodeInfo, std::string& errorMsg);
UbseResult FillSrcNuma(UbseMemNumaBorrowReq& numaBorrowReq, const election::UbseRoleInfo& currentNodeInfo);

uint32_t DeleteMemoryHandler(const UbseIpcMessage& request, const UbseRequestContext& context);
} // namespace ubse::mem::controller::agent
#endif
// UBSE_MEM_CONTROLLER_API_AGENT_H
