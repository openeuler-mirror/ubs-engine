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

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "ubse_storage.h"
#include "iostream"
#include "mem_json_def.h"
#include "mem_manager.h"
#include "mempool_migrate_module.h"
#include "mempooling_return_module.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_smap_controller.h"
#include "mp_string_util.h"
#include "over_commit_fault_memid_module.h"
#include "process_mem_pid_manager_def.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {
using namespace std;

class TestMemBorrowExecutor : public ::testing::Test {
protected:
    TestMemBorrowExecutor() {}
    virtual ~TestMemBorrowExecutor() {}
    virtual void SetUp() {}
    virtual void TearDown()
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestMemBorrowExecutor, MemFree)
{
    MpResult ret;
    std::string name;
    ret = MemBorrowExecutor::Instance().MemFree(name);

    ASSERT_EQ(MEM_POOLING_OK, ret);
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsFailed)
{
    std::string name = "test";
    bool isForceDelete = false;
    bool smapBack = false;
    auto res = MemBorrowExecutor::Instance().MemFreeWithOps(name, isForceDelete, smapBack);
    ASSERT_EQ(MEM_POOLING_OK, res);
}

TEST_F(TestMemBorrowExecutor, TestMemFreeWithOpsFailed_GetBorrowIdRedirectionError)
{
    MOCKER_CPP(&BorrowIdRedirection::Query, uint32_t(*)(const std::string key, std::string& value))
        .stubs()
        .will(returnValue(1));
    std::string name = "test";
    bool isForceDelete = false;
    bool smapBack = false;
    auto res = MemBorrowExecutor::Instance().MemFreeWithOps(name, isForceDelete, smapBack);
    ASSERT_NE(MEM_POOLING_OK, res);
}

uint32_t TestGetBorrowIdRedirectionMock(BorrowIdRedirection* memManager, const std::string key, std::string& value)
{
    if (value == "test") {
        value = "t";
    } else {
        value.clear();
    }
    return 0;
}

uint32_t TestGetNullBorrowIdRedirectionMock(BorrowIdRedirection* memManager, const std::string key, std::string& value)
{
    value.clear();
    return 0;
}

TEST_F(TestMemBorrowExecutor, TestMemFreeWithOpsFailed_GetBorrowIdRedirectionNotEquals)
{
    MOCKER_CPP(&BorrowIdRedirection::Query,
               uint32_t(*)(BorrowIdRedirection * memManager, const std::string key, std::string& value))
        .stubs()
        .will(invoke(TestGetBorrowIdRedirectionMock));
    std::string name = "test";
    bool isForceDelete = false;
    bool smapBack = false;
    auto res = MemBorrowExecutor::Instance().MemFreeWithOps(name, isForceDelete, smapBack);
    ASSERT_EQ(MEM_POOLING_OK, res);
}

TEST_F(TestMemBorrowExecutor, TestMemFreeWithOpsFailed_RemoveBorrowIdRedirectionError)
{
    MOCKER_CPP(&BorrowIdRedirection::Query,
               uint32_t(*)(BorrowIdRedirection * memManager, const std::string key, std::string& value))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MemBorrowExecutor::RemoveBorrowIdRedirectionRecursively, uint32_t(*)(const std::string& name))
        .stubs()
        .will(returnValue(1));
    std::string name = "test";
    bool isForceDelete = false;
    bool smapBack = false;
    auto res = MemBorrowExecutor::Instance().MemFreeWithOps(name, isForceDelete, smapBack);
    ASSERT_NE(MEM_POOLING_OK, res);
}

TEST_F(TestMemBorrowExecutor, TestMemFreeWithOpsFailed_RackDeleteResourceError_Failed)
{
    MOCKER_CPP(&BorrowIdRedirection::Query,
               uint32_t(*)(BorrowIdRedirection * memManager, const std::string key, std::string& value))
        .stubs()
        .will(returnValue(0));
    std::string name = "mock";
    bool isForceDelete = false;
    bool smapBack = false;
    auto res = MemBorrowExecutor::Instance().MemFreeWithOps(name, isForceDelete, smapBack);
    ASSERT_EQ(MEM_POOLING_OK, res);
}

TEST_F(TestMemBorrowExecutor, TestMemFreeWithOpsFailed_RackDeleteResourceError_Running)
{
    MOCKER_CPP(&BorrowIdRedirection::Query,
               uint32_t(*)(BorrowIdRedirection * memManager, const std::string key, std::string& value))
        .stubs()
        .will(returnValue(0));
    std::string name = "run";
    bool isForceDelete = false;
    bool smapBack = false;
    auto res = MemBorrowExecutor::Instance().MemFreeWithOps(name, isForceDelete, smapBack);
    ASSERT_EQ(MEM_POOLING_OK, res);
}

TEST_F(TestMemBorrowExecutor, RemoveBorrowIdRedirectionRecursivelyFailed1)
{
    MOCKER_CPP(&BorrowIdRedirection::Query, uint32_t(*)(const std::string key, std::string& value))
        .stubs()
        .will(returnValue(1));
    std::string name = "test";
    auto ret = MemBorrowExecutor::Instance().RemoveBorrowIdRedirectionRecursively(name);
    ASSERT_NE(MEM_POOLING_OK, ret);
}

MpResult QueryMockEmpty(BorrowIdRedirection*, const std::string key, std::string& value)
{
    value = "";
    return 0;
}

MpResult QueryMockNoEmpty(BorrowIdRedirection*, const std::string key, std::string& value)
{
    value = " ";
    return 0;
}

TEST_F(TestMemBorrowExecutor, RemoveBorrowIdRedirectionRecursivelySuccess)
{
    MOCKER_CPP(&BorrowIdRedirection::Query, uint32_t(*)(const std::string key, std::string& value))
        .stubs()
        .will(invoke(QueryMockEmpty));
    MOCKER_CPP(&BorrowIdRedirection::Remove, uint32_t(*)(const std::string key, std::string& value))
        .stubs()
        .will(returnValue(0));
    std::string name = "test";
    auto ret = MemBorrowExecutor::Instance().RemoveBorrowIdRedirectionRecursively(name);
    ASSERT_EQ(0, ret);
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsBySmapFailed1)
{
    MOCKER_CPP(&MemBorrowExecutor::SmapMigreatBackRpc,
               uint32_t(*)(const std::string importNodeId, const MigrateBackMsg& migrateBackMsg))
        .stubs()
        .will(returnValue(1));

    std::string name = "test";
    std::string deleteName = "test";
    std::string deleteAttr = "test";
    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsBySmap(name, deleteName);
    ASSERT_NE(MEM_POOLING_OK, ret);
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsBySmapFailed2)
{
    MOCKER_CPP(&MemBorrowExecutor::SmapMigreatBackRpc,
               uint32_t(*)(const std::string importNodeId, const MigrateBackMsg& migrateBackMsg))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(returnValue(1));
    std::string name = "test";
    std::string deleteName = "test";
    std::string deleteAttr = "test";
    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsBySmap(name, deleteName);
    ASSERT_NE(MEM_POOLING_OK, ret); // CollectBorrowRecordsAll failed
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsBySmapFailed3)
{
    MOCKER_CPP(&MemBorrowExecutor::SmapMigreatBackRpc,
               uint32_t(*)(const std::string importNodeId, const MigrateBackMsg& migrateBackMsg))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(returnValue(0));
    std::string name = "test";
    std::string deleteName = "run";
    std::string deleteAttr = "test";
    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsBySmap(name, deleteName);
    ASSERT_NE(MEM_POOLING_OK, ret); // state: running
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsBySmapFailed4)
{
    MOCKER_CPP(&MemBorrowExecutor::SmapMigreatBackRpc,
               uint32_t(*)(const std::string importNodeId, const MigrateBackMsg& migrateBackMsg))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(returnValue(0));
    std::string name = "test";
    std::string deleteName = "test";
    std::string deleteAttr = "test";
    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsBySmap(name, deleteName);
    ASSERT_NE(MEM_POOLING_OK, ret);
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsByMemfabricSuccess)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(returnValue(0));
    std::string name = "test";
    std::string deleteName = "test";
    std::string deleteAttr = "test";
    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsByMemfabric(name, deleteName);
    ASSERT_EQ(0, ret);
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsByMemfabricFailed)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(returnValue(0));
    std::string name = "test";
    std::string deleteName = "run";
    std::string deleteAttr = "test";
    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsByMemfabric(name, deleteName);
    ASSERT_EQ(0, ret); // state: running
}

MpResult CollectBorrowRecordsAllMock1(BorrowRecordHelper* This, std::vector<BorrowRecord>& borrowRecords)
{
    std::string name{"test"};                 //  借用标识符
    uint64_t size{1024};                      //  借用内存大小，单位kB
    std::string lentNode{"1"};                //  借出节点
    std::vector<uint64_t> lentMemId{1};       //  借出内存id, 无法自采
    uint16_t lentSocketId{0};                 //  借出内存socketId
    std::vector<LentNuma> lentNuma{{0, 100}}; //  借出numa
    std::string borrowNode{"2"};              //  借入节点
    int16_t borrowLocalNuma{0};               //  借入numa, app 借用时有效，否则为-1
    int16_t borrowRemoteNuma{3};              //  借入numa, remote 借用时有效，否则为-1
    std::vector<uint64_t> borrowMemId{1};     //  借入memId

    BorrowRecord record = {name,     size,       lentNode,        lentMemId,        lentSocketId,
                           lentNuma, borrowNode, borrowLocalNuma, borrowRemoteNuma, borrowMemId};

    borrowRecords.emplace_back(record);

    return 0;
}

TEST_F(TestMemBorrowExecutor, GenerateSmapParamsSuccess)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecordsAll,
               MpResult(*)(BorrowRecordHelper * This, std::vector<BorrowRecord> & borrowRecords))
        .stubs()
        .will(invoke(CollectBorrowRecordsAllMock1));
    std::string name = "test";
    std::vector<MigrateBackMsg> migrateBackMsg;
    EnableNodeMsg enableMsg;
    std::string importNodeId;
    auto ret = MemBorrowExecutor::Instance().GenerateSmapParams(name, migrateBackMsg, enableMsg, importNodeId);
    ASSERT_EQ(0, ret); // state: running
}

TEST_F(TestMemBorrowExecutor, PrepareMemNumaCreateParams_1)
{
    RackCreateResourceWaterBorrowAttr attr;
    attr.size = 1024 * 1024 * 1024;
    attr.perfLevel = PerfLevel::L1;
    attr.highWatermark = 90;
    attr.lowWatermark = 10;
    attr.waterMallocAttr = MemMallocAttr{.srcNid = "1",       // 源节点ID
                                         .srcSocket = 0,      // 源节点Socket ID（-1表示无效）
                                         .srcNuma = 0,        // 源节点NUMA ID（-1表示无效）
                                         .uid = getuid(),     // 当前用户UID
                                         .username = "admin", // 用户名
                                         .dstNodeNum = 1,     // 从多个节点借用（0=单节点，1=多节点）
                                         .lenderNumaSize = 2, // 最多从2个NUMA借出
                                         .lenderLocs = {RackMemNumaLoc{.nodeId = "1", .socketId = 0, .numaId = 0},
                                                        RackMemNumaLoc{.nodeId = "1", .socketId = 0, .numaId = 1}},
                                         .lenderSizes = {
                                             // 对应位置的内存大小（字节）
                                             512 * 1024 * 1024, // 512MB
                                             256 * 1024 * 1024  // 256MB
                                         }};                    // 需填充具体属性

    UbseMemBorrower borrower;
    borrower.nodeId = "4";
    borrower.affinitySocketId = 0;
    borrower.uid = getuid();
    borrower.username = "admin";

    std::vector<UbseMemNumaLender> lenders = {{
                                                  .slotId = 1,
                                                  .socketId = 0,
                                                  .numaId = 0,
                                                  .size = 512 * 1024 * 1024 // 512MB
                                              },
                                              {
                                                  .slotId = 2,
                                                  .socketId = 1,
                                                  .numaId = 1,
                                                  .size = 512 * 1024 * 1024 // 512MB
                                              }};

    uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN] = {0};

    MpResult ret =
        mempooling::MemBorrowExecutor::Instance().PrepareMemNumaCreateParams("1", attr, borrower, lenders, usrInfo);

    EXPECT_EQ(ret, MEM_POOLING_OK); // state: running
}

TEST_F(TestMemBorrowExecutor, PrepareMemNumaCreateParams_2)
{
    RackCreateResourceWaterBorrowAttr attr;
    attr.size = 1024 * 1024 * 1024;
    attr.perfLevel = PerfLevel::L1;
    attr.highWatermark = 90;
    attr.lowWatermark = 10;
    attr.waterMallocAttr = MemMallocAttr{.srcNid = "node01",  // 源节点ID
                                         .srcSocket = 0,      // 源节点Socket ID（-1表示无效）
                                         .srcNuma = 0,        // 源节点NUMA ID（-1表示无效）
                                         .uid = getuid(),     // 当前用户UID
                                         .username = "admin", // 用户名
                                         .dstNodeNum = 1,     // 从多个节点借用（0=单节点，1=多节点）
                                         .lenderNumaSize = 2, // 最多从2个NUMA借出
                                         .lenderLocs = {RackMemNumaLoc{.nodeId = "1", .socketId = 0, .numaId = 0},
                                                        RackMemNumaLoc{.nodeId = "1", .socketId = 0, .numaId = 1}},
                                         .lenderSizes = {
                                             // 对应位置的内存大小（字节）
                                             512 * 1024 * 1024, // 512MB
                                             256 * 1024 * 1024  // 256MB
                                         }};                    // 需填充具体属性

    UbseMemBorrower borrower;
    borrower.nodeId = "node01";
    borrower.affinitySocketId = 0;
    borrower.uid = getuid();
    borrower.username = "admin";

    std::vector<UbseMemNumaLender> lenders = {{
                                                  .slotId = 1,
                                                  .socketId = 0,
                                                  .numaId = 0,
                                                  .size = 512 * 1024 * 1024 // 512MB
                                              },
                                              {
                                                  .slotId = 2,
                                                  .socketId = 1,
                                                  .numaId = 1,
                                                  .size = 512 * 1024 * 1024 // 512MB
                                              }};

    MpResult ret =
        mempooling::MemBorrowExecutor::Instance().PrepareMemNumaCreateParams("node01", attr, borrower, lenders, NULL);

    EXPECT_NE(ret, MEM_POOLING_OK); // state: running
}

MpResult BorrowIdsCompletedQueryMockOne(BorrowIdsCompleted*, std::vector<std::string>& list)
{
    list.push_back("borrow_001");
    return MEM_POOLING_OK;
}

MpResult BorrowIdsCompletedQueryMockTwo(BorrowIdsCompleted*, std::vector<std::string>& list)
{
    list.push_back("borrow_001");
    list.push_back("borrow_002");
    return MEM_POOLING_OK;
}

TEST_F(TestMemBorrowExecutor, MemBorrow_GenerateUniqueIdFailed)
{
    MOCKER_CPP(&MemBorrowExecutor::GenerateUniqueId,
               MpResult(*)(MemBorrowExecutor*, const std::string&, std::string&, const bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::string attachNode = "1";
    RackCreateResourceWaterBorrowAttr attr;
    attr.waterMallocAttr.lenderLocs = {RackMemNumaLoc{.nodeId = "2", .socketId = 0, .numaId = 0}};
    attr.waterMallocAttr.lenderSizes = {1024 * 1024};
    attr.waterMallocAttr.srcSocket = 0;
    attr.waterMallocAttr.srcNuma = 0;
    attr.waterMallocAttr.uid = getuid();
    attr.waterMallocAttr.username = "admin";
    std::string name;
    int16_t presentNumaId = 0;
    auto ret = MemBorrowExecutor::Instance().MemBorrow(attachNode, attr, name, presentNumaId);
    EXPECT_NE(ret, MEM_POOLING_OK);
}

TEST_F(TestMemBorrowExecutor, MemBorrow_PrepareMemNumaCreateParamsFailed)
{
    MOCKER_CPP(&MemBorrowExecutor::GenerateUniqueId,
               MpResult(*)(MemBorrowExecutor*, const std::string&, std::string&, const bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    std::string attachNode = "1";
    RackCreateResourceWaterBorrowAttr attr;
    attr.waterMallocAttr.lenderLocs = {RackMemNumaLoc{.nodeId = "2", .socketId = 0, .numaId = 0}};
    attr.waterMallocAttr.lenderSizes = {1024 * 1024, 2048 * 1024};
    attr.waterMallocAttr.srcSocket = 0;
    attr.waterMallocAttr.srcNuma = 0;
    attr.waterMallocAttr.uid = getuid();
    attr.waterMallocAttr.username = "admin";
    std::string name;
    int16_t presentNumaId = 0;
    auto ret = MemBorrowExecutor::Instance().MemBorrow(attachNode, attr, name, presentNumaId);
    EXPECT_NE(ret, MEM_POOLING_OK);
}

TEST_F(TestMemBorrowExecutor, MemBorrow_UbseMemNumaCreateFailed)
{
    MOCKER_CPP(&MemBorrowExecutor::GenerateUniqueId,
               MpResult(*)(MemBorrowExecutor*, const std::string&, std::string&, const bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowIdsCompleted::Update, MpResult(*)(const std::string)).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&UbseMemNumaCreateWithLender,
               UbseResult(*)(const std::string&, const UbseMemBorrower&, const std::vector<UbseMemNumaLender>&,
                             const uint8_t*, UbseMemNumaDesc&))
        .stubs()
        .will(returnValue(UBSE_ERR_INTERNAL));
    std::string attachNode = "1";
    RackCreateResourceWaterBorrowAttr attr;
    attr.waterMallocAttr.lenderLocs = {RackMemNumaLoc{.nodeId = "2", .socketId = 0, .numaId = 0}};
    attr.waterMallocAttr.lenderSizes = {1024 * 1024};
    attr.waterMallocAttr.srcSocket = 0;
    attr.waterMallocAttr.srcNuma = 0;
    attr.waterMallocAttr.uid = getuid();
    attr.waterMallocAttr.username = "admin";
    std::string name;
    int16_t presentNumaId = 0;
    auto ret = MemBorrowExecutor::Instance().MemBorrow(attachNode, attr, name, presentNumaId);
    EXPECT_NE(ret, MEM_POOLING_OK);
}

TEST_F(TestMemBorrowExecutor, DeleteFailedBorrowIds_QueryFailed)
{
    MOCKER_CPP(&BorrowIdsCompleted::Query, MpResult(*)(std::vector<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpMemBorrowExecutorModule module;
    auto ret = module.DeleteFailedBorrowIds();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemBorrowExecutor, DeleteFailedBorrowIds_EmptyList)
{
    MOCKER_CPP(&BorrowIdsCompleted::Query, MpResult(*)(std::vector<std::string>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpMemBorrowExecutorModule module;
    auto ret = module.DeleteFailedBorrowIds();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemBorrowExecutor, DeleteFailedBorrowIds_MemFreeFailed)
{
    MOCKER_CPP(&BorrowIdsCompleted::Query, MpResult(*)(std::vector<std::string>&))
        .stubs()
        .will(invoke(BorrowIdsCompletedQueryMockOne));
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
               MpResult(*)(MemBorrowExecutor*, const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpMemBorrowExecutorModule module;
    auto ret = module.DeleteFailedBorrowIds();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemBorrowExecutor, DeleteFailedBorrowIds_RemoveFailed)
{
    MOCKER_CPP(&BorrowIdsCompleted::Query, MpResult(*)(std::vector<std::string>&))
        .stubs()
        .will(invoke(BorrowIdsCompletedQueryMockOne));
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
               MpResult(*)(MemBorrowExecutor*, const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowIdsCompleted::Remove, MpResult(*)(const std::string))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MpMemBorrowExecutorModule module;
    auto ret = module.DeleteFailedBorrowIds();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemBorrowExecutor, DeleteFailedBorrowIds_Success)
{
    MOCKER_CPP(&BorrowIdsCompleted::Query, MpResult(*)(std::vector<std::string>&))
        .stubs()
        .will(invoke(BorrowIdsCompletedQueryMockTwo));
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOps,
               MpResult(*)(MemBorrowExecutor*, const std::string&, bool, bool, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&BorrowIdsCompleted::Remove, MpResult(*)(const std::string)).stubs().will(returnValue(MEM_POOLING_OK));
    MpMemBorrowExecutorModule module;
    auto ret = module.DeleteFailedBorrowIds();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

static int g_smapEnableNumaProcessCallCount = 0;

using DebtQueryByNameResult = mempooling::DebtQueryResult;

MpResult MockGenerateSmapParamsForProcessMem(MemBorrowExecutor* This, const UbseNumaMemoryDebtInfo& matchedDebtInfo,
                                             std::vector<MigrateBackMsg>& migrateBackMsgs, EnableNodeMsg& enableMsg)
{
    (void)This;
    (void)matchedDebtInfo;
    MigrateBackMsg msg;
    msg.count = 1;
    msg.payload[0].srcNid = 1;
    msg.payload[0].destNid = 2;
    msg.payload[0].memid = 100;
    msg.taskID = 1;
    migrateBackMsgs.push_back(msg);

    enableMsg.nid = 1;
    enableMsg.enable = 1;

    return MEM_POOLING_OK;
}

uint32_t MockSmapEnableNumaProcess(EnableNodeMsg msg)
{
    g_smapEnableNumaProcessCallCount++;
    return MEM_POOLING_OK;
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsBySmapForProcessMem_MemfabricFail_EnableNumaProcess)
{
    g_smapEnableNumaProcessCallCount = 0;

    MOCKER_CPP(&MemBorrowExecutor::GetDebtInfoByNameWithRetry,
               DebtQueryByNameResult(*)(const std::string&, const std::string&, std::vector<UbseNumaMemoryDebtInfo>&,
                                        UbseNumaMemoryDebtInfo&))
        .stubs()
        .will(returnValue(DebtQueryByNameResult{MEM_POOLING_OK, MEM_POOLING_ERROR}));

    MOCKER_CPP(
        &MemBorrowExecutor::GenerateSmapParamsForProcessMem,
        MpResult(*)(MemBorrowExecutor*, const UbseNumaMemoryDebtInfo&, std::vector<MigrateBackMsg>&, EnableNodeMsg&))
        .stubs()
        .will(invoke(MockGenerateSmapParamsForProcessMem));

    MOCKER_CPP(&MemBorrowExecutor::UpdateSmapRemoteNumaInfoBeforeMigrateBack,
               MpResult(*)(const UbseNumaMemoryDebtInfo&, const std::vector<UbseNumaMemoryDebtInfo>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(SmapMigrateBackProcess, uint32_t(*)(MigrateBackMsg)).stubs().will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MpSmapHelper::GetLocalSmapBackResult, MpResult(*)(uint64_t)).stubs().will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOpsByMemfabric, MpResult(*)(const std::string&, const std::string&, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    MOCKER_CPP(SmapEnableNumaProcess, uint32_t(*)(EnableNodeMsg)).stubs().will(invoke(MockSmapEnableNumaProcess));

    MOCKER_CPP(UbseStoragePutData, uint32_t(*)(const std::string&, const std::string&, UbseByteBuffer*))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    MOCKER_CPP(UbseStorageQueryData,
               uint32_t(*)(const std::string&, const std::string&, void*, UbseStorageDealDataFunc))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    UbseNumaMemoryDebtInfo matchedDebtInfo;
    matchedDebtInfo.name = "test";
    matchedDebtInfo.remoteNumaId = 1;
    std::vector<UbseNumaMemoryDebtInfo> debtInfos{matchedDebtInfo};
    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsBySmapForProcessMem(matchedDebtInfo, debtInfos, true);
    EXPECT_NE(ret, MEM_POOLING_OK);
    EXPECT_EQ(g_smapEnableNumaProcessCallCount, 1);
}

// ==================== MemFreeWithOpsForProcessMem smap分支新增检查 ====================

/*
 * 用例描述：账本中该borrowId已存在remoteNumaId<0条目（导入方已释放）
 * 预期：跳过本轮释放，返回MEM_POOLING_OK，不进入smap释放流程
 */
TEST_F(TestMemBorrowExecutor, MemFreeWithOpsForProcessMem_Released_SkipFreeReturnOk)
{
    MOCKER_CPP(&MemBorrowExecutor::GetDebtInfoByNameWithRetry,
               DebtQueryByNameResult(*)(const std::string&, const std::string&, std::vector<UbseNumaMemoryDebtInfo>&,
                                        UbseNumaMemoryDebtInfo&))
        .stubs()
        .will(returnValue(DebtQueryByNameResult{MEM_POOLING_ERROR, MEM_POOLING_OK}));
    // 反向证明：若流程误入smap释放将返回ERROR，使断言失败
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOpsBySmapForProcessMem,
               MpResult(*)(MemBorrowExecutor*, const UbseNumaMemoryDebtInfo&,
                           const std::vector<UbseNumaMemoryDebtInfo>&, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsForProcessMem("test-released", true, false);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

/*
 * 用例描述：账本查询失败/无有效条目（重试耗尽仍未找到）
 * 预期：返回MEM_POOLING_ERROR，不进入smap释放流程
 */
TEST_F(TestMemBorrowExecutor, MemFreeWithOpsForProcessMem_DebtQueryFail_ReturnError)
{
    MOCKER_CPP(&MemBorrowExecutor::GetDebtInfoByNameWithRetry,
               DebtQueryByNameResult(*)(const std::string&, const std::string&, std::vector<UbseNumaMemoryDebtInfo>&,
                                        UbseNumaMemoryDebtInfo&))
        .stubs()
        .will(returnValue(DebtQueryByNameResult{MEM_POOLING_ERROR, MEM_POOLING_ERROR}));
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOpsBySmapForProcessMem,
               MpResult(*)(MemBorrowExecutor*, const UbseNumaMemoryDebtInfo&,
                           const std::vector<UbseNumaMemoryDebtInfo>&, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsForProcessMem("test-notfound", true, false);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

/*
 * 用例描述：远端NUMA本机锁被占用（TryAcquireSelf返回故障码）
 * 预期：返回锁错误码MEM_POOLING_HANDLING_FAULT，不进入smap释放流程
 */
TEST_F(TestMemBorrowExecutor, MemFreeWithOpsForProcessMem_FaultNumaLockFail_ReturnLockRet)
{
    MOCKER_CPP(&MemBorrowExecutor::GetDebtInfoByNameWithRetry,
               DebtQueryByNameResult(*)(const std::string&, const std::string&, std::vector<UbseNumaMemoryDebtInfo>&,
                                        UbseNumaMemoryDebtInfo&))
        .stubs()
        .will(returnValue(DebtQueryByNameResult{MEM_POOLING_OK, MEM_POOLING_ERROR}));
    MOCKER_CPP(&FaultNumaLock::TryAcquireSelf, MpResult(*)(FaultNumaLock*, uint16_t))
        .stubs()
        .will(returnValue(MEM_POOLING_HANDLING_FAULT));
    MOCKER_CPP(&MemBorrowExecutor::MemFreeWithOpsBySmapForProcessMem,
               MpResult(*)(MemBorrowExecutor*, const UbseNumaMemoryDebtInfo&,
                           const std::vector<UbseNumaMemoryDebtInfo>&, bool))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsForProcessMem("test-lockfail", true, false);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, MEM_POOLING_HANDLING_FAULT);
}

} // namespace mempooling