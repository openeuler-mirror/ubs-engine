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

#ifndef UBSE_VIP_MODULE_H
#define UBSE_VIP_MODULE_H

#include <memory>

#include "ubse_module.h"
#include "ubse_vip_manager.h"
#include "ubse_vip_injection_handler.h"
#include "ubse_election.h"
#include "ubse_common_def.h"

namespace ubse::vip {
using namespace ubse::module;
using namespace ubse::common::def;
using namespace ubse::election;

class UbseVipModule : public UbseModule {
public:
    static constexpr const char *kModuleName = "UbseVipModule";
    std::string Name() const override { return kModuleName; }

    UbseResult Initialize() override;
    void UnInitialize() override;
    UbseResult Start() override;
    void Stop() override;

private:
    UbseResult LoadConfig();
    UbseResult RegisterElectionHandlers();
    void UnregisterElectionHandlers();
    UbseResult RegisterContainerInjectionHandler();
    void UnregisterContainerInjectionHandler();

    uint32_t HandleChangeToMaster(UbseElectionEventType &type, UBSE_ID_TYPE &nodeId);
    uint32_t HandleStandbyChangeToMaster(UbseElectionEventType &type, UBSE_ID_TYPE &nodeId);
    uint32_t HandleChangeToStandby(UbseElectionEventType &type, UBSE_ID_TYPE &nodeId);
    uint32_t HandleChangeToAgent(UbseElectionEventType &type, UBSE_ID_TYPE &nodeId);

    UbseVipConfig config_;
    // shared_ptr + weak_ptr 捕获:注销时 reset，在飞 Handle 由 lock 出的 shared_ptr 保活，避免 use-after-free
    std::shared_ptr<UbseVipInjectionHandler> injectionHandler_;   // 容器模式 UDS 注入处理器
};

} // namespace ubse::vip

#endif // UBSE_VIP_MODULE_H
