/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UCACHE_RPC_HANDLER_H
#define UCACHE_RPC_HANDLER_H

#include <string>
#include "turbo_ucache_interface.h"

namespace ucache::master {
using namespace turbo::ucache;
uint32_t DispatchTask(const TaskRequest& tReq, TaskResponse& tResp, const std::string& destNode);

}; // namespace ucache::master

#endif /* UCACHE_RPC_HANDLER_H */
