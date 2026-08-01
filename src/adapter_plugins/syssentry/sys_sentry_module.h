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

#ifndef UBSE_MANAGER_SYS_SENTRY_MODULE_H
#define UBSE_MANAGER_SYS_SENTRY_MODULE_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace syssentry {
using ubse::common::def::UbseResult;
using ubse::module::UbseModule;

class SysSentryModule : public UbseModule {
public:
    static constexpr const char* kModuleName = "SysSentryModule";
    std::string Name() const override
    {
        return kModuleName;
    }

    ~SysSentryModule() override = default;

    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

private:
    UbseResult SubscribeBroadcastDomainEvents();
    void UnsubscribeBroadcastDomainEvents();

    bool nodeDiscoverySubscribed_{false};
};

struct SysSentryBroadcastDomain {
    std::string clientEid;
    std::string clientCna;
    std::string serverEids;
    std::string serverCnas;
    std::vector<uint32_t> peerNodeIds;
};

UbseResult SetSysSentryFaultReporter();
UbseResult RefreshSysSentryFaultBroadcastDomain();
void ResetSysSentryFaultBroadcastDomain();
UbseResult BuildSysSentryBroadcastDomain(SysSentryBroadcastDomain& domain);
UbseResult BuildClosBroadcastDomain(
    const std::string& localNodeId, const std::vector<uint32_t>& peerNodeIds,
    const std::map<ubse::adapter_plugins::mti::UbseMtiIouInfo, ubse::adapter_plugins::mti::UbseMtiEidGroup>& allEids,
    SysSentryBroadcastDomain& domain);
UbseResult Build1dCnaConfig(const std::string& localNodeId,
                            const ubse::adapter_plugins::mti::UbseMtiCpuTopoInfoMap& topology, std::string& clientCna,
                            std::string& serverCnas);
} // namespace syssentry
#endif // UBSE_MANAGER_SYS_SENTRY_MODULE_H
