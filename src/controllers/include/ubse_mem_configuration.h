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

#ifndef MEM_STRATEGY_SERVER_CONFIGURATION_H
#define MEM_STRATEGY_SERVER_CONFIGURATION_H

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_mem_constants.h"
#include "ubse_node_controller.h"
#include "ubse_str_util.h"

namespace ubse::mem::strategy {
#define MODULE_LOG_NAME "ubse_mem_strategy"
using namespace ubse::log;
using namespace ubse::nodeController;
// mem
constexpr auto UBSE_ADMISSION_CONFIG_SECTION_NAME = "ubse_plugin_admission";
constexpr auto MEM_LOG_CONFIG_SECTION_NAME = "ubse.log";
constexpr auto OCK_MEM_SYSTEM_POOL_MEMORY_RATIO = "system.pool.memory.ratio";
constexpr auto OCK_VM_ENABLE = "virt_agent";
constexpr auto OCK_MEM_SERVER_ALGO_LOG_LEVEL = "log.level";
constexpr auto UBSE_MEMORY = "ubse.memory";
constexpr auto UBSE_LENDER_BALANCE = "lender.balance";

template <typename T>
ubse::common::def::UbseResult GetUbseConf(const std::string &section, const std::string &configKey, T &configValue)
{
    // 调取UbseConfModule类的单例对象
    auto &ctxRef = ubse::context::UbseContext::GetInstance();
    auto cfgPtr = ctxRef.GetModule<ubse::config::UbseConfModule>();
    if (cfgPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to get configuration module instance.";
        return UBSE_ERROR;
    }
    return cfgPtr->GetConf(section, configKey, configValue);
}

struct NodeConfig {
    bool isLender;
    UbseAllocator allocator;
    uint32_t blockSize;
    uint32_t pmdMapping;
};

enum class PageSizeType  {
    Page4K,
    Page64K
};

inline std::unordered_map<UbseAllocator, std::string> allocatorStrMap = {
    {UbseAllocator::HUGETLB_PMD, "HUGETLB_PMD"},
    {UbseAllocator::HUGETLB_PUD, "HUGETLB_PUD"},
    {UbseAllocator::BUDDY_HIGHMEM, "BUDDY_HIGHMEM"},
};

class UbseMemConfiguration {
public:
    using NodeInfoMap = std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>;

    inline static UbseMemConfiguration &GetInstance()
    {
        static UbseMemConfiguration instance;
        return instance;
    }

    void Init();

    UbseMemConfiguration(const UbseMemConfiguration &other) = delete;
    UbseMemConfiguration(UbseMemConfiguration &&other) = delete;
    UbseMemConfiguration &operator=(const UbseMemConfiguration &other) = delete;
    UbseMemConfiguration &operator=(UbseMemConfiguration &&other) noexcept = delete;
    void SetConfig(const NodeInfoMap &nodeMap);
    void setPageType();

    [[nodiscard]] bool GetManagerVmEnable() const
    {
        std::string vmCode;
        if (GetUbseConf(UBSE_ADMISSION_CONFIG_SECTION_NAME, OCK_VM_ENABLE, vmCode) != UBSE_OK) {
            return false;
        }
        return true;
    }

    /* 节点最大借用内存总量, 单位M */
    [[nodiscard]] uint64_t GetMaxBorrowSize() const
    {
        return maxBorrowSize;
    }

    /* socket上最大导入内存总量,单位M */
    [[nodiscard]] uint64_t GetMaxSocketImportSize() const
    {
        return maxSocketImportSize;
    }

    /* 获取当前环境配置， 4K/64K页环境 */
    [[nodiscard]] PageSizeType GetPageType() const
    {
        return pageType;
    }

    /* 获取节点的pmdMapping配置，单位% */
    std::optional<uint32_t> GetPmdMappingById(const std::string &nodeId) const;

    /* 获取节点的obmm allocator， */
    std::optional<UbseAllocator> GetObmmAllocatorById(const std::string &nodeId) const;

    /* 获取节点的blocksize */
    std::optional<uint32_t> GetBlockSizeById(const std::string &nodeId) const;

    /* 获取一个lender节点的blocksize */
    std::optional<uint32_t> GetBlockSizeFromLenderNode() const;

    /* 获取一个lender节点的ObmmAllocator */
    std::optional<UbseAllocator> GetAllocatorFromLenderNode() const;

    /* 获取所有节点配置 */
    std::unordered_map<std::string, NodeConfig> GetAllConfigs() const;

    bool IsLenderBalance();

private:
    std::unordered_map<std::string, NodeConfig> nodeConfigs;
    uint64_t maxBorrowSize{MAX_BORROW_MEM_PER_NODE};
    uint64_t maxSocketImportSize{MAX_IMPORT_MEM_SIZE_PER_SOCKET * ONE_M};
    PageSizeType pageType{PageSizeType::Page4K};
    UbseMemConfiguration() = default;
};
#undef MODULE_LOG_NAME
} // namespace ubse::mem::strategy

#endif // SERVER_CONFIGURATION_H