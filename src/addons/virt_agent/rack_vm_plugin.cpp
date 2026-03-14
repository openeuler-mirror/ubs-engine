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

#include "rack_vm_plugin.h"

#include <ubse_logger.h>
#include "alarm_handler.h"
#include "case_conf.h"
#include "escape_algorithm_helper.h"
#include "fragmentation_vm_collection.h"
#include "ham_migrate.h"
#include "mem_handler.h"
#include "mempooling_module.h"
#include "router.h"
#include "status_manager.h"
#include "vm_configuration.h"
#include "libvirt_helper.h"
#include "resource_collect.h"

UBSE_DEFINE_THIS_MODULE("vm_plugin");
using namespace vm;
using namespace ubse::log;

uint32_t UbsePluginInit(uint16_t modCode)
{
    UBSE_LOG_INFO << "Start to init VirtAgent plugin.";
    auto res = VmConfiguration::GetInstance().Initialize(modCode);
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init VirtAgent config, " << FormatRetCode(res);
        return res;
    }
    res = CommonInit();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init common, " << FormatRetCode(res);
        return res;
    }
    if (!VmConfiguration::GetInstance().GetVirtSceneType()) {
        UBSE_LOG_INFO << "start VirtAgent container scene.";
        res = ContainerSceneInit();
    } else {
        UBSE_LOG_INFO << "start VirtAgent vm scene.";
        res = VmSceneInit();
    }
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init VirtAgent, " << FormatRetCode(res);
        UbsePluginDeInit();
        return res;
    }
    UBSE_LOG_INFO << "Succeed to init VirtAgent plugin.";
    return res;
}

void UbsePluginDeInit()
{
    UBSE_LOG_INFO << "Start to deInit VirtAgent plugin.";
    // Carefully adjust the order;
    // when calling the corresponding module for deinitialization, ensure that the module has already been initialized.
    MemHandler::GetInstance().Terminate();
    FragmentationVmCollection::GetInstance().FragTerminate();
    StopThread();
    LibvirtHelper::GetInstance().DeInit();
    EscapeAlgorithmHelper::GetInstance().EscapeAlgorithmHandleClose();
    vm::mempooling::MempoolingModule::DeInit();
    UBSE_LOG_INFO << "End to deInit VirtAgent plugin.";
}

namespace vm {
void StopThread()
{
    VmConfiguration::exitFlag.store(true);
    StatusManager::migrateCv.notify_all();
    StatusManager::borrowCv.notify_all();
    HamMigrate::Stop();
}

uint32_t CommonInit()
{
    // Dynamic loading initialization of RMRS
    auto res = mempooling::MempoolingModule::Init();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init MempoolingModule, " << FormatRetCode(res);
        return res;
    }
    return res;
}

uint32_t VmSceneInit()
{
    // Initialize the resource collect
    auto res = ResourceCollect::GetInstance().Init();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init resourceCollect, " << FormatRetCode(res);
        return res;
    }

    // Registering sdk server
    res = VMCommonSdkServerInit();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init vm sdk server, " << FormatRetCode(res);
        return res;
    }

    // Initialization the ham migration function
    res = HamMigrate::Start();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init vm HamMigrate, " << FormatRetCode(res);
        return res;
    }
    res = CaseConf::GetInstance().Init();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init vm CaseConf, " << FormatRetCode(res);
        return res;
    }
    // Preset thread starts, overcommitment or fragmentation scenario
    CaseConf::CaseRegisterRun();
    return res;
}

uint32_t ContainerSceneInit()
{
    // Registering VirtContainerSdk Interfaces
    auto res = ContainerSdkServerInit();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init container SDK, " << FormatRetCode(res);
    }
    return res;
}

uint32_t StrategyInit()
{
    // Initialize the alarm module
    auto res = AlarmHandler::GetInstance().Init();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init alarmHandler, " << FormatRetCode(res);
        return res;
    }
    StatusManager::LoadGlobalBorrowMap();
    res = StatusManager::GetInstance().Init();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init statusManager, " << FormatRetCode(res);
        return res;
    }
    // Initialize the escape algorithm
    res = EscapeAlgorithmHelper::GetInstance().Init();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init escape algorithm, " << FormatRetCode(res);
        return res;
    }

    std::thread borrowThread(&StatusManager::BorrowQueueOperation);
    UBSE_LOG_INFO << "borrowThread start ";
    borrowThread.detach();
    // Initialize the memory handler module
    res = MemHandler::GetInstance().Init();
    if (res != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init mem handler, " << FormatRetCode(res);
        return res;
    }

    return res;
}

uint32_t MemFragRegister()
{
    VmResult ret = VMMemFragSdkServerInit();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init memFragmentation router, " << FormatRetCode(ret);
        return ret;
    }
    ret = FragmentationVmCollection::GetInstance().FragInit();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Failed to init FragmentationVmCollection.Frag, " << FormatRetCode(ret);
        return ret;
    }
    return ret;
}
} // namespace vm
