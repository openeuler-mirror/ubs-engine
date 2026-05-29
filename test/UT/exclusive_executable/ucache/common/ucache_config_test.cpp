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

#include <gtest/gtest.h>
#include "mockcpp/mokc.h"
#define private public
#include <ubse_election.h>
#include "ucache_config.h"
#include "ucache_config_check.h"
#include "ucache_error.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace ucache {

using namespace ubse::config;
using namespace ubse::log;

class UcacheConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(UcacheConfigTest, LoadConfig)
{
    UcacheConfig obj;

    uint32_t ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_OK);

    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetFloat).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(ubse::election::UbseGetNodeIds).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER_CPP(&ucache::UcacheConfig::LoadMasterConfig, uint32_t(*)(void*)).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER_CPP(&ucache::UcacheConfig::LoadStrategyConfig, uint32_t(*)(void*)).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER_CPP(&ucache::UcacheConfig::LoadBottleneckConfig, uint32_t(*)(void*)).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    // nodeIds为空的情况
    MOCKER(ubse::election::UbseGetNodeIds).stubs().will(returnValue(UCACHE_OK));
    obj.nodeIds.clear();
    ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    // exportInterval小于0的情况
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_OK));
    obj.exportInterval = 0;
    ret = obj.UcacheConfig::LoadConfig();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(UcacheConfigTest, LoadStrategyConfigTest1)
{
    UcacheConfig obj;

    // 正常情况
    uint32_t ret = obj.UcacheConfig::LoadStrategyConfig();
    EXPECT_EQ(ret, UCACHE_OK);

    // 验证配置值
    uint64_t tmpBorrowSize = obj.UcacheConfig::GetBorrowSize();
    EXPECT_EQ(tmpBorrowSize, 0x40000000UL);
    uint32_t tmpMaxActionSize = obj.UcacheConfig::GetMaxActionSize();
    EXPECT_EQ(tmpMaxActionSize, 10);
    float tmpBalanceThreshold = obj.UcacheConfig::GetBalanceThreshold();
    EXPECT_EQ(tmpBalanceThreshold, 1.5);

    GlobalMockObject::verify();

    MOCKER(UbseGetULong).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadStrategyConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadStrategyConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetFloat).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadStrategyConfig();
    EXPECT_EQ(ret, UCACHE_ERR);
    GlobalMockObject::verify();

    MOCKER_CPP(&ucache::UcacheConfig::RackGetUInt32AndCheck, uint32_t(*)(void*)).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadStrategyConfig();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(UcacheConfigTest, LoadStrategyConfigTest2)
{
    UcacheConfig obj;
    MOCKER(UbseGetULong).stubs().will(returnValue(UCACHE_OK));
    uint64_t invalidBorrowSize = MIN_BORROW_SIZE - 1;
    obj.borrowSize = invalidBorrowSize;
    uint32_t ret = obj.UcacheConfig::LoadStrategyConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetULong).stubs().will(returnValue(UCACHE_OK));
    invalidBorrowSize = MAX_BORROW_SIZE + 1;
    obj.borrowSize = invalidBorrowSize;
    ret = obj.UcacheConfig::LoadStrategyConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetFloat).stubs().will(returnValue(UCACHE_OK));
    float invalidBalanceThreshold = MIN_BALANCE_THRESHOLD - 0.1f;
    obj.balanceThreshold = invalidBalanceThreshold;
    ret = obj.UcacheConfig::LoadStrategyConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetFloat).stubs().will(returnValue(UCACHE_OK));
    obj.balanceThreshold = MIN_BALANCE_THRESHOLD;
    obj.scarcityThreshold = -1;
    ret = obj.UcacheConfig::LoadStrategyConfig();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(UcacheConfigTest, LoadMasterConfigTest)
{
    UcacheConfig obj;
    const uint32_t MAX_MASTER_INTERVAL = 15;
    const uint32_t MIN_MASTER_INTERVAL = 1;
    uint32_t ret = obj.UcacheConfig::LoadMasterConfig();
    EXPECT_EQ(ret, UCACHE_OK);

    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadMasterConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetULong).stubs().will(returnValue(UCACHE_ERR));
    ret = obj.UcacheConfig::LoadMasterConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_OK));
    uint32_t invalidBorrowSize = MAX_MASTER_INTERVAL + 1;
    obj.masterInterval = invalidBorrowSize;
    ret = obj.UcacheConfig::LoadMasterConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_OK));
    invalidBorrowSize = MIN_MASTER_INTERVAL - 1;
    obj.masterInterval = invalidBorrowSize;
    ret = obj.UcacheConfig::LoadMasterConfig();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();

    MOCKER(UbseGetULong).stubs().will(returnValue(UCACHE_OK));
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_OK));
    obj.masterInterval = MIN_MASTER_INTERVAL + 1;
    obj.masterMaxBorrowSize = obj.borrowSize - 1;
    ret = obj.UcacheConfig::LoadMasterConfig();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(UcacheConfigTest, InitializeTest)
{
    UcacheConfig obj;
    uint32_t ret = obj.UcacheConfig::Initialize(0);
    EXPECT_EQ(ret, UCACHE_OK);
    uint32_t tmpExportInterval = obj.UcacheConfig::GetExportInterval();
    EXPECT_EQ(tmpExportInterval, 10);
    std::vector<std::string> nodeIds = obj.UcacheConfig::GetNodeIds();
    EXPECT_EQ(nodeIds.size(), 2);

    UcacheConfig obj2;
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    ret = obj2.UcacheConfig::Initialize(0);
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(UcacheConfigTest, LoadBottleneckConfigOKTest)
{
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);

    uint32_t val = UcacheConfig::GetInstance().GetBottleneckThreshold();
    EXPECT_EQ(val, 10);
    val = UcacheConfig::GetInstance().GetBottleneckShortSize();
    EXPECT_EQ(val, 180);
    val = UcacheConfig::GetInstance().GetBottleneckShortThreshold();
    EXPECT_EQ(val, 10);
    val = UcacheConfig::GetInstance().GetBottleneckLongSize();
    EXPECT_EQ(val, 600);
    val = UcacheConfig::GetInstance().GetBottleneckLongThreshold();
    EXPECT_EQ(val, 10);
}

TEST_F(UcacheConfigTest, LoadBottleneckConfigFailedTest)
{
    MOCKER(UbseGetUInt).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UcacheConfig::GetInstance().LoadBottleneckConfig();
    EXPECT_EQ(ret, UCACHE_OK);

    // 读 rack 配置文件失败时，使用默认值
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
    val = UcacheConfig::GetInstance().GetMaxReliableTimes();
    EXPECT_EQ(val, 3);
    val = UcacheConfig::GetInstance().GetMasterInterval();
    EXPECT_EQ(val, 10);
}

TEST_F(UcacheConfigTest, RackGetUInt32AndCheckFailedTest)
{
    uint32_t val = 1;
    uint32_t ret = UcacheConfig::GetInstance().RackGetUInt32AndCheck("not_exist_key", val);
    EXPECT_EQ(ret, UCACHE_ERR);
    EXPECT_EQ(val, 1);
}

TEST_F(UcacheConfigTest, JudgedTest)
{
    UcacheConfig::GetInstance().JudgeWindowLength(1, 2);
    UcacheConfig::GetInstance().JudgeWindowLength(2, 1);
    UcacheConfig::GetInstance().JudgeThreshold(2, 1);
    UcacheConfig::GetInstance().JudgeThreshold(1, 2);
}

} // namespace ucache
