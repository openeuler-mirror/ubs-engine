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

#include <time.h>
#include <ubse_com.h>
#include <atomic>
#include <ctime>
#include <thread>

#include "borrow_strategy.h"
#include "bottleneck_strategy.h"
#include "event_handler.h"
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "ucache_error.h"
#include "ucache_master.h"
#include "ucache_migrate_strategy.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ucache;
using namespace ucache::master;
using namespace ucache::master::migration;
using namespace ucache::data_collect;
using namespace ubse::com;

unsigned long oneGB = 0x40000000UL;

std::atomic<uint32_t> g_collectDataCallCnt{0};

uint32_t MockCollectData()
{
    g_collectDataCallCnt.fetch_add(1);
    return UCACHE_OK;
}

class UcacheMasterTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        MOCKER_CPP(&nanosleep, int (*)(const struct timespec*, struct timespec*)).stubs().will(returnValue(0));
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

namespace ucache::master {
void UcacheMasterMain();
std::vector<ucache::master::migration::MigrationAction> CalMemoryMigrationStrategy();
uint32_t SendMigrateCommands(ucache::master::migration::MigrationAction action);
uint32_t SetPageCacheAppNums();
} // namespace ucache::master

TEST_F(UcacheMasterTest, ExitTest)
{
    Exit();
}

void GetCgroupInfos(std::map<std::string, std::map<std::string, CgroupInfo>>& cgInfos)
{
    std::map<std::string, std::map<std::string, CgroupInfo>> MyCgInfos = {
        {"Node0",
         {
             {"docker0",
              {
                  .pageCacheIn = 0,
                  .ioReadBandwidth = 0,
              }},
             {"docker1",
              {
                  .pageCacheIn = 2000,
                  .ioReadBandwidth = 2000,
              }},
         }},
        {"Node1",
         {
             {" docker0",
              {
                  .pageCacheIn = 0,
                  .ioReadBandwidth = 0,
              }},
             {"docker1",
              {
                  .pageCacheIn = 2000,
                  .ioReadBandwidth = 2000,
              }},
         }},
    };
    cgInfos = MyCgInfos;
}

void GetRawDatas(std::vector<BorrowStrategyRawData>& rawDatas)
{
    std::vector<BorrowStrategyRawData> MyRawDatas = {
        {
            .nodeId = "Node0",
            .pagecacheAppNums = 0,
            .freeMemMin = 4 * oneGB,
            .localMemInfo =
                {
                    .total = 32 * oneGB,
                    .available = 14 * oneGB,
                    .used = 18 * oneGB,
                    .pagecache = 12 * oneGB,
                },
            .remoteNumaMemInfo =
                {
                    {5,
                     {
                         .total = 2 * oneGB,
                         .available = 1 * oneGB,
                         .used = 1 * oneGB,
                         .pagecache = 1 * oneGB,
                     }},
                },
        },
        {
            .nodeId = "Node1",
            .pagecacheAppNums = 0,
            .freeMemMin = 4 * oneGB,
            .localMemInfo =
                {
                    .total = 32 * oneGB,
                    .available = 10 * oneGB,
                    .used = 22 * oneGB,
                    .pagecache = 18 * oneGB,
                },
            .remoteNumaMemInfo = {},
        },
    };
    rawDatas = MyRawDatas;
}

void GetBorrowLendMap(std::map<std::string, std::vector<NodeMemBorrowInfo>>& borrowMap,
                      std::map<std::string, std::vector<NodeMemBorrowInfo>>& lendMap)
{
    std::map<std::string, std::vector<NodeMemBorrowInfo>> MyBorrowMap = {
        {"Node0",
         {
             {
                 .totalSize = oneGB,
                 .srcNodeId = "Node1",
                 .destNodeId = "Node0",
                 .numaNodeBorrowSize =
                     {
                         {0, {{"mem1", oneGB}, {"mem2", oneGB}}},
                         {1, {{"mem3", oneGB}}},
                     },
                 .dstNumaId = 5,
             },
         }},
    };

    std::map<std::string, std::vector<NodeMemBorrowInfo>> MyLendMap = {
        {"Node1",
         {
             {
                 .totalSize = oneGB,
                 .srcNodeId = "Node1",
                 .destNodeId = "Node0",
                 .numaNodeBorrowSize =
                     {
                         {0, {{"mem1", oneGB}, {"mem2", oneGB}}},
                         {1, {{"mem3", oneGB}}},
                     },
                 .dstNumaId = 5,
             },
         }},
    };
    borrowMap = MyBorrowMap;
    lendMap = MyLendMap;
}

TEST_F(UcacheMasterTest, UcacheMasterMainSuccessTest)
{
    std::map<std::string, std::vector<std::string>> physicalTopo = {
        {"Node0", {"Node1"}},
        {"Node1", {"Node0"}},
    };
    DataCollect::SetPhysicalTopo(physicalTopo);

    std::map<std::string, std::map<int, uint64_t>> loanableMemRawData = {
        {"Node0", {{0, 2 * oneGB}, {1, 2 * oneGB}}},
        {"Node1", {{0, 2 * oneGB}, {1, 2 * oneGB}}},
    };
    DataCollect::SetLoanableTotalBorrowMemMap(loanableMemRawData);

    std::map<std::string, std::map<std::string, CgroupInfo>> cgInfos{};
    GetCgroupInfos(cgInfos);
    DataCollect::SetCgroupInfo(cgInfos);

    std::map<std::string, std::vector<NodeMemBorrowInfo>> borrowMap{};
    std::map<std::string, std::vector<NodeMemBorrowInfo>> lendMap{};
    GetBorrowLendMap(borrowMap, lendMap);
    DataCollect::SetborrowMemMap(borrowMap);
    DataCollect::SetlendMemMap(lendMap);

    std::vector<BorrowStrategyRawData> rawDatas{};
    GetRawDatas(rawDatas);
    DataCollect::SetBorrowStrategyRawData(rawDatas);

    g_collectDataCallCnt.store(0);
    MOCKER(DataCollect::CollectData).stubs().will(invoke(MockCollectData));

    MOCKER(ucache::borrow_action::ExecuteBorrowActions).stubs().will(returnValue(UCACHE_OK));
    ucache::fault_handler::EventHandler::gNodeFaultFlag.store(false);
    EXPECT_EQ(Init(), UCACHE_OK);
    const auto waitUntil = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (g_collectDataCallCnt.load() == 0 && std::chrono::steady_clock::now() < waitUntil) {
        std::this_thread::yield();
    }
    EXPECT_GT(g_collectDataCallCnt.load(), 0U);
    Exit();
}

TEST_F(UcacheMasterTest, SendMigrateCommandsTest)
{
    std::map<std::string, std::vector<std::string>> physicalTopo = {
        {"Node0", {"Node1"}},
        {"Node1", {"Node0"}},
    };
    DataCollect::SetPhysicalTopo(physicalTopo);

    std::map<std::string, std::map<int, uint64_t>> loanableMemRawData = {
        {"Node0", {{0, 2 * oneGB}, {1, 2 * oneGB}}},
        {"Node1", {{0, 2 * oneGB}, {1, 2 * oneGB}}},
    };
    DataCollect::SetLoanableTotalBorrowMemMap(loanableMemRawData);

    MemBorrowTopo::InitGlobalMemBorrowTopo();

    MigrationAction action = {.fromNode = "Node0",
                              .dockerIds = {"docker1"},
                              .toNode = "Node1",
                              .dstNumaId = 0,
                              .startWatermark = 100,
                              .stopWatermark = 200};
    uint32_t ret = SendMigrateCommands(action);
    EXPECT_EQ(ret, UCACHE_OK);

    MigrationAction action1 = {.fromNode = "Node2",
                               .dockerIds = {"docker1"},
                               .toNode = "Node1",
                               .dstNumaId = 0,
                               .startWatermark = 100,
                               .stopWatermark = 200};
    ret = SendMigrateCommands(action1);
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);

    uint32_t result = UCACHE_ERR;
    MOCKER(UbseRpcSend)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBoundP((void*)&result, sizeof(result)))
        .will(returnValue(UCACHE_ERR));
    ret = SendMigrateCommands(action);
    EXPECT_EQ(ret, UCACHE_ERR);
}