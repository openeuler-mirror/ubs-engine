/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MEMPOOLING_OVER_COMMIT_ELECTION_HANDLER_H
#define MEMPOOLING_OVER_COMMIT_ELECTION_HANDLER_H
#include <cstdint>
#include "ubse_election.h"
namespace mempooling::over_commit {
using namespace ubse::election;
uint32_t OverCommitSubscribeSwitchover();
uint32_t OverCommitSwitchoverHandler(UbseElectionEventType &type, UBSE_ID_TYPE &nodeId);
} // namespace mempooling::over_commit

#endif
