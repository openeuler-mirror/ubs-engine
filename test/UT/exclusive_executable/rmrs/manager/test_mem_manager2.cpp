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

#include <gmock/gmock.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include "ubse_storage.h"
#include "exporter.h"
#include "mem_manager.h"
#include "mp_configuration.h"
#include "mp_mem_json_util.h"
#include "mp_sync_data_helper.h"
#include "numa_info.h"
#include "rmrs_serialize.h"
#include "securec.h"

#undef private
using namespace std;
using namespace mempooling;
using namespace mockcpp;
using namespace ubse::storage;
using namespace mempooling::sync;
using namespace rmrs::serialize;

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
    mempooling::BorrowRecordHelper& obj = mempooling::BorrowRecordHelper::Instance();
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
    MOCKER_CPP(&JsonUtil::GetJsonInt16Value, bool (*)(const rapidjson::Value&, const char*, int16_t&))
        .stubs()
        .will(returnValue(false));
    auto ret = MemRequestHelper::ParseBorrowRecordFields(borrowRecordInfo, borrowRecord);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

uint32_t RackMemGetTopologyInfoMock1(std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology)
{
    MemNodeData memNodeDataA;
    memNodeDataA.nodeId = "NodeA";
    memNodeDataA.hostname = "hostA";
    memNodeDataA.isRegisterRm = true;
    memNodeDataA.socket.socketId = "100";                              // 只允许socketId=100
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

TEST_F(TestMemManager2, SmapEnableCompleted_Update_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    int16_t numaId = 1;
    auto ret = obj.Update(numaId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, SmapEnableCompleted_Update_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    int16_t numaId = 1;
    auto ret = obj.Update(numaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, SmapEnableCompleted_Remove_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    obj.smapEnableCompleted.insert(1);
    int16_t numaId = 1;
    auto ret = obj.Remove(numaId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, SmapEnableCompleted_Remove_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    obj.smapEnableCompleted.insert(1);
    int16_t numaId = 1;
    auto ret = obj.Remove(numaId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, SmapEnableCompleted_Query_succeed)
{
    MOCKER_CPP(UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key, void* ctx,
                                                 UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    std::vector<int16_t> smapEnableCompletedList;
    auto ret = obj.Query(smapEnableCompletedList);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, SmapEnableCompleted_Query_failed)
{
    MOCKER_CPP(UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key, void* ctx,
                                                 UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    std::vector<int16_t> smapEnableCompletedList;
    auto ret = obj.Query(smapEnableCompletedList);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, SmapEnableCompleted_GetRawData_empty)
{
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    obj.smapEnableCompleted.clear();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(buffer.len, 1);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, SmapEnableCompleted_GetRawData_succeed)
{
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    obj.smapEnableCompleted = {1, 2, 3};
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, SmapEnableCompleted_PutRawData_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, SmapEnableCompleted_PutRawData_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::SmapEnableCompleted& obj = mempooling::SmapEnableCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_Update_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    std::string borrowId = "borrow_001";
    auto ret = obj.Update(borrowId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_Update_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    std::string borrowId = "borrow_001";
    auto ret = obj.Update(borrowId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_Remove_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    obj.borrowIdInFaultProcess.insert("borrow_001");
    std::string borrowId = "borrow_001";
    auto ret = obj.Remove(borrowId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_Remove_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    obj.borrowIdInFaultProcess.insert("borrow_001");
    std::string borrowId = "borrow_001";
    auto ret = obj.Remove(borrowId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_Query_succeed)
{
    MOCKER_CPP(UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key, void* ctx,
                                                 UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    std::vector<std::string> borrowIdInFaultProcessList;
    auto ret = obj.Query(borrowIdInFaultProcessList);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_Query_failed)
{
    MOCKER_CPP(UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key, void* ctx,
                                                 UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    std::vector<std::string> borrowIdInFaultProcessList;
    auto ret = obj.Query(borrowIdInFaultProcessList);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_GetRawData_empty)
{
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    obj.borrowIdInFaultProcess.clear();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(buffer.len, 1);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_GetRawData_succeed)
{
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    obj.borrowIdInFaultProcess = {"borrow_001", "borrow_002"};
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_PutRawData_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_PutRawData_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_Clear_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    obj.borrowIdInFaultProcess = {"borrow_001", "borrow_002"};
    auto ret = obj.Clear();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, BorrowIdInFaultProcess_Clear_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowIdInFaultProcess& obj = mempooling::BorrowIdInFaultProcess::Instance();
    auto ret = obj.Clear();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, RemovePidCompleted_Update_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    uint16_t numaId = 1;
    std::vector<pid_t> pids = {100, 200, 300};
    auto ret = obj.Update(numaId, pids);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, RemovePidCompleted_Update_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    uint16_t numaId = 1;
    std::vector<pid_t> pids = {100, 200, 300};
    auto ret = obj.Update(numaId, pids);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, RemovePidCompleted_Remove_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    obj.removePidCompleted[1] = {100, 200, 300};
    uint16_t numaId = 1;
    std::vector<pid_t> pids = {100, 200};
    auto ret = obj.Remove(numaId, pids);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, RemovePidCompleted_Remove_not_found)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    obj.removePidCompleted.clear();
    uint16_t numaId = 1;
    std::vector<pid_t> pids = {100, 200};
    auto ret = obj.Remove(numaId, pids);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, RemovePidCompleted_Remove_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    obj.removePidCompleted[1] = {100, 200, 300};
    uint16_t numaId = 1;
    std::vector<pid_t> pids = {100, 200};
    auto ret = obj.Remove(numaId, pids);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, RemovePidCompleted_Query_succeed)
{
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    obj.removePidCompleted[1] = {100, 200, 300};
    obj.removePidCompleted[2] = {400, 500};
    std::unordered_map<uint16_t, std::unordered_set<pid_t>> removePidCompletedList;
    auto ret = obj.Query(removePidCompletedList);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(removePidCompletedList.size(), 2);
}

TEST_F(TestMemManager2, RemovePidCompleted_Query_empty)
{
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    obj.removePidCompleted.clear();
    std::unordered_map<uint16_t, std::unordered_set<pid_t>> removePidCompletedList;
    auto ret = obj.Query(removePidCompletedList);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(removePidCompletedList.size(), 0);
}

TEST_F(TestMemManager2, RemovePidCompleted_GetRawData_empty)
{
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    obj.removePidCompleted.clear();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(buffer.len, 1);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, RemovePidCompleted_GetRawData_succeed)
{
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    obj.removePidCompleted[1] = {100, 200, 300};
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, RemovePidCompleted_PutRawData_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, RemovePidCompleted_PutRawData_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::RemovePidCompleted& obj = mempooling::RemovePidCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, MemReturnManager_Update_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::MemReturnManager& obj = mempooling::MemReturnManager::Instance();
    std::string borrowId = "borrow_001";
    BorrowItem item;
    item.borrowId = borrowId;
    item.srcNid = "node1";
    item.srcRemoteNumaId = 1;
    item.dstNid = "node2";
    item.dstSocketId = 2;
    item.scanCount = 10;
    auto ret = obj.Update(borrowId, item);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, MemReturnManager_Update_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::MemReturnManager& obj = mempooling::MemReturnManager::Instance();
    std::string borrowId = "borrow_001";
    BorrowItem item;
    item.borrowId = borrowId;
    auto ret = obj.Update(borrowId, item);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, MemReturnManager_Query_found)
{
    mempooling::MemReturnManager& obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_001", {"borrow_001", "node1", 1, "node2", 2, 10}}};
    std::string borrowId = "borrow_001";
    BorrowItem value;
    auto ret = obj.Query(borrowId, value);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(value.borrowId, "borrow_001");
    EXPECT_EQ(value.srcNid, "node1");
}

TEST_F(TestMemManager2, MemReturnManager_Query_not_found)
{
    mempooling::MemReturnManager& obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_001", {"borrow_001", "node1", 1, "node2", 2, 10}}};
    std::string borrowId = "borrow_002";
    BorrowItem value;
    auto ret = obj.Query(borrowId, value);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager2, MemReturnManager_QueryAll_succeed)
{
    mempooling::MemReturnManager& obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_001", {"borrow_001", "node1", 1, "node2", 2, 10}},
                       {"borrow_002", {"borrow_002", "node1", 2, "node3", 3, 20}}};
    std::unordered_map<std::string, BorrowItem> borrowCacheAll;
    auto ret = obj.QueryAll(borrowCacheAll);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(borrowCacheAll.size(), 2);
}

TEST_F(TestMemManager2, MemReturnManager_QueryAll_empty)
{
    mempooling::MemReturnManager& obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache.clear();
    std::unordered_map<std::string, BorrowItem> borrowCacheAll;
    auto ret = obj.QueryAll(borrowCacheAll);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(borrowCacheAll.size(), 0);
}

TEST_F(TestMemManager2, MemReturnManager_PutRawData_succeed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::MemReturnManager& obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_001", {"borrow_001", "node1", 1, "node2", 2, 10}}};
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer);
    ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    delete[] buffer.data;
}

TEST_F(TestMemManager2, MemReturnManager_PutRawData_failed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::MemReturnManager& obj = mempooling::MemReturnManager::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, ParseMemIdArray_Succeed)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    rapidjson::Value lentMemIdArr(kArrayType);
    lentMemIdArr.PushBack(1001, alloc);
    lentMemIdArr.PushBack(1002, alloc);
    rapidjson::Value borrowMemIdArr(kArrayType);
    borrowMemIdArr.PushBack(2001, alloc);
    borrowMemIdArr.PushBack(2002, alloc);
    borrowRecordInfo.AddMember("lentMemId", lentMemIdArr, alloc);
    borrowRecordInfo.AddMember("borrowMemId", borrowMemIdArr, alloc);
    BorrowRecord record;
    auto ret = MemRequestHelper::ParseMemIdArray(borrowRecordInfo, record);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(record.lentMemId.size(), 2);
    EXPECT_EQ(record.borrowMemId.size(), 2);
}

TEST_F(TestMemManager2, ParseMemIdArray_NoLentMemId)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    borrowRecordInfo.AddMember("borrowMemId", rapidjson::Value(kArrayType), alloc);
    BorrowRecord record;
    auto ret = MemRequestHelper::ParseMemIdArray(borrowRecordInfo, record);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, ParseMemIdArray_NoBorrowMemId)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    borrowRecordInfo.AddMember("lentMemId", rapidjson::Value(kArrayType), alloc);
    BorrowRecord record;
    auto ret = MemRequestHelper::ParseMemIdArray(borrowRecordInfo, record);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, ParseMemIdArray_LentMemIdNotArray)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    borrowRecordInfo.AddMember("lentMemId", "not_array", alloc);
    borrowRecordInfo.AddMember("borrowMemId", rapidjson::Value(kArrayType), alloc);
    BorrowRecord record;
    auto ret = MemRequestHelper::ParseMemIdArray(borrowRecordInfo, record);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, ParseMemIdArray_BorrowMemIdNotArray)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    rapidjson::Value lentMemIdArr(kArrayType);
    lentMemIdArr.PushBack(1001, alloc);
    borrowRecordInfo.AddMember("lentMemId", lentMemIdArr, alloc);
    borrowRecordInfo.AddMember("borrowMemId", "not_array", alloc);
    BorrowRecord record;
    auto ret = MemRequestHelper::ParseMemIdArray(borrowRecordInfo, record);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, ParseMemIdArray_LentMemIdItemNotUint64)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    rapidjson::Value lentMemIdArr(kArrayType);
    lentMemIdArr.PushBack("not_uint64", alloc);
    borrowRecordInfo.AddMember("lentMemId", lentMemIdArr, alloc);
    borrowRecordInfo.AddMember("borrowMemId", rapidjson::Value(kArrayType), alloc);
    BorrowRecord record;
    auto ret = MemRequestHelper::ParseMemIdArray(borrowRecordInfo, record);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, ParseMemIdArray_BorrowMemIdItemNotUint64)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    rapidjson::Value lentMemIdArr(kArrayType);
    lentMemIdArr.PushBack(1001, alloc);
    borrowRecordInfo.AddMember("lentMemId", lentMemIdArr, alloc);
    rapidjson::Value borrowMemIdArr(kArrayType);
    borrowMemIdArr.PushBack("not_uint64", alloc);
    borrowRecordInfo.AddMember("borrowMemId", borrowMemIdArr, alloc);
    BorrowRecord record;
    auto ret = MemRequestHelper::ParseMemIdArray(borrowRecordInfo, record);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, ParseLentNumaArray_Succeed)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    rapidjson::Value lentNumaArr(kArrayType);
    rapidjson::Value lentNumaItem(kObjectType);
    lentNumaItem.AddMember("numaId", 1, alloc);
    lentNumaItem.AddMember("lentSize", 1024 * 1024, alloc);
    lentNumaArr.PushBack(lentNumaItem, alloc);
    borrowRecordInfo.AddMember("lentNuma", lentNumaArr, alloc);
    std::vector<LentNuma> lentNumaVec;
    auto ret = MemRequestHelper::ParseLentNumaArray(borrowRecordInfo, lentNumaVec);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(lentNumaVec.size(), 1);
}

TEST_F(TestMemManager2, ParseLentNumaArray_NoLentNuma)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    std::vector<LentNuma> lentNumaVec;
    auto ret = MemRequestHelper::ParseLentNumaArray(borrowRecordInfo, lentNumaVec);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, ParseLentNumaArray_NotArray)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    borrowRecordInfo.AddMember("lentNuma", "not_array", alloc);
    std::vector<LentNuma> lentNumaVec;
    auto ret = MemRequestHelper::ParseLentNumaArray(borrowRecordInfo, lentNumaVec);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, ParseLentNumaArray_ParseFailed)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value borrowRecordInfo(kObjectType);
    rapidjson::Value lentNumaArr(kArrayType);
    rapidjson::Value lentNumaItem(kObjectType);
    lentNumaItem.AddMember("numaId", "invalid", alloc);
    lentNumaArr.PushBack(lentNumaItem, alloc);
    borrowRecordInfo.AddMember("lentNuma", lentNumaArr, alloc);
    std::vector<LentNuma> lentNumaVec;
    auto ret = MemRequestHelper::ParseLentNumaArray(borrowRecordInfo, lentNumaVec);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, GenBorrowRecords_Succeed)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value recordArr(kArrayType);
    rapidjson::Value recordItem(kObjectType);
    recordItem.AddMember("name", "borrow_001", alloc);
    recordItem.AddMember("size", 1024 * 1024, alloc);
    recordItem.AddMember("lentNode", "nodeA", alloc);
    recordItem.AddMember("lentSocketId", 1, alloc);
    recordItem.AddMember("borrowNode", "nodeB", alloc);
    recordItem.AddMember("borrowLocalNuma", 0, alloc);
    recordItem.AddMember("borrowRemoteNuma", 1, alloc);
    rapidjson::Value lentMemIdArr(kArrayType);
    lentMemIdArr.PushBack(1001, alloc);
    recordItem.AddMember("lentMemId", lentMemIdArr, alloc);
    rapidjson::Value borrowMemIdArr(kArrayType);
    borrowMemIdArr.PushBack(2001, alloc);
    recordItem.AddMember("borrowMemId", borrowMemIdArr, alloc);
    rapidjson::Value lentNumaArr(kArrayType);
    rapidjson::Value lentNumaItem(kObjectType);
    lentNumaItem.AddMember("numaId", 1, alloc);
    lentNumaItem.AddMember("lentSize", 1024 * 1024, alloc);
    lentNumaArr.PushBack(lentNumaItem, alloc);
    recordItem.AddMember("lentNuma", lentNumaArr, alloc);
    recordArr.PushBack(recordItem, alloc);
    std::vector<BorrowRecord> borrowRecords;
    auto ret = BorrowRecordHelper::Instance().GenBorrowRecords(recordArr, borrowRecords);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(borrowRecords.size(), 1);
}

TEST_F(TestMemManager2, GenBorrowRecords_NotArray)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value notArray(kObjectType);
    std::vector<BorrowRecord> borrowRecords;
    auto ret = BorrowRecordHelper::Instance().GenBorrowRecords(notArray, borrowRecords);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager2, GenBorrowRecords_ParseFailed)
{
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value recordArr(kArrayType);
    rapidjson::Value recordItem(kObjectType);
    recordItem.AddMember("name", "borrow_001", alloc);
    rapidjson::Value lentMemIdArr(kArrayType);
    lentMemIdArr.PushBack(1001, alloc);
    recordItem.AddMember("lentMemId", lentMemIdArr, alloc);
    recordArr.PushBack(recordItem, alloc);
    std::vector<BorrowRecord> borrowRecords;
    auto ret = BorrowRecordHelper::Instance().GenBorrowRecords(recordArr, borrowRecords);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

} // namespace mempooling