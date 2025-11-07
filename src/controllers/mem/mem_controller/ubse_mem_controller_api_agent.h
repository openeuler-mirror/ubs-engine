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
#include "ubse_mem_obj.h"

namespace ubse::mem::controller::agent {
using namespace ubse::mem::obj;
#ifdef NDEBUG
#ifdef UB_ENVIRONMENT
const std::chrono::seconds WAIT_TIMEOUT(3600); // 秒
#else
const std::chrono::seconds WAIT_TIMEOUT(60); // 秒
#endif
#else
#ifdef UBSE_MEM_CONTROLLER_UT
const std::chrono::seconds WAIT_TIMEOUT(1); // 秒
#else
const std::chrono::seconds WAIT_TIMEOUT(60000000); // 秒
#endif
#endif

const uint32_t MAX_TIMEOUT_SECONDS = 3600; // 1小时

uint32_t Init();

/* *
 * Fd内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdBorrow(const UbseMemFdBorrowReq &req, UbseMemOperationResp &resp);

/* *
 * Numa内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemNumaBorrow(const UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp);

/* *
 * 内存归还，适用于所有借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp);
} // namespace ubse::mem::controller::agent
#endif
// UBSE_MEM_CONTROLLER_API_AGENT_H
