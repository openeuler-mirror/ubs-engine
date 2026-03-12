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

#include "rack_mempooling_plugin.h"

#include <ubse_com.h>
#include <ubse_logger.h>
#include <ubse_election.h>
#include <securec.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "mp_configuration.h"
#include "event_listener.h"
#include "mem_manager.h"
#include "mempool_borrow_module.h"
#include "mempool_migrate_helper.h"
#include "mempooling_message.h"
#include "mp_error.h"
#include "mp_heartbeat_monitor.h"
#include "mp_smap_controller.h"
#include "mp_sync_data_helper.h"
#include "exporter.h"
#include "rmrs_resource_query.h"
#include "rmrs_serialize.h"
#include "mp_module.h"
#include "fault_node_module.h"
#include "fault_memid_helper.h"
#include "over_commit_msg_handler.h"
#include "ubse_storage.h"
#include "mem_borrow_executor.h"

using namespace mempooling;
using namespace ubse::com;
using namespace ubse::log;
using namespace ubse::election;
using namespace mempooling::smap;
using namespace mempooling::sync;
using namespace rmrs::serialize;
using namespace ubse::storage;

using namespace mempooling::migrate;

const std::vector<std::shared_ptr<mempooling::MpSubModule>> g_modules = {
    std::make_shared<mempooling::message::MpMessageSubModule>(),
    std::make_shared<mempooling::event::MpEventSubModule>(),
    std::make_shared<over_commit::MpOverCommitSubModule>(),
    std::make_shared<mempooling::MpExportSubModule>(),
    std::make_shared<mempooling::smap::MpSmapSubModule>(),
    std::make_shared<mempooling::sync::MpSyncDataSubModule>(),
    std::make_shared<mempooling::MpMigrateSubModule>(),
    std::make_shared<mempooling::MpFaultMemIdSubModule>(),
    std::make_shared<mempooling::MpFaultNodeSubModule>(),
    std::make_shared<mempooling::MpManagerSubModule>(),
    std::make_shared<mempooling::MpMemBorrowExecutorModule>()
};

uint32_t UbsePluginInit(const uint16_t modCode)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PluginInit] Start init mempooling plugin.";
    auto res = MpConfiguration::GetInstance().Initialize(modCode);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PluginInit] Failed to initialize mempooling config, res=" << res << ".";
        return res;
    }

    for (auto &modulePtr : g_modules) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PluginInit] " << modulePtr->Name() << " is starting.";
        res = modulePtr->Init();
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[PluginInit] " << modulePtr->Name()
                << " init failed, res=" << res << ".";
            return MEM_POOLING_ERROR;
        }
    }

    res = DataReloadInit();
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[PluginInit] Failed to reload data.";
        return res;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PluginInit] Init mempooling plugin end.";
    return res;
}

void UbsePluginDeInit()
{
    for (int i = static_cast<int>(g_modules.size()) - 1; i >= 0; i--) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PluginDeInit] " << g_modules[i]->Name() << " is ending.";
        g_modules[i]->DeInit();
    }
    return ;
}