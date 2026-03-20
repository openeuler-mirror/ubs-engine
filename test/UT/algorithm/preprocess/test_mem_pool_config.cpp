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

#include "test_mem_pool_config.h"
#include <mockcpp/mockcpp.h>
#include <mockcpp/mockcpp.hpp>
#include "mem_pool_config.h"

namespace ubse::ut::algorithm {
using namespace tc::rs::mem;

void TestMemPoolConfig::SetUp()
{
    Test::SetUp();
}
void TestMemPoolConfig::TearDown()
{
    Test::TearDown();
}

TEST_F(TestMemPoolConfig, TestMemPoolConfig)
{
    StrategyParam param;
    EXPECT_NO_THROW(MemPoolConfig config(param));
}

TEST_F(TestMemPoolConfig, TestIsHostMeshLocValid)
{
    StrategyParam param;
    MemPoolConfig config(param);
    EXPECT_EQ(true, config.IsHostMeshLocValid());
}

TEST_F(TestMemPoolConfig, TestRefreshNumaDelays)
{
    StrategyParam param;
    MemPoolConfig config(param);
    EXPECT_EQ(UBSE_OK, config.RefreshNumaDelays());
}

TEST_F(TestMemPoolConfig, TestRefreshNuma)
{
    StrategyParam param;
    MemPoolConfig config(param);
    config.memStaticParam.numAvailNumas = 1;
    EXPECT_EQ(UBSE_OK, config.RefreshNumaDelays());
}

TEST_F(TestMemPoolConfig, TestSysLatencyProcess)
{
    StrategyParam param;
    MemPoolConfig config(param);
    config.memStaticParam.numAvailNumas = 1;
    config.memStaticParam.numHosts = 1;
    config.memAvailSocketsCnt = 1;
    config.memSocketLoc2Idx[0][0] = 0;
    MemLoc loc{0, 0, 0};
    config.memStaticParam.availNumas[0] = loc;
    EXPECT_EQ(UBSE_OK, config.SysLatencyProcess());
}

TEST_F(TestMemPoolConfig, TestGetNumaIndex)
{
    StrategyParam param;
    MemPoolConfig config(param);
    config.memStaticParam.numAvailNumas = 1;
    config.memStaticParam.numHosts = 1;
    config.memAvailSocketsCnt = 1;
    config.memNumaLoc2Idx[0][0] = 0;
    MemLoc loc{0, 0, 0};
    config.memStaticParam.availNumas[0] = loc;
    EXPECT_EQ(0, config.GetNumaIndex(loc));
}

TEST_F(TestMemPoolConfig, TestGetNumaListInSocket)
{
    StrategyParam param;
    MemPoolConfig config(param);
    config.memStaticParam.numAvailNumas = 1;
    config.memStaticParam.numHosts = 1;
    config.memAvailSocketsCnt = 1;
    config.memNumaLoc2Idx[0][0] = 0;
    MemLoc loc{0, 0, 0};
    config.memStaticParam.availNumas[0] = loc;
    EXPECT_EQ(-1, config.GetNumaListInSocket(0, 0)[0]);
}

TEST_F(TestMemPoolConfig, TestGetNumaListInHost)
{
    StrategyParam param;
    MemPoolConfig config(param);
    config.memStaticParam.numAvailNumas = 1;
    config.memStaticParam.numHosts = 1;
    config.memAvailSocketsCnt = 1;
    config.memNumaLoc2Idx[0][0] = 0;
    MemLoc loc{0, 0, 0};
    config.memStaticParam.availNumas[0] = loc;
    EXPECT_EQ(0, config.GetNumaListInHost(0)[0]);
}

TEST_F(TestMemPoolConfig, TestIsNonPositive)
{
    StrategyParam param;
    MemPoolConfig config(param);
    MeshLoc cord;
    EXPECT_EQ(true, config.IsNonPositive(cord));
}
} // namespace ubse::ut::algorithm
