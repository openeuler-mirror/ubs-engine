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

#include <memory>

#include "ubse_api_server.h"
#include "ubse_ipc_common.h"
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

OPTIONAL_MODULE_IMPL(UbseVipModule, ubse::election::UbseElectionModule, ubse::config::UbseConfModule);

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

    // 在 Initialize 阶段注册 election handler：VIP 拓扑序晚于 UbseElectionModule（依赖 Election），
    // 故 Election 已 Init；又早于 Election Start（启动选主线程），故 handler 注册先于首次选主，
    // 避免 CHANGE_TO_MASTER 丢失。
    ret = RegisterElectionHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] RegisterElectionHandlers failed";
        return ret;
    }

    // 容器模式:注册 UDS 注入 handler
    if (config_.containerMode) {
        ret = RegisterContainerInjectionHandler();
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[VIP] RegisterContainerInjectionHandler failed";
            return ret;
        }
    }

    UBSE_LOG_INFO << "[VIP] Module Initialize success";
    return UBSE_OK;
}

void UbseVipModule::UnInitialize()
{
    UnregisterContainerInjectionHandler();
    UbseVipManager::GetInstance().Deinit();
    UBSE_LOG_INFO << "[VIP] Module UnInitialize completed";
}

UbseResult UbseVipModule::Start()
{
    UBSE_LOG_INFO << "[VIP] Module Start success";
    return UBSE_OK;
}

void UbseVipModule::Stop()
{
    UnregisterElectionHandlers();
    UnregisterContainerInjectionHandler();
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

    uint32_t rateLimitRps = 0;
    ret = confModule->GetConf(section, "vip.httpServer.rateLimitRps", rateLimitRps);
    if (ret == UBSE_OK) {
        if (rateLimitRps <= 10000) {  // 0=不限流，上限 10000 防误配置
            config_.rateLimitRps = rateLimitRps;
        } else {
            UBSE_LOG_WARN << "[VIP] vip.httpServer.rateLimitRps=" << rateLimitRps
                          << " is out of range [0, 10000], using default: " << config_.rateLimitRps;
        }
    }

    uint32_t maxQueuedRequests = 0;
    ret = confModule->GetConf(section, "vip.httpServer.maxQueuedRequests", maxQueuedRequests);
    if (ret == UBSE_OK) {
        if (maxQueuedRequests <= 100000) {  // 0=不限制，上限 10 万防误配置
            config_.maxQueuedRequests = maxQueuedRequests;
        } else {
            UBSE_LOG_WARN << "[VIP] vip.httpServer.maxQueuedRequests=" << maxQueuedRequests
                          << " is out of range [0, 100000], using default: " << config_.maxQueuedRequests;
        }
    }

    ret = confModule->GetConf(section, "vip.httpServer.listen.ip", config_.listenIp);
    if (ret != UBSE_OK || config_.listenIp.empty()) {
        // 容器模式:enable=true 且缺省 listenIp,配置经 UDS 由 helper 注入,不在此处报错。
        // 用 WARN 而非 INFO,避免"配置丢失"被误看成正常启动;并提示仅 helper 托管部署应走此分支。
        config_.containerMode = true;
        UBSE_LOG_WARN << "[VIP] listenIp not configured, entering container mode; VIP will bind only after UDS"
                      << " injection (expected for helper-managed container deployment; verify ubse-helper DaemonSet"
                      << " if this is a host deployment)";
        return UBSE_OK;
    }

    uint32_t listenPort = 10002;
    ret = confModule->GetConf(section, "vip.httpServer.listen.port", listenPort);
    if (ret == UBSE_OK && listenPort >= 1024 && listenPort <= 65535) {
        config_.listenPort = listenPort;
    }

    // 主机模式网卡名直接来自配置(替代原 /var/run/ubse/ubse_iface 文件);容器模式由 UDS 注入,不走此分支
    ret = confModule->GetConf(section, "vip.iface", config_.interface);
    if (ret != UBSE_OK || config_.interface.empty()) {
        UBSE_LOG_ERROR << "[VIP] vip.iface is not configured";
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "[VIP] Config loaded: listenIp=" << config_.listenIp
                  << ", listenPort=" << config_.listenPort
                  << ", iface=" << config_.interface
                  << ", rateLimitRps=" << config_.rateLimitRps
                  << ", maxQueuedRequests=" << config_.maxQueuedRequests;
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

UbseResult UbseVipModule::RegisterContainerInjectionHandler()
{
    injectionHandler_ = std::make_shared<UbseVipInjectionHandler>();
    // 以 weak_ptr 捕获:注销时 reset shared_ptr,回调内 lock 成 shared_ptr 保证 Handle 执行期间对象存活,
    // 消除"判空通过后、解引用前被 reset"的 check-then-use 竞态(use-after-free)。
    std::weak_ptr<UbseVipInjectionHandler> weakHandler = injectionHandler_;

    // 注册 IPC handler
    auto ret = api::server::RegisterIpcHandler(
        UBSE_VIP, UBSE_VIP_CFG_PUSH,
        [weakHandler](const api::server::UbseIpcMessage &msg, const api::server::UbseRequestContext &ctx) {
            auto handler = weakHandler.lock();
            if (!handler) {
                UBSE_LOG_WARN << "[VIP] injection handler already unregistered, reject late push";
                return UBSE_ERR_IPC_SERVICE_UNAVAILABLE;
            }
            return handler->Handle(msg, ctx);
        },
        "vip.cfg.push");

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] RegisterIpcHandler failed, ret=" << ret;
        injectionHandler_.reset();
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "[VIP] Container injection handler registered (UBSE_VIP, UBSE_VIP_CFG_PUSH)";
    return UBSE_OK;
}

void UbseVipModule::UnregisterContainerInjectionHandler()
{
    if (injectionHandler_) {
        UBSE_LOG_INFO << "[VIP] Container injection handler unregistered";
        injectionHandler_.reset();
    }
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