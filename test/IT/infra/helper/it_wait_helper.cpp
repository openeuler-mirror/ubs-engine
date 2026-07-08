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

#include "it_wait_helper.h"

#include <sys/stat.h>
#include <thread>

#include "ubse_error.h"
#include "it_console_log.h"
#include "ubs_error.h"

namespace ubse::it::infra {

UbseResult ItWaitHelper::WaitForCondition(std::function<bool()> condition, uint32_t timeoutMs, uint32_t pollIntervalMs)
{
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        if (condition()) {
            return UBSE_OK;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
    }
    // Final check after timeout
    if (condition()) {
        return UBSE_OK;
    }
    IT_LOG_ERROR << "WaitForCondition timed out after " << timeoutMs << "ms";
    return UBSE_ERR_TIMED_OUT;
}

UbseResult ItWaitHelper::WaitForElectionConvergence(const std::map<std::string, ItCliInvoker*>& cliInvokers,
                                                    const std::vector<std::string>& nodeIds, uint32_t timeoutMs,
                                                    uint32_t pollIntervalMs)
{
    constexpr uint32_t REQUIRED_STABLE_CONVERGENCE_POLLS = 2;
    uint32_t stableConvergenceCount = 0;

    auto convergenceCheck = [&]() -> bool {
        bool hasMaster = false;
        bool hasStandby = false;
        uint32_t queriedNodeCount = 0;

        for (const auto& nodeId : nodeIds) {
            auto it = cliInvokers.find(nodeId);
            if (it == cliInvokers.end() || !it->second) {
                stableConvergenceCount = 0;
                return false;
            }

            std::string role;
            int32_t ret = it->second->GetRole(role);
            if (ret != UBS_SUCCESS) {
                stableConvergenceCount = 0;
                return false;
            }
            ++queriedNodeCount;

            if (role == ubse::election::ELECTION_ROLE_MASTER) {
                hasMaster = true;
            } else if (role == ubse::election::ELECTION_ROLE_STANDBY) {
                hasStandby = true;
            }
        }

        if (queriedNodeCount != nodeIds.size()) {
            stableConvergenceCount = 0;
            return false;
        }

        // Single-node cluster: just need a master
        bool converged = nodeIds.size() == 1 ? hasMaster : (hasMaster && hasStandby);
        if (!converged) {
            stableConvergenceCount = 0;
            return false;
        }

        ++stableConvergenceCount;
        if (stableConvergenceCount >= REQUIRED_STABLE_CONVERGENCE_POLLS) {
            IT_LOG_INFO << "Election converged by CLI after " << stableConvergenceCount
                        << " consecutive successful poll(s)";
        }
        return stableConvergenceCount >= REQUIRED_STABLE_CONVERGENCE_POLLS;
    };

    return WaitForCondition(convergenceCheck, timeoutMs, pollIntervalMs);
}

UbseResult ItWaitHelper::WaitForNodeReadiness(const std::string& udsSocketPath, uint32_t timeoutMs)
{
    auto readinessCheck = [&]() -> bool {
        struct stat st {
        };
        return stat(udsSocketPath.c_str(), &st) == 0;
    };

    return WaitForCondition(readinessCheck, timeoutMs, DEFAULT_POLL_INTERVAL_MS);
}

UbseResult ItWaitHelper::WaitForNodeRole(ItCliInvoker& cliInvoker, const std::string& expectedRole, uint32_t timeoutMs)
{
    auto roleCheck = [&]() -> bool {
        std::string role;
        int32_t ret = cliInvoker.GetRole(role);
        if (ret != UBS_SUCCESS) {
            return false;
        }
        return role == expectedRole;
    };

    return WaitForCondition(roleCheck, timeoutMs, DEFAULT_ELECTION_POLL_INTERVAL_MS);
}

} // namespace ubse::it::infra
