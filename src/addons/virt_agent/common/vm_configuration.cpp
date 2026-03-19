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

#include "vm_configuration.h"
#include <ubse_election.h>

#include "vm_error.h"
#include "vm_string_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
ReadWriteLock VmConfiguration::waterConfigLock{};
using namespace ubse::config;
using namespace ubse::log;
using namespace ubse::election;

std::atomic<bool> VmConfiguration::exitFlag(false);

VmConfiguration &VmConfiguration::GetInstance()
{
    static VmConfiguration gInstance;
    return gInstance;
}

VmResult VmConfiguration::Initialize(uint16_t modCode)
{
    moduleCode = modCode;
    LoadConfig();
    auto ret = LoadWatermarkConf();
    if (ret != VM_OK) {
        return VM_ERROR;
    }

    return VM_OK;
}

VmResult VmConfiguration::LoadConfig()
{
    UbseRoleInfo currentNode;
    auto ret = UbseGetCurrentNodeInfo(currentNode);
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "The value of the key does not exist or is invalid, key=" << NODEID_KEY << ", ret=" << ret;
    }
    nodeId = currentNode.nodeId;

    GetConfigWithCheckRange(ubseGetUintFuncPtr, EXPORT_INTERVAL, "plugin_vm", "ubse.plugin.vm.export.interval",
                            exportInterval);
    GetConfigWithCheckRange(ubseGetUintFuncPtr, HAM_MIGRATION_MAX_TIMEOUT, "plugin_vm", HAM_MIGRATION_MAX_TIMEOUT_KEY,
                            hamMigrateMaxTimeout);
    ret = UbseGetUInt("plugin_vm", "virt.sceneType", virtSceneType);
    if (ret != VM_OK) {
        virtSceneType = DEFAULT_VIRT_SCENE_TYPE;
        UBSE_LOG_WARN << "The value of the key does not exist or is invalid, key=virt.sceneType, ret=" << ret
                      << ", use default_value=" << DEFAULT_VIRT_SCENE_TYPE;
    }
    ret = UbseGetStr("plugin_vm", "escape.algorithm.dir", escapeAlgorithmDir);
    if (ret != VM_OK) {
        escapeAlgorithmDir = DEFAULT_ESCAPE_ALGORITHM_DIR;
        UBSE_LOG_WARN << "The value of the key does not exist or is invalid, key=escape.algorithm.dir, ret=" << ret
                      << ", use default_value=" << escapeAlgorithmDir;
    }
    LoadStrategyConfig();
    return VM_OK;
}

void VmConfiguration::LoadStrategyConfig()
{
    InitMaxBorrow();

    GetConfigWithCheckEnum(ubseGetULongFuncPtr, MAX_MEM_PERBORROW_MB, "plugin_vm", "borrow.maxMemPerBorrowSize",
                           maxMemPerBorrowSize);

    GetConfigWithCheckEnum(ubseGetULongFuncPtr, MIN_MEM_PERBORROW_MB, "plugin_vm", "borrow.minMemPerBorrowSize",
                           minMemPerBorrowSize);

    GetConfigWithCheckRange(ubseGetULongFuncPtr, OOM_BORROW_MEM_SIZE, "plugin_vm", "borrow.oomBorrowMemSize",
                            oomBorrowMemSize);

    CheckConfigValidity();
}

std::string VmConfiguration::GetEscapeAlgorithmDir() const
{
    return escapeAlgorithmDir;
}

uint32_t VmConfiguration::GetExportInterval() const
{
    return exportInterval;
}

std::string VmConfiguration::GetNodeId() const
{
    return nodeId;
}

float VmConfiguration::GetMaxMemBorrow() const
{
    return maxMemBorrow;
}
uint32_t VmConfiguration::SetMaxMemBorrow(float_t value)
{
    maxMemBorrow = value;
    return VM_OK;
}
uint32_t VmConfiguration::GetVirtSceneType() const
{
    return virtSceneType;
}
uint64_t VmConfiguration::GetMaxMemPerBorrowBytes() const
{
    return maxMemPerBorrowSize * MB_TO_BYTES;
}
uint64_t VmConfiguration::GetMinMemPerBorrowBytes() const
{
    return minMemPerBorrowSize * MB_TO_BYTES;
}
uint64_t VmConfiguration::GetOomEventBorrowBytes() const
{
    return oomBorrowMemSize * MB_TO_BYTES;
}

std::string VmConfiguration::GetLowWatermark()
{
    ReadLocker<ReadWriteLock> lock(&waterConfigLock);
    return lowWatermark;
}

std::string VmConfiguration::GetHighWatermark()
{
    ReadLocker<ReadWriteLock> lock(&waterConfigLock);
    return highWatermark;
}

std::string VmConfiguration::GetBorrowWatermark()
{
    ReadLocker<ReadWriteLock> lock(&waterConfigLock);
    return borrowWatermark;
}

uint64_t VmConfiguration::GetMaxPerTotalMemBorrowBytes() const
{
    return maxPerTotalMemBorrowSize * MB_TO_BYTES;
}

void VmConfiguration::InitMaxBorrow()
{
    GetConfigWithCheckRange(ubseGetULongFuncPtr, MAX_PER_TOTAL_MEMBORROW_SIZE, "plugin_vm",
                            MAX_PER_TOTAL_MEMBORROW_SIZE_KEY, maxPerTotalMemBorrowSize);
}

void VmConfiguration::CheckConfigValidity()
{
    if (maxMemPerBorrowSize < minMemPerBorrowSize) {
        UBSE_LOG_WARN << "The values of the keys do not exist or are invalid, your config: maxMemPerBorrowSize="
                      << maxMemPerBorrowSize << ", minMemPerBorrowSize=" << minMemPerBorrowSize
                      << ". The maxMemPerBorrowSize use default value: " << MAX_MEM_PERBORROW_MB.defaultValue
                      << ", minMemPerBorrowSize use default value: " << MIN_MEM_PERBORROW_MB.defaultValue;
        maxMemPerBorrowSize = MAX_MEM_PERBORROW_MB.defaultValue;
        minMemPerBorrowSize = MIN_MEM_PERBORROW_MB.defaultValue;
    }
}

VmResult VmConfiguration::SetDefaultWaterConf()
{
    UBSE_LOG_WARN << "The values of the keys do not exist or are invalid, your config: borrowWatermark="
                  << borrowWatermark << ", highWatermark=" << highWatermark << ", lowWatermark=" << lowWatermark
                  << ". The borrowWatermark use default value: " << DEFAULT_BORROW_WATER_MARK
                  << ", highWatermark use default value: " << DEFAULT_HIGH_WATER_MARK
                  << ", lowWatermark use default value: " << DEFAULT_LOW_WATER_MARK;
    borrowWatermark = DEFAULT_BORROW_WATER_MARK;
    highWatermark = DEFAULT_HIGH_WATER_MARK;
    lowWatermark = DEFAULT_LOW_WATER_MARK;
    return VM_OK;
}
VmResult VmConfiguration::LoadWatermarkConf()
{
    UbseGetStr(PLUGIN_VM_NAME, BORROW_WATER_MARK_KEY, borrowWatermark);

    UbseGetStr(PLUGIN_VM_NAME, LOW_WATER_MARK_KEY, lowWatermark);
    UbseGetStr(PLUGIN_VM_NAME, HIGH_WATER_MARK_KEY, highWatermark);
    return VerifyWaterConfig();
}

VmResult VmConfiguration::CheckWaterConfRange(const float_t &borrowWater, const float_t &migrateWater,
                                              const float_t &returnWater)
{
    // The range of the borrow water line is [70, 95]
    if (borrowWater < 70 || borrowWater > 95) {
        UBSE_LOG_WARN << "The config exceeds range, key=" << BORROW_WATER_MARK_KEY << ", your config=" << borrowWater
                      << ", its range must be: [70, 95]";
        return VM_ERROR;
    }
    // The range of the migrate water line is [65, 90]
    if (migrateWater < 65 || migrateWater > 90) {
        UBSE_LOG_WARN << "The config exceeds range, key=" << HIGH_WATER_MARK_KEY << ", your config=" << migrateWater
                      << ", its range must be: [65, 90]";
        return VM_ERROR;
    }
    // The range of the return water line is [60, 80]
    if (returnWater < 60 || returnWater > 80) {
        UBSE_LOG_WARN << "The config exceeds range, key=" << LOW_WATER_MARK_KEY << ", your config=" << returnWater
                      << ", its range must be: [60, 80]";
        return VM_ERROR;
    }
    // The minimum difference between waterlines is 5
    if ((returnWater + 5 <= migrateWater) && (migrateWater + 5 <= borrowWater)) {
        return VM_OK;
    }
    UBSE_LOG_WARN << "The difference between the values of these keys is less than 5, these keys must be: "
                     "lowWatermark + 5 <= highWatermark <= borrowWatermark - 5";
    return VM_ERROR;
}

VmResult VmConfiguration::VerifyWaterConfig()
{
    float_t borrowWater{};
    float_t migrateWater{};
    float_t returnWater{};
    try {
        borrowWater = VmStringUtil::SafeStof(borrowWatermark);
        migrateWater = VmStringUtil::SafeStof(highWatermark);
        returnWater = VmStringUtil::SafeStof(lowWatermark);
    } catch (const std::exception &e) {
        // In the default configuration, input parameters will be printed.
        return SetDefaultWaterConf();
    }

    if (CheckWaterConfRange(borrowWater, migrateWater, returnWater) != VM_OK) {
        return SetDefaultWaterConf();
    }
    return VM_OK;
}
} // namespace vm