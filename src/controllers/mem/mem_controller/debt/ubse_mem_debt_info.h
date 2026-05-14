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

#ifndef UBSE_MEM_DEBT_INFO_H
#define UBSE_MEM_DEBT_INFO_H
#include "ubse_mmi_interface.h"
#include "lock/ubse_lock.h"

namespace ubse::mem::controller {
using namespace ubse::utils;
using namespace ubse::adapter_plugins::mmi;

/**
* 获取全量账本
* @return 账本
*
*/
NodeMemDebtInfoMap GetNodeMemDebtInfoMap();

/**
* 根据节点id查询账本
* @param nodeId
* @return 账本
*/
NodeMemDebtInfo GetNodeMemDebtInfoById(const std::string& nodeId);

/**
* 根据节点id查询账本，并过滤已删除账本
* @param nodeId
* @return 账本
*/
NodeMemDebtInfo GetNoDeletedNodeMemDebtInfoById(const std::string& nodeId);
} // namespace ubse::mem::controller
#endif // UBSE_MEM_DEBT_INFO_H
