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
#include "ubse_node.h"
#include "data_collect.h"
#include "deserialize.h"
#include "ucache_config.h"
#include "ucache_error.h"

using namespace std;
using namespace ucache;
using namespace turbo::ucache;
using namespace ubse::log;
using namespace ucache::deserialize;
using namespace ucache::data_collect;

constexpr double GB_TO_BYTE = 1024 * 1024 * 1024;

namespace ucache::data_collect {
uint32_t ParseRackNodeId(const std::string& originNodeId, std::string& nodeId);
void SetCgroupInfoDefault(CgroupInfo& cgInfo);
void PrintPhysicalTopo(const std::map<std::string, std::vector<std::string>>& physicalTopo);
uint32_t GeneratePerNodeNumaSocketMap(const std::vector<ubse::nodeController::MemNodeData>& memNodeDataVec,
                                      std::map<std::string, std::map<int, int>>& tmpMap);
void PrintNumaSocketMap(const std::map<std::string, std::map<int, int>>& numaSocketMap);
} // namespace ucache::data_collect

class DataCollectTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        BorrowStrategyRawData data1 = {
            .nodeId = "Node1",
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
        std::vector<BorrowStrategyRawData> rawData;
        rawData.emplace_back(data1);
        DataCollect::SetBorrowStrategyRawData(rawData);
        DataCollect::nodeInfos.clear();
        DataCollect::memWaterMarkInfos.clear();
        DataCollect::nodeMemInfos.clear();
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        std::vector<BorrowStrategyRawData> emptyData;
        DataCollect::SetBorrowStrategyRawData(emptyData);
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;

        // 清空全局数据
        DataCollect::borrowStrategyRawData.clear();
        DataCollect::nodeInfos.clear();
        DataCollect::memWaterMarkInfos.clear();
        DataCollect::nodeMemInfos.clear();
    }

    void AddMockCgroupInfoData()
    {
        std::map<std::string, std::map<std::string, CgroupInfos>> mockData;

        // Node 1 data
        std::map<std::string, CgroupInfos> node1Data;
        CgroupInfos cgInfo1;
        cgInfo1.pageCacheInfo.pageCacheIn = 1000;
        cgInfo1.ioInfo.ioReadBytes = 2000;
        node1Data["docker1"] = cgInfo1;
        mockData["node1"] = node1Data;

        // Node 2 data
        std::map<std::string, CgroupInfos> node2Data;
        CgroupInfos cgInfo2;
        cgInfo2.pageCacheInfo.pageCacheIn = 1500;
        cgInfo2.ioInfo.ioReadBytes = 2500;
        node2Data["docker2"] = cgInfo2;
        mockData["node2"] = node2Data;

        DataCollect::cgroupInfosQueue.push(mockData);
    }
};

TEST_F(DataCollectTest, GetBorrowStrategyRawDataTest)
{
    std::vector<BorrowStrategyRawData> ret;
    DataCollect::GetBorrowStrategyRawData(ret);
    EXPECT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0].nodeId, "Node1");
}

TEST_F(DataCollectTest, SetBorrowStrategyRawDataTest)
{
    std::vector<BorrowStrategyRawData> ret;
    DataCollect::GetBorrowStrategyRawData(ret);
    BorrowStrategyRawData data2 = {
        .nodeId = "Node2",
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
    ret.emplace_back(data2);
    DataCollect::SetBorrowStrategyRawData(ret);
    DataCollect::GetBorrowStrategyRawData(ret);
    EXPECT_EQ(ret.size(), 2);
    EXPECT_EQ(ret[1].nodeId, "Node2");
}

TEST_F(DataCollectTest, GenerateBorrowStrategyRawData_Success)
{
    std::string testNodeId = "node1";

    // 模拟 nodeInfos
    NodeInfo localNodeInfo = {.memTotalBytes = 1024,
                              .memFreeBytes = 512,
                              .memUsedBytes = 512,
                              .activeFileBytes = 100,
                              .inactiveFileBytes = 50,
                              .isRemote = false};
    NodeInfo remoteNodeInfo = {.memTotalBytes = 2048,
                               .memFreeBytes = 1024,
                               .memUsedBytes = 1024,
                               .activeFileBytes = 200,
                               .inactiveFileBytes = 100,
                               .isRemote = true};

    DataCollect::nodeInfos[testNodeId] = {{"numa0", localNodeInfo}, {"numa1", remoteNodeInfo}};

    // 模拟 memWaterMarkInfos
    DataCollect::memWaterMarkInfos[testNodeId] = 128;

    uint32_t ret = DataCollect::GenerateBorrowStrategyRawData();

    EXPECT_EQ(ret, UCACHE_OK);
    EXPECT_EQ(DataCollect::borrowStrategyRawData.size(), 1);

    BorrowStrategyRawData& rawData = DataCollect::borrowStrategyRawData[0];

    EXPECT_EQ(rawData.nodeId, testNodeId);
    EXPECT_EQ(rawData.freeMemMin, 128);
    EXPECT_EQ(rawData.localMemInfo.total, 1024);
    EXPECT_EQ(rawData.localMemInfo.available, 512);
    EXPECT_EQ(rawData.localMemInfo.used, 512);
    EXPECT_EQ(rawData.localMemInfo.pagecache, 150);

    ASSERT_EQ(rawData.remoteNumaMemInfo.size(), 1);
    EXPECT_EQ(rawData.remoteNumaMemInfo[1].total, 2048);
    EXPECT_EQ(rawData.remoteNumaMemInfo[1].available, 1024);
    EXPECT_EQ(rawData.remoteNumaMemInfo[1].used, 1024);
    EXPECT_EQ(rawData.remoteNumaMemInfo[1].pagecache, 300);
}

TEST_F(DataCollectTest, GenerateBorrowStrategyRawData_MissingMemWaterMarkInfo)
{
    std::string testNodeId = "node1";

    // 仅设置 nodeInfos，没有 memWaterMarkInfos
    NodeInfo localNodeInfo = {.memTotalBytes = 1024,
                              .memFreeBytes = 512,
                              .memUsedBytes = 512,
                              .activeFileBytes = 100,
                              .inactiveFileBytes = 50,
                              .isRemote = false};
    DataCollect::nodeInfos[testNodeId] = {{"numa0", localNodeInfo}};

    uint32_t ret = DataCollect::GenerateBorrowStrategyRawData();

    EXPECT_EQ(ret, UCACHE_ERR);
    EXPECT_TRUE(DataCollect::borrowStrategyRawData.empty());
}

TEST_F(DataCollectTest, GenerateBorrowStrategyRawData_MultipleNumaNodes)
{
    std::string testNodeId = "node1";

    // 设置多个 NUMA 节点
    NodeInfo localNodeInfo = {.memTotalBytes = 1024,
                              .memFreeBytes = 512,
                              .memUsedBytes = 512,
                              .activeFileBytes = 100,
                              .inactiveFileBytes = 50,
                              .isRemote = false};
    NodeInfo remoteNodeInfo1 = {.memTotalBytes = 2048,
                                .memFreeBytes = 1024,
                                .memUsedBytes = 1024,
                                .activeFileBytes = 200,
                                .inactiveFileBytes = 100,
                                .isRemote = true};
    NodeInfo remoteNodeInfo2 = {.memTotalBytes = 4096,
                                .memFreeBytes = 2048,
                                .memUsedBytes = 2048,
                                .activeFileBytes = 400,
                                .inactiveFileBytes = 200,
                                .isRemote = true};

    DataCollect::nodeInfos[testNodeId] = {
        {"numa0", localNodeInfo}, {"numa1", remoteNodeInfo1}, {"numa2", remoteNodeInfo2}};

    DataCollect::memWaterMarkInfos[testNodeId] = 128;

    uint32_t ret = DataCollect::GenerateBorrowStrategyRawData();

    EXPECT_EQ(ret, UCACHE_OK);
    EXPECT_EQ(DataCollect::borrowStrategyRawData.size(), 1);

    BorrowStrategyRawData& rawData = DataCollect::borrowStrategyRawData[0];

    ASSERT_EQ(rawData.remoteNumaMemInfo.size(), 2);
    EXPECT_EQ(rawData.remoteNumaMemInfo[1].total, 2048);
    EXPECT_EQ(rawData.remoteNumaMemInfo[2].total, 4096);
}

TEST_F(DataCollectTest, GenerateNodeMemoryInfoTest)
{
    uint32_t ret = DataCollect::GenerateNodeMemoryInfo();
    EXPECT_EQ(ret, UCACHE_OK);
    EXPECT_TRUE(DataCollect::nodeMemInfos.empty());

    // memWaterMarkInfos 缺少对应的 nodeId
    DataCollect::nodeInfos["node1"] = {}; // 添加一个 nodeId，但 memWaterMarkInfos 为空
    ret = DataCollect::GenerateNodeMemoryInfo();
    EXPECT_EQ(ret, UCACHE_ERR);

    // 只有远程节点
    DataCollect::memWaterMarkInfos["node1"] = 1024; // 设置水线

    NodeInfo remoteNode;
    remoteNode.isRemote = true;
    remoteNode.memTotalBytes = 8192;
    remoteNode.memFreeBytes = 4096;
    remoteNode.memUsedBytes = 4096;
    remoteNode.activeFileBytes = 1024;
    remoteNode.inactiveFileBytes = 512;

    DataCollect::nodeInfos["node1"]["numa0"] = remoteNode;

    ret = DataCollect::GenerateNodeMemoryInfo();
    EXPECT_EQ(ret, UCACHE_OK);
    EXPECT_EQ(DataCollect::nodeMemInfos["node1"].totalMemory, 0);

    // 包含本地节点
    DataCollect::memWaterMarkInfos["node1"] = 1024;

    NodeInfo localNode;
    localNode.isRemote = false;
    localNode.memTotalBytes = 8192;
    localNode.memFreeBytes = 4096;
    localNode.memUsedBytes = 4096;
    localNode.activeFileBytes = 1024;
    localNode.inactiveFileBytes = 512;

    DataCollect::nodeInfos["node1"]["numa0"] = localNode;

    ret = DataCollect::GenerateNodeMemoryInfo();
    EXPECT_EQ(ret, UCACHE_OK);

    // 确保统计正确
    auto& nodeMemInfo = DataCollect::nodeMemInfos["node1"];
    EXPECT_EQ(nodeMemInfo.minFreeKbytes, 1024);
    EXPECT_EQ(nodeMemInfo.totalMemory, 8192);
    EXPECT_EQ(nodeMemInfo.freeMemory, 4096);
    EXPECT_EQ(nodeMemInfo.usedMemory, 4096);
    EXPECT_EQ(nodeMemInfo.pageCacheMemory, 1024 + 512);
}

TEST_F(DataCollectTest, CollectDataTest)
{
    MOCKER(DataCollect::GetResourceExportData).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateBorrow).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateNodeMemoryInfo).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateCgroupInfo).stubs().will(returnValue(UCACHE_OK));

    uint32_t ret = DataCollect::CollectData();
    EXPECT_EQ(ret, UCACHE_OK);

    GlobalMockObject::verify();

    MOCKER(DataCollect::GetResourceExportData).stubs().will(returnValue(UCACHE_ERR));
    ret = DataCollect::CollectData();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();
    MOCKER(DataCollect::GetResourceExportData).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateBorrow).stubs().will(returnValue(UCACHE_ERR));
    ret = DataCollect::CollectData();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();
    MOCKER(DataCollect::GetResourceExportData).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateBorrow).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateNodeMemoryInfo).stubs().will(returnValue(UCACHE_ERR));
    ret = DataCollect::CollectData();
    EXPECT_EQ(ret, UCACHE_ERR);

    GlobalMockObject::verify();
    MOCKER(DataCollect::GetResourceExportData).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateBorrow).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateNodeMemoryInfo).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateCgroupInfo).stubs().will(returnValue(UCACHE_ERR));
    ret = DataCollect::CollectData();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(DataCollectTest, TestGenerateLendMap_InsertData)
{
    std::map<std::string, std::vector<NodeMemBorrowInfo>> memMap;
    BorrowMemInfo borrowMemInfo;
    borrowMemInfo.name = "mem1";
    borrowMemInfo.borrowNodeId = "node1";
    borrowMemInfo.lentNodeId = "node2";
    borrowMemInfo.size = 1024;
    borrowMemInfo.lentNumaInfos = {{0, 512}};
    borrowMemInfo.borrowRemoteNuma = -1;
    borrowMemInfo.borrowLocalNuma = 0;

    std::string keyId = "key1";

    // 调用 GenerateLendMap 插入数据
    DataCollect::GenerateLendMap(borrowMemInfo, keyId, memMap);

    // 验证 MemMap 是否正确插入数据
    EXPECT_EQ(memMap.size(), 1);
    EXPECT_TRUE(memMap.find(keyId) != memMap.end());

    auto& nodeMemBorrowInfos = memMap[keyId];
    EXPECT_EQ(nodeMemBorrowInfos.size(), 1);
    EXPECT_EQ(nodeMemBorrowInfos[0].srcNodeId, "node2");
    EXPECT_EQ(nodeMemBorrowInfos[0].destNodeId, "node1");
    EXPECT_EQ(nodeMemBorrowInfos[0].totalSize, 1024);
    EXPECT_EQ(nodeMemBorrowInfos[0].numaNodeBorrowSize[0]["mem1"], 512);
}

TEST_F(DataCollectTest, TestGenerateBorrowMap_InsertData)
{
    std::map<std::string, std::vector<NodeMemBorrowInfo>> memMap;
    BorrowMemInfo borrowMemInfo;
    borrowMemInfo.name = "mem1";
    borrowMemInfo.borrowNodeId = "node1";
    borrowMemInfo.lentNodeId = "node2";
    borrowMemInfo.size = 1024;
    borrowMemInfo.lentNumaInfos = {{0, 512}};
    borrowMemInfo.borrowRemoteNuma = -1;
    borrowMemInfo.borrowLocalNuma = 0;

    std::string keyId = "key1";

    // 调用 GenerateBorrowMap 插入数据
    DataCollect::GenerateBorrowMap(borrowMemInfo, keyId, memMap);

    // 验证 MemMap 是否正确插入数据
    EXPECT_EQ(memMap.size(), 1);
    EXPECT_TRUE(memMap.find(keyId) != memMap.end());

    auto& nodeMemBorrowInfos = memMap[keyId];
    EXPECT_EQ(nodeMemBorrowInfos.size(), 1);
    EXPECT_EQ(nodeMemBorrowInfos[0].srcNodeId, "node2");
    EXPECT_EQ(nodeMemBorrowInfos[0].destNodeId, "node1");
    EXPECT_EQ(nodeMemBorrowInfos[0].totalSize, 1024);
    EXPECT_EQ(nodeMemBorrowInfos[0].numaNodeBorrowSize[0]["mem1"], 512);
}

TEST_F(DataCollectTest, TestGenerateLendMap_UpdateData)
{
    std::map<std::string, std::vector<NodeMemBorrowInfo>> memMap;

    BorrowMemInfo borrowMemInfo1;
    borrowMemInfo1.name = "mem1";
    borrowMemInfo1.borrowNodeId = "node1";
    borrowMemInfo1.lentNodeId = "node2";
    borrowMemInfo1.size = 1024;
    borrowMemInfo1.lentNumaInfos = {{0, 512}};
    borrowMemInfo1.borrowRemoteNuma = -1;
    borrowMemInfo1.borrowLocalNuma = 0;

    BorrowMemInfo borrowMemInfo2;
    borrowMemInfo2.name = "mem2";
    borrowMemInfo2.borrowNodeId = "node1";
    borrowMemInfo2.lentNodeId = "node2";
    borrowMemInfo2.size = 2048;
    borrowMemInfo2.lentNumaInfos = {{0, 1024}};
    borrowMemInfo2.borrowRemoteNuma = -1;
    borrowMemInfo2.borrowLocalNuma = 0;

    std::string keyId = "key1";

    DataCollect::GenerateLendMap(borrowMemInfo1, keyId, memMap);
    DataCollect::GenerateLendMap(borrowMemInfo2, keyId, memMap);

    EXPECT_EQ(memMap.size(), 1);
    EXPECT_TRUE(memMap.find(keyId) != memMap.end());

    auto& nodeMemBorrowInfos = memMap[keyId];
    EXPECT_EQ(nodeMemBorrowInfos.size(), 1);
    EXPECT_EQ(nodeMemBorrowInfos[0].totalSize, 1024 + 2048); // 应该合并 size
    EXPECT_EQ(nodeMemBorrowInfos[0].numaNodeBorrowSize[0]["mem1"], 512);
    EXPECT_EQ(nodeMemBorrowInfos[0].numaNodeBorrowSize[0]["mem2"], 1024);
}

TEST_F(DataCollectTest, TestGenerateBorrowMap_UpdateData)
{
    std::map<std::string, std::vector<NodeMemBorrowInfo>> memMap;

    BorrowMemInfo borrowMemInfo1;
    borrowMemInfo1.name = "mem1";
    borrowMemInfo1.borrowNodeId = "node1";
    borrowMemInfo1.lentNodeId = "node2";
    borrowMemInfo1.size = 1024;
    borrowMemInfo1.lentNumaInfos = {{0, 512}};
    borrowMemInfo1.borrowRemoteNuma = -1;
    borrowMemInfo1.borrowLocalNuma = 0;

    BorrowMemInfo borrowMemInfo2;
    borrowMemInfo2.name = "mem2";
    borrowMemInfo2.borrowNodeId = "node1";
    borrowMemInfo2.lentNodeId = "node2";
    borrowMemInfo2.size = 2048;
    borrowMemInfo2.lentNumaInfos = {{0, 1024}};
    borrowMemInfo2.borrowRemoteNuma = -1;
    borrowMemInfo2.borrowLocalNuma = 0;

    std::string keyId = "key1";

    DataCollect::GenerateBorrowMap(borrowMemInfo1, keyId, memMap);
    DataCollect::GenerateBorrowMap(borrowMemInfo2, keyId, memMap);

    EXPECT_EQ(memMap.size(), 1);
    EXPECT_TRUE(memMap.find(keyId) != memMap.end());

    auto& nodeMemBorrowInfos = memMap[keyId];
    EXPECT_EQ(nodeMemBorrowInfos.size(), 1);
    EXPECT_EQ(nodeMemBorrowInfos[0].totalSize, 1024 + 2048); // 应该合并 size
    EXPECT_EQ(nodeMemBorrowInfos[0].numaNodeBorrowSize[0]["mem1"], 512);
    EXPECT_EQ(nodeMemBorrowInfos[0].numaNodeBorrowSize[0]["mem2"], 1024);
}

TEST_F(DataCollectTest, TestGenerateLendMap_AddNewData)
{
    std::map<std::string, std::vector<NodeMemBorrowInfo>> memMap;
    BorrowMemInfo borrowMemInfo1;
    borrowMemInfo1.name = "mem1";
    borrowMemInfo1.borrowNodeId = "node1";
    borrowMemInfo1.lentNodeId = "node2";
    borrowMemInfo1.size = 1024;
    borrowMemInfo1.lentNumaInfos = {{0, 512}};
    borrowMemInfo1.borrowRemoteNuma = -1;
    borrowMemInfo1.borrowLocalNuma = 0;

    BorrowMemInfo borrowMemInfo2;
    borrowMemInfo2.name = "mem2";
    borrowMemInfo2.borrowNodeId = "node3"; // 不同的 nodeId
    borrowMemInfo2.lentNodeId = "node4";
    borrowMemInfo2.size = 2048;
    borrowMemInfo2.lentNumaInfos = {{0, 1024}};
    borrowMemInfo2.borrowRemoteNuma = -1;
    borrowMemInfo2.borrowLocalNuma = 0;

    std::string keyId = "key1";

    DataCollect::GenerateLendMap(borrowMemInfo1, keyId, memMap);
    DataCollect::GenerateLendMap(borrowMemInfo2, keyId, memMap);

    EXPECT_EQ(memMap.size(), 1);
    EXPECT_TRUE(memMap.find(keyId) != memMap.end());

    auto& nodeMemBorrowInfos = memMap[keyId];
    EXPECT_EQ(nodeMemBorrowInfos.size(), 2); // 第二个条目应该被添加
    EXPECT_EQ(nodeMemBorrowInfos[1].srcNodeId, "node4");
    EXPECT_EQ(nodeMemBorrowInfos[1].destNodeId, "node3");
    EXPECT_EQ(nodeMemBorrowInfos[1].totalSize, 2048);
    EXPECT_EQ(nodeMemBorrowInfos[1].numaNodeBorrowSize[0]["mem2"], 1024);
}

TEST_F(DataCollectTest, TestGenerateBorrowMap_AddNewData)
{
    std::map<std::string, std::vector<NodeMemBorrowInfo>> memMap;
    BorrowMemInfo borrowMemInfo1;
    borrowMemInfo1.name = "mem1";
    borrowMemInfo1.borrowNodeId = "node1";
    borrowMemInfo1.lentNodeId = "node2";
    borrowMemInfo1.size = 1024;
    borrowMemInfo1.lentNumaInfos = {{0, 512}};
    borrowMemInfo1.borrowRemoteNuma = -1;
    borrowMemInfo1.borrowLocalNuma = 0;

    BorrowMemInfo borrowMemInfo2;
    borrowMemInfo2.name = "mem2";
    borrowMemInfo2.borrowNodeId = "node3"; // 不同的 nodeId
    borrowMemInfo2.lentNodeId = "node4";
    borrowMemInfo2.size = 2048;
    borrowMemInfo2.lentNumaInfos = {{0, 1024}};
    borrowMemInfo2.borrowRemoteNuma = -1;
    borrowMemInfo2.borrowLocalNuma = 0;

    std::string keyId = "key1";

    DataCollect::GenerateBorrowMap(borrowMemInfo1, keyId, memMap);
    DataCollect::GenerateBorrowMap(borrowMemInfo2, keyId, memMap);

    EXPECT_EQ(memMap.size(), 1);
    EXPECT_TRUE(memMap.find(keyId) != memMap.end());

    auto& nodeMemBorrowInfos = memMap[keyId];
    EXPECT_EQ(nodeMemBorrowInfos.size(), 2); // 第二个条目应该被添加
    EXPECT_EQ(nodeMemBorrowInfos[1].srcNodeId, "node4");
    EXPECT_EQ(nodeMemBorrowInfos[1].destNodeId, "node3");
    EXPECT_EQ(nodeMemBorrowInfos[1].totalSize, 2048);
    EXPECT_EQ(nodeMemBorrowInfos[1].numaNodeBorrowSize[0]["mem2"], 1024);
}

TEST_F(DataCollectTest, GenerateBorrow_Success)
{
    MOCKER(DataCollect::GenerateBorrowStrategyRawData).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateLoanableTotalBorrowMemMap).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GeneratePhysicalTopo).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GeneratelendMemMap).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateborrowMemMap).stubs().will(returnValue(UCACHE_OK));

    uint32_t ret = DataCollect::GenerateBorrow();

    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(DataCollectTest, TestGenerateBorrow_ExceedMaxFailedTimes)
{
    MOCKER(DataCollect::GenerateBorrowStrategyRawData).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateLoanableTotalBorrowMemMap).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GeneratePhysicalTopo).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GeneratelendMemMap).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateborrowMemMap).stubs().will(returnValue(UCACHE_OK));

    uint32_t ret = DataCollect::GenerateBorrow();
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(DataCollectTest, GetNodeMemoryInfo_WithData)
{
    NodeMemoryInfo info1 = {1024, 512, 512, 256, 0.5, 200, 2, 100};
    NodeMemoryInfo info2 = {2048, 1024, 1024, 512, 0.75, 500, 4, 200};

    DataCollect::nodeMemInfos = {{"node1", info1}, {"node2", info2}};

    std::map<std::string, NodeMemoryInfo> nodes;
    DataCollect::GetNodeMemoryInfo(nodes);

    EXPECT_EQ(nodes.size(), 2);
    EXPECT_EQ(nodes["node1"].totalMemory, 1024);
    EXPECT_EQ(nodes["node1"].usedMemory, 512);
    EXPECT_EQ(nodes["node1"].freeMemory, 512);
    EXPECT_EQ(nodes["node1"].pageCacheMemory, 256);
    EXPECT_DOUBLE_EQ(nodes["node1"].borrowedUsageRate, 0.5);
    EXPECT_EQ(nodes["node1"].memoryShortage, 200);
    EXPECT_EQ(nodes["node1"].pageCacheAppNums, 2);
    EXPECT_EQ(nodes["node1"].minFreeKbytes, 100);

    EXPECT_EQ(nodes["node2"].totalMemory, 2048);
    EXPECT_EQ(nodes["node2"].usedMemory, 1024);
    EXPECT_EQ(nodes["node2"].freeMemory, 1024);
    EXPECT_EQ(nodes["node2"].pageCacheMemory, 512);
    EXPECT_DOUBLE_EQ(nodes["node2"].borrowedUsageRate, 0.75);
    EXPECT_EQ(nodes["node2"].memoryShortage, 500);
    EXPECT_EQ(nodes["node2"].pageCacheAppNums, 4);
    EXPECT_EQ(nodes["node2"].minFreeKbytes, 200);
}

TEST_F(DataCollectTest, GetCgroupInfo_WithData)
{
    CgroupInfo info1 = {1000, 2000};
    CgroupInfo info2 = {1500, 2500};

    DataCollect::cgroupInfo = {{"cgroup1", {{"node1", info1}}}, {"cgroup2", {{"node2", info2}}}};

    std::map<std::string, std::map<std::string, CgroupInfo>> cgInfos;
    DataCollect::GetCgroupInfo(cgInfos);

    EXPECT_EQ(cgInfos.size(), 2);

    EXPECT_EQ(cgInfos["cgroup1"].size(), 1);
    EXPECT_EQ(cgInfos["cgroup1"]["node1"].pageCacheIn, 1000);
    EXPECT_EQ(cgInfos["cgroup1"]["node1"].ioReadBandwidth, 2000);

    EXPECT_EQ(cgInfos["cgroup2"].size(), 1);
    EXPECT_EQ(cgInfos["cgroup2"]["node2"].pageCacheIn, 1500);
    EXPECT_EQ(cgInfos["cgroup2"]["node2"].ioReadBandwidth, 2500);
}

TEST_F(DataCollectTest, SetNumaSocketMap_WithData)
{
    std::map<std::string, std::map<int, int>> socketMap = {{"node1", {{0, 100}, {1, 200}}},
                                                           {"node2", {{2, 300}, {3, 400}}}};

    DataCollect::SetNumaSocketMap(socketMap);

    ASSERT_EQ(DataCollect::numaSocketMap.size(), 2);
    ASSERT_EQ(DataCollect::numaSocketMap["node1"].size(), 2);
    ASSERT_EQ(DataCollect::numaSocketMap["node2"].size(), 2);

    EXPECT_EQ(DataCollect::numaSocketMap["node1"][0], 100);
    EXPECT_EQ(DataCollect::numaSocketMap["node1"][1], 200);
    EXPECT_EQ(DataCollect::numaSocketMap["node2"][2], 300);
    EXPECT_EQ(DataCollect::numaSocketMap["node2"][3], 400);
}

TEST_F(DataCollectTest, SetborrowMemMap_SetlendMemMap_WithData)
{
    NodeMemBorrowInfo info1;
    info1.totalSize = 1024;
    info1.srcNodeId = "node1";
    info1.destNodeId = "node2";
    info1.numaNodeBorrowSize = {{0, {{"mem1", 512}}}};
    info1.dstNumaId = 0;

    NodeMemBorrowInfo info2;
    info2.totalSize = 2048;
    info2.srcNodeId = "node3";
    info2.destNodeId = "node4";
    info2.numaNodeBorrowSize = {{1, {{"mem2", 1024}}}};
    info2.dstNumaId = 1;

    std::map<std::string, std::vector<NodeMemBorrowInfo>> lendMap = {{"group1", {info1}}, {"group2", {info2}}};

    std::map<std::string, std::vector<NodeMemBorrowInfo>> borrowMap = {{"group1", {info1}}, {"group2", {info2}}};

    DataCollect::SetborrowMemMap(borrowMap);
    DataCollect::SetlendMemMap(lendMap);

    EXPECT_EQ(DataCollect::borrowMemMap.size(), 2);

    EXPECT_EQ(DataCollect::borrowMemMap["group1"].size(), 1);
    EXPECT_EQ(DataCollect::borrowMemMap["group1"][0].totalSize, 1024);
    EXPECT_EQ(DataCollect::borrowMemMap["group1"][0].srcNodeId, "node1");
    EXPECT_EQ(DataCollect::borrowMemMap["group1"][0].destNodeId, "node2");
    EXPECT_EQ(DataCollect::borrowMemMap["group1"][0].numaNodeBorrowSize[0]["mem1"], 512);
    EXPECT_EQ(DataCollect::borrowMemMap["group1"][0].dstNumaId, 0);

    EXPECT_EQ(DataCollect::borrowMemMap["group2"].size(), 1);
    EXPECT_EQ(DataCollect::borrowMemMap["group2"][0].totalSize, 2048);
    EXPECT_EQ(DataCollect::borrowMemMap["group2"][0].srcNodeId, "node3");
    EXPECT_EQ(DataCollect::borrowMemMap["group2"][0].destNodeId, "node4");
    EXPECT_EQ(DataCollect::borrowMemMap["group2"][0].numaNodeBorrowSize[1]["mem2"], 1024);
    EXPECT_EQ(DataCollect::borrowMemMap["group2"][0].dstNumaId, 1);

    EXPECT_EQ(DataCollect::lendMemMap.size(), 2);

    EXPECT_EQ(DataCollect::lendMemMap["group1"].size(), 1);
    EXPECT_EQ(DataCollect::lendMemMap["group1"][0].totalSize, 1024);
    EXPECT_EQ(DataCollect::lendMemMap["group1"][0].srcNodeId, "node1");
    EXPECT_EQ(DataCollect::lendMemMap["group1"][0].destNodeId, "node2");
    EXPECT_EQ(DataCollect::lendMemMap["group1"][0].numaNodeBorrowSize[0]["mem1"], 512);
    EXPECT_EQ(DataCollect::lendMemMap["group1"][0].dstNumaId, 0);

    EXPECT_EQ(DataCollect::lendMemMap["group2"].size(), 1);
    EXPECT_EQ(DataCollect::lendMemMap["group2"][0].totalSize, 2048);
    EXPECT_EQ(DataCollect::lendMemMap["group2"][0].srcNodeId, "node3");
    EXPECT_EQ(DataCollect::lendMemMap["group2"][0].destNodeId, "node4");
    EXPECT_EQ(DataCollect::lendMemMap["group2"][0].numaNodeBorrowSize[1]["mem2"], 1024);
    EXPECT_EQ(DataCollect::lendMemMap["group2"][0].dstNumaId, 1);
}

TEST_F(DataCollectTest, SetNodeMemoryInfo_WithData)
{
    NodeMemoryInfo info1;
    info1.totalMemory = 1024;
    info1.usedMemory = 512;
    info1.freeMemory = 512;
    info1.pageCacheMemory = 256;
    info1.borrowedUsageRate = 0.5;
    info1.memoryShortage = 0.1;
    info1.pageCacheAppNums = 3;
    info1.minFreeKbytes = 128;

    NodeMemoryInfo info2;
    info2.totalMemory = 2048;
    info2.usedMemory = 1024;
    info2.freeMemory = 1024;
    info2.pageCacheMemory = 512;
    info2.borrowedUsageRate = 0.4;
    info2.memoryShortage = 0.2;
    info2.pageCacheAppNums = 5;
    info2.minFreeKbytes = 256;

    std::map<std::string, NodeMemoryInfo> nodes = {{"node1", info1}, {"node2", info2}};

    DataCollect::SetNodeMemoryInfo(nodes);

    EXPECT_EQ(DataCollect::nodeMemInfos.size(), 2);

    EXPECT_EQ(DataCollect::nodeMemInfos["node1"].totalMemory, 1024);
    EXPECT_EQ(DataCollect::nodeMemInfos["node1"].usedMemory, 512);
    EXPECT_EQ(DataCollect::nodeMemInfos["node1"].freeMemory, 512);
    EXPECT_EQ(DataCollect::nodeMemInfos["node1"].pageCacheMemory, 256);
    EXPECT_DOUBLE_EQ(DataCollect::nodeMemInfos["node1"].borrowedUsageRate, 0.5);
    EXPECT_DOUBLE_EQ(DataCollect::nodeMemInfos["node1"].memoryShortage, 0.1);
    EXPECT_EQ(DataCollect::nodeMemInfos["node1"].pageCacheAppNums, 3);
    EXPECT_EQ(DataCollect::nodeMemInfos["node1"].minFreeKbytes, 128);

    EXPECT_EQ(DataCollect::nodeMemInfos["node2"].totalMemory, 2048);
    EXPECT_EQ(DataCollect::nodeMemInfos["node2"].usedMemory, 1024);
    EXPECT_EQ(DataCollect::nodeMemInfos["node2"].freeMemory, 1024);
    EXPECT_EQ(DataCollect::nodeMemInfos["node2"].pageCacheMemory, 512);
    EXPECT_DOUBLE_EQ(DataCollect::nodeMemInfos["node2"].borrowedUsageRate, 0.4);
    EXPECT_DOUBLE_EQ(DataCollect::nodeMemInfos["node2"].memoryShortage, 0.2);
    EXPECT_EQ(DataCollect::nodeMemInfos["node2"].pageCacheAppNums, 5);
    EXPECT_EQ(DataCollect::nodeMemInfos["node2"].minFreeKbytes, 256);
}

TEST_F(DataCollectTest, SetCgroupInfo_WithData)
{
    CgroupInfo cgInfo1;
    cgInfo1.pageCacheIn = 1024;
    cgInfo1.ioReadBandwidth = 512;

    CgroupInfo cgInfo2;
    cgInfo2.pageCacheIn = 2048;
    cgInfo2.ioReadBandwidth = 1024;

    std::map<std::string, std::map<std::string, CgroupInfo>> cgInfos = {{"group1", {{"cgroup1", cgInfo1}}},
                                                                        {"group2", {{"cgroup2", cgInfo2}}}};

    DataCollect::SetCgroupInfo(cgInfos);

    EXPECT_EQ(DataCollect::cgroupInfo.size(), 2);

    EXPECT_EQ(DataCollect::cgroupInfo["group1"]["cgroup1"].pageCacheIn, 1024);
    EXPECT_EQ(DataCollect::cgroupInfo["group1"]["cgroup1"].ioReadBandwidth, 512);

    EXPECT_EQ(DataCollect::cgroupInfo["group2"]["cgroup2"].pageCacheIn, 2048);
    EXPECT_EQ(DataCollect::cgroupInfo["group2"]["cgroup2"].ioReadBandwidth, 1024);
}

TEST_F(DataCollectTest, GenerateCgroupInfo_SingleElement)
{
    DataCollect::cgroupInfo.clear();
    AddMockCgroupInfoData();

    uint32_t result = DataCollect::GenerateCgroupInfo();
    ASSERT_EQ(result, UCACHE_OK);

    ASSERT_EQ(DataCollect::cgroupInfo.size(), 2); // Should contain 2 nodes
    ASSERT_EQ(DataCollect::cgroupInfo["node1"]["docker1"].pageCacheIn, 0);
    ASSERT_EQ(DataCollect::cgroupInfo["node1"]["docker1"].ioReadBandwidth,
              0); // ioReadBytes (2000) / (KB_TO_BYTE * interval)
    ASSERT_EQ(DataCollect::cgroupInfo["node2"]["docker2"].pageCacheIn, 0);
    ASSERT_EQ(DataCollect::cgroupInfo["node2"]["docker2"].ioReadBandwidth,
              0); // ioReadBytes (2500) / (KB_TO_BYTE * interval)
}

TEST_F(DataCollectTest, GenerateCgroupInfo_MultipleElements)
{
    AddMockCgroupInfoData();

    std::map<std::string, std::map<std::string, CgroupInfos>> mockData2;
    std::map<std::string, CgroupInfos> node3Data;
    CgroupInfos cgInfo3;
    cgInfo3.pageCacheInfo.pageCacheIn = 2000;
    cgInfo3.ioInfo.ioReadBytes = 3000;
    node3Data["docker3"] = cgInfo3;
    mockData2["node3"] = node3Data;
    DataCollect::cgroupInfosQueue.push(mockData2);

    uint32_t result = DataCollect::GenerateCgroupInfo();

    ASSERT_EQ(result, UCACHE_OK);

    ASSERT_EQ(DataCollect::cgroupInfo.size(), 3);
    ASSERT_EQ(DataCollect::cgroupInfo["node1"]["docker1"].pageCacheIn, 0);
    ASSERT_EQ(DataCollect::cgroupInfo["node1"]["docker1"].ioReadBandwidth, 0); // Expected value
    ASSERT_EQ(DataCollect::cgroupInfo["node2"]["docker2"].pageCacheIn, 0);
    ASSERT_EQ(DataCollect::cgroupInfo["node2"]["docker2"].ioReadBandwidth, 0); // Expected value
    ASSERT_EQ(DataCollect::cgroupInfo["node3"]["docker3"].pageCacheIn, 0);
    ASSERT_EQ(DataCollect::cgroupInfo["node3"]["docker3"].ioReadBandwidth, 0); // Expected value
}

TEST_F(DataCollectTest, PrintCgroupInfo_SingleRackNodeSingleDocker)
{
    std::map<std::string, std::map<std::string, CgroupInfo>> cgroupInfo = {
        {"rack1", {{"docker1", {.pageCacheIn = 100, .ioReadBandwidth = 200}}}}};
    std::string result = DataCollect::PrintCgroupInfo(cgroupInfo);
    EXPECT_EQ(result, "{\n"
                      "  rack_node_id: \"rack1\",\n"
                      "    docker_id: \"docker1\",\n"
                      "    {\n"
                      "      pageCacheIn: 100,\n"
                      "      ioReadBandwidth: 200\n"
                      "    },\n"
                      "  },\n"
                      "}\n");
}

void GetCgroupInfo(std::map<std::string, std::map<std::string, CgroupInfo>>& cgroupInfo)
{
    std::map<std::string, std::map<std::string, CgroupInfo>> info = {
        {"rack1",
         {{"docker1", {.pageCacheIn = 100, .ioReadBandwidth = 200}},
          {"docker2", {.pageCacheIn = 300, .ioReadBandwidth = 400}}}},
        {"rack2",
         {{"docker3", {.pageCacheIn = 500, .ioReadBandwidth = 600}},
          {"docker4", {.pageCacheIn = 700, .ioReadBandwidth = 800}}}}};
    cgroupInfo = info;
}

TEST_F(DataCollectTest, PrintCgroupInfo_MultipleRackNodesMultipleDocker)
{
    std::map<std::string, std::map<std::string, CgroupInfo>> cgroupInfo{};
    GetCgroupInfo(cgroupInfo);
    std::string result = DataCollect::PrintCgroupInfo(cgroupInfo);
    EXPECT_EQ(result, "{\n"
                      "  rack_node_id: \"rack1\",\n"
                      "    docker_id: \"docker1\",\n"
                      "    {\n"
                      "      pageCacheIn: 100,\n"
                      "      ioReadBandwidth: 200\n"
                      "    },\n"
                      "    docker_id: \"docker2\",\n"
                      "    {\n"
                      "      pageCacheIn: 300,\n"
                      "      ioReadBandwidth: 400\n"
                      "    },\n"
                      "  },\n"
                      "  rack_node_id: \"rack2\",\n"
                      "    docker_id: \"docker3\",\n"
                      "    {\n"
                      "      pageCacheIn: 500,\n"
                      "      ioReadBandwidth: 600\n"
                      "    },\n"
                      "    docker_id: \"docker4\",\n"
                      "    {\n"
                      "      pageCacheIn: 700,\n"
                      "      ioReadBandwidth: 800\n"
                      "    },\n"
                      "  },\n"
                      "}\n");
}

TEST_F(DataCollectTest, PrintCgroupInfo_SingleRackNodeMultipleDocker)
{
    std::map<std::string, std::map<std::string, CgroupInfo>> cgroupInfo = {
        {"rack1",
         {{"docker1", {.pageCacheIn = 100, .ioReadBandwidth = 200}},
          {"docker2", {.pageCacheIn = 300, .ioReadBandwidth = 400}}}}};
    std::string result = DataCollect::PrintCgroupInfo(cgroupInfo);
    EXPECT_EQ(result, "{\n"
                      "  rack_node_id: \"rack1\",\n"
                      "    docker_id: \"docker1\",\n"
                      "    {\n"
                      "      pageCacheIn: 100,\n"
                      "      ioReadBandwidth: 200\n"
                      "    },\n"
                      "    docker_id: \"docker2\",\n"
                      "    {\n"
                      "      pageCacheIn: 300,\n"
                      "      ioReadBandwidth: 400\n"
                      "    },\n"
                      "  },\n"
                      "}\n");
}

TEST_F(DataCollectTest, GenerateTEST)
{
    DataCollect dc;
    uint32_t ret = dc.GenerateLoanableTotalBorrowMemMap();
    ret = dc.GeneratePhysicalTopo();
    ret = dc.GenerateNumaSocketMap();
    ret = dc.GeneratelendMemMap();
    ret = dc.GenerateborrowMemMap();
}

TEST_F(DataCollectTest, GenerateLoanableTotalBorrowMemMap_GetNodeMemInfo_ERR_Test)
{
    DataCollect dc;
    MOCKER(Deserialize::GetNodeMemInfo).stubs().with(mockcpp::any(), mockcpp::any()).will(returnValue(UCACHE_ERR));

    uint32_t ret = dc.GenerateLoanableTotalBorrowMemMap();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(DataCollectTest, ParseRackNodeIdReturnOKTest)
{
    std::string originNodeId = "123-456";
    std::string nodeId;

    uint32_t ret = ucache::data_collect::ParseRackNodeId(originNodeId, nodeId);
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(DataCollectTest, ParseRackNodeIdReturnERRTest)
{
    std::string originNodeId = "123456";
    std::string nodeId;

    uint32_t ret = ucache::data_collect::ParseRackNodeId(originNodeId, nodeId);
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(DataCollectTest, PrintPhysicalTopoTest)
{
    std::map<std::string, std::vector<std::string>> physicalTopo;
    std::vector<std::string> moduleName = {"testModule1", "testModule2"};
    physicalTopo["1"] = moduleName;

    PrintPhysicalTopo(physicalTopo);
}

TEST_F(DataCollectTest, SetCgroupInfoDefaultTEST)
{
    CgroupInfo cgInfo;
    SetCgroupInfoDefault(cgInfo);
}

TEST_F(DataCollectTest, GeneratePhysicalTopo_UbseMemGetTopologyInfoERR_TEST)
{
    DataCollect dc;

    MOCKER(ubse::nodeController::UbseMemGetTopologyInfo).stubs().will(returnValue(1));

    uint32_t ret = dc.GeneratePhysicalTopo();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(DataCollectTest, PrintNumaSocketMap_TEST)
{
    std::map<int, int> mp = {{1, 2}, {3, 4}};
    std::map<std::string, std::map<int, int>> numaSocketMap;
    numaSocketMap["1"] = mp;

    PrintNumaSocketMap(numaSocketMap);
}

TEST_F(DataCollectTest, GenerateNumaSocketMap_UbseMemGetTopologyInfoERR_TEST)
{
    DataCollect dc;

    MOCKER(ubse::nodeController::UbseMemGetTopologyInfo).stubs().will(returnValue(1));

    uint32_t ret = dc.GenerateNumaSocketMap();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(DataCollectTest, GenerateBorrow_ERR_TEST)
{
    bool needCollect = false;
    uint32_t failedTimes = 0;

    MOCKER(DataCollect::GenerateBorrowStrategyRawData).stubs().will(returnValue(UCACHE_OK));
    MOCKER(DataCollect::GenerateLoanableTotalBorrowMemMap).stubs().will(returnValue(UCACHE_ERR));
    MOCKER(DataCollect::GeneratePhysicalTopo).stubs().will(returnValue(UCACHE_ERR));
    MOCKER(DataCollect::GenerateNumaSocketMap).stubs().will(returnValue(UCACHE_ERR));
    MOCKER(DataCollect::GeneratelendMemMap).stubs().will(returnValue(UCACHE_ERR));
    MOCKER(DataCollect::GenerateborrowMemMap).stubs().will(returnValue(UCACHE_ERR));

    uint32_t ret = DataCollect::GenerateBorrow();

    EXPECT_EQ(ret, UCACHE_ERR);
}

class GeneratePerNodeNumaSocketMapTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "[Phase SetUp Begin]" << std::endl;
        std::cout << "[Phase SetUp End]" << std::endl;
    }

    void TearDown() override
    {
        std::cout << "[Phase TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        std::cout << "[Phase TearDown End]" << std::endl;
    }

    std::map<std::string, std::map<int, int>> numaSocketMap;
    ubse::nodeController::MemNodeData testNodeData1;
    ubse::nodeController::MemNodeData testNodeData2;
    ubse::nodeController::MemNodeData testNodeData3;
};

TEST_F(GeneratePerNodeNumaSocketMapTest, BasicFunctionalityTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);
    ubse::nodeController::NumaData numa1_2;
    numa1_2.numaId = "1";
    testNodeData1.socket.numas.push_back(numa1_2);
    ubse::nodeController::NumaData numa1_3;
    numa1_3.numaId = "2";
    testNodeData1.socket.numas.push_back(numa1_3);

    testNodeData2.nodeId = "node1";
    testNodeData2.hostname = "host1";
    testNodeData2.socket.socketId = "1";
    testNodeData2.isRegisterRm = false;
    ubse::nodeController::NumaData numa2_1;
    numa2_1.numaId = "3";
    testNodeData2.socket.numas.push_back(numa2_1);
    ubse::nodeController::NumaData numa2_2;
    numa2_2.numaId = "4";
    testNodeData2.socket.numas.push_back(numa2_2);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1, testNodeData2};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 2);
    EXPECT_EQ(numaSocketMap["node0"].size(), 3);
    EXPECT_EQ(numaSocketMap["node0"][0], 0);
    EXPECT_EQ(numaSocketMap["node0"][1], 0);
    EXPECT_EQ(numaSocketMap["node0"][2], 0);
    EXPECT_EQ(numaSocketMap["node1"].size(), 2);
    EXPECT_EQ(numaSocketMap["node1"][3], 1);
    EXPECT_EQ(numaSocketMap["node1"][4], 1);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, EmptyInputTest)
{
    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec;
    std::map<std::string, std::map<int, int>> emptyMap;

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, emptyMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_TRUE(emptyMap.empty());
}

TEST_F(GeneratePerNodeNumaSocketMapTest, InvalidSocketIdTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "invalid_socket";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, INVALID_ARGUMENT);
    EXPECT_TRUE(numaSocketMap.empty());
}

TEST_F(GeneratePerNodeNumaSocketMapTest, InvalidNumaIdTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "invalid_numa";
    testNodeData1.socket.numas.push_back(numa1_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, INVALID_ARGUMENT);
    EXPECT_TRUE(numaSocketMap.empty());
}

TEST_F(GeneratePerNodeNumaSocketMapTest, NegativeSocketIdTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "-1";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 1);
    EXPECT_EQ(numaSocketMap["node0"][0], -1);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, NegativeNumaIdTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "-1";
    testNodeData1.socket.numas.push_back(numa1_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 1);
    EXPECT_EQ(numaSocketMap["node0"][-1], 0);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, LargeSocketIdTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "2147483647";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 1);
    EXPECT_EQ(numaSocketMap["node0"][0], 2147483647);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, ZeroSocketIdTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 1);
    EXPECT_EQ(numaSocketMap["node0"][0], 0);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, ZeroNumaIdTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 1);
    EXPECT_EQ(numaSocketMap["node0"][0], 0);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, DuplicateNodeIdTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);

    testNodeData2.nodeId = "node0";
    testNodeData2.hostname = "host1";
    testNodeData2.socket.socketId = "1";
    testNodeData2.isRegisterRm = false;
    ubse::nodeController::NumaData numa2_1;
    numa2_1.numaId = "1";
    testNodeData2.socket.numas.push_back(numa2_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1, testNodeData2};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 1);
    EXPECT_EQ(numaSocketMap["node0"].size(), 2);
    EXPECT_EQ(numaSocketMap["node0"][0], 0);
    EXPECT_EQ(numaSocketMap["node0"][1], 1);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, SingleNodeMultipleNumaTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);
    ubse::nodeController::NumaData numa1_2;
    numa1_2.numaId = "1";
    testNodeData1.socket.numas.push_back(numa1_2);
    ubse::nodeController::NumaData numa1_3;
    numa1_3.numaId = "2";
    testNodeData1.socket.numas.push_back(numa1_3);
    ubse::nodeController::NumaData numa1_4;
    numa1_4.numaId = "3";
    testNodeData1.socket.numas.push_back(numa1_4);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 1);
    EXPECT_EQ(numaSocketMap["node0"].size(), 4);
    EXPECT_EQ(numaSocketMap["node0"][0], 0);
    EXPECT_EQ(numaSocketMap["node0"][1], 0);
    EXPECT_EQ(numaSocketMap["node0"][2], 0);
    EXPECT_EQ(numaSocketMap["node0"][3], 0);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, SameSocketDiffNumaTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);
    ubse::nodeController::NumaData numa1_2;
    numa1_2.numaId = "1";
    testNodeData1.socket.numas.push_back(numa1_2);
    ubse::nodeController::NumaData numa1_3;
    numa1_3.numaId = "2";
    testNodeData1.socket.numas.push_back(numa1_3);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 1);
    EXPECT_EQ(numaSocketMap["node0"].size(), 3);
    EXPECT_EQ(numaSocketMap["node0"][0], 0);
    EXPECT_EQ(numaSocketMap["node0"][1], 0);
    EXPECT_EQ(numaSocketMap["node0"][2], 0);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, MultiNodeMultiNumaTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "0";
    testNodeData1.socket.numas.push_back(numa1_1);
    ubse::nodeController::NumaData numa1_2;
    numa1_2.numaId = "1";
    testNodeData1.socket.numas.push_back(numa1_2);

    testNodeData2.nodeId = "node1";
    testNodeData2.hostname = "host1";
    testNodeData2.socket.socketId = "1";
    testNodeData2.isRegisterRm = false;
    ubse::nodeController::NumaData numa2_1;
    numa2_1.numaId = "2";
    testNodeData2.socket.numas.push_back(numa2_1);
    ubse::nodeController::NumaData numa2_2;
    numa2_2.numaId = "3";
    testNodeData2.socket.numas.push_back(numa2_2);
    ubse::nodeController::NumaData numa2_3;
    numa2_3.numaId = "4";
    testNodeData2.socket.numas.push_back(numa2_3);

    testNodeData3.nodeId = "node2";
    testNodeData3.hostname = "host2";
    testNodeData3.socket.socketId = "2";
    testNodeData3.isRegisterRm = false;
    ubse::nodeController::NumaData numa3_1;
    numa3_1.numaId = "5";
    testNodeData3.socket.numas.push_back(numa3_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1, testNodeData2, testNodeData3};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 3);
    EXPECT_EQ(numaSocketMap["node0"].size(), 2);
    EXPECT_EQ(numaSocketMap["node1"].size(), 3);
    EXPECT_EQ(numaSocketMap["node2"].size(), 1);
    EXPECT_EQ(numaSocketMap["node0"][0], 0);
    EXPECT_EQ(numaSocketMap["node0"][1], 0);
    EXPECT_EQ(numaSocketMap["node1"][2], 1);
    EXPECT_EQ(numaSocketMap["node1"][3], 1);
    EXPECT_EQ(numaSocketMap["node1"][4], 1);
    EXPECT_EQ(numaSocketMap["node2"][5], 2);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, EmptyNumaVectorTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "0";
    testNodeData1.isRegisterRm = false;
    testNodeData1.socket.numas.clear();

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_TRUE(numaSocketMap.empty());
}

TEST_F(GeneratePerNodeNumaSocketMapTest, MaxValueTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "2147483647";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "2147483647";
    testNodeData1.socket.numas.push_back(numa1_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    EXPECT_EQ(result, UCACHE_OK);
    EXPECT_EQ(numaSocketMap.size(), 1);
    EXPECT_EQ(numaSocketMap["node0"][2147483647], 2147483647);
}

TEST_F(GeneratePerNodeNumaSocketMapTest, NonNumericStringTest)
{
    testNodeData1.nodeId = "node0";
    testNodeData1.hostname = "host0";
    testNodeData1.socket.socketId = "abc";
    testNodeData1.isRegisterRm = false;
    ubse::nodeController::NumaData numa1_1;
    numa1_1.numaId = "def";
    testNodeData1.socket.numas.push_back(numa1_1);

    std::vector<ubse::nodeController::MemNodeData> memNodeDataVec = {testNodeData1};

    // 调用被测试函数
    uint32_t result = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);

    // 验证结果
    EXPECT_EQ(result, INVALID_ARGUMENT);
    EXPECT_TRUE(numaSocketMap.empty());
}