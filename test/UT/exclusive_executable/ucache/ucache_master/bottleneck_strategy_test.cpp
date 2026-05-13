/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <ubse_logger.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public

#include "ubse_conf.h"
#include "bottleneck_strategy.h"
#include "data_collect.h"
#include "ucache_config.h"
#include "ucache_config_check.h"
#include "ucache_error.h"
using namespace std;
using namespace ucache;
using namespace ubse::config;
using namespace ubse::log;
using namespace ucache::master::bottleneck;
using namespace ucache::data_collect;

class BottleneckStrategyTest : public testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

void GetCgrouopInfo(map<string, map<string, CgroupInfo>>& cgroupInfo, std::string name, uint64_t pageCacheIn,
                    uint64_t ioReadBandwidth)
{
    map<string, map<string, CgroupInfo>> MyCgroupInfo = {
        {"node0", {{name, {.pageCacheIn = pageCacheIn, .ioReadBandwidth = ioReadBandwidth}}}}};
    cgroupInfo = MyCgroupInfo;
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_doris_sensitive)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);

    map<string, map<string, CgroupInfo>> cgroupInfo{};
    GetCgrouopInfo(cgroupInfo, "doris-b1", static_cast<uint64_t>(5120), static_cast<uint64_t>(5120));

    map<string, map<string, CgroupInfo>> cgroupInfo2{};
    GetCgrouopInfo(cgroupInfo2, "doris-b1", static_cast<uint64_t>(20480), static_cast<uint64_t>(20480));

    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;

    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();

    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
    sensitiveMap = strategy.GetContainerState();

    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行六次
    for (int i = 1; i <= 18; i++) {
        uint32_t result1 = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();

        EXPECT_EQ(sensitiveMap["node0"]["doris-b1"], PageCacheSensitiveTag::UNKNOWN);
    }
    for (int i = 1; i <= 20; i++) {
        uint32_t result1 = strategy.JudgeSensitive(cgroupInfo2, 10);
        sensitiveMap = strategy.GetContainerState();

        if (i < 13) {
            EXPECT_EQ(sensitiveMap["node0"]["doris-b1"], PageCacheSensitiveTag::UNKNOWN);
        } else {
            EXPECT_EQ(sensitiveMap["node0"]["doris-b1"], PageCacheSensitiveTag::SENSITIVE);
        }
    }
    EXPECT_EQ(sensitiveMap["node0"]["doris-b1"], PageCacheSensitiveTag::SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_doris_sensitive_to_not_sensitive)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"sensitiveToNot", {.pageCacheIn = 5120, .ioReadBandwidth = 5120}}}}};
    const map<string, map<string, CgroupInfo>> cgroupInfo2 = {
        {"node0", {{"sensitiveToNot", {.pageCacheIn = 25120, .ioReadBandwidth = 25120}}}}};
    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;
    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();
    // 调用 JudgeSensitive 方法
    for (int i = 1; i <= 100; i++) {
        uint32_t result = strategy.JudgeSensitive(cgroupInfo2, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["sensitiveToNot"], PageCacheSensitiveTag::SENSITIVE);
    for (int i = 1; i <= 100; i++) {
        uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["sensitiveToNot"], PageCacheSensitiveTag::NOT_SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_flink)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    map<string, map<string, CgroupInfo>> cgroupInfo{};
    GetCgrouopInfo(cgroupInfo, "flinkt1", static_cast<uint64_t>(5120), static_cast<uint64_t>(5120));
    map<string, map<string, CgroupInfo>> cgroupInfo2{};
    GetCgrouopInfo(cgroupInfo2, "flinkt1", static_cast<uint64_t>(20480), static_cast<uint64_t>(20480));
    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;
    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();

    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
    sensitiveMap = strategy.GetContainerState();

    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行六次
    for (int i = 1; i < 6; i++) {
        uint32_t result1 = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
        EXPECT_EQ(sensitiveMap["node0"]["flinkt1"], PageCacheSensitiveTag::UNKNOWN);
    }
    result = strategy.JudgeSensitive(cgroupInfo2, 10);
    sensitiveMap = strategy.GetContainerState();
    EXPECT_EQ(sensitiveMap["node0"]["flinkt1"], PageCacheSensitiveTag::UNKNOWN);
    for (int i = 1; i < 20; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
        EXPECT_EQ(sensitiveMap["node0"]["flinkt1"], PageCacheSensitiveTag::UNKNOWN);
    }
    result = strategy.JudgeSensitive(cgroupInfo2, 10);
    sensitiveMap = strategy.GetContainerState();

    EXPECT_EQ(sensitiveMap["node0"]["flinkt1"], PageCacheSensitiveTag::UNKNOWN);
    for (int i = 1; i < 20; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();

        EXPECT_EQ(sensitiveMap["node0"]["flinkt1"], PageCacheSensitiveTag::UNKNOWN);
    }
    for (int i = 1; i < 40; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["flinkt1"], PageCacheSensitiveTag::NOT_SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_not_sensitive_to_sensitive)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);

    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"noToSensitive", {.pageCacheIn = 5120, .ioReadBandwidth = 5120}}}}};
    const map<string, map<string, CgroupInfo>> cgroupInfo2 = {
        {"node0", {{"noToSensitive", {.pageCacheIn = 20480, .ioReadBandwidth = 20480}}}}};
    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;
    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();
    // 调用 JudgeSensitive 方法
    for (int i = 1; i <= 61; i++) {
        uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    sensitiveMap = strategy.GetContainerState();
    EXPECT_EQ(sensitiveMap["node0"]["noToSensitive"], PageCacheSensitiveTag::NOT_SENSITIVE);
    for (int i = 1; i <= 60; i++) {
        uint32_t result = strategy.JudgeSensitive(cgroupInfo2, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["noToSensitive"], PageCacheSensitiveTag::SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_not_sensitive)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"doris-b3", {.pageCacheIn = 5120, .ioReadBandwidth = 5120}}}}};
    const map<string, map<string, CgroupInfo>> cgroupInfo2 = {
        {"node0", {{"doris-b3", {.pageCacheIn = 20480, .ioReadBandwidth = 20480}}}}};
    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;

    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();

    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo2, 10);
    sensitiveMap = strategy.GetContainerState();
    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行六次
    for (int i = 1; i <= 60; i++) {
        uint32_t result1 = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
        if (i == 40) {
            EXPECT_EQ(sensitiveMap["node0"]["doris-b3"], PageCacheSensitiveTag::UNKNOWN);
        }
    }
    EXPECT_EQ(sensitiveMap["node0"]["doris-b3"], PageCacheSensitiveTag::NOT_SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_highIO_lowPgin_NOT_SENSITIVE)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"highIO_lowPgin", {.pageCacheIn = 5120, .ioReadBandwidth = 20240}}}}};

    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;
    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();

    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
    sensitiveMap = strategy.GetContainerState();

    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行19次
    for (int i = 1; i < 19; i++) {
        uint32_t result1 = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["highIO_lowPgin"], PageCacheSensitiveTag::UNKNOWN);
    // 执行50次
    for (int i = 1; i < 50; i++) {
        uint32_t result1 = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["highIO_lowPgin"], PageCacheSensitiveTag::NOT_SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_highPgin_lowIO_NOT_SENSITIVE)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"highPgin_lowIO", {.pageCacheIn = 5120, .ioReadBandwidth = 20240}}}}};
    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;
    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();
    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
    sensitiveMap = strategy.GetContainerState();
    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行19次
    for (int i = 1; i < 19; i++) {
        uint32_t result1 = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["highPgin_lowIO"], PageCacheSensitiveTag::UNKNOWN);
    // 执行50次
    for (int i = 1; i < 50; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["highPgin_lowIO"], PageCacheSensitiveTag::NOT_SENSITIVE);
}

TEST_F(BottleneckStrategyTest, Test_uint32_boundary)
{
    // 测试边界值（1和4,294,967,294）
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"test-container", {.pageCacheIn = 5120, .ioReadBandwidth = 5120}}}}};

    auto& strategy = BottleneckStrategy::GetInstance();
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 1); // 传入1
    EXPECT_EQ(result, UCACHE_OK);                             // 验证处理正确

    result = strategy.JudgeSensitive(cgroupInfo, 4294967294); // 传入4,294,967,294
    EXPECT_EQ(result, UCACHE_OK);                             // 验证处理正确
}

TEST_F(BottleneckStrategyTest, Test_uint32_invalid)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    // 测试无效值（负值和超出范围）
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"test-container", {.pageCacheIn = 5120, .ioReadBandwidth = 5120}}}}};
    auto& strategy = BottleneckStrategy::GetInstance();
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 0); // 传入0（无效）
    EXPECT_EQ(result, UCACHE_ERR);                            // 验证返回错误
}

TEST_F(BottleneckStrategyTest, Test_threshold_edge_cases)
{
    // 测试刚好达到或低于阈值的情况
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);

    const map<string, map<string, CgroupInfo>> cgroupInfo = {{"node0",
                                                              {{"edgeCase",
                                                                {.pageCacheIn = 20480, // 超过阈值
                                                                 .ioReadBandwidth = 20480}}}}};
    const map<string, map<string, CgroupInfo>> cgroupInfo2 = {{"node0",
                                                               {{"edgeCase",
                                                                 {.pageCacheIn = 480, // 都低于阈值
                                                                  .ioReadBandwidth = 480}}}}};

    auto& strategy = BottleneckStrategy::GetInstance();
    for (int i = 0; i < 4; ++i) { // 80% of 10 samples
        strategy.JudgeSensitive(cgroupInfo2, 10);
    }
    // 刚好达到短窗口阈值
    for (int i = 0; i < 16; ++i) { // 80% of 10 samples
        strategy.JudgeSensitive(cgroupInfo, 10);
    }
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap = strategy.GetContainerState();
    EXPECT_EQ(sensitiveMap["node0"]["edgeCase"], PageCacheSensitiveTag::SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_lowhPgin_lowIO_NOT_SENSITIVE)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"lowPgin_lowIO", {.pageCacheIn = 5120, .ioReadBandwidth = 5120}}}}};
    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;
    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();
    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
    sensitiveMap = strategy.GetContainerState();
    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行19次
    for (int i = 1; i < 19; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["lowPgin_lowIO"], PageCacheSensitiveTag::UNKNOWN);
    // 执行50次
    for (int i = 1; i < 50; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["lowPgin_lowIO"], PageCacheSensitiveTag::NOT_SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_lowhPgin_lowIO_NOT_SENSITIVE2)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"lowPgin_lowIO2", {.pageCacheIn = 5120, .ioReadBandwidth = 5120}}}}};
    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;
    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();
    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
    sensitiveMap = strategy.GetContainerState();
    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行19次
    for (int i = 1; i < 19; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["lowPgin_lowIO2"], PageCacheSensitiveTag::UNKNOWN);
    // 执行50次
    for (int i = 1; i < 50; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["lowPgin_lowIO2"], PageCacheSensitiveTag::NOT_SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_lowhPgin_lowIO_NOT_SENSITIVE3)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"lowPgin_lowIO3", {.pageCacheIn = 5120, .ioReadBandwidth = 5120}}}}};
    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;
    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();
    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
    sensitiveMap = strategy.GetContainerState();
    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行19次
    for (int i = 1; i < 19; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["lowPgin_lowIO3"], PageCacheSensitiveTag::UNKNOWN);
    // 执行50次
    for (int i = 1; i < 50; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
    }
    EXPECT_EQ(sensitiveMap["node0"]["lowPgin_lowIO3"], PageCacheSensitiveTag::NOT_SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_not_sensitive2)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
    const map<string, map<string, CgroupInfo>> cgroupInfo = {
        {"node0", {{"doris-b4", {.pageCacheIn = 5120, .ioReadBandwidth = 5120}}}}};
    const map<string, map<string, CgroupInfo>> cgroupInfo2 = {
        {"node0", {{"doris-b4", {.pageCacheIn = 20480, .ioReadBandwidth = 20480}}}}};
    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;

    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();

    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo2, 10);
    sensitiveMap = strategy.GetContainerState();
    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行六次
    for (int i = 1; i <= 60; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
        if (i == 40) {
            EXPECT_EQ(sensitiveMap["node0"]["doris-b4"], PageCacheSensitiveTag::UNKNOWN);
        }
    }
    EXPECT_EQ(sensitiveMap["node0"]["doris-b4"], PageCacheSensitiveTag::NOT_SENSITIVE);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_doris)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10240);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 70);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 30);
}

TEST_F(BottleneckStrategyTest, BottleneckStrategyTest_doris_remove_UNKOWN)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    map<string, map<string, CgroupInfo>> cgroupInfo{};
    GetCgrouopInfo(cgroupInfo, "remove_UNKOWN", static_cast<uint64_t>(5120), static_cast<uint64_t>(5120));

    map<string, map<string, CgroupInfo>> cgroupInfo2{};
    GetCgrouopInfo(cgroupInfo2, "remove_UNKOWN", static_cast<uint64_t>(20480), static_cast<uint64_t>(20480));

    map<string, map<string, CgroupInfo>> cgroupInfo3{};
    GetCgrouopInfo(cgroupInfo3, "remove_ed", static_cast<uint64_t>(20480), static_cast<uint64_t>(20480));

    // 创建敏感状态映射
    map<string, map<string, PageCacheSensitiveTag>> sensitiveMap;

    // 获取 BottleneckStrategy 实例
    auto& strategy = BottleneckStrategy::GetInstance();

    // 调用 JudgeSensitive 方法
    uint32_t result = strategy.JudgeSensitive(cgroupInfo, 10);
    sensitiveMap = strategy.GetContainerState();

    // 验证结果
    EXPECT_EQ(result, UCACHE_OK);
    // 执行六次
    for (int i = 1; i <= 18; i++) {
        result = strategy.JudgeSensitive(cgroupInfo, 10);
        sensitiveMap = strategy.GetContainerState();
        EXPECT_EQ(sensitiveMap["node0"]["remove_UNKOWN"], PageCacheSensitiveTag::UNKNOWN);
    }
    for (int i = 1; i <= 20; i++) {
        result = strategy.JudgeSensitive(cgroupInfo2, 10);
        sensitiveMap = strategy.GetContainerState();
        if (i < 13) {
            EXPECT_EQ(sensitiveMap["node0"]["remove_UNKOWN"], PageCacheSensitiveTag::UNKNOWN);
        } else {
            EXPECT_EQ(sensitiveMap["node0"]["remove_UNKOWN"], PageCacheSensitiveTag::SENSITIVE);
        }
    }
    sensitiveMap = strategy.GetContainerState();
    EXPECT_EQ(sensitiveMap["node0"]["remove_UNKOWN"], PageCacheSensitiveTag::SENSITIVE);

    result = strategy.JudgeSensitive(cgroupInfo3, 10);
    sensitiveMap = strategy.GetContainerState();
    EXPECT_EQ(sensitiveMap["node0"]["remove_UNKOWN"], PageCacheSensitiveTag::UNKNOWN);
}