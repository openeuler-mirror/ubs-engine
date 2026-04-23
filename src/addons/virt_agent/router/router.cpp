/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "router.h"
#include <ubse_logger.h>
#include "vm_migrate.h"
#include "mem_fragmentation_sdk_server.h"
#include "case_conf_sdk_server.h"
#include "container_sdk_server.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
VmResult VMCommonSdkServerInit()
{
    // Enable/disable hot/cold page migration and update VM state
    auto ret = VmMigrate::Register();
    if (ret != VM_OK) {
        return ret;
    }
    ret = VirtMemFragSdk::QueryRegister();
    if (ret != VM_OK) {
        return ret;
    }
    ret = VirtCaseConfSdk::Register();
    if (ret != VM_OK) {
        return ret;
    }
    UBSE_LOG_INFO << "The common sdk was registered successfully.";
    return VM_OK;
}

VmResult VMMemFragSdkServerInit()
{
    // Non-Overcommitment scenario
    auto ret = VirtMemFragSdk::Register();
    if (ret != VM_OK) {
        return ret;
    }
    UBSE_LOG_INFO << "The Memory Fragmentation sdk was registered successfully.";
    return ret;
}

VmResult ContainerSdkServerInit()
{
    // Registering VirtContainerSdk Interfaces
    auto ret = VirtContainerSdk::Register();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init container SDK, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "The container sdk was registered successfully.";
    return ret;
}
} // namespace vm