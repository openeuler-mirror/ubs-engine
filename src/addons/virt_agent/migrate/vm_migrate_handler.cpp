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


#include "vm_migrate_handler.h"

#include <thread>
#include <chrono>

#include <ubse_logger.h>
#include "resource_collect.h"
#include "vm_http_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
using namespace ubse::log;
std::atomic<bool> VmMigrateHandler::exitFlag(false);
constexpr uint64_t MIGRATE_TIME_MAX_LIMIT = 10800;

void VmMigrateHandler::FlushExpireDataThread()
{
    std::thread timerThread(&VmMigrateHandler::FlushExpireData, VmMigrateHandler{});
    timerThread.detach();
}

void VmMigrateHandler::Stop()
{
    UBSE_LOG_INFO << "[flush vm] stop migrate handler.";
    exitFlag.store(true);
}

VmResult vm::VmMigrateHandler::InitVmMigrateData()
{
    if (this->init) {
        return VM_OK;
    }
    auto &vmResourceCollect = ResourceCollect::GetInstance();
    VmResult ret = vmResourceCollect.LoadVmMigrateData();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[flush vm] init vm status cache failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "[flush vm] init vm status cache successfully.";
    this->init = true;
    return VM_OK;
}

void vm::VmMigrateHandler::FlushExpireData()
{
    while (!exitFlag.load()) {
        VmResult ret = this->InitVmMigrateData();
        if (ret != VM_OK) {
            std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
            continue;
        }
        auto &vmResourceCollect = ResourceCollect::GetInstance();
        NumaVMInfoMap globalNumaVMInfoMap{};
        {
            std::lock_guard lockGuard(ResourceCollect::mAllLock);
            globalNumaVMInfoMap = vmResourceCollect.GetGlobalSampleVMInfo();
        }
        time_t currentTime = std::time(nullptr);

        for (const auto &[nodeLoc, vmMap] : globalNumaVMInfoMap) {
            FlushExpireVm(nodeLoc, vmMap, currentTime);
        }
        // Thread sleeps to control task frequency and prevent CPU overload
        std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
    }
}

void VmMigrateHandler::FlushExpireVm(const VMNodeLocInfo &nodeLoc,
                                     const std::unordered_map<std::string, VMBasicInfo> &vmMap, time_t currentTime)
{
    for (const auto &[uuid, vmInfo] : vmMap) {
        if (vmInfo.VMBasicInfoToKeep::vmMigrateStatus != VmMigrateStatus::MIGRATING) {
            continue;
        }
        UBSE_LOG_INFO << "[flush vm] currentTime = " << currentTime
                      << ", migrateTime = " << vmInfo.VMBasicInfoToKeep::vmMigrateInTime
                      << ", diff = " << currentTime - vmInfo.VMBasicInfoToKeep::vmMigrateInTime;
        if (currentTime - vmInfo.VMBasicInfoToKeep::vmMigrateInTime < MIGRATE_TIME_MAX_LIMIT) {
            continue;
        }
        if (vmInfo.VMBasicInfoToKeep::vmMigrateInTime == 0) {
            UBSE_LOG_WARN << "[flush vm] vm migrate time is empty, pid = " << vmInfo.VMBasicInfoCollected::pid;
            continue;
        }
        UBSE_LOG_WARN << "[flush vm] vm status in disable timeout, start to clear, pid = "
                      << vmInfo.VMBasicInfoCollected::pid;
        // enable VM
        VmResult ret = HttpUtil::EnableProcessMigrate(static_cast<int>(vmInfo.VMBasicInfoCollected::pid), true);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[flush vm] enable process: " << vmInfo.VMBasicInfoCollected::pid << " failed, "
                           << FormatRetCode(ret);
            continue;
        }
        // update VM status
        auto &vmResourceCollect = ResourceCollect::GetInstance();
        ret = vmResourceCollect.UpdateVMStatus(vmInfo.numaMemInfo, vmInfo.VMBasicInfoCollected::uuid,
                                               static_cast<int>(vmInfo.VMBasicInfoCollected::pid),
                                               VmMigrateStatus::MIGRATEABLE);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[flush vm] update vmStatus: " << vmInfo.VMBasicInfoCollected::pid << "failed, "
                           << FormatRetCode(ret);
            continue;
        }
        UBSE_LOG_INFO << "[flush vm] clear vm status successfully, pid = " << vmInfo.VMBasicInfoCollected::pid;
    }
}
}
