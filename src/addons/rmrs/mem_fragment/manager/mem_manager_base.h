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

#ifndef MEM_MANAGER_BASE_H
#define MEM_MANAGER_BASE_H

#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "mem_json_def.h"
#include "mempooling_message.h"
#include "mp_anti_param_json_util.h"
#include "mp_error.h"
#include "mp_memory_info.h"
#include "mp_module.h"
#include "mp_sync_data_helper.h"
#include "turbo_rmrs_interface.h"
#include "ubse_common_def.h"
#include "ubse_def.h"
#include "ubse_mem_controller.h"
#include "ubse_election.h"

namespace mempooling {

uint32_t GetNodeInfoImmediatelyRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
uint32_t SyncAntiDataRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
uint32_t SyncAntiDataStandByRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
uint32_t GetAllNodeInfoImmediatelyRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
uint32_t UpdateDataBaseAndCache(UbseByteBuffer &buffer, const MpUpdateAntiNodeParam &antiParam,
                                ubse::election::UbseRoleInfo &standbyRole);
} // namespace mempooling

#endif // MEM_MANAGER_BASE_H