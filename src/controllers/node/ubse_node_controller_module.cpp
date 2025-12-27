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

#include "ubse_node_controller_module.h"

#include "ubse_node_api.h"
#include "ubse_node_controller_agent.h"
#include "ubse_node_controller_collector.h"
#include "ubse_node_controller_master.h"

namespace ubse::nodeController {
using namespace ubse::election;
using namespace ubse::ipc;
using namespace api::server;
using namespace ubse::node::api;

DYNAMIC_CREATE(UbseNodeControllerModule);

UbseResult UbseNodeControllerModule::Initialize()
{
    // 先注册Com模块
    auto ret = UbseNodeApi::Register();
    if (ret != UBSE_OK) {
        return ret;
    }

    // 先初始化Agent
    ret = UbseNodeControllerAgent::GetInstance().Initialize();
    if (ret != UBSE_OK) {
        return ret;
    }

    // 注册Agent消息处理器 (Agent 已初始化)
    if (ret != UBSE_OK) {
        UbseNodeControllerAgent::GetInstance().UnInitialize();
        return ret;
    }

    // 再初始化master
    ret = UbseNodeControllerMaster::GetInstance().Initialize();
    if (ret != UBSE_OK) {
        // 回滚Agent
        UbseNodeControllerAgent::GetInstance().UnInitialize();
        return ret;
    }

    // 注册Master消息处理器 (Master已初始化)
    ret = RegMasterMsgHandler();
    if (ret != UBSE_OK) {
        // 回滚Master和Agent
        UbseNodeControllerMaster::GetInstance().UnInitialize();
        UbseNodeControllerAgent::GetInstance().UnInitialize();
        return ret;
    }

    return UBSE_OK;
}

UbseResult UbseNodeControllerModule::Start()
{
    UbseNodeControllerAgent::GetInstance().Start();
    UbseNodeControllerMaster::GetInstance().Start();
    return UBSE_OK;
}

void UbseNodeControllerModule::UnInitialize()
{
    UbseNodeControllerAgent::GetInstance().UnInitialize();
    UbseNodeControllerMaster::GetInstance().UnInitialize();
}

void UbseNodeControllerModule::Stop()
{
    UbseNodeControllerAgent::GetInstance().Stop();
    UbseNodeControllerMaster::GetInstance().Stop();
}

} // namespace ubse::nodeController