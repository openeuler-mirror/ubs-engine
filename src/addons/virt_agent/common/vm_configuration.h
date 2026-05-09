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

#ifndef VM_CONFIGURATION_H
#define VM_CONFIGURATION_H

#include <atomic>
#include <cmath>
#include <functional>
#include <unordered_set>

#include <ubse_conf.h>
#include <ubse_logger.h>

#include "vm_def.h"
#include "vm_error.h"
#include "vm_lock.h"

namespace vm {
#define MODULE_LOG_NAME "virt_agent_plugin"
using namespace ubse::config;
using namespace ubse::log;

#define VM_MODULE_NAME VmConfiguration::GetInstance().GetModuleName()
#define VM_MODULE_CODE VmConfiguration::GetInstance().GetModuleCode()

template <typename T>
struct VmConfigRange {
    T defaultValue;             // default value
    std::pair<T, T> valueRange; // value range
};

template <typename T>
struct VmConfigEnum {
    T defaultValue;                  // default value
    std::unordered_set<T> valueEnum; // Enumeration values
};

const uint64_t SPECS_4G = static_cast<uint64_t>(4) << BYTE2GB;
const uint64_t SPECS_16G = static_cast<uint64_t>(16) << BYTE2GB;
const uint64_t SPECS_20G = static_cast<uint64_t>(20) << BYTE2GB;
const VmConfigRange<uint32_t> EXPORT_INTERVAL{2, {1, 60}};
const VmConfigRange<float_t> SECOND_WATER_MARK{92, {70, 95}};
const VmConfigRange<uint32_t> MAX_FAILURE_TIME{3, {1, 5}};
// Minimum amount of memory borrowed per single block, Unit: MB, Value range (enumerated values) {1, 2, 3, 4} GB
const VmConfigEnum<uint64_t> MIN_MEM_PERBORROW_MB{1024, {1024, 2048, 3072, 4096}};
// Maximum amount of memory borrowed per single block, Unit: MB. Value range (enumerated values) {1, 2, 3, 4} GB
const VmConfigEnum<uint64_t> MAX_MEM_PERBORROW_MB{4096, {1024, 2048, 3072, 4096}};
// Size of memory borrowed during OOM events, Unit: MB, Value range [1, 4] GB
const VmConfigRange<uint64_t> OOM_BORROW_MEM_SIZE{1024, {1024, 4096}};
// Maximum amount of memory borrowed at a single time, Unit: MB, Value range [4, 20] GB
const VmConfigRange<uint64_t> MAX_PER_TOTAL_MEMBORROW_SIZE{16384, {4096, 20480}};
// Ham migration timeout period, Unit: seconds. Value range: [10, 10800]. Default value: 60s
const VmConfigRange<uint32_t> HAM_MIGRATION_MAX_TIMEOUT{60, {10, 10800}};
const std::string PLUGIN_VM_NAME = "plugin_virt_agent";
const std::string PLUGIN_MEM_NAME = "plugin_mem_master";
const std::string DEFAULT_LOW_WATER_MARK = "80";
const std::string DEFAULT_HIGH_WATER_MARK = "85";
const std::string DEFAULT_BORROW_WATER_MARK = "92";
const std::string BORROW_WATER_MARK_KEY = "borrow.watermark";
const std::string LOW_WATER_MARK_KEY = "low.watermark";
const std::string HIGH_WATER_MARK_KEY = "high.watermark";

const std::string NODEID_KEY = "nodeId";
const std::string MAX_PER_TOTAL_MEMBORROW_SIZE_KEY = "borrow.maxPerTotalMemBorrowSize";
const std::string HAM_MIGRATION_MAX_TIMEOUT_KEY = "mig.ham.maxTimeout";
const std::string DEFAULT_UDS_ADDRESS = "/var/run/ubse/ubseAgentUds.socket";
const std::string DEFAULT_ESCAPE_ALGORITHM_DIR = "/usr/lib64/libstrategy.so";
const uint32_t DEFAULT_VIRT_SCENE_TYPE = 1;

class VmConfiguration {
public:
    static std::atomic<bool> exitFlag;
    static VmConfiguration &GetInstance();

    VmResult Initialize(uint16_t modCode);

    VmResult LoadConfig();

    inline const char *GetModuleName() const
    {
        return moduleName.c_str();
    }
    inline uint16_t GetModuleCode() const
    {
        return moduleCode;
    }
    void LoadStrategyConfig();
    std::string GetEscapeAlgorithmDir() const;
    uint32_t GetExportInterval() const;
    std::string GetNodeId() const;
    uint64_t GetMaxMemPerBorrowBytes() const;
    uint64_t GetMinMemPerBorrowBytes() const;
    uint64_t GetOomEventBorrowBytes() const;
    std::string GetLowWatermark();
    std::string GetHighWatermark();
    std::string GetBorrowWatermark();
    float GetMaxMemBorrow() const;
    uint32_t SetMaxMemBorrow(float_t value);
    uint32_t GetVirtSceneType() const;
    void InitMaxBorrow();
    uint64_t GetMaxPerTotalMemBorrowBytes() const;

    inline uint32_t GetHamMigrateMaxTimeout() const
    {
        return hamMigrateMaxTimeout;
    };

    template <typename T>
    using GetConfigFunc = std::function<uint32_t(const std::string &, const std::string &, T &)>;
    GetConfigFunc<uint32_t> ubseGetUintFuncPtr = UbseGetUInt;
    GetConfigFunc<float> ubseGetFloatFuncPtr = UbseGetFloat;
    GetConfigFunc<uint64_t> ubseGetULongFuncPtr = UbseGetULong;
    GetConfigFunc<std::string> ubseGetStrFuncPtr = UbseGetStr;
    GetConfigFunc<bool> ubseGetBoolFuncPtr = UbseGetBool;

    template <typename T>
    void GetConfigWithCheckRange(const GetConfigFunc<T> &getFunc, const VmConfigRange<T> &range,
                                 const std::string &fileName, const std::string &config, T &param)
    {
        auto ret = getFunc(fileName, config, param);
        if (ret != VM_OK) {
            UBSE_LOG_WARN << "The value of the key does not exist or is invalid, key=" << config << ", ret=" << ret
                          << ", use default value: " << range.defaultValue;
            param = range.defaultValue;
            return;
        }
        if (param < range.valueRange.first || param > range.valueRange.second) {
            UBSE_LOG_WARN << "The config exceeds range, key=" << config << ", ret=" << ret << ", your config: " << param
                          << ", use default value: " << range.defaultValue;
            param = range.defaultValue;
        }
    }

    template <typename T>
    void GetConfigWithCheckEnum(const GetConfigFunc<T> &getFunc, const VmConfigEnum<T> &enums,
                                const std::string &fileName, const std::string &config, T &param)
    {
        auto ret = getFunc(fileName, config, param);
        if (ret != VM_OK) {
            UBSE_LOG_WARN << "The value of the key does not exist or is invalid, key=" << config << ", ret=" << ret
                          << ", your config: " << param << ", use default value: " << enums.defaultValue;
            param = enums.defaultValue;
            return;
        }
        if (enums.valueEnum.find(param) == enums.valueEnum.end()) {
            UBSE_LOG_WARN << "The config exceeds range, key=" << config << ", ret=" << ret << ", your config: " << param
                          << ", use default value: " << enums.defaultValue;
            param = enums.defaultValue;
        }
    }

    VmResult LoadWatermarkConf();
    VmResult SetDefaultWaterConf();
    VmResult CheckWaterConfRange(const float_t &borrowWater, const float_t &migrateWater, const float_t &returnWater);
    VmResult VerifyWaterConfig();
    void CheckConfigValidity();

private:
    VmConfiguration() = default;
    ~VmConfiguration() = default;
    std::string moduleName = "virt_agent_plugin"; // Module Name
    uint16_t moduleCode = 0;                      // Module Code

    uint32_t exportInterval = 10; // Export Period

    // Decision configuration
    std::string borrowWatermark; // Borrow water line
    std::string lowWatermark;    // Return water line
    std::string highWatermark;   // Migrate water line

    // Decision configuration, but it depends on mem for provisioning.
    // In case of loading failure, default values must be provided.
    float_t maxMemBorrow = 0;
    uint64_t maxMemPerBorrowSize = 0;
    uint64_t minMemPerBorrowSize = 0;
    uint64_t maxPerTotalMemBorrowSize = 0;
    uint64_t oomBorrowMemSize = 0;
    uint32_t virtSceneType = DEFAULT_VIRT_SCENE_TYPE;
    // Common configuration
    std::string nodeId{};

    // HanMigration configuration
    uint32_t hamMigrateMaxTimeout = 0;

    std::string escapeAlgorithmDir = ""; // Absolute path of the custom escape algorithm.so file path
    static ReadWriteLock waterConfigLock;
};
#undef MODULE_LOG_NAME
} // namespace vm

#endif // VM_CONFIGURATION_H
