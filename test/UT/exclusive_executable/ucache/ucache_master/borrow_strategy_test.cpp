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

#include <vector>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "data_collect.h"
#include "mem_borrow.h"
#include "borrow_action.h"
#include "borrow_strategy.h"

using namespace std;
using namespace ucache;
using namespace ucache::data_collect;
using namespace ucache::mem_borrow;
using namespace ucache::borrow_action;
using namespace ucache::borrow_strategy;

unsigned long one_GB = 0x40000000UL;

// 物理拓扑
static std::map<std::string, std::vector<std::string>> physicalTopo = {
    {"Node0", {"Node1", "Node3"}},
    {"Node1", {"Node0", "Node2"}},
    {"Node2", {"Node1", "Node3"}},
    {"Node3", {"Node0", "Node2"}},
};

// 各节点可借用内存信息
static std::map<std::string, std::map<int, uint64_t>> loanableMemRawData = {
    {"Node0", {{0, one_GB}, {1, one_GB}, {2, one_GB}, {3, 0}}},
    {"Node1", {{0, one_GB}, {1, one_GB}, {2, one_GB}, {3, 0}}},
    {"Node2", {{0, 4 * one_GB}, {1, 4 * one_GB}, {2, 4 * one_GB}, {3, 0}}},
    {"Node3", {{0, one_GB}, {1, one_GB}, {2, one_GB}, {3, 0}}},
};

// 各节点内存初始使用信息
static std::vector<BorrowStrategyRawData> rawData = {
    {
        .nodeId = "Node0",
        .pagecacheAppNums = 0,
        .freeMemMin = 4 * one_GB,
        .localMemInfo = {
            .total = 32 * one_GB,
            .available = 14 * one_GB,
            .used = 18 * one_GB,
            .pagecache = 12 * one_GB,
        },
        .remoteNumaMemInfo = {},
    },
    {
        .nodeId = "Node1",
        .pagecacheAppNums = 0,
        .freeMemMin = 4 * one_GB,
        .localMemInfo = {
            .total = 32 * one_GB,
            .available = 10 * one_GB,
            .used = 22 * one_GB,
            .pagecache = 18 * one_GB,
        },
        .remoteNumaMemInfo = {},
    },
    {
        .nodeId = "Node2",
        .pagecacheAppNums = 3,
        .freeMemMin = 4 * one_GB,
        .localMemInfo = {
            .total = 32 * one_GB,
            .available = 10 * one_GB,
            .used = 22 * one_GB,
            .pagecache = 18 * one_GB,
        },
        .remoteNumaMemInfo = {},
    },
    {
        .nodeId = "Node3",
        .pagecacheAppNums = 3,
        .freeMemMin = 4 * one_GB,
        .localMemInfo = {
            .total = 32 * one_GB,
            .available = 10 * one_GB,
            .used = 22 * one_GB,
            .pagecache = 18 * one_GB,
        },
        .remoteNumaMemInfo = {},
    }
};

class BorrowStrategyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        DataCollect::SetPhysicalTopo(physicalTopo);
        DataCollect::SetLoanableTotalBorrowMemMap(loanableMemRawData);
        MemBorrowTopo::InitGlobalMemBorrowTopo();
        DataCollect::SetBorrowStrategyRawData(rawData);
        BorrowNodeStat::InitBorrowNodeStat();
        cout << "[Phase SetUp End]" << endl;
    }

    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        MemBorrowTopo::globalMemBorrowTopo.lendMap.clear();
        MemBorrowTopo::globalMemBorrowTopo.borrowMap.clear();
        MemBorrowTopo::globalMemBorrowTopo.borrowMemInfoPerNodeMap.clear();
        BorrowNodeStat::ClearBorrowNodeStat();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(BorrowStrategyTest, BorrowStrategyInitTest)
{
    int32_t ret = UCACHE_OK;
    BorrowStrategy strategy = {};
    ret = strategy.Init();
    EXPECT_EQ(ret, UCACHE_OK);

    BorrowNodeStat::ClearBorrowNodeStat();
    ret = strategy.Init();
    EXPECT_EQ(ret, SCARCITY_INVALID_INPUT);
}

TEST_F(BorrowStrategyTest, GetBalanceTest)
{
    int32_t ret = UCACHE_OK;
    double balance;
    BorrowStrategy strategy = {};
    ret = strategy.Init();
    EXPECT_EQ(ret, UCACHE_OK);

    balance = strategy.GetBalance();
    EXPECT_NEAR(balance, 1.83013, 0.00001);
}

void TestSetRawData(std::vector<BorrowStrategyRawData> &RawDatas,
                    std::string nodeId, int *localInfo, bool isRemote, int *remoteInfo, size_t remoteInfoLen)
{
    if (isRemote && remoteInfoLen >= 4) {
        BorrowStrategyRawData rawData = {
            .nodeId = nodeId,
            .pagecacheAppNums = localInfo[0],
            .freeMemMin = localInfo[1] * one_GB,
            .localMemInfo = {
                .total = localInfo[2] * one_GB,
                .available = localInfo[3] * one_GB,
                .used = localInfo[4] * one_GB,
                .pagecache = localInfo[5] * one_GB,
            },
            .remoteNumaMemInfo = {
                {5, {
                    .total = remoteInfo[0] * one_GB,
                    .available = remoteInfo[1] * one_GB,
                    .used = remoteInfo[2] * one_GB,
                    .pagecache = remoteInfo[3] * one_GB,
                }},
            },
        };
        RawDatas.emplace_back(rawData);
    } else {
        BorrowStrategyRawData rawData = {
            .nodeId = nodeId,
            .pagecacheAppNums = localInfo[0],
            .freeMemMin = localInfo[1] * one_GB,
            .localMemInfo = {
                .total = localInfo[2] * one_GB,
                .available = localInfo[3] * one_GB,
                .used = localInfo[4] * one_GB,
                .pagecache = localInfo[5] * one_GB,
            },
            .remoteNumaMemInfo = {},
        };
        RawDatas.emplace_back(rawData);
    }
}

void GetRawDataBorrowStrategyStartTest(std::vector<BorrowStrategyRawData> &rawDatas)
{
    int localInfo0[6] = {3, 4, 29, 6, 24, 8};
    int localInfo1[6] = {0, 4, 29, 18, 11, 6};
    int localInfo2[6] = {3, 4, 32, 7, 26, 6};
    int localInfo3[6] = {0, 4, 32, 10, 22, 18};
    int remoteInfo2[4] = {3, 3, 1, 1};
    int remoteInfo3[4] = {3, 1, 2, 2};
    TestSetRawData(rawDatas, "Node0", localInfo0, false, remoteInfo2, 4);
    TestSetRawData(rawDatas, "Node1", localInfo1, false, remoteInfo2, 4);
    TestSetRawData(rawDatas, "Node2", localInfo2, true, remoteInfo2, 4);
    TestSetRawData(rawDatas, "Node3", localInfo3, true, remoteInfo3, 4);
}

TEST_F(BorrowStrategyTest, BorrowStrategyStartTest)
{
    int32_t ret = UCACHE_OK;
    double balanceBeforeStrategy;
    double balanceAfterStrategy;
    BorrowStrategy strategy = {};
    ret = strategy.Init();
    EXPECT_EQ(ret, UCACHE_OK);

    balanceBeforeStrategy = strategy.GetBalance();
    strategy.Start();
    balanceAfterStrategy = strategy.GetBalance();
    EXPECT_EQ(balanceAfterStrategy <= balanceBeforeStrategy, true);

    // 模拟借用动作执行成功
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node0", "Node3");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node0", "Node3");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node0", "Node3");
    EXPECT_EQ(ret, UCACHE_OK);

    // 模拟第二次数据上报

    std::vector<BorrowStrategyRawData> newRawData{};
    GetRawDataBorrowStrategyStartTest(newRawData);

    DataCollect::SetBorrowStrategyRawData(newRawData);
    BorrowNodeStat::InitBorrowNodeStat();

    strategy.Init();
    balanceBeforeStrategy = strategy.GetBalance();
    strategy.Start();
    balanceAfterStrategy = strategy.GetBalance();
    EXPECT_EQ(balanceAfterStrategy <= balanceBeforeStrategy, true);

    std::vector<BorrowAction> actions = strategy.GetActionSet();
    EXPECT_EQ(actions.size(), 5);

    EXPECT_EQ(actions[0].type, ActionType::RETURN);
    EXPECT_EQ(actions[0].nodeMemBorrowInfo.srcNodeId, "Node0");
    EXPECT_EQ(actions[0].nodeMemBorrowInfo.destNodeId, "Node3");

    EXPECT_EQ(actions[1].type, ActionType::RETURN);
    EXPECT_EQ(actions[1].nodeMemBorrowInfo.srcNodeId, "Node0");
    EXPECT_EQ(actions[1].nodeMemBorrowInfo.destNodeId, "Node3");

    EXPECT_EQ(actions[2].type, ActionType::RETURN);
    EXPECT_EQ(actions[2].nodeMemBorrowInfo.srcNodeId, "Node0");
    EXPECT_EQ(actions[2].nodeMemBorrowInfo.destNodeId, "Node3");

    EXPECT_EQ(actions[3].type, ActionType::RETURN);
    EXPECT_EQ(actions[3].nodeMemBorrowInfo.srcNodeId, "Node1");
    EXPECT_EQ(actions[3].nodeMemBorrowInfo.destNodeId, "Node2");

    EXPECT_EQ(actions[4].type, ActionType::BORROW);
    EXPECT_EQ(actions[4].nodeMemBorrowInfo.srcNodeId, "Node1");
    EXPECT_EQ(actions[4].nodeMemBorrowInfo.destNodeId, "Node0");

    strategy.Init();
}

TEST_F(BorrowStrategyTest, GetActionSetTest)
{
    int32_t ret = UCACHE_OK;
    BorrowStrategy strategy = {};
    ret = strategy.Init();
    EXPECT_EQ(ret, UCACHE_OK);

    strategy.Start();
    std::vector<BorrowAction> actions = strategy.GetActionSet();
    EXPECT_EQ(actions.size(), 6);

    EXPECT_EQ(actions[0].type, ActionType::BORROW);
    EXPECT_EQ(actions[0].nodeMemBorrowInfo.srcNodeId, "Node1");
    EXPECT_EQ(actions[0].nodeMemBorrowInfo.destNodeId, "Node2");

    EXPECT_EQ(actions[1].type, ActionType::BORROW);
    EXPECT_EQ(actions[1].nodeMemBorrowInfo.srcNodeId, "Node0");
    EXPECT_EQ(actions[1].nodeMemBorrowInfo.destNodeId, "Node3");

    EXPECT_EQ(actions[2].type, ActionType::BORROW);
    EXPECT_EQ(actions[2].nodeMemBorrowInfo.srcNodeId, "Node1");
    EXPECT_EQ(actions[2].nodeMemBorrowInfo.destNodeId, "Node2");

    EXPECT_EQ(actions[3].type, ActionType::BORROW);
    EXPECT_EQ(actions[3].nodeMemBorrowInfo.srcNodeId, "Node0");
    EXPECT_EQ(actions[3].nodeMemBorrowInfo.destNodeId, "Node3");

    EXPECT_EQ(actions[4].type, ActionType::BORROW);
    EXPECT_EQ(actions[4].nodeMemBorrowInfo.srcNodeId, "Node1");
    EXPECT_EQ(actions[4].nodeMemBorrowInfo.destNodeId, "Node2");

    EXPECT_EQ(actions[5].type, ActionType::BORROW);
    EXPECT_EQ(actions[5].nodeMemBorrowInfo.srcNodeId, "Node0");
    EXPECT_EQ(actions[5].nodeMemBorrowInfo.destNodeId, "Node3");
}

TEST_F(BorrowStrategyTest, StartWithoutInitTest)
{
    BorrowStrategy strategy = {};
    strategy.Start();

    std::vector<BorrowAction> actions = strategy.GetActionSet();
    EXPECT_EQ(actions.size(), 0);
}

// 所有节点稀缺度都小于阈值
TEST_F(BorrowStrategyTest, ScarcityLessThanThresholdTest)
{
    int localInfo0[6] = {1, 12, 256, 240, 16, 1};
    int localInfo1[6] = {0, 10, 128, 120, 8, 1};
    int localInfo2[6] = {0, 10, 128, 118, 18, 1};
    int localInfo3[6] = {1, 10, 256, 200, 56, 1};
    int remoteInfo[1] = {0};
    std::vector<BorrowStrategyRawData> rawData1{};
    TestSetRawData(rawData1, "Node0", localInfo0, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node1", localInfo1, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node2", localInfo2, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node3", localInfo3, false, remoteInfo, 1);

    DataCollect::SetBorrowStrategyRawData(rawData1);
    BorrowNodeStat::InitBorrowNodeStat();

    BorrowStrategy strategy = {};
    strategy.Init();
    strategy.Start();
    std::vector<BorrowAction> actions = strategy.GetActionSet();
    EXPECT_EQ(actions.size(), 0);
}

// 没有节点可以借用内存
TEST_F(BorrowStrategyTest, NoNodeCanBorrowTest)
{
    static std::map<std::string, std::vector<std::string>> wrongPhysicalTopo = {
        {"Node0", {"Node1", "Node3"}},
        {"Node1", {"Node0"}},
        {"Node2", {}},
        {"Node3", {"Node0"}},
    };
    DataCollect::SetPhysicalTopo(wrongPhysicalTopo);
    MemBorrowTopo::InitGlobalMemBorrowTopo();

    BorrowStrategy strategy = {};
    strategy.Init();
    strategy.Start();
    std::vector<BorrowAction> actions = strategy.GetActionSet();
    EXPECT_EQ(actions.size(), 0);
}

// 单节点借用超过限制
TEST_F(BorrowStrategyTest, BorrowSizeLargeThanLimitTest)
{
    uint32_t ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);

    int localInfo0[6] = {0, 12, 128, 100, 28, 18};
    int localInfo1[6] = {3, 10, 128, 50, 78, 30};
    int localInfo2[6] = {0, 10, 256, 228, 28, 2};
    int localInfo3[6] = {0, 10, 128, 100, 28, 18};
    int remoteInfo[1] = {0};
    std::vector<BorrowStrategyRawData> rawData1{};
    TestSetRawData(rawData1, "Node0", localInfo0, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node1", localInfo1, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node2", localInfo2, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node3", localInfo3, false, remoteInfo, 1);

    DataCollect::SetBorrowStrategyRawData(rawData1);
    BorrowNodeStat::InitBorrowNodeStat();

    BorrowStrategy strategy = {};
    strategy.Init();
    strategy.Start();
    std::vector<BorrowAction> actions = strategy.GetActionSet();
    EXPECT_EQ(actions.size(), 0);
}

// 没有瓶颈型应用后回收所有借用内存
TEST_F(BorrowStrategyTest, ReclaimAllMemoryTest)
{
    // 模拟借用动作执行成功
    uint32_t ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node0");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node3", "Node0");
    EXPECT_EQ(ret, UCACHE_OK);
    int localInfo0[6] = {0, 12, 256, 240, 16, 1};
    int localInfo1[6] = {0, 10, 128, 120, 8, 1};
    int localInfo2[6] = {0, 10, 128, 118, 18, 1};
    int localInfo3[6] = {0, 10, 256, 200, 56, 1};
    int remoteInfo[1] = {0};
    std::vector<BorrowStrategyRawData> rawData1{};
    TestSetRawData(rawData1, "Node0", localInfo0, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node1", localInfo1, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node2", localInfo2, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node3", localInfo3, false, remoteInfo, 1);

    DataCollect::SetBorrowStrategyRawData(rawData1);
    BorrowNodeStat::InitBorrowNodeStat();

    BorrowStrategy strategy = {};
    strategy.Init();
    strategy.Start();
    std::vector<BorrowAction> actions = strategy.GetActionSet();
    EXPECT_EQ(actions.size(), 3);

    EXPECT_EQ(actions[0].type, ActionType::RETURN);
    EXPECT_EQ(actions[0].nodeMemBorrowInfo.srcNodeId, "Node1");
    EXPECT_EQ(actions[0].nodeMemBorrowInfo.destNodeId, "Node0");

    EXPECT_EQ(actions[1].type, ActionType::RETURN);
    EXPECT_EQ(actions[1].nodeMemBorrowInfo.srcNodeId, "Node3");
    EXPECT_EQ(actions[1].nodeMemBorrowInfo.destNodeId, "Node0");

    EXPECT_EQ(actions[2].type, ActionType::RETURN);
    EXPECT_EQ(actions[2].nodeMemBorrowInfo.srcNodeId, "Node1");
    EXPECT_EQ(actions[2].nodeMemBorrowInfo.destNodeId, "Node2");
}

TEST_F(BorrowStrategyTest, ReclaimAllMemoryLimitTest)
{
    uint32_t ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);
    int localInfo0[6] = {0, 12, 256, 240, 16, 1};
    int localInfo1[6] = {0, 10, 128, 120, 8, 1};
    int localInfo2[6] = {0, 10, 128, 118, 18, 1};
    int localInfo3[6] = {0, 10, 256, 200, 56, 1};
    int remoteInfo[1] = {0};
    std::vector<BorrowStrategyRawData> rawData1{};
    TestSetRawData(rawData1, "Node0", localInfo0, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node1", localInfo1, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node2", localInfo2, false, remoteInfo, 1);
    TestSetRawData(rawData1, "Node3", localInfo3, false, remoteInfo, 1);

    DataCollect::SetBorrowStrategyRawData(rawData1);
    BorrowNodeStat::InitBorrowNodeStat();

    BorrowStrategy strategy = {};
    strategy.Init();
    strategy.Start();
    std::vector<BorrowAction> actions = strategy.GetActionSet();
    EXPECT_EQ(actions.size(), 10);
}