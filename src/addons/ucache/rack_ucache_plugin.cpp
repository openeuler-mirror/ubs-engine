/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "rack_ucache_plugin.h"

#include <ubse_election.h>
#include <ubse_logger.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "event_listener.h"
#include "ucache_agent.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_master.h"

using namespace ucache;
using namespace ubse::log;
using namespace ubse::election;

namespace ucache {

static std::atomic<bool> g_masterInitThreadStopFlag{false};
static std::thread g_tryInitUCacheMasterThread;

uint32_t HandleSwitchOver(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Handling switch over event for node " << nodeId;
    ucache::master::Exit();
    return UCACHE_OK;
}

uint32_t HandleStandbyToMaster(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Handling standby to master for node " << nodeId;
    ucache::master::Exit();
    auto ret = ucache::master::Init();
    return ret; // 返回 0 表示成功
}

uint32_t HandleChangeToMaster(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Handling change to master for node " << nodeId;
    ucache::master::Exit();
    auto ret = ucache::master::Init();
    return ret;
}

uint32_t HandleChangeToStandby(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Handling change to standby for node " << nodeId;
    ucache::master::Exit();
    return UCACHE_OK;
}

uint32_t HandleChangeToAgent(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Handling change to agent for node " << nodeId;
    ucache::master::Exit();
    return UCACHE_OK;
}

uint32_t RegEvent(UbseElectionEventType type, const std::string& handlerName, ElectinHandler HandlerFunction)
{
    UbseElectionHandlerBuilder builder;
    const int seqId = 100;
    builder.SetType(static_cast<UbseElectionEventType>(type))
        .SetPriority(UbseElectionHandlerPriority::MEDIUM)
        .SetSequenceId(seqId)
        .SetName(handlerName)
        .SetHandler(HandlerFunction);
    UbseElectionHandler handler = builder.Build();
    uint32_t result = UbseElectionChangeAttachHandler(handler);
    return result;
}

uint32_t DeregEvent(UbseElectionEventType type, const std::string& handlerName, ElectinHandler HandlerFunction)
{
    UbseElectionHandlerBuilder builder;
    const int seqId = 100;
    builder.SetType(static_cast<UbseElectionEventType>(type))
        .SetPriority(UbseElectionHandlerPriority::MEDIUM)
        .SetSequenceId(seqId)
        .SetName(handlerName)
        .SetHandler(HandlerFunction);
    UbseElectionHandler handler = builder.Build();
    uint32_t result = UbseElectionChangeDeAttachHandler(handler);
    return result;
}

void RegRackElectionEvent()
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "RegRackElectionEvent start";
    auto ret = RegEvent(UbseElectionEventType::CHANGE_TO_MASTER, "UCacheChangeToMasterHandler", HandleChangeToMaster);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to register the UCacheChangeToMasterHandler handler. Error code: " << ret;
    }

    ret = RegEvent(UbseElectionEventType::CHANGE_TO_SWITCHOVER, "UCacheSwitchOverHandler", HandleSwitchOver);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to register the UCacheSwitchOverHandler handler. Error code: " << ret;
    }

    ret = RegEvent(UbseElectionEventType::STANDBY_CHANGE_TO_MASTER, "UCacheStandbyToMasterHandler",
                   HandleStandbyToMaster);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to register the UCacheHandleStandbyToMaster handler. Error code: " << ret;
    }

    ret = RegEvent(UbseElectionEventType::CHANGE_TO_STANDBY, "UCacheChangeToStandbyHandler", HandleChangeToStandby);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to register the UCacheChangeToStandbyHandler handler. Error code: " << ret;
    }

    ret = RegEvent(UbseElectionEventType::CHANGE_TO_AGENT, "UCacheChangeToAgentHandler", HandleChangeToAgent);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to register the UCacheChangeToAgentHandler handler. Error code: " << ret;
    }

    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "RegRackElectionEvent ok";
}

void DeregRackElectionEvent()
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "DeregRackElectionEvent start";
    auto ret = DeregEvent(UbseElectionEventType::CHANGE_TO_MASTER, "UCacheChangeToMasterHandler", HandleChangeToMaster);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to deregister the UCacheChangeToMasterHandler handler. Error code: " << ret;
    }

    ret = DeregEvent(UbseElectionEventType::CHANGE_TO_SWITCHOVER, "UCacheSwitchOverHandler", HandleSwitchOver);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to deregister the UCacheSwitchOverHandler handler. Error code: " << ret;
    }

    ret = DeregEvent(UbseElectionEventType::STANDBY_CHANGE_TO_MASTER, "UCacheStandbyToMasterHandler",
                     HandleStandbyToMaster);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to deregister the UCacheStandbyToMasterHandler handler. Error code: " << ret;
    }

    ret = DeregEvent(UbseElectionEventType::CHANGE_TO_STANDBY, "UCacheChangeToStandbyHandler", HandleChangeToStandby);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to deregister the UCacheChangeToStandbyHandler handler. Error code: " << ret;
    }

    ret = DeregEvent(UbseElectionEventType::CHANGE_TO_AGENT, "UCacheChangeToAgentHandler", HandleChangeToAgent);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Failed to deregister the UCacheChangeToAgentHandler handler. Error code: " << ret;
    }

    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "DeregRackElectionEvent ok";
}

void TryInitUCacheMaster()
{
    uint32_t ret;
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Try init UCache master.";

    uint32_t cnt = 0;
    const uint32_t interval = 3;
    std::string masterNodeId;

    ret = UbseGetMasterNodeId(masterNodeId);
    while (!g_masterInitThreadStopFlag && ret != UCACHE_OK) {
        cnt++;
        std::this_thread::sleep_for(std::chrono::seconds(interval));
        ret = UbseGetMasterNodeId(masterNodeId);
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Wait Rack Master Election, cost: " << cnt * interval << "s.";
    }

    std::string curNodeId;
    ret = UbseGetCurrentNodeId(curNodeId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get current NodeId failed.";
        return;
    }

    // 这里的顺序需要严格保证。
    // 1. 先确保UbseGetCurrentNodeId 成功
    // 2. 再注册事件
    // 3. 再初始化 ucache master
    RegRackElectionEvent();
    if (curNodeId == masterNodeId) {
        ret = ucache::master::Init();
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to init ucache master, ret: " << ret;
            return;
        }
    }
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Try init ucache master end";
}

} // namespace ucache

uint32_t UbsePluginInit(const uint16_t modCode)
{
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Start init ucache plugin";
    auto ret = UcacheConfig::GetInstance().Initialize(modCode);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to init ucache config, ret: " << ret;
        return ret;
    }

    // 故障事件监听
    ret = ucache::fault_handler::EventListener::Init();
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to init fault_handler";
        return ret;
    }

    ret = ucache::agent::Init();
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to init ucache_agent, ret: " << ret;
        return ret;
    }

    g_tryInitUCacheMasterThread = std::thread(TryInitUCacheMaster);

    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "End init ucache plugin";
    return UCACHE_OK;
}

void UbsePluginDeInit()
{
    g_masterInitThreadStopFlag.store(true);
    if (g_tryInitUCacheMasterThread.joinable()) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Start join master init thread.";
        g_tryInitUCacheMasterThread.join();
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "End join master init thread.";
    }

    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Start deinit ucache plugin";

    std::string masterNodeId;
    uint32_t ret = UbseGetMasterNodeId(masterNodeId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get MasterNodeId failed.";
        return;
    }

    std::string curNodeId;
    ret = UbseGetCurrentNodeId(curNodeId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get current NodeId failed.";
        return;
    }

    if (curNodeId == masterNodeId) {
        ucache::master::Exit();
    }
    ucache::agent::Exit();
    DeregRackElectionEvent();
    UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "End deinit ucache plugin";
}
