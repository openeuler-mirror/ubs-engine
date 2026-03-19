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

#ifndef UCACHE_AGENET_RPC_HANDLER_H
#define UCACHE_AGENET_RPC_HANDLER_H

#include <map>
#include <string>
#include "turbo_def.h"
#include "ubse_com.h"

namespace ucache::agent {

uint32_t InitAgentTaskProcessor();
void ProcessTask(const UbseByteBuffer &req, UbseByteBuffer &resp);
} // namespace ucache::agent

#endif /* UCACHE_AGENET_RPC_HANDLER_H */
