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

#include "ubse_vip_module.h"

#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_conf_module.h"
#include "ubse_conf_manager.h"
#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_module.h"

namespace ubse::vip {
using namespace ubse::election;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::http;
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");

CORE_MODULE_IMPL(UbseVipModule, ubse::election::UbseElectionModule, ubse::config::UbseConfModule);

UbseResult UbseVipModule::Initialize()
{
    auto ret = LoadConfig();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] LoadConfig failed";
        return ret;
    }

    ret = UbseVipManager::GetInstance().Init(config_);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] VipManager Init failed";
        return ret;
    }

    UBSE_LOG_INFO << "[VIP] Module Initialize success";
    return UBSE_OK;
}

void UbseVipModule::UnInitialize()
{
    UbseVipManager::GetInstance().Deinit();
    UBSE_LOG_INFO << "[VIP] Module UnInitialize completed";
}

UbseResult UbseVipModule::Start()
{
    auto ret = RegisterElectionHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] RegisterElectionHandlers failed";
        return ret;
    }

    UBSE_LOG_INFO << "[VIP] Module Start success";
    return UBSE_OK;
}

void UbseVipModule::Stop()
{
    UnregisterElectionHandlers();
    UBSE_LOG_INFO << "[VIP] Module Stop completed";
}

UbseResult UbseVipModule::LoadConfig()
{
    auto &ctx = UbseContext::GetInstance();
    auto confModule = ctx.GetModule<UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_ERROR << "[VIP] Get UbseConfModule failed";
        return UBSE_ERROR;
    }

    const std::string section = "ubse.vip";

    bool enable = false;
    auto ret = confModule->GetConf(section, "vip.enable", enable);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "[VIP] vip.enable not configured, default false";
        config_.enable = false;
        return UBSE_OK;
    }
    config_.enable = enable;

    if (!config_.enable) {
        UBSE_LOG_INFO << "[VIP] VIP management is disabled by config";
        return UBSE_OK;
    }

    ret = confModule->GetConf(section, "vip.httpServer.listen.ip", config_.listenIp);
    if (ret != UBSE_OK || config_.listenIp.empty()) {
        UBSE_LOG_ERROR << "[VIP] vip.httpServer.listen.ip is required but not configured";
        return UBSE_ERROR;
    }

    uint32_t listenPort = 10002;
    ret = confModule->GetConf(section, "vip.httpServer.listen.port", listenPort);
    if (ret == UBSE_OK && listenPort >= 1024 && listenPort <= 65535) {
        config_.listenPort = listenPort;
    }

    uint32_t arpCount = 5;
    ret = confModule->GetConf(section, "vip.arpCount", arpCount);
    if (ret == UBSE_OK) {
        if (arpCount >= 1 && arpCount <= 10) {
            config_.arpCount = arpCount;
        } else {
            UBSE_LOG_WARN << "[VIP] vip.arpCount=" << arpCount << " is out of range [1, 10], using default: "
                          << config_.arpCount;
        }
    }

    uint32_t arpInterval = 200;
    ret = confModule->GetConf(section, "vip.arpInterval", arpInterval);
    if (ret == UBSE_OK) {
        if (arpInterval >= 100 && arpInterval <= 5000) {
            config_.arpInterval = arpInterval;
        } else {
            UBSE_LOG_WARN << "[VIP] vip.arpInterval=" << arpInterval << " is out of range [100, 5000], using default: "
                          << config_.arpInterval;
        }
    }

    UBSE_LOG_INFO << "[VIP] Config loaded: listenIp=" << config_.listenIp
                  << ", listenPort=" << config_.listenPort
                  << ", arpCount=" << config_.arpCount << ", arpInterval=" << config_.arpInterval;
    return UBSE_OK;
}

UbseResult UbseVipModule::RegisterElectionHandlers()
{
    if (!config_.enable) {
        return UBSE_OK;
    }

    UbseElectionHandlerBuilder masterBuilder;
    masterBuilder.SetType(UbseElectionEventType::CHANGE_TO_MASTER)
        .SetPriority(UbseElectionHandlerPriority::HIGH)
        .SetSequenceId(50)
        .SetName("VipChangeToMasterHandler")
        .SetHandler([this](UbseElectionEventType &type, UBSE_ID_TYPE &nodeId) {
            return HandleChangeToMaster(type, nodeId);
        });
    auto ret = UbseElectionChangeAttachHandler(masterBuilder.Build());
    if (ret != UbseElectionOk) {
        UBSE_LOG_ERROR << "[VIP] Register CHANGE_TO_MASTER handler failed, ret=" << ret;
        return UBSE_ERROR;
    }

    UbseElectionHandlerBuilder standbyToMasterBuilder;
    standbyToMasterBuilder.SetType(UbseElectionEventType::STANDBY_CHANGE_TO_MASTER)
        .SetPriority(UbseElectionHandlerPriority::HIGH)
        .SetSequenceId(50)
        .SetName("VipStandbyChangeToMasterHandler")
        .SetHandler([this](UbseElectionEventType &type, UBSE_ID_TYPE &nodeId) {
            return HandleStandbyChangeToMaster(type, nodeId);
        });
    ret = UbseElectionChangeAttachHandler(standbyToMasterBuilder.Build());
    if (ret != UbseElectionOk) {
        UBSE_LOG_ERROR << "[VIP] Register STANDBY_CHANGE_TO_MASTER handler failed, ret=" << ret;
        return UBSE_ERROR;
    }

    UbseElectionHandlerBuilder standbyBuilder;
    standbyBuilder.SetType(UbseElectionEventType::CHANGE_TO_STANDBY)
        .SetPriority(UbseElectionHandlerPriority::HIGH)
        .SetSequenceId(50)
        .SetName("VipChangeToStandbyHandler")
        .SetHandler([this](UbseElectionEventType &type, UBSE_ID_TYPE &nodeId) {
            return HandleChangeToStandby(type, nodeId);
        });
    ret = UbseElectionChangeAttachHandler(standbyBuilder.Build());
    if (ret != UbseElectionOk) {
        UBSE_LOG_ERROR << "[VIP] Register CHANGE_TO_STANDBY handler failed, ret=" << ret;
        return UBSE_ERROR;
    }

    UbseElectionHandlerBuilder agentBuilder;
    agentBuilder.SetType(UbseElectionEventType::CHANGE_TO_AGENT)
        .SetPriority(UbseElectionHandlerPriority::HIGH)
        .SetSequenceId(50)
        .SetName("VipChangeToAgentHandler")
        .SetHandler([this](UbseElectionEventType &type, UBSE_ID_TYPE &nodeId) {
            return HandleChangeToAgent(type, nodeId);
        });
    ret = UbseElectionChangeAttachHandler(agentBuilder.Build());
    if (ret != UbseElectionOk) {
        UBSE_LOG_ERROR << "[VIP] Register CHANGE_TO_AGENT handler failed, ret=" << ret;
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "[VIP] Election handlers registered";
    return UBSE_OK;
}

void UbseVipModule::UnregisterElectionHandlers()
{
    if (!config_.enable) {
        return;
    }

    UbseElectionHandlerBuilder builder;
    builder.SetPriority(UbseElectionHandlerPriority::HIGH).SetSequenceId(50);

    builder.SetType(UbseElectionEventType::CHANGE_TO_MASTER).SetName("VipChangeToMasterHandler");
    UbseElectionChangeDeAttachHandler(builder.Build());

    builder.SetType(UbseElectionEventType::STANDBY_CHANGE_TO_MASTER).SetName("VipStandbyChangeToMasterHandler");
    UbseElectionChangeDeAttachHandler(builder.Build());

    builder.SetType(UbseElectionEventType::CHANGE_TO_STANDBY).SetName("VipChangeToStandbyHandler");
    UbseElectionChangeDeAttachHandler(builder.Build());

    builder.SetType(UbseElectionEventType::CHANGE_TO_AGENT).SetName("VipChangeToAgentHandler");
    UbseElectionChangeDeAttachHandler(builder.Build());

    UBSE_LOG_INFO << "[VIP] Election handlers unregistered";
}

uint32_t UbseVipModule::HandleChangeToMaster(UbseElectionEventType &, UBSE_ID_TYPE &nodeId)
{
    UBSE_LOG_INFO << "[VIP] CHANGE_TO_MASTER event, nodeId=" << nodeId;
    auto ret = UbseVipManager::GetInstance().BindVip();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] BindVip failed on CHANGE_TO_MASTER";
        return UbseElectionError;
    }
    return UbseElectionOk;
}

uint32_t UbseVipModule::HandleStandbyChangeToMaster(UbseElectionEventType &, UBSE_ID_TYPE &nodeId)
{
    UBSE_LOG_INFO << "[VIP] STANDBY_CHANGE_TO_MASTER event, nodeId=" << nodeId;
    auto ret = UbseVipManager::GetInstance().BindVip();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] BindVip failed on STANDBY_CHANGE_TO_MASTER";
        return UbseElectionError;
    }
    return UbseElectionOk;
}

uint32_t UbseVipModule::HandleChangeToStandby(UbseElectionEventType &, UBSE_ID_TYPE &nodeId)
{
    UBSE_LOG_INFO << "[VIP] CHANGE_TO_STANDBY event, nodeId=" << nodeId;
    auto ret = UbseVipManager::GetInstance().UnbindVip();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] UnbindVip failed on CHANGE_TO_STANDBY";
        return UbseElectionError;
    }
    return UbseElectionOk;
}

uint32_t UbseVipModule::HandleChangeToAgent(UbseElectionEventType &, UBSE_ID_TYPE &nodeId)
{
    UBSE_LOG_INFO << "[VIP] CHANGE_TO_AGENT event, nodeId=" << nodeId;
    auto ret = UbseVipManager::GetInstance().UnbindVip();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] UnbindVip failed on CHANGE_TO_AGENT";
        return UbseElectionError;
    }
    return UbseElectionOk;
}

} // namespace ubse::vip