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

#include "adapter_plugins/mti/ubse_smbios.h"
#include "ubse_node_api.h"
#include "ubse_node_controller_agent.h"
#include "ubse_node_controller_collector.h"
#include "ubse_node_controller_master.h"
#include "ubse_election_module.h"
#include "ubse_lcne_module.h"

namespace ubse::nodeController {
using namespace ubse::election;
using namespace ubse::ipc;
using namespace api::server;
using namespace ubse::config;
using namespace ubse::node::api;
using namespace ubse::adapter_plugins::smbios;

OPTIONAL_MODULE_IMPL(UbseNodeControllerModule,
                     ubse::mti::UbseLcneModule,
                     ubse::election::UbseElectionModule);

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

    // 再初始化Master
    ret = UbseNodeControllerMaster::GetInstance().Initialize();
    if (ret != UBSE_OK) {
        UbseNodeControllerAgent::GetInstance().UnInitialize();
        return ret;
    }

    return UBSE_OK;
}

UbseResult UbseNodeControllerModule::Start()
{
    auto ret = UbseNodeControllerAgent::GetInstance().Start();
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = UbseNodeControllerMaster::GetInstance().Start();
    if (ret != UBSE_OK) {
        return ret;
    }

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
}