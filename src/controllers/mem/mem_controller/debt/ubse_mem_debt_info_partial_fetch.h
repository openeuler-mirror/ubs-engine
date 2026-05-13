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

#ifndef UBS_ENGINE_UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_H
#define UBS_ENGINE_UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_H

#include "ubse_common_def.h"
#include "../message/ubse_mem_debt_info_partial_fetch_req.h"
#include "../message/ubse_mem_debt_info_partial_fetch_res.h"
namespace ubse::mem::controller::debt {
ubse::common::def::UbseResult FetchDebtInfoByTypeAndPage(
    const ubse::mem::controller::message::DebtFetchInfo& debtFetchInfo, message::PartialFetchRes& flatDebtInformation);
ubse::common::def::UbseResult ValidateDebtFetchInfo(message::DebtFetchInfo debtFetchInfo);
} // namespace ubse::mem::controller::debt

#endif // UBS_ENGINE_UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_H
