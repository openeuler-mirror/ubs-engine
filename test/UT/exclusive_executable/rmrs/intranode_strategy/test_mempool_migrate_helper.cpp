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
#include <cstring>
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
 
#include "mempool_migrate_helper.cpp"
 
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
 
namespace mempooling {
using namespace ubse::com;
using namespace std;
using namespace mempooling::migrate;

// 测试类
class TestMemPoolMigrateHelper : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[TestMemPoolMigrateHelper SetUp Begin]" << endl;
        cout << "[TestMemPoolMigrateHelper SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestMemPoolMigrateHelper TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestMemPoolMigrateHelper TearDown End]" << endl;
    }
};

TEST_F(TestMemPoolMigrateHelper, ValidBorrowIdPidMapSucceed)
{
    std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap;
    auto ret = ValidBorrowIdPidMap(borrowIdsPidsMap);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemPoolMigrateHelper, ValidBorrowIdPidMapFailed2)
{
    std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap;
    BorrowIdInfo info;
    info.pid = 0;
    borrowIdsPidsMap["a"].insert(info);
    info.pid = 1;
    borrowIdsPidsMap["b"].insert(info);
    auto ret = ValidBorrowIdPidMap(borrowIdsPidsMap);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemPoolMigrateHelper, GetRollBackBorrowIdPid1)
{
    GTEST_SKIP();
    RollBackBorrowIdPid entry;
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
               MpResult(*)(std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(1));
    std::vector<std::string> borrowIdList;
    // auto res = GetRollBackBorrowIdPid("1", entry, borrowIdList);
    // ASSERT_EQ(res, 1);
}
 
TEST_F(TestMemPoolMigrateHelper, GetRollBackBorrowIdPid2)
{
    GTEST_SKIP();
    RollBackBorrowIdPid entry;
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
               MpResult(*)(std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&mempooling::Name2VmInfo::Update,
               MpResult(*)(std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    std::vector<std::string> borrowIdList;
    borrowIdList.push_back("0");
    // auto res = GetRollBackBorrowIdPid("1", entry, borrowIdList);
    // ASSERT_EQ(res, 0);
}

MpResult TestGetName2VmInfo(Name2VmInfo* module, const std::string &nodeId,
    std::map<std::string, std::set<BorrowIdInfo>> &borrowIdsPidsMap)
{
    std::map<std::string, std::set<BorrowIdInfo>> tmp;
    std::set<BorrowIdInfo> pidSet;
    pidSet.insert(BorrowIdInfo{1, 0});
    tmp["1"] = pidSet;
    borrowIdsPidsMap = tmp;
    return 0;
}

TEST_F(TestMemPoolMigrateHelper, GetRollBackBorrowIdPid3)
{
    GTEST_SKIP();
    RollBackBorrowIdPid entry;
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
            MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
            std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(invoke(TestGetName2VmInfo));
    MOCKER_CPP(&mempooling::Name2VmInfo::Update,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    std::vector<std::string> borrowIdList;
    borrowIdList.push_back("1");
    // auto res = GetRollBackBorrowIdPid("1", entry, borrowIdList);
    // ASSERT_EQ(res, 0);
}

TEST_F(TestMemPoolMigrateHelper, GetRollBackBorrowIdPid4)
{
    GTEST_SKIP();
    RollBackBorrowIdPid entry;
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(invoke(TestGetName2VmInfo));
    MOCKER_CPP(&mempooling::Name2VmInfo::Update,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    std::vector<std::string> borrowIdList;
    borrowIdList.push_back("0");
    borrowIdList.push_back("1");
    // auto res = GetRollBackBorrowIdPid("1", entry, borrowIdList);
    // ASSERT_EQ(res, 1);
}

TEST_F(TestMemPoolMigrateHelper, TestGetRollBackBorrowIdPidFailed_NotValidBorrowId)
{
    GTEST_SKIP();
    RollBackBorrowIdPid entry;
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(invoke(TestGetName2VmInfo));
    MOCKER_CPP(&mempooling::Name2VmInfo::Update,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    std::vector<std::string> borrowIdList;
    borrowIdList.push_back("1");
    borrowIdList.push_back("0");
    // auto res = GetRollBackBorrowIdPid("1", entry, borrowIdList);
    // ASSERT_EQ(res, 1);
}

TEST_F(TestMemPoolMigrateHelper, TestGetRollBackBorrowIdPidFailed_NotSameBatch)
{
    GTEST_SKIP();
    RollBackBorrowIdPid entry;
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(invoke(TestGetName2VmInfo));
    MOCKER_CPP(&mempooling::Name2VmInfo::Update,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&ValidBorrowIdPidMap,
               MpResult(*)(std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap))
        .stubs()
        .will(returnValue(1));
    std::vector<std::string> borrowIdList;
    borrowIdList.push_back("1");
    // auto res = GetRollBackBorrowIdPid("1", entry, borrowIdList);
    // ASSERT_EQ(res, 1);
}

TEST_F(TestMemPoolMigrateHelper, TestGetRollBackBorrowIdPidSuccess)
{
    GTEST_SKIP();
    RollBackBorrowIdPid entry;
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(invoke(TestGetName2VmInfo));
    MOCKER_CPP(&mempooling::Name2VmInfo::Update,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&ValidBorrowIdPidMap,
               MpResult(*)(std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap))
        .stubs()
        .will(returnValue(0));
    std::vector<std::string> borrowIdList;
    borrowIdList.push_back("1");
    // auto res = GetRollBackBorrowIdPid("1", entry, borrowIdList);
    // ASSERT_EQ(res, 0);
}

TEST_F(TestMemPoolMigrateHelper, PersistentBorrowIdFailed1)
{
    RollBackBorrowIdPid entry;
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(1));
    std::string nodeId("1");
    auto res = PersistentBorrowIdPid(nodeId, entry);
    ASSERT_EQ(res, 1);
}

TEST_F(TestMemPoolMigrateHelper, PersistentBorrowIdFailed2)
{
    RollBackBorrowIdPid entry;
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&mempooling::Name2VmInfo::Update,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(1));
    std::string nodeId("1");
    auto res = PersistentBorrowIdPid(nodeId, entry);
    ASSERT_EQ(res, 1);
}

TEST_F(TestMemPoolMigrateHelper, PersistentBorrowIdSucceed)
{
    RollBackBorrowIdPid entry;
    entry.borrowIdList = {"0", "1"};
    MOCKER_CPP(&mempooling::Name2VmInfo::Query,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(invoke(TestGetName2VmInfo));
    MOCKER_CPP(&mempooling::Name2VmInfo::Update,
               MpResult(*)(Name2VmInfo* module, const std::string &nodeId,
               std::map<std::string, std::set<BorrowIdInfo>> & value))
        .stubs()
        .will(returnValue(0));
    std::string nodeId("1");
    auto res = PersistentBorrowIdPid(nodeId, entry);
    ASSERT_EQ(res, 0);
}

TEST_F(TestMemPoolMigrateHelper, RpcMemBorrowRollback1)
{
    GTEST_SKIP();
    std::string nodeId;
    std::vector<std::string> borrowIdsList;
    MOCKER_CPP(&GetRollBackBorrowIdPid, MpResult(*)(const std::string &, RollBackBorrowIdPid & entry,
        std::vector<std::string> borrowIdList)).stubs().will(returnValue(0));
    MOCKER_CPP(UbseRpcSend, uint32_t(*)(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData, void *ctx,
                                         const UbseComRespHandler &handler))
        .stubs()
        .will(returnValue(1));
    auto res = RpcMemBorrowRollback(nodeId, borrowIdsList);
    ASSERT_EQ(res, 1);
}

TEST_F(TestMemPoolMigrateHelper, InitSuccess)
{
    MOCKER_CPP(&ubse::com::UbseRegRpcService,
               uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::MpMigrateSubModule subModule;
    auto res = subModule.Init();
    ASSERT_EQ(res, 0);
}

TEST_F(TestMemPoolMigrateHelper, InitFailed0)
{
    MOCKER_CPP(&ubse::com::UbseRegRpcService,
               uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::MpMigrateSubModule subModule;
    auto res = subModule.Init();
    ASSERT_EQ(res, 1);
}

MpResult GetVmInfosCompletedMapMock(VmInfosCompleted *This, std::unordered_map<pid_t, std::string> &vmInfosCompletedMap)
{
    std::string value = "Node0-5";
    pid_t pid = 1234;
    vmInfosCompletedMap[pid] = value;

    return 0;
}

MpResult GetVmInfosCompletedMapMockEmpty(VmInfosCompleted *This,
    std::unordered_map<pid_t, std::string> &vmInfosCompletedMap)
{
    std::string value = "";
    pid_t pid = 1234;
    vmInfosCompletedMap[pid] = value;

    return 0;
}

MpResult GetVmInfosCompletedMapMockGetLineFailed1(VmInfosCompleted *This,
                                                  std::unordered_map<pid_t, std::string> &vmInfosCompletedMap)
{
    std::string value = "Node0";
    pid_t pid = 1234;
    vmInfosCompletedMap[pid] = value;

    return 0;
}

MpResult GetVmInfosCompletedMapMockGetLineFailed2(VmInfosCompleted *This,
                                                  std::unordered_map<pid_t, std::string> &vmInfosCompletedMap)
{
    std::string value = "Node0-";
    pid_t pid = 1234;
    vmInfosCompletedMap[pid] = value;

    return 0;
}

TEST_F(TestMemPoolMigrateHelper, RollBackMigratedVmsInStandbyToMasterEvent_get_info_failed)
{
    MOCKER_CPP(&VmInfosCompleted::Query,
               MpResult(*)(VmInfosCompleted * This, std::unordered_map<pid_t, std::string> &vmInfosCompletedMap))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = RollBackMigratedVmsInStandbyToMasterEvent();
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolMigrateHelper, RollBackMigratedVmsInStandbyToMasterEvent_get_empty_info_failed)
{
    MOCKER_CPP(&VmInfosCompleted::Query,
               MpResult(*)(VmInfosCompleted * This, std::unordered_map<pid_t, std::string> &vmInfosCompletedMap))
        .stubs()
        .will(invoke(GetVmInfosCompletedMapMockEmpty));

    MpResult ret = RollBackMigratedVmsInStandbyToMasterEvent();
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolMigrateHelper, RollBackMigratedVmsInStandbyToMasterEvent_getline1_failed)
{
    MOCKER_CPP(&VmInfosCompleted::Query,
               MpResult(*)(VmInfosCompleted * This, std::unordered_map<pid_t, std::string> &vmInfosCompletedMap))
        .stubs()
        .will(invoke(GetVmInfosCompletedMapMockGetLineFailed1));

    MpResult ret = RollBackMigratedVmsInStandbyToMasterEvent();
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolMigrateHelper, RollBackMigratedVmsInStandbyToMasterEvent_getline2_failed)
{
    MOCKER_CPP(&VmInfosCompleted::Query,
               MpResult(*)(VmInfosCompleted * This, std::unordered_map<pid_t, std::string> &vmInfosCompletedMap))
        .stubs()
        .will(invoke(GetVmInfosCompletedMapMockGetLineFailed2));

    MpResult ret = RollBackMigratedVmsInStandbyToMasterEvent();
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolMigrateHelper, RollBackMigratedVmsInStandbyToMasterEvent_migrate_execute_failed)
{
    MOCKER_CPP(&VmInfosCompleted::Query,
               MpResult(*)(VmInfosCompleted *obj, std::unordered_map<pid_t, std::string> &vmInfosCompletedMap))
        .stubs()
        .will(invoke(GetVmInfosCompletedMapMock));

    MOCKER_CPP(&MigrateExecuteInStandbyToMasterEvent,
               MpResult(*)(const pid_t pid, const std::string borrowInNode, const std::string remoteNumaId))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&RemoveVmInfosCompletedWithRetry, MpResult(*)(const pid_t)).stubs().will(returnValue(MEM_POOLING_OK));
    MpResult ret = RollBackMigratedVmsInStandbyToMasterEvent();
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolMigrateHelper, RollBackMigratedVmsInStandbyToMasterEvent_remove_info_failed)
{
    MOCKER_CPP(&VmInfosCompleted::Query,
               MpResult(*)(VmInfosCompleted * This, std::unordered_map<pid_t, std::string> &vmInfosCompletedMap))
        .stubs()
        .will(invoke(GetVmInfosCompletedMapMock));
    MOCKER_CPP(&MigrateExecuteInStandbyToMasterEvent,
               MpResult(*)(const pid_t pid, const std::string borrowInNode, const std::string remoteNumaId))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&RemoveVmInfosCompletedWithRetry, MpResult(*)(const pid_t pid))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MpResult ret = RollBackMigratedVmsInStandbyToMasterEvent();
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

} // namespace mempooling