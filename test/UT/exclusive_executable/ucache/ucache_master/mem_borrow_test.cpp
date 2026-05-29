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
#define private public
#include "data_collect.h"
#include "mem_borrow.h"
#include "ucache_config.h"
#include "ucache_error.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
using namespace std;
using namespace ucache;
using namespace ucache::data_collect;
using namespace ucache::mem_borrow;
uint32_t DeleteFromMap(const std::string& from, const std::string& to, const std::string& memName, const int fromNumaId,
                       const uint32_t size, std::map<std::string, std::vector<NodeMemBorrowInfo>>::iterator& mapIter,
                       std::vector<NodeMemBorrowInfo>::iterator& mapInfoIter);
// 以下rawData、physicalTopo、loanableMemRawData组成默认topo
// 按稀缺度排序：Node1 < Node 2 < Node3
static std::vector<BorrowStrategyRawData> rawData = {{
                                                         .nodeId = "Node1",
                                                         .pagecacheAppNums = 0,
                                                         .freeMemMin = 100,
                                                         .localMemInfo =
                                                             {
                                                                 .total = 1000,
                                                                 .available = 500,
                                                                 .used = 500,
                                                                 .pagecache = 400,
                                                             },
                                                         .remoteNumaMemInfo =
                                                             {
                                                                 {5,
                                                                  {
                                                                      .total = 1000,
                                                                      .available = 500,
                                                                      .used = 500,
                                                                      .pagecache = 400,
                                                                  }},
                                                             },
                                                     },
                                                     {
                                                         .nodeId = "Node2",
                                                         .pagecacheAppNums = 6,
                                                         .freeMemMin = 100,
                                                         .localMemInfo =
                                                             {
                                                                 .total = 1000,
                                                                 .available = 500,
                                                                 .used = 500,
                                                                 .pagecache = 300,
                                                             },
                                                         .remoteNumaMemInfo =
                                                             {
                                                                 {5,
                                                                  {
                                                                      .total = 1000,
                                                                      .available = 500,
                                                                      .used = 500,
                                                                      .pagecache = 300,
                                                                  }},
                                                             },
                                                     },
                                                     {
                                                         .nodeId = "Node3",
                                                         .pagecacheAppNums = 15,
                                                         .freeMemMin = 100,
                                                         .localMemInfo =
                                                             {
                                                                 .total = 1000,
                                                                 .available = 500,
                                                                 .used = 500,
                                                                 .pagecache = 200,
                                                             },
                                                         .remoteNumaMemInfo =
                                                             {
                                                                 {5,
                                                                  {
                                                                      .total = 1000,
                                                                      .available = 500,
                                                                      .used = 500,
                                                                      .pagecache = 200,
                                                                  }},
                                                             },
                                                     }};

static std::map<std::string, std::vector<std::string>> physicalTopo = {
    {"Node1", {"Node2"}},
    {"Node2", {"Node1", "Node3"}},
    {"Node3", {"Node2"}},
};

std::map<std::string, std::map<int, uint64_t>> loanableMemRawData = {
    {"Node1", {{0, 0x40000000UL}, {1, 800}, {2, 800}, {3, 800}}},
    {"Node2", {{0, 800}, {1, 800}, {2, 800}, {3, 800}}},
    {"Node3", {{0, 800}, {1, 800}, {2, 800}, {3, 800}}},
};

class BorrowNodeStatTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        DataCollect::SetBorrowStrategyRawData(rawData);
        cout << "[Phase SetUp End]" << endl;
    }

    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        std::vector<BorrowStrategyRawData> emptyData;
        DataCollect::SetBorrowStrategyRawData(emptyData);
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(BorrowNodeStatTest, SortNodeByScarcityTest)
{
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    vector<BorrowNodeStat*> nodeStats;
    for (auto it = BorrowNodeStat::nodeIdToNodeStatMap.begin(); it != BorrowNodeStat::nodeIdToNodeStatMap.end(); it++) {
        nodeStats.emplace_back(&(it->second));
    }
    BorrowNodeStat::SortNodeByScarcity(nodeStats);

    EXPECT_EQ(nodeStats[0]->GetNodeId(), rawData[2].nodeId);
    EXPECT_EQ(nodeStats[1]->GetNodeId(), rawData[1].nodeId);
    EXPECT_EQ(nodeStats[2]->GetNodeId(), rawData[0].nodeId);
    uint64_t mem = nodeStats[0]->GetUsedMem();
    mem = nodeStats[0]->GetPageCacheMem();
    mem = nodeStats[0]->GetFreeMemMin();
    double memUse = nodeStats[0]->GetBorrowedMemUsage();
    int pageCache = nodeStats[0]->GetPagecacheAppNums();
}

void GetData4InsertBorrowNodeStatTest(BorrowStrategyRawData& data)
{
    BorrowStrategyRawData MyData = {
        .nodeId = "Node4",
        .pagecacheAppNums = 0,
        .freeMemMin = 100,
        .localMemInfo =
            {
                .total = 1000,
                .available = 500,
                .used = 500,
                .pagecache = 300,
            },
        .remoteNumaMemInfo =
            {
                {5,
                 {
                     .total = 1000,
                     .available = 500,
                     .used = 500,
                     .pagecache = 300,
                 }},
            },
    };
    data = MyData;
}

TEST_F(BorrowNodeStatTest, InsertBorrowNodeStatTest)
{
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    vector<BorrowNodeStat*> nodeStats;
    for (auto it = BorrowNodeStat::nodeIdToNodeStatMap.begin(); it != BorrowNodeStat::nodeIdToNodeStatMap.end(); it++) {
        nodeStats.emplace_back(&(it->second));
    }
    BorrowNodeStat::SortNodeByScarcity(nodeStats);

    // 按稀缺度排序：Node4 < Node1 < Node 2 < Node3
    BorrowStrategyRawData data4{};
    GetData4InsertBorrowNodeStatTest(data4);
    BorrowNodeStat nodeStat4(data4);
    BorrowNodeStat::InsertBorrowNodeStat(&nodeStat4, nodeStats);
    EXPECT_EQ(nodeStats[nodeStats.size() - 1]->GetNodeId(), data4.nodeId);

    // 按稀缺度排序：Node4 < Node5 < Node1 < Node 2 < Node3
    BorrowStrategyRawData data5 = {
        .nodeId = "Node5",
        .pagecacheAppNums = 0,
        .freeMemMin = 100,
        .localMemInfo =
            {
                .total = 1000,
                .available = 500,
                .used = 500,
                .pagecache = 350,
            },
        .remoteNumaMemInfo =
            {
                {5,
                 {
                     .total = 1000,
                     .available = 500,
                     .used = 500,
                     .pagecache = 350,
                 }},
            },
    };
    BorrowNodeStat nodeStat5(data5);
    BorrowNodeStat::InsertBorrowNodeStat(&nodeStat5, nodeStats);
    EXPECT_EQ(nodeStats[nodeStats.size() - 2]->GetNodeId(), data5.nodeId);

    BorrowNodeStat::ClearBorrowNodeStat();
}

TEST_F(BorrowNodeStatTest, EraseBorrowNodeStatTest)
{
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    vector<BorrowNodeStat*> nodeStats;
    for (auto it = BorrowNodeStat::nodeIdToNodeStatMap.begin(); it != BorrowNodeStat::nodeIdToNodeStatMap.end(); it++) {
        nodeStats.emplace_back(&(it->second));
    }
    BorrowNodeStat::SortNodeByScarcity(nodeStats);

    BorrowNodeStat::EraseBorrowNodeStat(nodeStats[nodeStats.size() - 2], nodeStats);
    EXPECT_EQ(nodeStats.size(), 2);

    BorrowNodeStat::ClearBorrowNodeStat();
}

TEST_F(BorrowNodeStatTest, ReturnMemTest)
{
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    double scarcity1 = BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].GetScarcity();
    BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].ReturnMem(100);
    double scarcity2 = BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].GetScarcity();
    EXPECT_EQ(scarcity1 > scarcity2, true);
}

TEST_F(BorrowNodeStatTest, LendMemTest)
{
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    double scarcity1 = BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].GetScarcity();
    BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].LendMem(100);
    double scarcity2 = BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].GetScarcity();
    EXPECT_EQ(scarcity1 < scarcity2, true);
}

TEST_F(BorrowNodeStatTest, BorrowMemTest)
{
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    double scarcity1 = BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].GetScarcity();
    BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].BorrowMem(100);
    double scarcity2 = BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].GetScarcity();
    EXPECT_EQ(scarcity1 > scarcity2, true);
}

TEST_F(BorrowNodeStatTest, CalKTest)
{
    BorrowStrategyRawData data = {
        .nodeId = "Node",
        .pagecacheAppNums = 0,
        .freeMemMin = 100,
        .localMemInfo =
            {
                .total = 1000,
                .available = 500,
                .used = 500,
                .pagecache = 30,
            },
        .remoteNumaMemInfo =
            {
                {5,
                 {
                     .total = 1000,
                     .available = 900,
                     .used = 100,
                     .pagecache = 50,
                 }},
            },
    };
    BorrowNodeStat nodeStat1(data);
    BorrowStrategyRawData data2 = {
        .nodeId = "Node",
        .pagecacheAppNums = 0,
        .freeMemMin = 100,
        .localMemInfo =
            {
                .total = 1000,
                .available = 50,
                .used = 950,
                .pagecache = 900,
            },
        .remoteNumaMemInfo =
            {
                {5,
                 {
                     .total = 1000,
                     .available = 50,
                     .used = 950,
                     .pagecache = 900,
                 }},
            },
    };
    BorrowNodeStat nodeStat2(data2);
}

TEST_F(BorrowNodeStatTest, GetRemoteNumaMemInfoTest)
{
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    std::map<int, MemInfo> retMap = BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].GetRemoteNumaMemInfo();
    EXPECT_EQ(retMap.size(), rawData[0].remoteNumaMemInfo.size());
}

TEST_F(BorrowNodeStatTest, InitBorrowNodeStatTest)
{
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    auto it = BorrowNodeStat::nodeIdToNodeStatMap.find(rawData[0].nodeId);
    EXPECT_NE(it, BorrowNodeStat::nodeIdToNodeStatMap.end());

    std::vector<BorrowStrategyRawData> emptyData;
    DataCollect::SetBorrowStrategyRawData(emptyData);
    BorrowNodeStat::ClearBorrowNodeStat();
    ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, BORROW_DATA_ERROR);
}

TEST_F(BorrowNodeStatTest, GetNodeIdTest)
{
    uint32_t ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    string retStr = BorrowNodeStat::nodeIdToNodeStatMap[rawData[0].nodeId].GetNodeId();
    EXPECT_EQ(retStr, rawData[0].nodeId);
}

class MemBorrowTopoTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        DataCollect::SetBorrowStrategyRawData(rawData);
        DataCollect::SetLoanableTotalBorrowMemMap(loanableMemRawData);
        DataCollect::SetPhysicalTopo(physicalTopo);
        BorrowNodeStat::InitBorrowNodeStat();
        cout << "[Phase SetUp End]" << endl;
    }

    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        std::vector<BorrowStrategyRawData> emptyData;
        DataCollect::SetBorrowStrategyRawData(emptyData);
        std::map<std::string, std::map<int, uint64_t>> emptyLoanableMemRawData;
        DataCollect::SetLoanableTotalBorrowMemMap(emptyLoanableMemRawData);
        BorrowNodeStat::ClearBorrowNodeStat();
        ClearGlobalMemBorrowTopo();
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }

    void ClearGlobalMemBorrowTopo()
    {
        MemBorrowTopo::globalMemBorrowTopo.lendMap.clear();
        MemBorrowTopo::globalMemBorrowTopo.borrowMap.clear();
        MemBorrowTopo::globalMemBorrowTopo.borrowMemInfoPerNodeMap.clear();
    }
};

TEST_F(MemBorrowTopoTest, HasRemoteMemTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    bool retBool = MemBorrowTopo::globalMemBorrowTopo.HasRemoteMem("Node1");
    EXPECT_EQ(retBool, true);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasRemoteMem("Node0");
    EXPECT_EQ(retBool, false);
}

TEST_F(MemBorrowTopoTest, HasLentMemTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    bool retBool = MemBorrowTopo::globalMemBorrowTopo.HasLentMem("Node0");
    EXPECT_EQ(retBool, false);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasLentMem("Node1");
    EXPECT_EQ(retBool, false);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasLentMem("Node1");
    EXPECT_EQ(retBool, true);
}

TEST_F(MemBorrowTopoTest, HasBorrowedMemTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    bool retBool = MemBorrowTopo::globalMemBorrowTopo.HasBorrowedMem("Node0");
    EXPECT_EQ(retBool, false);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasBorrowedMem("Node1");
    EXPECT_EQ(retBool, false);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasBorrowedMem("Node2");
    EXPECT_EQ(retBool, true);
}

TEST_F(MemBorrowTopoTest, HasLendMemExceptDstTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    bool retBool = MemBorrowTopo::globalMemBorrowTopo.HasLendMemExceptDst("Node0", "Node1");
    EXPECT_EQ(retBool, false);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasLendMemExceptDst("Node2", "Node1");
    EXPECT_EQ(retBool, false);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node1");
    EXPECT_EQ(ret, UCACHE_OK);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasLendMemExceptDst("Node2", "Node1");
    EXPECT_EQ(retBool, false);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node2", "Node3");
    EXPECT_EQ(ret, UCACHE_OK);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasLendMemExceptDst("Node2", "Node1");
    EXPECT_EQ(retBool, true);
}

TEST_F(MemBorrowTopoTest, HasAvailableMemToBorrowTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    bool retBool = MemBorrowTopo::globalMemBorrowTopo.HasAvailableMemToBorrow("Node0");
    EXPECT_EQ(retBool, false);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasAvailableMemToBorrow("Node1");
    EXPECT_EQ(retBool, false);

    ClearGlobalMemBorrowTopo();
    uint64_t tmp = loanableMemRawData["Node1"][0];
    loanableMemRawData["Node1"][0] = 2 * UcacheConfig::GetInstance().GetBorrowSize();
    DataCollect::SetLoanableTotalBorrowMemMap(loanableMemRawData);
    ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasAvailableMemToBorrow("Node1");
    EXPECT_EQ(retBool, false);

    std::string tmpName = rawData[0].nodeId;
    rawData[0].nodeId = "Node0";
    DataCollect::SetBorrowStrategyRawData(rawData);
    ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasAvailableMemToBorrow("Node1");
    EXPECT_EQ(retBool, false);

    rawData[0].nodeId = tmpName;
    DataCollect::SetBorrowStrategyRawData(rawData);
    ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    uint64_t tmpFree = rawData[0].localMemInfo.available;
    rawData[0].localMemInfo.available = 2 * UcacheConfig::GetInstance().GetBorrowSize();
    DataCollect::SetBorrowStrategyRawData(rawData);
    ret = BorrowNodeStat::InitBorrowNodeStat();
    EXPECT_EQ(ret, UCACHE_OK);

    retBool = MemBorrowTopo::globalMemBorrowTopo.HasAvailableMemToBorrow("Node1");
    EXPECT_EQ(retBool, true);

    loanableMemRawData["Node1"][0] = tmp;
}

TEST_F(MemBorrowTopoTest, DeleteNodeMemBorrowInfoTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    ret = MemBorrowTopo::globalMemBorrowTopo.DeleteNodeMemBorrowInfo("Node0", "Node2");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);

    ret = MemBorrowTopo::globalMemBorrowTopo.DeleteNodeMemBorrowInfo("Node1", "Node3");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);

    ret = MemBorrowTopo::globalMemBorrowTopo.DeleteNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);

    ret = MemBorrowTopo::globalMemBorrowTopo.DeleteNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);

    ret = MemBorrowTopo::globalMemBorrowTopo.DeleteNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(MemBorrowTopoTest, AddNodeMemBorrowInfoTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node0", "Node2");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node3");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(MemBorrowTopoTest, InitGlobalMemBorrowTopoTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    ClearGlobalMemBorrowTopo();
    std::map<std::string, std::vector<std::string>> emptyPhysicalTopo;
    DataCollect::SetPhysicalTopo(emptyPhysicalTopo);

    ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);
}

TEST_F(MemBorrowTopoTest, GetBorrowableNumaInfoTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    int socketId = -1;
    int numaId = -1;

    ret = MemBorrowTopo::globalMemBorrowTopo.GetBorrowableNumaInfo("Node0", socketId, numaId);
    EXPECT_EQ(ret, EXEC_MEM_BORROW_ERROR);
    ret = MemBorrowTopo::globalMemBorrowTopo.GetBorrowableNumaInfo("Node1", socketId, numaId);
    EXPECT_EQ(ret, UCACHE_OK);
    EXPECT_NE(socketId, -1);
    EXPECT_NE(numaId, -1);
}

TEST_F(MemBorrowTopoTest, SetNumaNodeBorrowSizeTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    ret = MemBorrowTopo::globalMemBorrowTopo.SetNumaNodeBorrowSize("test_mem", "Node0", "Node2", 0, 5);
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);

    ret = MemBorrowTopo::globalMemBorrowTopo.SetNumaNodeBorrowSize("test_mem", "Node1", "Node2", 0, 5);
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(MemBorrowTopoTest, AddNumaSizeTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    MemBorrowTopo::globalMemBorrowTopo.AddNumaSize("Node1", 0, 1024);

    auto& nodeInfo = MemBorrowTopo::globalMemBorrowTopo.borrowMemInfoPerNodeMap["Node1"];
    EXPECT_NE(nodeInfo.numaNodeSize.find(0), nodeInfo.numaNodeSize.end());
}

TEST_F(MemBorrowTopoTest, SubNumaSizeTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    MemBorrowTopo::globalMemBorrowTopo.SubNumaSize("Node1", 0, 512);

    auto& nodeInfo = MemBorrowTopo::globalMemBorrowTopo.borrowMemInfoPerNodeMap["Node1"];
    EXPECT_NE(nodeInfo.numaNodeSize.find(0), nodeInfo.numaNodeSize.end());
}

TEST_F(MemBorrowTopoTest, BoundaryConditionsTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("", "Node2");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);
    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(MemBorrowTopoTest, ErrorHandlingTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node3");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR); // Node3不在物理拓扑中

    ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node3", "Node1");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR); // Node3不在物理拓扑中

    ret = MemBorrowTopo::globalMemBorrowTopo.DeleteNodeMemBorrowInfo("Node1", "Node3");
    EXPECT_EQ(ret, BORROW_TOPO_ERROR);
}

TEST_F(MemBorrowTopoTest, PerformanceTest)
{
    uint32_t ret = MemBorrowTopo::InitGlobalMemBorrowTopo();
    EXPECT_EQ(ret, UCACHE_OK);

    for (int i = 0; i < 100; i++) {
        ret = MemBorrowTopo::globalMemBorrowTopo.AddNodeMemBorrowInfo("Node1", "Node2");
        EXPECT_EQ(ret, UCACHE_OK);

        ret = MemBorrowTopo::globalMemBorrowTopo.DeleteNodeMemBorrowInfo("Node1", "Node2");
        EXPECT_EQ(ret, UCACHE_OK);
    }
}