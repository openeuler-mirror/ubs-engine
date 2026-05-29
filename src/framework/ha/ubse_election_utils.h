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

#ifndef UBSE_MANAGER_JSONTOVEC_H
#define UBSE_MANAGER_JSONTOVEC_H

#include <vector>
#include "ubse_common_def.h"
#include "ubse_election_comm_mgr.h"
#include "ubse_error.h"

namespace ubse::election::utils {
using ubse::election::NodeLinkInfo;
std::vector<std::string> Split(const std::string& s, char delimiter);
std::vector<NodeLinkInfo> ParseEventMessage(const std::string& eventMessage);
bool parseFaultEventMsg(const std::string& msg, std::string& faultNodeId, std::string& faultType);
} // namespace ubse::election::utils
#endif // UBSE_MANAGER_JSONTOVEC_H
