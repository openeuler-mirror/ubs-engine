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

#include "iostream"
#include "mem_borrow_executor.cpp"
#include "mem_json_def.h"
#include "mem_manager.h"
#include "mp_error.h"
#include "ubse_node.h"
#include "mempool_migrate_module.h"

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
    MOCKER_CPP(&BorrowIdRedirection::Query, uint32_t(*)(const std::string key, std::string &value))
        .stubs()
        .will(returnValue(1));
    std::string name = "test";
    bool isForceDelete = false;
    bool smapBack = false;
    auto res = MemBorrowExecutor::Instance().MemFreeWithOps(name, isForceDelete, smapBack);
    ASSERT_EQ(1, res);
}

uint32_t TestGetBorrowIdRedirectionMock(BorrowIdRedirection *memManager, const std::string key, std::string &value)
{
    if (value == "test") {
        value = "t";
    } else {
        value.clear();
    }
    return 0;
}

uint32_t TestGetNullBorrowIdRedirectionMock(BorrowIdRedirection *memManager, const std::string key, std::string &value)
{
    value.clear();
    return 0;
}

TEST_F(TestMemBorrowExecutor, TestMemFreeWithOpsFailed_GetBorrowIdRedirectionNotEquals)
{
    MOCKER_CPP(&BorrowIdRedirection::Query,
               uint32_t(*)(BorrowIdRedirection * memManager, const std::string key, std::string &value))
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
               uint32_t(*)(BorrowIdRedirection * memManager, const std::string key, std::string &value))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MemBorrowExecutor::RemoveBorrowIdRedirectionRecursively, uint32_t(*)(const std::string &name))
        .stubs()
        .will(returnValue(1));
    std::string name = "test";
    bool isForceDelete = false;
    bool smapBack = false;
    auto res = MemBorrowExecutor::Instance().MemFreeWithOps(name, isForceDelete, smapBack);
    ASSERT_EQ(1, res);
}

TEST_F(TestMemBorrowExecutor, TestMemFreeWithOpsFailed_RackDeleteResourceError_Failed)
{
    MOCKER_CPP(&BorrowIdRedirection::Query,
               uint32_t(*)(BorrowIdRedirection * memManager, const std::string key, std::string &value))
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
               uint32_t(*)(BorrowIdRedirection * memManager, const std::string key, std::string &value))
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
    MOCKER_CPP(&BorrowIdRedirection::Query, uint32_t(*)(const std::string key, std::string &value))
        .stubs()
        .will(returnValue(1));
    std::string name = "test";
    auto ret = MemBorrowExecutor::Instance().RemoveBorrowIdRedirectionRecursively(name);
    ASSERT_EQ(1, ret);
}

MpResult QueryMockEmpty(BorrowIdRedirection *, const std::string key, std::string &value)
{
    value = "";
    return 0;
}

MpResult QueryMockNoEmpty(BorrowIdRedirection *, const std::string key, std::string &value)
{
    value = " ";
    return 0;
}

TEST_F(TestMemBorrowExecutor, RemoveBorrowIdRedirectionRecursivelySuccess)
{
    MOCKER_CPP(&BorrowIdRedirection::Query, uint32_t(*)(const std::string key, std::string &value))
        .stubs()
        .will(invoke(QueryMockEmpty));
    MOCKER_CPP(&BorrowIdRedirection::Remove, uint32_t(*)(const std::string key, std::string &value))
        .stubs()
        .will(returnValue(0));
    std::string name = "test";
    auto ret = MemBorrowExecutor::Instance().RemoveBorrowIdRedirectionRecursively(name);
    ASSERT_EQ(0, ret);
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsBySmapFailed1)
{
    MOCKER_CPP(&MemBorrowExecutor::SmapMigreatBackRpc,
               uint32_t(*)(const std::string importNodeId, const MigrateBackMsg &migrateBackMsg))
        .stubs()
        .will(returnValue(1));

    std::string name = "test";
    std::string deleteName = "test";
    std::string deleteAttr = "test";
    auto ret = MemBorrowExecutor::Instance().MemFreeWithOpsBySmap(name, deleteName);
    ASSERT_EQ(1, ret);
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsBySmapFailed2)
{
    MOCKER_CPP(&MemBorrowExecutor::SmapMigreatBackRpc,
               uint32_t(*)(const std::string importNodeId, const MigrateBackMsg &migrateBackMsg))
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
    ASSERT_EQ(1, ret); // CollectBorrowRecordsAll failed
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsBySmapFailed3)
{
    MOCKER_CPP(&MemBorrowExecutor::SmapMigreatBackRpc,
               uint32_t(*)(const std::string importNodeId, const MigrateBackMsg &migrateBackMsg))
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
    ASSERT_EQ(1, ret); // state: running
}

TEST_F(TestMemBorrowExecutor, MemFreeWithOpsBySmapFailed4)
{
    MOCKER_CPP(&MemBorrowExecutor::SmapMigreatBackRpc,
               uint32_t(*)(const std::string importNodeId, const MigrateBackMsg &migrateBackMsg))
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
    ASSERT_EQ(1, ret);
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

MpResult CollectBorrowRecordsAllMock1(BorrowRecordHelper *This, std::vector<BorrowRecord> &borrowRecords)
{
    std::string name{"test"};                       //  借用标识符
    uint64_t size{1024};                         //  借用内存大小，单位kB
    std::string lentNode{"1"};                   //  借出节点
    std::vector<uint64_t> lentMemId{1};        //  借出内存id, 无法自采
    uint16_t lentSocketId{0};                 //  借出内存socketId
    std::vector<LentNuma> lentNuma{{0, 100}};         //  借出numa
    std::string borrowNode{"2"};                 //  借入节点
    int16_t borrowLocalNuma{0};               //  借入numa, app 借用时有效，否则为-1
    int16_t borrowRemoteNuma{3};              //  借入numa, remote 借用时有效，否则为-1
    std::vector<uint64_t> borrowMemId{1};        //  借入memId

    BorrowRecord record = {
        name,
        size,
        lentNode,
        lentMemId,
        lentSocketId,
        lentNuma,
        borrowNode,
        borrowLocalNuma,
        borrowRemoteNuma,
        borrowMemId
    };

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
    MigrateBackMsg migrateBackMsg;
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
    attr.waterMallocAttr = MemMallocAttr{
        .srcNid = "1",                   // 源节点ID
        .srcSocket = 0,                       // 源节点Socket ID（-1表示无效）
        .srcNuma = 0,                         // 源节点NUMA ID（-1表示无效）
        .uid = getuid(),                      // 当前用户UID
        .username = "admin",                  // 用户名
        .dstNodeNum = 1,                      // 从多个节点借用（0=单节点，1=多节点）
        .lenderNumaSize = 2,                  // 最多从2个NUMA借出
        .lenderLocs = {
            RackMemNumaLoc{.nodeId = "1", .socketId = 0, .numaId = 0},
            RackMemNumaLoc{.nodeId = "1", .socketId = 0, .numaId = 1}
        },
        .lenderSizes = {                     // 对应位置的内存大小（字节）
            512 * 1024 * 1024,                // 512MB
            256 * 1024 * 1024                 // 256MB
        }
    }; // 需填充具体属性

    UbseMemBorrower borrower;
    borrower.nodeId = "4";
    borrower.affinitySocketId = 0;
    borrower.uid = getuid();
    borrower.username = "admin";

    std::vector<UbseMemNumaLender> lenders = {
        {
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
        }
    };

    uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN] = {0};

    MpResult ret = mempooling::MemBorrowExecutor::Instance().PrepareMemNumaCreateParams(
        "1", attr, borrower, lenders, usrInfo);

    EXPECT_EQ(ret, MEM_POOLING_OK); // state: running
}

TEST_F(TestMemBorrowExecutor, PrepareMemNumaCreateParams_2)
{
    RackCreateResourceWaterBorrowAttr attr;
    attr.size = 1024 * 1024 * 1024;
    attr.perfLevel = PerfLevel::L1;
    attr.highWatermark = 90;
    attr.lowWatermark = 10;
    attr.waterMallocAttr = MemMallocAttr{
        .srcNid = "node01",                   // 源节点ID
        .srcSocket = 0,                       // 源节点Socket ID（-1表示无效）
        .srcNuma = 0,                         // 源节点NUMA ID（-1表示无效）
        .uid = getuid(),                      // 当前用户UID
        .username = "admin",                  // 用户名
        .dstNodeNum = 1,                      // 从多个节点借用（0=单节点，1=多节点）
        .lenderNumaSize = 2,                  // 最多从2个NUMA借出
        .lenderLocs = {
            RackMemNumaLoc{.nodeId = "1", .socketId = 0, .numaId = 0},
            RackMemNumaLoc{.nodeId = "1", .socketId = 0, .numaId = 1}
        },
        .lenderSizes = {                     // 对应位置的内存大小（字节）
            512 * 1024 * 1024,                // 512MB
            256 * 1024 * 1024                 // 256MB
        }
    }; // 需填充具体属性

    UbseMemBorrower borrower;
    borrower.nodeId = "node01";
    borrower.affinitySocketId = 0;
    borrower.uid = getuid();
    borrower.username = "admin";

    std::vector<UbseMemNumaLender> lenders = {
        {
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
        }
    };

    MpResult ret = mempooling::MemBorrowExecutor::Instance().PrepareMemNumaCreateParams(
        "node01", attr, borrower, lenders, NULL);

    EXPECT_EQ(ret, MEM_POOLING_ERROR); // state: running
}

} // namespace mempooling