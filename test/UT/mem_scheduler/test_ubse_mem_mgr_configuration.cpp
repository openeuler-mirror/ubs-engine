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
#include "test_ubse_mem_mgr_configuration.h"

#include "test_ubse_mem_algo_account.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mgr_configuration.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_node_controller.h"
#include "ubse_node_topology.h"
#include "ubse_mem_account_helper.h"

namespace ubse::mem_scheduler::ut {
using namespace ubse::mem::strategy;
constexpr uint64_t MB_128 = 128 * ONE_M;
constexpr uint64_t MB_512 = 512 * ONE_M;

void TestMgrConfiguration::SetUp()
{
    Test::SetUp();
}
void TestMgrConfiguration::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestMgrConfiguration, GetAlgoLogLevelDebug)
{
    std::string tmp = "DEBUG";
    MOCKER_CPP(GetUbseConf<std::string>).stubs().with(any(), any(), outBound(tmp)).will(returnValue(UBSE_OK));
    const auto ret = MemMgrConfiguration::GetInstance().GetAlgoLogLevel();
    EXPECT_EQ(ret, tc::rs::mem::LogLevel::DEBUG);
}

TEST_F(TestMgrConfiguration, GetAlgoLogLevelInfo)
{
    std::string tmp = "INFO";
    MOCKER_CPP(GetUbseConf<std::string>).stubs().with(any(), any(), outBound(tmp)).will(returnValue(UBSE_OK));
    const auto ret = MemMgrConfiguration::GetInstance().GetAlgoLogLevel();
    EXPECT_EQ(ret, tc::rs::mem::LogLevel::INFO);
}

TEST_F(TestMgrConfiguration, GetAlgoLogLevelWarn)
{
    std::string tmp = "WARN";
    MOCKER_CPP(GetUbseConf<std::string>).stubs().with(any(), any(), outBound(tmp)).will(returnValue(UBSE_OK));
    const auto ret = MemMgrConfiguration::GetInstance().GetAlgoLogLevel();
    EXPECT_EQ(ret, tc::rs::mem::LogLevel::WARN);
}

TEST_F(TestMgrConfiguration, GetAlgoLogLevelError)
{
    std::string tmp = "ERROR";
    MOCKER_CPP(GetUbseConf<std::string>).stubs().with(any(), any(), outBound(tmp)).will(returnValue(UBSE_OK));
    const auto ret = MemMgrConfiguration::GetInstance().GetAlgoLogLevel();
    EXPECT_EQ(ret, tc::rs::mem::LogLevel::ERROR);
}

TEST_F(TestMgrConfiguration, GetHtracePath)
{
    std::string tmp = "htrace.path";
    MOCKER_CPP(GetUbseConf<std::string>).stubs().with(any(), any(), outBound(tmp)).will(returnValue(UBSE_OK));
    const auto ret = MemMgrConfiguration::GetInstance().GetHtracePath();
    EXPECT_EQ(ret, "htrace.path");
}

TEST_F(TestMgrConfiguration, GetObmmSystemPoolMemRatioFromConfSuccess)
{
    std::string tmp = "50";
    MOCKER_CPP(GetUbseConf<std::string>).stubs().with(any(), any(), outBound(tmp)).will(returnValue(UBSE_OK));
    MemMgrConfiguration::GetInstance().GetObmmSystemPoolMemRatioFromConf();
    auto ret = MemMgrConfiguration::GetInstance().systemPoolMemRatio;
    uint64_t expect = 50;
    EXPECT_EQ(ret, expect);
}

TEST_F(TestMgrConfiguration, GetObmmSystemPoolMemRatioFromConfFail)
{
    MemMgrConfiguration::GetInstance().systemPoolMemRatio = 100;
    MOCKER_CPP(GetUbseConf<std::string>).stubs().will(returnValue(UBSE_ERROR));
    MemMgrConfiguration::GetInstance().GetObmmSystemPoolMemRatioFromConf();
    auto ret = MemMgrConfiguration::GetInstance().systemPoolMemRatio;
    uint64_t expect = 100;
    EXPECT_EQ(ret, expect);
}

TEST_F(TestMgrConfiguration, GetBlockSizeFromConfSuccess)
{
    std::string tmp = "512";
    MOCKER_CPP(GetUbseConf<std::string>).stubs().with(any(), any(), outBound(tmp)).will(returnValue(UBSE_OK));
    MemMgrConfiguration::GetInstance().GetBlockSizeFromConf();
    auto ret = MemMgrConfiguration::GetInstance().blockSize;
    uint64_t expect = MB_512;
    EXPECT_EQ(ret, expect);
}

TEST_F(TestMgrConfiguration, GetBlockSizeFromConfFail)
{
    MemMgrConfiguration::GetInstance().blockSize = MB_128;
    MOCKER_CPP(GetUbseConf<std::string>).stubs().will(returnValue(UBSE_ERROR));
    MemMgrConfiguration::GetInstance().GetBlockSizeFromConf();
    auto ret = MemMgrConfiguration::GetInstance().blockSize;
    uint64_t expect = MB_128;
    EXPECT_EQ(ret, expect);
}

}  // namespace ubse::mem_scheduler::ut