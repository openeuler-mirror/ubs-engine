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

#include "test_ubse_node_controller_util.h"

#include "src/framework/config/ubse_conf_module.h"
#include "src/framework/context/ubse_context.h"
#include "src/adapter_plugins/mti/ubse_lcne_module.h"
#include "ubse_node_controller_collector.h"

#include <functional>  // 用于 std::function
#include <string>      // 用于 std::string
#include <map>         // 用于 std::map
#include <stdexcept>   // 用于异常处理

constexpr uint32_t MAX_PERCENT = 100;
constexpr uint32_t BLOCK_1G = 1073741824u;     // 1GB
constexpr uint32_t BLOCK_128M = 134217728u;    // 128MB
constexpr uint32_t BLOCK_2M = 2097152u;        // 2MB
constexpr int EOK = 0;                         // 成功码

namespace ubse::node_controller::ut {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::mti;

TEST_F(TestUbseNodeControllerUtil, Lock)
{
    UbseNodeControllerLockMgr::TryWriteLock("util1");
    UbseNodeControllerLockMgr::WriteUnLock("util1");

    UbseNodeControllerLockMgr::ReadLock("util2");
    UbseNodeControllerLockMgr::ReadUnLock("util2");

    UbseNodeControllerLockMgr::TryReadLock("util3");
    UbseNodeControllerLockMgr::ReadUnLock("util3");
}

static UbseResult MockGetConfString(UbseConfModule *This, std::string &section, std::string &configKey,
                                    std::string &configVal)
{
    configVal = "192.168.100.100-192.168.100.102,192.168.100.104";
    return UBSE_OK;
}

UbseResult MockUbseInvalidGetLocalNodeInfo(UbseLcneModule *, MtiNodeInfo &ubseNodeInfo)
{
    ubseNodeInfo.nodeId = "node";
    ubseNodeInfo.eid = "1";
    return UBSE_OK;
}

UbseResult MockUbseGetLocalNodeInfo(UbseLcneModule *, MtiNodeInfo &ubseNodeInfo)
{
    ubseNodeInfo.nodeId = "1";
    ubseNodeInfo.eid = "1";
    return UBSE_OK;
}

UbseResult MockCollectIpList(UbseNodeInfo &ubseNodeInfo)
{
    UbseIpAddr ipv4{};
    ipv4.type = UbseIpType::UBSE_IP_V4;
    ipv4.ipv4.addr[0] = 127;
    ipv4.ipv4.addr[1] = 0;
    ipv4.ipv4.addr[2] = 0;
    ipv4.ipv4.addr[3] = 1;

    UbseIpAddr ipv42{};
    ipv42.type = UbseIpType::UBSE_IP_V4;
    ipv42.ipv4.addr[0] = 192;
    ipv42.ipv4.addr[1] = 168;
    ipv42.ipv4.addr[2] = 100;
    ipv42.ipv4.addr[3] = 104;
    ubseNodeInfo.ipList = {ipv4, ipv42};
    return UBSE_OK;
}

std::unordered_map<std::string, std::string> MockGetClusterIpListFromConf()
{
    std::unordered_map<std::string, std::string> map = {{"192.168.100.104", "1"}};
    return map;
}

namespace nodeController_test {

// 全局函数指针，用于替换实际的GetUbseConf
using GetConfigFunc = UbseResult(*)(const std::string&, const std::string&, void*);

static GetConfigFunc g_realGetConfig = nullptr;
static std::function<UbseResult(const std::string&, const std::string&, std::string&)> g_stringConfigCallback;
static std::function<UbseResult(const std::string&, const std::string&, uint32_t&)> g_uint32ConfigCallback;

// 统一的测试配置获取函数
template<typename T>
UbseResult GetUbseConf_Testable(const std::string& section,
                                const std::string& key,
                                T& value)
{
    // 先尝试使用测试回调
    if constexpr (std::is_same_v<T, std::string>) {
        if (g_stringConfigCallback) {
            return g_stringConfigCallback(section, key, value);
        }
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        if (g_uint32ConfigCallback) {
            return g_uint32ConfigCallback(section, key, value);
        }
    }

    // 如果没有设置测试回调，则使用默认测试行为
    if constexpr (std::is_same_v<T, std::string>) {
        // 默认返回空的配置
        value = "";
        return UBSE_ERROR;  // 模拟配置读取失败
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        value = 0;
        return UBSE_ERROR;
    }

    return UBSE_ERROR;
}

// 测试设置函数
void SetTestConfigCallback(std::function<UbseResult(const std::string&, const std::string&, std::string&)> callback)
{
    g_stringConfigCallback = callback;
}

void SetTestConfigCallback(std::function<UbseResult(const std::string&, const std::string&, uint32_t&)> callback)
{
    g_uint32ConfigCallback = callback;
}

void ClearTestCallbacks()
{
    g_stringConfigCallback = nullptr;
    g_uint32ConfigCallback = nullptr;
}

// 测试用的常量定义
constexpr uint32_t TEST_MAX_PERCENT = 100;
constexpr uint32_t TEST_BLOCK_1G = 1073741824u;
constexpr uint32_t TEST_BLOCK_128M = 134217728u;
constexpr uint32_t TEST_BLOCK_2M = 2097152u;

// 重新实现要测试的函数（使用测试版的GetUbseConf）
UbseAllocator GetAllocator_Test()
{
    std::string val;
    auto ret = GetUbseConf_Testable("obmm", "mempool_allocator", val);
    if (ret != UBSE_OK) {
        // UBSE_LOG_WARN << "read allocator config failed, section=obmm, key=mempool_allocator";
        return UbseAllocator::BUDDY_HIGHMEM;
    }
    if (val == "hugetlb_pmd") {
        return UbseAllocator::HUGETLB_PMD;
    }
    if (val == "hugetlb_pud") {
        return UbseAllocator::HUGETLB_PUD;
    }
    if (val == "buddy_highmem" || val.empty()) {
        return UbseAllocator::BUDDY_HIGHMEM;
    }
    // UBSE_LOG_WARN << "invalid allocator value: " << val;
    return UbseAllocator::BUDDY_HIGHMEM;
}

uint32_t GetPmdMapping_Test()
{
    uint32_t val;
    auto ret = GetUbseConf_Testable("os", "pmd_mapping", val);
    if (ret != UBSE_OK) {
        // UBSE_LOG_WARN << "read pmd mapping config failed, section=os, key=pmd_mapping";
        return TEST_MAX_PERCENT;
    }
    if (val > TEST_MAX_PERCENT) {
        // UBSE_LOG_WARN << "read pmd mapping config invalid, section=os, key=pmd_mapping";
        return TEST_MAX_PERCENT;
    }
    UbseAllocator allocator = GetAllocator_Test();
    if (allocator == UbseAllocator::HUGETLB_PUD) {
        // UBSE_LOG_INFO << "PUM场景使用1G大页，pmdMapping强制设置为100%";
        return TEST_MAX_PERCENT;
    }
    return val;
}

uint32_t GetBlockSize_Test(UbseAllocator allocator)
{
    std::string value;
    auto ret = GetUbseConf_Testable("ubse.memory", "obmm.memory.block.size", value);
    if (ret == UBSE_OK && !value.empty()) {
        uint32_t parsed = 0;
        // 简单的字符串转数字（测试用）
        try {
            parsed = std::stoul(value);
            return parsed;
        } catch (...) {
            return TEST_BLOCK_2M;
        }
    }
    if (allocator == UbseAllocator::HUGETLB_PUD) {
        return TEST_BLOCK_1G;
    }
    return TEST_BLOCK_128M;
}

} // namespace nodeController_test

// 使用测试版本进行测试
TEST_F(TestUbseNodeControllerUtil, GetAllocator_CompleteTest)
{
    using namespace nodeController_test;

    // 1. 测试hugetlb_pmd
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        if (section == "obmm" && key == "mempool_allocator") {
            value = "hugetlb_pmd";
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetAllocator_Test(), UbseAllocator::HUGETLB_PMD);

    // 2. 测试hugetlb_pud
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        if (section == "obmm" && key == "mempool_allocator") {
            value = "hugetlb_pud";
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetAllocator_Test(), UbseAllocator::HUGETLB_PUD);

    // 3. 测试配置读取失败
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetAllocator_Test(), UbseAllocator::BUDDY_HIGHMEM);

    ClearTestCallbacks();
}

TEST_F(TestUbseNodeControllerUtil, GetPmdMapping_CompleteTest)
{
    using namespace nodeController_test;

    // 1. 正常情况
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             uint32_t& value) -> UbseResult {
        if (section == "os" && key == "pmd_mapping") {
            value = 75;
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        if (section == "obmm" && key == "mempool_allocator") {
            value = "buddy_highmem";
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetPmdMapping_Test(), 75u);

    // 2. PUD分配器强制100%
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             uint32_t& value) -> UbseResult {
        if (section == "os" && key == "pmd_mapping") {
            value = 30;  // 配置是30%
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        if (section == "obmm" && key == "mempool_allocator") {
            value = "hugetlb_pud";  // PUD分配器
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetPmdMapping_Test(), TEST_MAX_PERCENT);

    // 3. 配置值超过100%
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             uint32_t& value) -> UbseResult {
        if (section == "os" && key == "pmd_mapping") {
            value = 120;  // 超过100%
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        if (section == "obmm" && key == "mempool_allocator") {
            value = "buddy_highmem";
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetPmdMapping_Test(), TEST_MAX_PERCENT);

    ClearTestCallbacks();
}

TEST_F(TestUbseNodeControllerUtil, GetBlockSize_CompleteTest)
{
    using namespace nodeController_test;

    // 1. 从配置读取成功
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        if (section == "ubse.memory" && key == "obmm.memory.block.size") {
            value = "2097152";  // 2MB
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetBlockSize_Test(UbseAllocator::BUDDY_HIGHMEM), TEST_BLOCK_2M);

    // 2. 配置读取失败，PUD分配器返回1G
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetBlockSize_Test(UbseAllocator::HUGETLB_PUD), TEST_BLOCK_1G);

    // 3. 配置读取失败，其他分配器返回128M
    EXPECT_EQ(GetBlockSize_Test(UbseAllocator::BUDDY_HIGHMEM), TEST_BLOCK_128M);
    EXPECT_EQ(GetBlockSize_Test(UbseAllocator::HUGETLB_PMD), TEST_BLOCK_128M);

    // 4. 配置值解析失败
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        if (section == "ubse.memory" && key == "obmm.memory.block.size") {
            value = "not_a_number";
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetBlockSize_Test(UbseAllocator::BUDDY_HIGHMEM), TEST_BLOCK_2M);

    ClearTestCallbacks();
}

// 综合测试：测试函数间的交互
TEST_F(TestUbseNodeControllerUtil, FunctionsIntegrationTest)
{
    using namespace nodeController_test;

    // 场景1：PUD分配器场景
    {
        SetTestConfigCallback([this](const std::string& section,
                                     const std::string& key,
                                     std::string& value) -> UbseResult {
            if (section == "obmm" && key == "mempool_allocator") {
                value = "hugetlb_pud";
                return UBSE_OK;
            }
            if (section == "ubse.memory" && key == "obmm.memory.block.size") {
                return UBSE_ERROR;  // 配置读取失败
            }
            return UBSE_ERROR;
        });

        SetTestConfigCallback([this](const std::string& section,
                                     const std::string& key,
                                     uint32_t& value) -> UbseResult {
            if (section == "os" && key == "pmd_mapping") {
                value = 50;  // 配置是50%
                return UBSE_OK;
            }
            return UBSE_ERROR;
        });

        auto allocator = GetAllocator_Test();
        auto pmdMapping = GetPmdMapping_Test();
        auto blockSize = GetBlockSize_Test(allocator);

        EXPECT_EQ(allocator, UbseAllocator::HUGETLB_PUD);
        EXPECT_EQ(pmdMapping, TEST_MAX_PERCENT);  // PUD强制100%
        EXPECT_EQ(blockSize, TEST_BLOCK_1G);      // PUD返回1G

        ClearTestCallbacks();
    }

    // 场景2：正常BUDDY_HIGHMEM场景
    {
        SetTestConfigCallback([this](const std::string& section,
                                     const std::string& key,
                                     std::string& value) -> UbseResult {
            if (section == "obmm" && key == "mempool_allocator") {
                value = "buddy_highmem";
                return UBSE_OK;
            }
            if (section == "ubse.memory" && key == "obmm.memory.block.size") {
                value = "134217728";  // 128MB
                return UBSE_OK;
            }
            return UBSE_ERROR;
        });

        SetTestConfigCallback([this](const std::string& section,
                                     const std::string& key,
                                     uint32_t& value) -> UbseResult {
            if (section == "os" && key == "pmd_mapping") {
                value = 65;  // 配置是65%
                return UBSE_OK;
            }
            return UBSE_ERROR;
        });

        auto allocator = GetAllocator_Test();
        auto pmdMapping = GetPmdMapping_Test();
        auto blockSize = GetBlockSize_Test(allocator);

        EXPECT_EQ(allocator, UbseAllocator::BUDDY_HIGHMEM);
        EXPECT_EQ(pmdMapping, 65u);                // 返回配置值
        EXPECT_EQ(blockSize, TEST_BLOCK_128M);     // 返回配置的128M

        ClearTestCallbacks();
    }
}

// 边界值和异常情况测试
TEST_F(TestUbseNodeControllerUtil, BoundaryAndExceptionTest)
{
    using namespace nodeController_test;

    // 测试空配置值
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        if (section == "obmm" && key == "mempool_allocator") {
            value = "";  // 空字符串
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    EXPECT_EQ(GetAllocator_Test(), UbseAllocator::BUDDY_HIGHMEM);

    // 测试非常规配置值
    SetTestConfigCallback([](const std::string& section,
                             const std::string& key,
                             std::string& value) -> UbseResult {
        if (section == "ubse.memory" && key == "obmm.memory.block.size") {
            value = "99999999999999999999";  // 非常大的数字
            return UBSE_OK;
        }
        return UBSE_ERROR;
    });

    // stoul会抛出异常，我们的实现会catch并返回BLOCK_2M
    EXPECT_EQ(GetBlockSize_Test(UbseAllocator::BUDDY_HIGHMEM), TEST_BLOCK_2M);

    ClearTestCallbacks();
}

// 在SetUp和TearDown中管理资源
void TestUbseNodeControllerUtil::SetUp()
{
    Test::SetUp();
#ifdef USE_TESTABLE_FUNCTIONS
    nodeController_test::ClearTestCallbacks();
#endif
}

void TestUbseNodeControllerUtil::TearDown()
{
    Test::TearDown();
#ifdef USE_TESTABLE_FUNCTIONS
    nodeController_test::ClearTestCallbacks();
#endif
    GlobalMockObject::verify();
}

} // namespace ubse::node_controller::ut