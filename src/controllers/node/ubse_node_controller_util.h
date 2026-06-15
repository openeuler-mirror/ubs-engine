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

#ifndef UBSE_NODE_CONTROLLER_UTIL_H
#define UBSE_NODE_CONTROLLER_UTIL_H

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_def.h"
#include "ubse_logger.h"
#include "ubse_node_controller.h"

namespace ubse::nodeController {
#define MODULE_LOG_NAME "ubse"

enum class UbseClosNodeRole {
    UNKNOWN,
    CABINET_AGENT,
    CABINET_MASTER,
    PD_MASTER,
    GLOBAL_MASTER
};

class UbseNodeControllerLockMgr {
public:
    static void WriteLock(const std::string &nodeId);

    static void WriteUnLock(const std::string &nodeId);

    static void TryWriteLock(const std::string &nodeId);

    static void ReadLock(const std::string &nodeId);

    static void ReadUnLock(const std::string &nodeId);

    static bool TryReadLock(const std::string &nodeId);

private:
    static std::shared_ptr<std::shared_mutex> GetLock(const std::string &nodeId);

private:
    static std::mutex nodeControllerMutex_;
    static std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> nodeControllerLocks_;
};

void GetCurNodeInfo(UbseNodeInfo &info);

UbseAllocator GetAllocator();

uint32_t GetPodId();

ubse::common::def::UbseResult GetClosHaTopology(ubse::election::HaTopologyInfo &topology);

ubse::common::def::UbseResult GetPrevReportNodeId(std::string &prevNodeId);

UbseClosNodeRole DetectClosRole();

UbseClosNodeRole GetClosRole();

bool IsCabinetMaster();

bool IsPdMaster();

bool IsGlobalMaster();

uint32_t IpToUint32(const std::string &ipStr, uint32_t &ip);

std::string Uint32ToIp(uint32_t ip);

std::vector<std::string> ParseIpList(const std::string &ipList);

uint32_t FindSameNetMask(std::string ipStr, std::string &localIp);
#undef MODULE_LOG_NAME
} // namespace ubse::nodeController

#endif // UBSE_NODE_CONTROLLER_UTIL_H