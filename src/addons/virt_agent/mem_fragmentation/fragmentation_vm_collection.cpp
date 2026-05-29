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

#include "fragmentation_vm_collection.h"

#include <ubse_timer.h>
#include "resource_query.h"
#include "vm_configuration.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
std::mutex FragmentationVmCollection::timerTaskMutex;
std::string FragmentationVmCollection::timerName = "fragCollectVmInfoTimer";

FragmentationVmCollection& FragmentationVmCollection::GetInstance()
{
    static FragmentationVmCollection fragmentationVmCollection;
    return fragmentationVmCollection;
}

VmResult FragmentationVmCollection::FragInit()
{
    UBSE_LOG_INFO << "FragInit initialization started.";
    auto collectPeriod = VmConfiguration::GetInstance().GetExportInterval();
    return ubse::timer::UbseTimerHandlerRegister(timerName, FragGetLocalHostVmCollectData, collectPeriod);
}

void FragmentationVmCollection::FragTerminate()
{
    std::unique_lock<std::mutex> lock(timerTaskMutex);
    ubse::timer::UbseTimerHandlerUnregister(timerName);
}

uint32_t FragmentationVmCollection::FragGetLocalHostVmCollectData()
{
    std::unique_lock<std::mutex> lock(timerTaskMutex);
    HostVmDomainInfo _{};
    auto ret = ResourceQuery::GetLocalHostVmDomainInfo(_);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[query] Get local host vm info failed. " << FormatRetCode(ret);
        return ret;
    }
    return VM_OK;
}

} // namespace vm
