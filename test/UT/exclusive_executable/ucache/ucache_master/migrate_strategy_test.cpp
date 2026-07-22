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

#include <limits>

#include "bottleneck_strategy.h"
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "ucache_error.h"
#include "ucache_migrate_strategy.h"

using namespace std;
using namespace ucache;
using namespace ucache::master::migration;

namespace ucache::master::migration {
bool CompareNodes(const std::pair<std::string, NodeMemoryInfo>& a, const std::pair<std::string, NodeMemoryInfo>& b);
std::pair<uint64_t, uint64_t> CalculateWatermarks(const NodeMemoryInfo& borrower);
uint32_t GetBestLenderNumaId(const std::vector<NodeMemBorrowInfo>& lenders, const std::string& bestLenderId,
                             int& bestLenderNumaId);
std::string PrintMigrationAction(const std::vector<MigrationAction>& actions);
} // namespace ucache::master::migration

class MigrateStrategyTest : public testing::Test {
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

void GetBorrowMap(std::map<std::string, std::vector<NodeMemBorrowInfo>>& borrowMap)
{
    const std::map<std::string, std::vector<NodeMemBorrowInfo>> MyBorrowMap = {
        {"A",
         {NodeMemBorrowInfo{
              .totalSize = 51200, .srcNodeId = "A", .destNodeId = "C", .numaNodeBorrowSize = {{}}, .dstNumaId = 3},
          NodeMemBorrowInfo{
              .totalSize = 61440, .srcNodeId = "A", .destNodeId = "B", .numaNodeBorrowSize = {{}}, .dstNumaId = 4}}},
        {"D",
         {NodeMemBorrowInfo{
             .totalSize = 61440, .srcNodeId = "D", .destNodeId = "E", .numaNodeBorrowSize = {{}}, .dstNumaId = 1}}}};
    borrowMap = MyBorrowMap;
}

void GetNodes(std::map<std::string, NodeMemoryInfo>& nodes)
{
    std::map<std::string, NodeMemoryInfo> MyNodes = {{"A",
                                                      {.usedMemory = 819200,
                                                       .freeMemory = 204800,
                                                       .pageCacheMemory = 307200,
                                                       .borrowedUsageRate = 0.6,
                                                       .memoryShortage = 60.5,
                                                       .pageCacheAppNums = 1,
                                                       .minFreeKbytes = 1000}},
                                                     {"B",
                                                      {.usedMemory = 1572864,
                                                       .freeMemory = 409600,
                                                       .pageCacheMemory = 524288,
                                                       .borrowedUsageRate = 0.4,
                                                       .memoryShortage = 65.2,
                                                       .pageCacheAppNums = 1,
                                                       .minFreeKbytes = 1000}},
                                                     {"C",
                                                      {.usedMemory = 1310720,
                                                       .freeMemory = 204800,
                                                       .pageCacheMemory = 262144,
                                                       .borrowedUsageRate = 0.7,
                                                       .memoryShortage = 65.1,
                                                       .pageCacheAppNums = 1,
                                                       .minFreeKbytes = 1000}},
                                                     {"D",
                                                      {.usedMemory = 32020236,
                                                       .freeMemory = 371940,
                                                       .pageCacheMemory = 27304424,
                                                       .borrowedUsageRate = 0.5,
                                                       .memoryShortage = 75.3,
                                                       .pageCacheAppNums = 1,
                                                       .minFreeKbytes = 1000}},
                                                     {"E",
                                                      {.usedMemory = 32793716,
                                                       .freeMemory = 227612,
                                                       .pageCacheMemory = 24613880,
                                                       .borrowedUsageRate = 0.8,
                                                       .memoryShortage = 45.7,
                                                       .pageCacheAppNums = 1,
                                                       .minFreeKbytes = 1000}}};
    nodes = MyNodes;
}

TEST_F(MigrateStrategyTest, MemoryMigrationStrategy_BasicScenario)
{
    std::map<std::string, std::vector<NodeMemBorrowInfo>> borrowMap{};
    GetBorrowMap(borrowMap);

    std::map<std::string, NodeMemoryInfo> nodes{};
    GetNodes(nodes);

    std::map<std::string, std::map<std::string, PageCacheSensitiveTag>> nodeTags = {
        {"A", {{"DOCKER1", PageCacheSensitiveTag::SENSITIVE}, {"DOCKER2", PageCacheSensitiveTag::NOT_SENSITIVE}}},
        {"B", {{"DOCKER3", PageCacheSensitiveTag::SENSITIVE}}},
        {"C", {{"DOCKER4", PageCacheSensitiveTag::SENSITIVE}, {"DOCKER5", PageCacheSensitiveTag::NOT_SENSITIVE}}},
        {"D", {{"DOCKER6", PageCacheSensitiveTag::SENSITIVE}, {"DOCKER7", PageCacheSensitiveTag::NOT_SENSITIVE}}}};

    std::vector<MigrationAction> expectedActions = {MigrationAction{
                                                        .fromNode = "A",
                                                        .dockerIds = {"DOCKER1"},
                                                        .toNode = "C",
                                                        .startWatermark = 164249,
                                                        .stopWatermark = 241049,
                                                    },
                                                    MigrationAction{
                                                        .fromNode = "D",
                                                        .dockerIds = {"DOCKER6"},
                                                        .toNode = "E",
                                                        .startWatermark = 215568,
                                                        .stopWatermark = 4367022,
                                                    }};

    std::vector<MigrationAction> actions = MemoryMigrationStrategy(borrowMap, nodes, nodeTags);
    EXPECT_EQ(actions.size(), expectedActions.size());

    // 遍历每个 action 并比较其属性
    for (int i = 0; i < actions.size(); ++i) {
        const auto& action = actions[i];
        const auto& expectedAction = expectedActions[i];
        EXPECT_EQ(action.fromNode, expectedAction.fromNode);
        EXPECT_EQ(action.toNode, expectedAction.toNode);
        EXPECT_EQ(action.dockerIds, expectedAction.dockerIds);
        EXPECT_EQ(action.startWatermark, expectedAction.startWatermark);
        EXPECT_EQ(action.stopWatermark, expectedAction.stopWatermark);
    }
}

TEST_F(MigrateStrategyTest, DifferentMemoryShortage)
{
    NodeMemoryInfo nodeAInfo = {.usedMemory = 0,
                                .freeMemory = 0,
                                .pageCacheMemory = 0,
                                .borrowedUsageRate = 0.1,
                                .memoryShortage = 0.2,
                                .pageCacheAppNums = 0,
                                .minFreeKbytes = 0};
    std::pair<std::string, NodeMemoryInfo> nodeA = {"node1", nodeAInfo};

    NodeMemoryInfo nodeBInfo = {.usedMemory = 0,
                                .freeMemory = 0,
                                .pageCacheMemory = 0,
                                .borrowedUsageRate = 0.1,
                                .memoryShortage = 0.1,
                                .pageCacheAppNums = 0,
                                .minFreeKbytes = 0};
    std::pair<std::string, NodeMemoryInfo> nodeB = {"node2", nodeBInfo};

    EXPECT_TRUE(CompareNodes(nodeB, nodeA)); // nodeB 应该排在 nodeA 前面，因为 memoryShortage 更小
}

TEST_F(MigrateStrategyTest, SameMemoryShortageDifferentBorrowedUsageRate)
{
    NodeMemoryInfo nodeAInfo = {.usedMemory = 0,
                                .freeMemory = 0,
                                .pageCacheMemory = 0,
                                .borrowedUsageRate = 0.2,
                                .memoryShortage = 0.1,
                                .pageCacheAppNums = 0,
                                .minFreeKbytes = 0};
    std::pair<std::string, NodeMemoryInfo> nodeA = {"node1", nodeAInfo};

    NodeMemoryInfo nodeBInfo = {.usedMemory = 0,
                                .freeMemory = 0,
                                .pageCacheMemory = 0,
                                .borrowedUsageRate = 0.1,
                                .memoryShortage = 0.1,
                                .pageCacheAppNums = 0,
                                .minFreeKbytes = 0};
    std::pair<std::string, NodeMemoryInfo> nodeB = {"node2", nodeBInfo};

    EXPECT_TRUE(CompareNodes(nodeB, nodeA)); // nodeB 应该排在 nodeA 前面，因为 borrowedUsageRate 更小
}

TEST_F(MigrateStrategyTest, SameMemoryShortageAndBorrowedUsageRateDifferentNodeIDs)
{
    NodeMemoryInfo nodeAInfo = {.usedMemory = 0,
                                .freeMemory = 0,
                                .pageCacheMemory = 0,
                                .borrowedUsageRate = 0.1,
                                .memoryShortage = 0.1,
                                .pageCacheAppNums = 0,
                                .minFreeKbytes = 0};
    std::pair<std::string, NodeMemoryInfo> nodeA = {"node2", nodeAInfo};

    NodeMemoryInfo nodeBInfo = {.usedMemory = 0,
                                .freeMemory = 0,
                                .pageCacheMemory = 0,
                                .borrowedUsageRate = 0.1,
                                .memoryShortage = 0.1,
                                .pageCacheAppNums = 0,
                                .minFreeKbytes = 0};
    std::pair<std::string, NodeMemoryInfo> nodeB = {"node1", nodeBInfo};

    EXPECT_TRUE(CompareNodes(nodeB, nodeA)); // nodeB 应该排在 nodeA 前面，因为节点ID字典序更小
}

TEST_F(MigrateStrategyTest, IdenticalNodes)
{
    NodeMemoryInfo nodeAInfo = {.usedMemory = 0,
                                .freeMemory = 0,
                                .pageCacheMemory = 0,
                                .borrowedUsageRate = 0.1,
                                .memoryShortage = 0.1,
                                .pageCacheAppNums = 0,
                                .minFreeKbytes = 0};
    std::pair<std::string, NodeMemoryInfo> nodeA = {"node1", nodeAInfo};

    NodeMemoryInfo nodeBInfo = {.usedMemory = 0,
                                .freeMemory = 0,
                                .pageCacheMemory = 0,
                                .borrowedUsageRate = 0.1,
                                .memoryShortage = 0.1,
                                .pageCacheAppNums = 0,
                                .minFreeKbytes = 0};
    std::pair<std::string, NodeMemoryInfo> nodeB = {"node1", nodeBInfo};

    EXPECT_FALSE(CompareNodes(nodeA, nodeB)); // nodeA 和 nodeB 相同，不应该排在对方前面
}

TEST_F(MigrateStrategyTest, BasicCalculateWatermarksTest)
{
    NodeMemoryInfo borrower = {.totalMemory = 1024000,
                               .usedMemory = 819200,
                               .freeMemory = 204800,
                               .pageCacheMemory = 307200,
                               .borrowedUsageRate = 0.6,
                               .memoryShortage = 60.5,
                               .pageCacheAppNums = 1,
                               .minFreeKbytes = 1000};

    auto [startWatermark, stopWatermark] = CalculateWatermarks(borrower);
    EXPECT_EQ(startWatermark, 164249);
    EXPECT_EQ(stopWatermark, 241049);
}

TEST_F(MigrateStrategyTest, HighWatermarkGreaterThanAvailableMemory)
{
    NodeMemoryInfo borrower = {.totalMemory = 819200,
                               .usedMemory = 819200,
                               .freeMemory = 0,
                               .pageCacheMemory = 0,
                               .borrowedUsageRate = 0.6,
                               .memoryShortage = 60.5,
                               .pageCacheAppNums = 1,
                               .minFreeKbytes = 1000};

    auto [startWatermark, stopWatermark] = CalculateWatermarks(borrower);
    EXPECT_EQ(startWatermark, 409);
    EXPECT_EQ(stopWatermark, 10649);
}

TEST_F(MigrateStrategyTest, MinFreeKbytesZero)
{
    NodeMemoryInfo borrower = {.usedMemory = 819200,
                               .freeMemory = 204800,
                               .pageCacheMemory = 307200,
                               .borrowedUsageRate = 0.6,
                               .memoryShortage = 60.5,
                               .pageCacheAppNums = 1,
                               .minFreeKbytes = 0};

    auto [startWatermark, stopWatermark] = CalculateWatermarks(borrower);
    EXPECT_EQ(startWatermark, 163840); // 预期的 startWatermark 值
    EXPECT_EQ(stopWatermark, 240640);  // 预期的 stopWatermark 值
}

TEST_F(MigrateStrategyTest, KValueZero)
{
    NodeMemoryInfo borrower = {.usedMemory = 819200,
                               .freeMemory = 204800,
                               .pageCacheMemory = 307200,
                               .borrowedUsageRate = 0.6,
                               .memoryShortage = 60.5,
                               .pageCacheAppNums = 0,
                               .minFreeKbytes = 1000};

    auto [startWatermark, stopWatermark] = CalculateWatermarks(borrower);
    EXPECT_EQ(startWatermark, 82329); // 预期的 startWatermark 值
    EXPECT_EQ(stopWatermark, 159129); // 预期的 stopWatermark 值
}

TEST_F(MigrateStrategyTest, LargeKValue)
{
    NodeMemoryInfo borrower = {.usedMemory = 819200,
                               .freeMemory = 204800,
                               .pageCacheMemory = 307200,
                               .borrowedUsageRate = 0.6,
                               .memoryShortage = 60.5,
                               .pageCacheAppNums = 10,
                               .minFreeKbytes = 1000};

    auto [startWatermark, stopWatermark] = CalculateWatermarks(borrower);
    EXPECT_EQ(startWatermark, 901529); // 预期的 startWatermark 值
    EXPECT_EQ(stopWatermark, 978329);  // 预期的 stopWatermark 值
}

TEST_F(MigrateStrategyTest, DeltaMemoryLessThanMinPageSize)
{
    NodeMemoryInfo borrower = {.usedMemory = 819200,
                               .freeMemory = 2514, // 设置为 high + minPageSize - 1
                               .pageCacheMemory = 0,
                               .borrowedUsageRate = 0.6,
                               .memoryShortage = 60.5,
                               .pageCacheAppNums = 1,
                               .minFreeKbytes = 1000};

    // 计算中间变量
    uint32_t wmarkMin = static_cast<uint32_t>(borrower.minFreeKbytes * 7 / 8); // 875
    uint32_t wmarkLow = static_cast<uint32_t>(wmarkMin * 5 / 4);               // 1093
    uint32_t high = static_cast<uint32_t>(wmarkLow * 3 / 2);                   // 1640
    uint32_t availableMemory = borrower.pageCacheMemory + borrower.freeMemory; // 2514

    uint32_t deltaMemory = availableMemory - high;                                                          // 874
    uint32_t startWaterMark = borrower.freeMemory;                                                          // 2514
    uint32_t stopWaterMark = std::max(startWaterMark, high) + static_cast<uint32_t>(availableMemory * 0.6); // 4022

    startWaterMark /= 4; // 628
    stopWaterMark /= 4;  // 1005

    auto [actualStartWatermark, actualStopWatermark] = CalculateWatermarks(borrower);
    EXPECT_EQ(actualStartWatermark, 2969); // 预期的 startWatermark 值
    EXPECT_EQ(actualStopWatermark, 3346);  // 预期的 stopWatermark 值
}

TEST_F(MigrateStrategyTest, GetBestLenderNumaId_Returns_UCACHE_ERR_When_BestLenderId_Not_Found)
{
    std::vector<NodeMemBorrowInfo> lenders = {
        NodeMemBorrowInfo{1024, "node1", "nodeA", {{0, {{"mem1", 512}}}, {1, {{"mem2", 512}}}}, 0},
        NodeMemBorrowInfo{2048, "node2", "nodeB", {{0, {{"mem3", 1024}}}}, 1}};
    std::string bestLenderId = "nodeC";
    int bestLenderNumaId = 0;

    uint32_t result = GetBestLenderNumaId(lenders, bestLenderId, bestLenderNumaId);

    EXPECT_EQ(result, UCACHE_ERR);
}

TEST_F(MigrateStrategyTest, PrintMigrationAction_SingleAction)
{
    MigrationAction action = {.fromNode = "A",
                              .dockerIds = {"docker1"},
                              .toNode = "B",
                              .dstNumaId = 0,
                              .startWatermark = 100,
                              .stopWatermark = 200};
    std::vector<MigrationAction> actions = {action};
    std::string result = PrintMigrationAction(actions);
    EXPECT_EQ(result, "[{fromNode:A,dockerIds:[docker1],toNode:B,dstNumaId:0,startWatermark:100,stopWatermark:200}]");
}

TEST_F(MigrateStrategyTest, PrintMigrationAction_EmptyDockerIds)
{
    MigrationAction action = {
        .fromNode = "A", .dockerIds = {}, .toNode = "B", .dstNumaId = 0, .startWatermark = 100, .stopWatermark = 200};
    std::vector<MigrationAction> actions = {action};
    std::string result = PrintMigrationAction(actions);
    EXPECT_EQ(result, "[{fromNode:A,dockerIds:[],toNode:B,dstNumaId:0,startWatermark:100,stopWatermark:200}]");
}

TEST_F(MigrateStrategyTest, PrintMigrationAction_NegativeDstNumaId)
{
    MigrationAction action = {.fromNode = "A",
                              .dockerIds = {"docker1"},
                              .toNode = "B",
                              .dstNumaId = -1,
                              .startWatermark = 100,
                              .stopWatermark = 200};
    std::vector<MigrationAction> actions = {action};
    std::string result = PrintMigrationAction(actions);
    EXPECT_EQ(result, "[{fromNode:A,dockerIds:[docker1],toNode:B,dstNumaId:-1,startWatermark:100,stopWatermark:200}]");
}

TEST_F(MigrateStrategyTest, PrintMigrationAction_MultipleActions)
{
    MigrationAction action1 = {.fromNode = "A",
                               .dockerIds = {"docker1"},
                               .toNode = "B",
                               .dstNumaId = 0,
                               .startWatermark = 100,
                               .stopWatermark = 200};
    MigrationAction action2 = {.fromNode = "C",
                               .dockerIds = {"docker2", "docker3"},
                               .toNode = "D",
                               .dstNumaId = 1,
                               .startWatermark = 300,
                               .stopWatermark = 400};
    std::vector<MigrationAction> actions = {action1, action2};
    std::string result = PrintMigrationAction(actions);
    EXPECT_EQ(result,
              "[{fromNode:A,dockerIds:[docker1],toNode:B,dstNumaId:0,startWatermark:100,stopWatermark:200},{fromNode:C,"
              "dockerIds:[docker2,docker3],toNode:D,dstNumaId:1,startWatermark:300,stopWatermark:400}]");
}

TEST_F(MigrateStrategyTest, PrintMigrationAction_MultipleDockerIds)
{
    MigrationAction action = {.fromNode = "A",
                              .dockerIds = {"docker1", "docker2", "docker3"},
                              .toNode = "B",
                              .dstNumaId = 0,
                              .startWatermark = 100,
                              .stopWatermark = 200};
    std::vector<MigrationAction> actions = {action};
    std::string result = PrintMigrationAction(actions);
    EXPECT_EQ(
        result,
        "[{fromNode:A,dockerIds:[docker1,docker2,docker3],toNode:B,dstNumaId:0,startWatermark:100,stopWatermark:200}]");
}
