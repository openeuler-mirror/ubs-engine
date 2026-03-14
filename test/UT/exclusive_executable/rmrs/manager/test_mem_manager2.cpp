/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <iostream>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include "ubse_storage.h"
#include "mem_manager.h"
#include "mp_configuration.h"
#include "mp_mem_json_util.h"
#include "mp_sync_data_helper.h"
#include "numa_info.h"
#include "securec.h"
#include "exporter.h"

#undef private
using namespace std;
using namespace mempooling;
using namespace mockcpp;
using namespace ubse::storage;
using namespace mempooling::sync;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace mempooling {
class TestMemManager2 : public ::testing::Test {
    void SetUp() override
    {
        cout << "[TestMemManager2 SetUp Begin]" << endl;
        cout << "[TestMemManager2 SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestMemManager2 TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestMemManager2 TearDown End]" << endl;
    }
};

std::string borrowStrList = R"({"boring":[1],})";

TEST_F(TestMemManager2, UpdateBorrowRecordsSucceed)
{
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    auto ret = obj.UpdateBorrowRecords();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, ParseBorrowRecordFieldsFail)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    borrowRecordInfo.AddMember("test", "test", alloc);
    BorrowRecord borrowRecord;
    MOCKER_CPP(&JsonUtil::GetJsonInt16Value, bool(*)(const rapidjson::Value &, const char *, int16_t &))
        .stubs()
        .will(returnValue(false));
    auto ret = MemRequestHelper::ParseBorrowRecordFields(borrowRecordInfo, borrowRecord);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

uint32_t RackMemGetTopologyInfoMock1(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology)
{
    MemNodeData memNodeDataA;
    memNodeDataA.nodeId = "NodeA";
    memNodeDataA.hostname = "hostA";
    memNodeDataA.isRegisterRm = true;
    memNodeDataA.socket.socketId = "100";        // 只允许socketId=100
    memNodeDataA.socket.numas = {ubse::nodeController::NumaData{"0"}}; // 只包含 numaId = 0
    nodeTopology["node0-36"].push_back(memNodeDataA);
    return 0;
}

TEST_F(TestMemManager2, JudgeSampPlaneSuccess)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(invoke(RackMemGetTopologyInfoMock1));
    auto ret = mempooling::MemManager::Instance().JudgeSampPlane("node0", 36, "NodeA", 100);
    EXPECT_EQ(ret, true);
}

TEST_F(TestMemManager2, JudgeSampPlaneFailed1)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(returnValue(1));
    auto ret = mempooling::MemManager::Instance().JudgeSampPlane("node0", 36, "NodeA", 100);
    EXPECT_EQ(ret, false);
}

TEST_F(TestMemManager2, JudgeSampPlaneFailed2)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(invoke(RackMemGetTopologyInfoMock1));
    auto ret = mempooling::MemManager::Instance().JudgeSampPlane("node1", 36, "NodeA", 100);
    EXPECT_EQ(ret, false);
}

TEST_F(TestMemManager2, JudgeSampPlaneFailed3)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(invoke(RackMemGetTopologyInfoMock1));
    auto ret = mempooling::MemManager::Instance().JudgeSampPlane("node0", 36, "NodeB", 100);
    EXPECT_EQ(ret, false);
}

TEST_F(TestMemManager2, JudgeSampPlaneFailed4)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(invoke(RackMemGetTopologyInfoMock1));
    auto ret = mempooling::MemManager::Instance().JudgeSampPlane("node0", 36, "NodeA", 101);
    EXPECT_EQ(ret, false);
}

TEST_F(TestMemManager2, GetVmInfosCompletedPairSuccess)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(invoke(RackMemGetTopologyInfoMock1));
    auto ret = mempooling::MemManager::Instance().JudgeSampPlane("node0", 36, "NodeA", 101);
    EXPECT_EQ(ret, false);
}

}