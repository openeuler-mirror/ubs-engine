/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_SSU_MASTER_ONLINE_HANDLER_H
#define UBSE_SSU_MASTER_ONLINE_HANDLER_H

#include <string>
#include "ubse_election.h"

namespace ubse::ssu::service {

using ubse::election::UBSE_ID_TYPE;
using ubse::election::UbseElectionEventType;

class UbseSsuMasterOnlineHandler {
public:
    static void Initial();

    static void Uninitial();

private:
    static uint32_t MasterOnlineHandler(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId);
};

} // namespace ubse::ssu::service
#endif // UBSE_SSU_MASTER_ONLINE_HANDLER_H
