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
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include "exporter.h"

#include "ubse_storage.h"
#include "mem_manager.h"
#include "mp_configuration.h"
#include "mp_mem_json_util.h"
#include "mp_sync_data_helper.h"
#include "numa_info.h"
#include "securec.h"
#include "rmrs_serialize.h"
#undef private
using namespace std;
using namespace mempooling;
using namespace mockcpp;
using namespace ubse::storage;
using namespace mempooling::sync;
using namespace rmrs::serialize;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

class TestMemManager : public ::testing::Test {
    void SetUp() override
    {
        cout << "[TestMemManager SetUp Begin]" << endl;
        cout << "[TestMemManager SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestMemManager TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestMemManager TearDown End]" << endl;
    }

public:
    UbseByteBuffer *CreateMockUbseByteBuffer(uint32_t len)
    {
        UbseByteBuffer *buffer = new UbseByteBuffer();
        buffer->len = len;
        buffer->data = new uint8_t[len];
        return buffer;
    }
};

TEST_F(TestMemManager, MemManagerInitSucceed)
{
    // Mock测试
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    MOCKER_CPP(&mempooling::BorrowRecordHelper::UpdateBorrowRecords,
        MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    BorrowRecord record;
    record.name = "borrow_task_001";
    record.size = 40960; // 40MB
    record.lentNode = "nodeA";
    record.lentMemId = {1001, 1002, 1003};
    record.lentSocketId = 1;
    record.borrowNode = "nodeB";
    record.borrowLocalNuma = 0;
    record.borrowRemoteNuma = -1;
    record.borrowMemId = {2001, 2002, 2003};
    obj.gBorrowRecords.push_back(record);
    MpResult res = obj.Init();
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MemManagerInitFailed)
{
    // Mock测试
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    MpResult res = obj.Init();
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestMemManager, GetNodeMemInfoFailed)
{
    MOCKER_CPP(&MemManager::InitBorrowableInfo, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    NodeMemInfo nodeMemInfo;
    MemManager::Instance().nodeMemMap.insert({{"node1", nodeMemInfo}});
    NodeMemInfo outInfo;
    MpResult res = MemManager::Instance().GetNodeMemInfo("node0", outInfo);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, GetNodeMemInfoSuccess)
{
    MOCKER_CPP(&MemManager::InitBorrowableInfo, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    NodeMemInfo nodeMemInfo;
    MemManager::Instance().nodeMemMap.insert({{"node1", nodeMemInfo}});
    NodeMemInfo outInfo;
    MpResult res = MemManager::Instance().GetNodeMemInfo("node1", outInfo);
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestMemManager, GetAntiNodeSucceed1)
{
    std::string srcNid = "Node0";
    std::vector<std::string> antiNodeMemVec = {"Node0"};
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    MpResult res = obj.Query(srcNid, antiNodeMemVec);
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestMemManager, GetAntiNodeSucceed2)
{
    std::string srcNid = "";
    std::vector<std::string> antiNodeMemVec = {""};
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    MpResult res = obj.Query(srcNid, antiNodeMemVec);
    EXPECT_EQ(res, MEM_POOLING_OK);
}

uint32_t RackStorageQueryDataForTest(const std::string &keyPrefix, const std::string &key, void *ctx,
                                     UbseStorageDealDataFunc func)
{
    UbseByteBuffer buff;
    func(keyPrefix, "1", buff, ctx);
    return 0;
}

TEST_F(TestMemManager, Name2VmInfoUpdate)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(0));
    Name2VmInfo obj;
    std::map<std::string, std::set<BorrowIdInfo>> value;
    std::string nodeId("1");
    MpResult ret = obj.Update(nodeId, value);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, Name2VmInfoUpdateDeleteOperation)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(0));
    Name2VmInfo obj;
    std::map<std::string, std::set<BorrowIdInfo>> value;
    value["a"].clear();
    std::string nodeId("1");
    MpResult ret = obj.Update(nodeId, value);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, Name2VmInfoGet)
{
    Name2VmInfo obj;
    std::map<std::string, std::set<BorrowIdInfo>> value;
    std::string nodeId("1");
    MpResult ret = obj.Query(nodeId, value);
    EXPECT_EQ(ret, 0);
}

UbseByteBuffer ctx{};
MpResult TestRackStorageQueryData(const std::string &keyPrefix, const std::string &key, void *ctx,
                                  UbseStorageDealDataFunc func)
{
    // 创建一个UbseByteBuffer对象作为ctx
    UbseByteBuffer &ctxBuffer = *(static_cast<UbseByteBuffer *>(ctx));
    ctxBuffer.len = 1;
    ctxBuffer.data = new uint8_t[ctxBuffer.len];
    return 0;
}

TEST_F(TestMemManager, GetAntiNodeSucceed3)
{
    std::string srcNid = "Node0";
    std::vector<std::string> antiNodeMemVec = {"Node0"};
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(TestRackStorageQueryData));
    MOCKER_CPP(&AntiDataReload, uint32_t(*)())
        .stubs()
        .will(returnValue(0));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    MpResult res = obj.Query(srcNid, antiNodeMemVec);
    EXPECT_EQ(res, MEM_POOLING_OK);
}

std::string borrowStr = R"({"borrows":[{"borrowLocalNuma":-1,"borrowMemId":[278],"borrowNode":"Node0",
"borrowRemoteNuma":5,"lentMemId":[265],"lentSocketId":0,"lentNode":"Node1","lentNuma":[{"lentSize":1073741824,
"numaId":1}],"name":"5c1275920d9ba8eb01efb249a3ee477d","obmmDescHccs":[{"addr":79509374107648,"belonging":0,
"size":134217728},{"addr":79509508325376,"belonging":0,"size":134217728},{"addr":79509642543104,"belonging":0,
"size":134217728},{"addr":79509776760832,"belonging":0,"size":134217728},{"addr":79509910978560,"belonging":0,
"size":134217728},{"addr":79510045196288,"belonging":0,"size":134217728},{"addr":79510179414016,"belonging":0,
"size":134217728},{"addr":79510313631744,"belonging":0,"size":134217728}],"size":1073741824}]})";

std::string borrowStrFake = R"({"borrows":[{"borrowLocalNuma":2,"borrowMemId":[278],"borrowNode":"Node0",
"borrowRemoteNuma":-1,"lentMemId":[265],"lentSocketId":0,"lentNode":"Node1","lentNuma":[{"lentSize":1073741824,
"numaId":1}],"name":"5c1275920d9ba8eb01efb249a3ee477d","obmmDescHccs":[{"addr":79509374107648,"belonging":0,
"size":134217728},{"addr":79509508325376,"belonging":0,"size":134217728},{"addr":79509642543104,"belonging":0,
"size":134217728},{"addr":79509776760832,"belonging":0,"size":134217728},{"addr":79509910978560,"belonging":0,
"size":134217728},{"addr":79510045196288,"belonging":0,"size":134217728},{"addr":79510179414016,"belonging":0,
"size":134217728},{"addr":79510313631744,"belonging":0,"size":134217728}],"size":1073741824}]})";

std::string borrowStrAddrInvalid = R"({"borrows":[{"borrowLocalNuma":2,"borrowMemId":[278],"borrowNode":"Node0",
"borrowRemoteNuma":-1,"lentMemId":[265],"lentSocketId":0,"lentNode":"Node1","lentNuma":[{"lentSize":1073741824,
"numaId":1}],"name":"5c1275920d9ba8eb01efb249a3ee477d","obmmDescHccs":[{"addr":-1,"belonging":0,"size":134217728},{
"addr":0,"belonging":0,"size":134217728},{"addr":-1,"belonging":0,"size":134217728},{"addr":-1,"belonging":0,
"size":134217728},{"addr":79509910978560,"belonging":0,"size":134217728},{"addr":79510045196288,"belonging":0,
"size":134217728},{"addr":79510179414016,"belonging":0,"size":134217728},{"addr":79510313631744,"belonging":0,
"size":134217728}],"size":1073741824}]})";

uint32_t RackStorageQueryDataForGetAntiNodeCompleted(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                     UbseStorageDealDataFunc func)
{
    MpUpdateAntiNodeParam param;
    param.nodeAntiAffinityMap["Node0"].emplace_back("Node1");
    std::string jsonStr = param.ToJson();

    UbseByteBuffer &buffer = *(static_cast<UbseByteBuffer *>(ctx));
    buffer.len = jsonStr.length();
    buffer.data = new uint8_t[buffer.len];
    memcpy_s(buffer.data, buffer.len, jsonStr.c_str(), buffer.len);
    return 0;
}

TEST_F(TestMemManager, GetAntiNodeEmpty)
{
    MOCKER_CPP(UbseStorageQueryData,
               MpResult(*)(const std::string &, const std::string &, void *ctx, UbseStorageDealDataFunc))
        .stubs()
        .will(returnValue(0));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    std::vector<std::string> vec;
    auto ret = obj.Query("", vec);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, GetAntiNodeFailed1)
{
    MOCKER_CPP(UbseStorageQueryData,
               MpResult(*)(const std::string &, const std::string &, void *ctx, UbseStorageDealDataFunc))
        .stubs()
        .will(returnValue(1));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    std::vector<std::string> vec;
    auto ret = obj.Query("", vec);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, GetAntiNodeFailed2)
{
    MOCKER_CPP(UbseStorageQueryData,
               MpResult(*)(const std::string &, const std::string &, void *ctx, UbseStorageDealDataFunc))
        .stubs()
        .will(invoke(RackStorageQueryDataForGetAntiNodeCompleted));
    MOCKER_CPP(&MpUpdateAntiNodeParam::FromJson, bool (*)(const std::string &)).stubs().will(returnValue(false));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    std::vector<std::string> vec;
    auto ret = obj.Query("Node0", vec);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, GetAntiNodeCompleted1)
{
    MOCKER_CPP(UbseStorageQueryData,
               MpResult(*)(const std::string &, const std::string &, void *ctx, UbseStorageDealDataFunc))
        .stubs()
        .will(invoke(RackStorageQueryDataForGetAntiNodeCompleted));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    std::vector<std::string> vec;
    auto ret = obj.Query("Node0", vec);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, GetAntiNodeCompleted2)
{
    MOCKER_CPP(UbseStorageQueryData,
               MpResult(*)(const std::string &, const std::string &, void *ctx, UbseStorageDealDataFunc))
        .stubs()
        .will(invoke(RackStorageQueryDataForGetAntiNodeCompleted));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    std::vector<std::string> vec;
    auto ret = obj.Query("Node1", vec);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, UpdateAntiNodeFailed2)
{
    MOCKER_CPP(UbseStoragePutData, uint32_t(*)(const std::string &, const std::string &, UbseByteBuffer *))
        .stubs()
        .will(returnValue(1));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    std::map<std::string, std::vector<std::string>> antiNodeMap;
    auto ret = obj.Update(antiNodeMap);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, GetAntiNodeRawDataSucceed)
{
    MOCKER_CPP(UbseStorageQueryData,
               MpResult(*)(const std::string &, const std::string &, void *ctx, UbseStorageDealDataFunc))
        .stubs()
        .will(invoke(RackStorageQueryDataForGetAntiNodeCompleted));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, GetAntiNodeRawDataFailed1)
{
    MOCKER_CPP(UbseStorageQueryData,
               MpResult(*)(const std::string &, const std::string &, void *ctx, UbseStorageDealDataFunc))
        .stubs()
        .will(returnValue(1));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, PutAntiNodeRawDataSucceed)
{
    MOCKER_CPP(UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(0));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, PutAntiNodeRawDataFailed1)
{
    MOCKER_CPP(UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(1));
    mempooling::AntiNode &obj = mempooling::AntiNode::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, UpdateBorrowIdRedirectionSucceed)
{
    MOCKER_CPP(UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(0));
    mempooling::BorrowIdRedirection &obj = mempooling::BorrowIdRedirection::Instance();
    auto ret = obj.Update("ab", "cd");
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, UpdateBorrowIdRedirectionFailed1)
{
    MOCKER_CPP(UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(1));
    mempooling::BorrowIdRedirection &obj = mempooling::BorrowIdRedirection::Instance();
    auto ret = obj.Update("ab", "cd");
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, GetBorrowIdRedirectionSucceed)
{
    mempooling::BorrowIdRedirection &obj = mempooling::BorrowIdRedirection::Instance();
    std::string key;
    std::string value;
    auto ret = obj.Query(key, value);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, PutBorrowIdRedirectionRawDataSucceed)
{
    MOCKER_CPP(UbseStoragePutData, MpResult(*)(const std::string &, const std::string &, UbseByteBuffer *))
        .stubs()
        .will(returnValue(0));
    mempooling::BorrowIdRedirection &obj = mempooling::BorrowIdRedirection::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer);
    ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, PutBorrowIdRedirectionRawDataFailed1)
{
    MOCKER_CPP(UbseStoragePutData, MpResult(*)(const std::string &, const std::string &, UbseByteBuffer *))
        .stubs()
        .will(returnValue(1));
    mempooling::BorrowIdRedirection &obj = mempooling::BorrowIdRedirection::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer);
    ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, GetBorrowIdRedirectionRawDataSucceed)
{
    mempooling::BorrowIdRedirection &obj = mempooling::BorrowIdRedirection::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, RemoveBorrowIdRedirection_succeed)
{
    mempooling::BorrowIdRedirection &obj = mempooling::BorrowIdRedirection::Instance();

    auto ret = obj.Remove("ab");
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, CollectBorrowRecordsSucceed)
{
    MOCKER_CPP(&BorrowRecordHelper::UpdateBorrowRecords, MpResult(*)()).stubs().will(returnValue(0));
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    std::vector<mempooling::BorrowRecord> vec;
    auto ret = obj.CollectBorrowRecords("ab", vec);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, GetBorrowIdByNumaIdSucceed)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords, MpResult(*)(const std::string, std::vector<BorrowRecord> &))
        .stubs()
        .will(returnValue(0));
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    std::vector<std::string> vec;
    uint16_t numaId = 0;
    std::string nodeId = "Node0";
    auto ret = obj.GetBorrowIdByNumaId(vec, numaId, nodeId);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, GetBorrowIdByNumaIdFailed)
{
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    std::vector<std::string> vec;
    uint16_t numaId = 0;
    std::string nodeId = "Node0";
    auto ret = obj.GetBorrowIdByNumaId(vec, numaId, nodeId);
    EXPECT_EQ(ret, 0);
}

MpResult CollectBorrowRecordsMock(BorrowRecordHelper *This, const std::string nodeId,
    std::vector<BorrowRecord> &borrowRecords)
{
    const int mockSize = 134217728;
    const int mockAddr = 79474074845184;
    const int mockRemoteNuma = 5;
    std::vector<BorrowRecord> tmpBorrowRecords;
    BorrowRecord tmp;
    LentNuma tmpLentNuma;
    tmpLentNuma.lentSize = mockSize;
    tmpLentNuma.numaId = 0;
    tmp.size = mockSize;
    tmp.name = "0815cc4efd79fb1a52ccde543b62e624";
    tmp.lentNode = "Node1";
    tmp.lentSocketId = 0;
    tmp.lentMemId.push_back(0);
    tmp.lentNuma.push_back(tmpLentNuma);
    tmp.borrowRemoteNuma = mockRemoteNuma;
    tmp.borrowNode = "Node0";
    tmp.borrowLocalNuma = 0;
    tmp.borrowMemId.push_back(0);
    tmpBorrowRecords.push_back(tmp);
    borrowRecords = std::move(tmpBorrowRecords);
    return MEM_POOLING_OK;
}

TEST_F(TestMemManager, Name2VmInfoGetName2VmInfoRawDataSucceed)
{
    std::vector<std::string> vec;
    uint16_t numaId = 0;
    mempooling::Name2VmInfo obj;
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, Name2VmInfoPutName2VmInfoRawDataSucceed)
{
    MOCKER_CPP(UbseStoragePutData, MpResult(*)(const std::string &, const std::string &, UbseByteBuffer *))
        .stubs()
        .will(returnValue(0));
    std::vector<std::string> vec;
    mempooling::Name2VmInfo obj;
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer);
    ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, UpdateNodeMemMapSucceed)
{
    std::unordered_map<std::string, NodeMemoryInfoWithReservedMem> srcMap;

    NodeMemoryInfoWithReservedMem nodeMemInfo;
    nodeMemInfo.timestamp = 1728135094; // 任意时间戳
    nodeMemInfo.nodeId = "nodeA";
    nodeMemInfo.totalMemory = 1024 * 1024;  // 1GB
    nodeMemInfo.usedMemory = 512 * 1024;    // 512MB
    nodeMemInfo.freeMemory = 512 * 1024;    // 512MB
    nodeMemInfo.lentMemory = 128 * 1024;    // 128MB
    nodeMemInfo.reservedMem = 256 * 1024;   // 256MB
    nodeMemInfo.sharedMem = 32 * 1024;      // 32MB

    RackNumaMemInfo numa0;
    numa0.numaId = 0;
    numa0.socketId = 0;
    numa0.memTotal = 512 * 1024;
    numa0.memUsed = 256 * 1024;
    numa0.memFree = 256 * 1024;
    numa0.vmMemTotal = 64 * 1024;
    numa0.vmMemFree = 32 * 1024;
    numa0.vmUsedMem = 32 * 1024;
    numa0.reservedMem = 128 * 1024;
    numa0.lentMem = 64 * 1024;
    numa0.sharedMem = 16 * 1024;
    numa0.canBorrowMem = 48 * 1024;

    RackNumaMemInfo numa1 = numa0;
    numa1.numaId = 1;
    numa1.socketId = 1;

    nodeMemInfo.numaMemInfo.push_back(numa0);
    nodeMemInfo.numaMemInfo.push_back(numa1);
    srcMap["nodeA"] = nodeMemInfo;

    MemManager::Instance().UpdateNodeMemMap(srcMap);
    MemManager::Instance().nodeMemMap.clear();
}

MpResult Name2VmInfoGetForTest(Name2VmInfo *This, std::map<std::string, std::set<BorrowIdInfo>> &value)
{
    std::map<std::string, std::set<BorrowIdInfo>> temp;
    temp["a"].insert(BorrowIdInfo{1, 0});
    value = temp;
    return MEM_POOLING_OK;
}

TEST_F(TestMemManager, InitBorrowableInfoFailed1)
{
    mempooling::MemManager &obj = mempooling::MemManager::Instance();
    auto ret = obj.InitBorrowableInfo();
    EXPECT_EQ(ret, 0);
}

bool FromJsonMock(NodeMemoryInfoList *This, const std::string &jsonString)
{
    NodeMemoryInfo node;
    node.timestamp = std::time(nullptr);
    node.nodeId = "Node0";
    node.totalMemory = 1024 * 1024;
    node.usedMemory = 512 * 1024;
    node.freeMemory = 256 * 1024;
    node.lentMemory = 20480;

    RackNumaMemInfo numa0{0, 524288, 262144, 131072, 8192, 4096};
    RackNumaMemInfo numa1{1, 524288, 262144, 131072, 8192, 4096};
    node.numaMemInfo = {numa0, numa1};

    This->nodeMemoryInfoList.push_back(node);
    return false;
}

TEST_F(TestMemManager, InitBorrowableInfoSuccess)
{
    MOCKER_CPP(&NodeMemoryInfoList::FromJson, bool (*)(NodeMemoryInfoList *This, const std::string &jsonString))
        .stubs()
        .will(invoke(FromJsonMock));
    mempooling::MemManager &obj = mempooling::MemManager::Instance();
    auto ret = obj.InitBorrowableInfo();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, CollectBorrowableInfoFailed1)
{
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    std::string nodeId = "Node0";
    NodeMemoryInfoWithReservedMem nodeInfo;
    auto ret = obj.CollectBorrowableInfo(nodeId, nodeInfo);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, CollectBorrowableInfoFailed2)
{
    std::string nodeId = "Node0";
    NodeMemoryInfoWithReservedMem nodeInfo;
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    auto ret = obj.CollectBorrowableInfo(nodeId, nodeInfo);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, CollectBorrowableInfoSuccess)
{
    MOCKER_CPP(&NodeMemoryInfoList::FromJson, bool (*)(NodeMemoryInfoList *This, const std::string &jsonString))
        .stubs()
        .will(invoke(FromJsonMock));
    std::string nodeId = "Node0";
    NodeMemoryInfoWithReservedMem nodeInfo;
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    auto ret = obj.CollectBorrowableInfo(nodeId, nodeInfo);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, CollectBorrowableInfoListSucceed)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowableInfo, MpResult(*)(const std::string &,
        NodeMemoryInfoWithReservedMem &))
        .stubs()
        .will(returnValue(0));
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    std::vector<std::string> nodeId;
    nodeId.push_back("Node0");
    std::vector<NodeMemoryInfoWithReservedMem> nodeMemoryInfoList;
    auto ret = obj.CollectBorrowableInfoList(nodeId, nodeMemoryInfoList);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, CollectBorrowableInfoListFailed1)
{
    MOCKER_CPP(&mempooling::BorrowRecordHelper::CollectBorrowableInfo, MpResult(*)(const std::string &,
        NodeMemoryInfoWithReservedMem &))
        .stubs()
        .will(returnValue(1));
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    std::vector<std::string> nodeId;
    nodeId.push_back("Node0");
    std::vector<NodeMemoryInfoWithReservedMem> nodeMemoryInfoList;
    auto ret = obj.CollectBorrowableInfoList(nodeId, nodeMemoryInfoList);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMemManager, GeneratePerNodeNumaSocketMap_success)
{
    std::vector<MemNodeData> memNodeDataVec;

    // 节点 1
    TelemetrySocketData tsd1;
    tsd1.nodeId = "NodeA";
    tsd1.hostname = "hostA";
    tsd1.socket.socketId = "0";
    tsd1.socket.numas = {ubse::nodeController::NumaData{"0"}, ubse::nodeController::NumaData{"1"}};

    MemNodeData nodeA(std::move(tsd1));
    memNodeDataVec.push_back(std::move(nodeA));

    // 节点 2
    TelemetrySocketData tsd2;
    tsd2.nodeId = "NodeB";
    tsd2.hostname = "hostB";
    tsd2.socket.socketId = "1";
    tsd2.socket.numas = {ubse::nodeController::NumaData{"0"}, ubse::nodeController::NumaData{"2"}};

    MemNodeData nodeB(std::move(tsd2));
    memNodeDataVec.push_back(std::move(nodeB));
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;

    uint32_t ret = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, GeneratePerNodeNumaSocketMap_failed1)
{
    std::vector<MemNodeData> memNodeDataVec;

    // 节点 1
    TelemetrySocketData tsd1;
    tsd1.nodeId = "NodeA";
    tsd1.hostname = "hostA";
    tsd1.socket.socketId = "s0";
    tsd1.socket.numas = {ubse::nodeController::NumaData{"0"}, ubse::nodeController::NumaData{"1"}};

    MemNodeData nodeA(std::move(tsd1));
    memNodeDataVec.push_back(std::move(nodeA));

    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;

    uint32_t ret = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, GeneratePerNodeNumaSocketMap_failed2)
{
    std::vector<MemNodeData> memNodeDataVec;

    // 节点 1
    TelemetrySocketData tsd1;
    tsd1.nodeId = "NodeA";
    tsd1.hostname = "hostA";
    tsd1.socket.socketId = "999999999999";
    tsd1.socket.numas = {ubse::nodeController::NumaData{"0"}, ubse::nodeController::NumaData{"1"}};

    MemNodeData nodeA(std::move(tsd1));
    memNodeDataVec.push_back(std::move(nodeA));

    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;

    uint32_t ret = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, GeneratePerNodeNumaSocketMap_failed3)
{
    std::vector<MemNodeData> memNodeDataVec;

    // 节点 1
    TelemetrySocketData tsd1;
    tsd1.nodeId = "NodeA";
    tsd1.hostname = "hostA";
    tsd1.socket.socketId = "0";
    tsd1.socket.numas = {ubse::nodeController::NumaData{"n0"}, ubse::nodeController::NumaData{"1"}};

    MemNodeData nodeA(std::move(tsd1));
    memNodeDataVec.push_back(std::move(nodeA));
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    uint32_t ret = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, GeneratePerNodeNumaSocketMap_failed4)
{
    std::vector<MemNodeData> memNodeDataVec;

    // 节点 1
    TelemetrySocketData tsd1;
    tsd1.nodeId = "NodeA";
    tsd1.hostname = "hostA";
    tsd1.socket.socketId = "0";
    tsd1.socket.numas = {ubse::nodeController::NumaData{"999999999999"}, ubse::nodeController::NumaData{"1"}};

    MemNodeData nodeA(std::move(tsd1));
    memNodeDataVec.push_back(std::move(nodeA));
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    uint32_t ret = GeneratePerNodeNumaSocketMap(memNodeDataVec, numaSocketMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, PrintNumaSocketMap_success)
{
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    numaSocketMap["Node0"][0] = 0;
    numaSocketMap["Node0"][1] = 1;
    numaSocketMap["Node1"][0] = 2;
    PrintNumaSocketMap(numaSocketMap);
}

TEST_F(TestMemManager, GetNumaSocket_success)
{
    // 构造 numaSocketMap 数据
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    numaSocketMap["Node0"][0] = 0;
    numaSocketMap["Node0"][1] = 1;
    numaSocketMap["Node1"][0] = 2;

    uint16_t socketId = 3;
    bool ret = GetNumaSocket(numaSocketMap, "Node0", 1, socketId);
    EXPECT_EQ(ret, true);
}

TEST_F(TestMemManager, GetNumaSocket_failed1)
{
    // 构造 numaSocketMap 数据
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    numaSocketMap["Node0"][0] = 0;
    numaSocketMap["Node0"][1] = 1;
    numaSocketMap["Node1"][0] = 2;

    uint16_t socketId;
    bool ret = GetNumaSocket(numaSocketMap, "Node0", 3, socketId);
    EXPECT_EQ(ret, false);
}

TEST_F(TestMemManager, GetNumaSocket_failed2)
{
    // 构造 numaSocketMap 数据
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    numaSocketMap["Node0"][0] = 0;
    numaSocketMap["Node0"][1] = 1;
    numaSocketMap["Node1"][0] = 2;

    uint16_t socketId = 3;
    bool ret = GetNumaSocket(numaSocketMap, "NodeX", 0, socketId);
    EXPECT_EQ(ret, false);
}

uint32_t RackMemGetTopologyInfoMock2(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology)
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

TEST_F(TestMemManager, GenerateNumaSocketMap_success)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(invoke(RackMemGetTopologyInfoMock2));
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    auto ret = MemManager::Instance().GenerateNumaSocketMap(numaSocketMap);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, GenerateNumaSocketMap_failed1)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(returnValue(1));
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    auto ret = MemManager::Instance().GenerateNumaSocketMap(numaSocketMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, GenerateNumaSocketMap_failed2)
{
    MOCKER_CPP(&UbseMemGetTopologyInfo,
               uint32_t(*)(std::unordered_map<std::string, std::vector<MemNodeData>> & nodeTopology))
        .stubs()
        .will(invoke(RackMemGetTopologyInfoMock2));
    MOCKER_CPP(&GeneratePerNodeNumaSocketMap,
               uint32_t(*)(const std::vector<MemNodeData> &memNodeDataVec,
                           std::map<std::string, std::map<int, uint16_t>> &numaSocketMap))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    auto ret = MemManager::Instance().GenerateNumaSocketMap(numaSocketMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, UpdateBorrowRecordsFailed1)
{
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    auto ret = obj.UpdateBorrowRecords();
    EXPECT_EQ(ret, 0);
}

bool NodeMemoryInfoListFromJsonForTest(NodeMemoryInfoList *This, const std::string &jsonString)
{
    RackNumaMemInfo numaInfo;
    NodeMemoryInfo nodeInfo;
    nodeInfo.numaMemInfo.push_back(numaInfo);
    This->nodeMemoryInfoList.push_back(nodeInfo);
    return false;
}

TEST_F(TestMemManager, LentNumaToJson)
{
    LentNuma obj;
    std::string jsonStr = obj.ToString();
    EXPECT_NE(jsonStr.length(), 0);
}

TEST_F(TestMemManager, BorrowRecordToJson)
{
    BorrowRecord obj;
    std::string jsonStr = obj.ToString();
    EXPECT_NE(jsonStr.length(), 0);
}

TEST_F(TestMemManager, GetBorrowIdByNumaIdSucceedWithNormalData)
{
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult(*)(BorrowRecordHelper* This, const std::string, std::vector<BorrowRecord> &))
        .stubs()
        .will(invoke(CollectBorrowRecordsMock));
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    std::vector<std::string> vec;
    uint16_t numaId = 0;
    std::string nodeId = "Node0";
    auto ret = obj.GetBorrowIdByNumaId(vec, numaId, nodeId);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, TestGetCanBorrowMemFromUbFail)
{
    RackNumaMemInfo numaMemInfo;
    uint64_t canBorrowMem;
    EXPECT_EQ(MemManager::Instance().GetCanBorrowMemFromUb(numaMemInfo, canBorrowMem), 0);
}

TEST_F(TestMemManager, TestGetCanBorrowMemFromUbSuccess)
{
    RackNumaMemInfo numaMemInfo;
    numaMemInfo.numaId = 4;
    uint64_t canBorrowMem;
    EXPECT_EQ(MemManager::Instance().GetCanBorrowMemFromUb(numaMemInfo, canBorrowMem), 0);
}

TEST_F(TestMemManager, TestResolveUbBorrowableInfoListFail)
{
    MOCKER_CPP(&MemManager::GetCanBorrowMemFromUb,
               MpResult(*)(RackNumaMemInfo numaMemInfo, uint64_t & canBorrowMem))
        .stubs()
        .will(returnValue(1));
    NodeMemoryInfoWithReservedMem nodeMemoryInfoWithReservedMem;
    RackNumaMemInfo memInfo;
    nodeMemoryInfoWithReservedMem.numaMemInfo.push_back(memInfo);
    std::vector<NodeMemoryInfoWithReservedMem> nodeMemoryInfoList;
    EXPECT_EQ(MemManager::Instance().ResolveUbBorrowableInfoList(nodeMemoryInfoWithReservedMem, nodeMemoryInfoList), 1);
}

TEST_F(TestMemManager, TestResolveUbBorrowableInfoListSuccess)
{
    MOCKER_CPP(&MemManager::GetCanBorrowMemFromUb, MpResult(*)(RackNumaMemInfo numaMemInfo, uint64_t & canBorrowMem))
        .stubs()
        .will(returnValue(0));
    NodeMemoryInfoWithReservedMem nodeMemoryInfoWithReservedMem;
    RackNumaMemInfo memInfo;
    memInfo.socketId = 0;
    nodeMemoryInfoWithReservedMem.numaMemInfo.push_back(memInfo);
    std::vector<NodeMemoryInfoWithReservedMem> nodeMemoryInfoList;
    EXPECT_EQ(MemManager::Instance().ResolveUbBorrowableInfoList(nodeMemoryInfoWithReservedMem, nodeMemoryInfoList), 0);
}
TEST_F(TestMemManager, GetNodeMemMap_succeed)
{
    MOCKER_CPP(&MemManager::InitBorrowableInfo, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));

    mempooling::MemManager &obj = mempooling::MemManager::Instance();
    std::unordered_map<std::string, NodeMemInfo> outMap;
    MpResult ret = obj.GetNodeMemMap(outMap);

    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, GetNodeMemMap_init_info_failed)
{
    MOCKER_CPP(&MemManager::InitBorrowableInfo, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_ERROR));
    mempooling::MemManager &obj = mempooling::MemManager::Instance();
    std::unordered_map<std::string, NodeMemInfo> outMap;
    MpResult ret = obj.GetNodeMemMap(outMap);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, BuildSyncAntiNode_success)
{
    // 构造输入 buffer
    const char *msg = "anti-node test data";
    UbseByteBuffer buffer;
    buffer.len = strlen(msg);
    buffer.data = reinterpret_cast<uint8_t*>(const_cast<char*>(msg));
    buffer.freeFunc = nullptr;

    UbseByteBuffer syncData;

    AntiNode antiNode;
    MpResult ret = antiNode.BuildSyncAntiNode(buffer, syncData);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_NE(syncData.data, nullptr);
    EXPECT_EQ(syncData.len, buffer.len);
    EXPECT_EQ(memcmp(syncData.data, buffer.data, buffer.len), 0);
    delete[] syncData.data;
}

TEST_F(TestMemManager, BuildSyncAntiNode_failed1)
{
    // 构造输入 buffer
    const char *msg = "anti-node test data";
    UbseByteBuffer buffer;
    buffer.len = strlen(msg);
    buffer.data = reinterpret_cast<uint8_t*>(const_cast<char*>(msg));
    buffer.freeFunc = nullptr;

    UbseByteBuffer syncData;

    AntiNode antiNode;
    MOCKER(memcpy_s).stubs().will(returnValue(MEM_POOLING_ERROR));
    MpResult ret = antiNode.BuildSyncAntiNode(buffer, syncData);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
    delete[] syncData.data;
}

TEST_F(TestMemManager, RemoveBorrowIdsCompleted_succeed)
{
    mempooling::BorrowIdsCompleted &obj = mempooling::BorrowIdsCompleted::Instance();

    auto ret = obj.Remove("ab");
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, RemoveBorrowIdsCompleted_rack_put_info_failed)
{
    MOCKER_CPP(&UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowIdsCompleted &obj = mempooling::BorrowIdsCompleted::Instance();
    auto ret = obj.Remove("ab");
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, UpdateVmInfosCompleted_succeed)
{
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();

    pid_t pid = 123;
    std::string remoteNumaId = "test";
    std::string borrowInNode = "test";
    auto ret = obj.Update(pid, remoteNumaId, borrowInNode);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, UpdateVmInfosCompleted_rack_storage_info_failed)
{
    MOCKER_CPP(&UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    pid_t pid = 123;
    std::string remoteNumaId = "test";
    std::string borrowInNode = "test";
    auto ret = obj.Update(pid, remoteNumaId, borrowInNode);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, RemoveVmInfosCompleted_succeed)
{
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();

    pid_t pid = 1234;
    auto ret = obj.Remove(pid);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, RemoveVmInfosCompleted_rack_put_info_failed)
{
    MOCKER_CPP(&UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    pid_t pid = 1234;
    auto ret = obj.Remove(pid);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, RemoveVmInfosCompleted_rack_storage_info_succeed)
{
    MOCKER_CPP(&UbseStorageDeleteData, MpResult(*)(const std::string &, const std::string &))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    pid_t pid = 1234;
    auto ret = obj.Remove(pid);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, GetVmInfosCompletedMap_succeed)
{
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    std::unordered_map<pid_t, std::string> vmInfosCompletedMap;
    auto ret = obj.Query(vmInfosCompletedMap);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, GetVmInfosCompletedMap_rack_query_all_info_failed)
{
    MOCKER_CPP(UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                 UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    std::unordered_map<pid_t, std::string> vmInfosCompletedMap;
    auto ret = obj.Query(vmInfosCompletedMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, GetVmInfosCompletedMapForPair_succeed)
{
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    std::unordered_map<pid_t, std::string> vmInfosCompletedMap;
    auto ret = obj.Query(vmInfosCompletedMap);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, PutBorrowIdsCompletedRawData_succeed)
{
    mempooling::BorrowIdsCompleted &obj = mempooling::BorrowIdsCompleted::Instance();

    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, PutBorrowIdsCompletedRawData_rack_query_all_info_failed)
{
    MOCKER_CPP(&UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowIdsCompleted &obj = mempooling::BorrowIdsCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, PutBorrowIdsCompletedRawData_rack_delete_info_failed)
{
    MOCKER_CPP(&UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowIdsCompleted &obj = mempooling::BorrowIdsCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, GetBorrowIdsCompletedRawDataFailed1)
{
    MOCKER_CPP(UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
        UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowIdsCompleted &obj = mempooling::BorrowIdsCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, GetBorrowIdsCompletedRawData_empty_succeed)
{
    mempooling::BorrowIdsCompleted &obj = mempooling::BorrowIdsCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, PutVmInfosCompletedRawData_succeed)
{
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMemManager, PutVmInfosCompletedRawData_rack_query_all_info_failed)
{
    MOCKER_CPP(&UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, PutVmInfosCompletedRawData_rack_delete_info_failed)
{
    MOCKER_CPP(&UbseStoragePutData,
               MpResult(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.PutRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, GetVmInfosCompletedRawData_empty_succeed)
{
    mempooling::VmInfosCompleted &obj = mempooling::VmInfosCompleted::Instance();
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer, true);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, CollectBorrowRecordsAllFailed)
{
    MOCKER_CPP(&mempooling::BorrowRecordHelper::CollectBorrowRecordsAll, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    std::vector<BorrowRecord> borrowRecords;
    auto ret = obj.CollectBorrowRecordsAll(borrowRecords);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, CollectBorrowRecordsAllSucceed)
{
    MOCKER_CPP(&mempooling::BorrowRecordHelper::CollectBorrowRecordsAll, MpResult(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::BorrowRecordHelper &obj = mempooling::BorrowRecordHelper::Instance();
    std::vector<BorrowRecord> borrowRecords;
    BorrowRecord item;
    obj.gBorrowRecords.push_back(item);
    auto ret = obj.CollectBorrowRecordsAll(borrowRecords);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, UpdateBorrowIdsCompletedShouldSucceedWhenAllMethodsPass)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::BorrowIdsCompleted::GetRawData, MpResult(*)(UbseByteBuffer & data, bool needLock))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::BorrowIdsCompleted &obj = mempooling::BorrowIdsCompleted::Instance();
    std::string borrowId;
    auto ret = obj.Update(borrowId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, UpdateBorrowIdsCompletedShouldFailedWhenRackStoragePutDataFailed)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&mempooling::BorrowIdsCompleted::GetRawData, MpResult(*)(UbseByteBuffer & data, bool needLock))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    mempooling::BorrowIdsCompleted &obj = mempooling::BorrowIdsCompleted::Instance();
    std::string borrowId;
    auto ret = obj.Update(borrowId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, GetBorrowIdCompletedValueSucceed)
{
    UbseByteBuffer buffer;
    std::unordered_set<std::string> expected = {"id1", "id2", "id3"};
    RmrsOutStream builder;
    builder << expected;
    buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    std::unordered_set<std::string> ctx;
    mempooling::GetBorrowIdCompletedValue("prefix", "key", buffer, &ctx);
    EXPECT_EQ(expected, ctx);
    delete[] buffer.data;
}

TEST_F(TestMemManager, GetVmInfosCompletedValueSucceed)
{
    UbseByteBuffer buffer;
    std::unordered_map<pid_t, std::string> expected = {
        {1234, "vm1"},
        {5678, "vm2"},
        {9012, "vm3"}
    };
    RmrsOutStream builder;
    builder << expected;
    buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    std::unordered_map<pid_t, std::string> ctx;
    mempooling::GetVmInfosCompletedValue("prefix", "key", buffer, &ctx);
    EXPECT_EQ(expected, ctx);
    delete[] buffer.data;
}

TEST_F(TestMemManager, LoadDataBaseSucceed)
{
    UbseByteBuffer buffer;
    buffer.len = 1;
    buffer.data = new uint8_t[buffer.len];
    UbseByteBuffer ctx;
    mempooling::LoadDataBase("prefix", "key", buffer, &ctx);
    EXPECT_EQ(buffer.len, ctx.len);
    EXPECT_EQ(buffer.data[0], ctx.data[0]);
    delete[] buffer.data;
    delete[] ctx.data;
}
 
TEST_F(TestMemManager, LoadDataBaseFailed)
{
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    UbseByteBuffer buffer;
    buffer.len = 1;
    buffer.data = new uint8_t[buffer.len];
    UbseByteBuffer ctx;
    mempooling::LoadDataBase("prefix", "key", buffer, &ctx);
    EXPECT_EQ(buffer.len, ctx.len);
    delete[] buffer.data;
    delete[] ctx.data;
}
 
TEST_F(TestMemManager, AntiDataReloadFailed1)
{
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(1));
    auto ret = mempooling::AntiDataReload();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
 
TEST_F(TestMemManager, AntiDataReloadFailed2)
{
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(TestRackStorageQueryData));
    MOCKER_CPP(&MpUpdateAntiNodeParam::FromJson, bool(*)(MpUpdateAntiNodeParam *, const std::string &jsonString))
        .stubs()
        .will(returnValue(false));
    auto ret = mempooling::AntiDataReload();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
 
TEST_F(TestMemManager, AntiDataReloadSucceed1)
{
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(0));
    auto ret = mempooling::AntiDataReload();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}
 
TEST_F(TestMemManager, AntiDataReloadSucceed2)
{
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(TestRackStorageQueryData));
    MOCKER_CPP(&MpUpdateAntiNodeParam::FromJson, bool(*)(MpUpdateAntiNodeParam *, const std::string &jsonString))
        .stubs()
        .will(returnValue(true));
    auto ret = mempooling::AntiDataReload();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}
 
TEST_F(TestMemManager, DataReloadInitFailed2)
{
    MOCKER_CPP(&AntiDataReload, uint32_t(*)())
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(1));
    auto ret = mempooling::DataReloadInit();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
 
TEST_F(TestMemManager, DataReloadInitFailed3)
{
    MOCKER_CPP(&AntiDataReload, uint32_t(*)())
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&Name2VmInfo::PutRawData, uint32_t(*)(Name2VmInfo *, UbseByteBuffer &))
        .stubs()
        .will(returnValue(1));
    auto ret = mempooling::DataReloadInit();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
 
TEST_F(TestMemManager, DataReloadInitFailed4)
{
    MOCKER_CPP(&AntiDataReload, uint32_t(*)())
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1));
    MOCKER_CPP(&Name2VmInfo::PutRawData, uint32_t(*)(Name2VmInfo *, UbseByteBuffer &))
        .stubs()
        .will(returnValue(1));
    auto ret = mempooling::DataReloadInit();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
 
TEST_F(TestMemManager, DataReloadInitFailed5)
{
    MOCKER_CPP(&AntiDataReload, uint32_t(*)())
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&Name2VmInfo::PutRawData, uint32_t(*)(Name2VmInfo *, UbseByteBuffer &))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&BorrowIdRedirection::PutRawData, uint32_t(*)(BorrowIdRedirection *, UbseByteBuffer &))
        .stubs()
        .will(returnValue(1));
    auto ret = mempooling::DataReloadInit();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
 
TEST_F(TestMemManager, DataReloadInitSucceed)
{
    MOCKER_CPP(&AntiDataReload, uint32_t(*)())
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&UbseStorageQueryData, uint32_t(*)(const std::string &keyPrefix, const std::string &key, void *ctx,
                                                  UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&Name2VmInfo::PutRawData, uint32_t(*)(Name2VmInfo *, UbseByteBuffer &))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&BorrowIdRedirection::PutRawData, uint32_t(*)(BorrowIdRedirection *, UbseByteBuffer &))
        .stubs()
        .will(returnValue(0));
    auto ret = mempooling::DataReloadInit();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MemReturnManager_not_found)
{
    mempooling::MemReturnManager &obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache.clear();
    auto ret = obj.Remove("ab");
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MemReturnManager_put_failed)
{
    mempooling::MemReturnManager &obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(1));
    auto ret = obj.Remove("borrow_0");
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, MemReturnManager_sync_succeed)
{
    mempooling::MemReturnManager &obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(0));

    auto ret = obj.Remove("borrow_0");
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MemReturnManager_success)
{
    mempooling::MemReturnManager &obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data))
        .stubs()
        .will(returnValue(0));

    auto ret = obj.Remove("borrow_0");
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MemReturnManager_query_found)
{
    mempooling::MemReturnManager &obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    auto ret = obj.Remove("borrow_0");
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MemReturnManager_query_not_found)
{
    mempooling::MemReturnManager &obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    auto ret = obj.Remove("borrow_1");
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MemReturnManager_getRawData)
{
    mempooling::MemReturnManager &obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    UbseByteBuffer buffer;
    auto ret = obj.GetRawData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MemReturnManager_allReturned)
{
    mempooling::MemReturnManager &obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    std::string srcNid = "srcNid_4";
    uint16_t srcRemoteNumaId = 5;
    auto ret = obj.IsAllReturned(srcNid, srcRemoteNumaId);
    EXPECT_EQ(ret, false);
}

TEST_F(TestMemManager, MemReturnManager_all_not_Returned)
{
    mempooling::MemReturnManager &obj = mempooling::MemReturnManager::Instance();
    obj.borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    std::string srcNid = "srcNid_5";
    uint16_t srcRemoteNumaId = 5;
    auto ret = obj.IsAllReturned(srcNid, srcRemoteNumaId);
    EXPECT_EQ(ret, true);
}

TEST_F(TestMemManager, MpManagerSubModuleInit)
{
    MOCKER_CPP(&mempooling::DataReloadInit, uint32_t(*)()).stubs().will(returnValue(1));
    MpManagerSubModule module;
    auto ret = module.Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemManager, MpManagerSubModuleInit1)
{
    MOCKER_CPP(&mempooling::DataReloadInit, uint32_t(*)()).stubs().will(returnValue(0));
    MpManagerSubModule module;
    auto ret = module.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MpManagerSubModuleInit2)
{
    MOCKER_CPP(&mempooling::DataReloadInit, uint32_t(*)()).stubs().will(returnValue(0));
    MpManagerSubModule module;
    auto ret = module.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MpManagerSubModuleInit3)
{
    MOCKER_CPP(&mempooling::DataReloadInit, uint32_t(*)()).stubs().will(returnValue(0));
    MpManagerSubModule module;
    auto ret = module.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MpManagerSubModuleInit4)
{
    MOCKER_CPP(&mempooling::DataReloadInit, uint32_t(*)()).stubs().will(returnValue(0));
    MpManagerSubModule module;
    auto ret = module.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MpManagerSubModuleInit5)
{
    MOCKER_CPP(&mempooling::DataReloadInit, uint32_t(*)()).stubs().will(returnValue(0));
    MpManagerSubModule module;
    auto ret = module.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MpManagerSubModuleInit6)
{
    MOCKER_CPP(&mempooling::DataReloadInit, uint32_t(*)()).stubs().will(returnValue(0));
    MpManagerSubModule module;
    auto ret = module.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemManager, MpManagerSubModuleInit7)
{
    MOCKER_CPP(&mempooling::DataReloadInit, uint32_t(*)()).stubs().will(returnValue(0));
    MpManagerSubModule module;
    auto ret = module.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

UbseResult UbseGetNumaMemDebtInfoMock(std::vector<UbseNumaMemoryDebtInfo> &debtInfos)
{
    debtInfos.clear();
    return (UbseResult)1003;
}

TEST_F(TestMemManager, GetDebtInfosWithRetryTest_InRetry)
{
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    MOCKER_CPP(&UbseGetNumaMemDebtInfo,
               UbseResult(*)(std::vector<UbseNumaMemoryDebtInfo> &))
        // .stubs()
        .expects(atLeast(1))
        .will(invoke(UbseGetNumaMemDebtInfoMock));   // 验证确实retry;

    auto &obj = mempooling::BorrowRecordHelper::Instance();
    auto ret = obj.GetDebtInfosWithRetry(debtInfos);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

UbseResult UbseGetNumaMemDebtInfoMockWithInvalidRes(std::vector<UbseNumaMemoryDebtInfo> &debtInfos)
{
    debtInfos.clear();
    UbseNumaMemoryDebtInfo info;
    info.name = "1-123a456b789d";
    info.borrowNodeId = "node1";
    info.lentNodeId = "node2";
    info.borrowSocketIdList = {0};
    info.lentSocketIdList = {1};
    info.lentNumaIdList = {2};
    info.lentNumaSizeList = {1024ULL * 1024 * 1024}; // 1GB
    info.borrowMemId = {};// borrowMemId为空
    info.lentMemId = {20001};
    info.size = 1024ULL * 1024 * 1024; // 1GB
    // 关键：remoteNumaId 设置为非法值
    info.remoteNumaId = -1;  // 触发 retry 条件
    info.uid = 1000;
    info.username = "testuser";
    return (UbseResult)1003;
}

TEST_F(TestMemManager, GetDebtInfosWithRetryTest_Invalid_InRetry)
{
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    MOCKER_CPP(&UbseGetNumaMemDebtInfo,
               UbseResult(*)(std::vector<UbseNumaMemoryDebtInfo> &))
        // .stubs()
        .expects(atLeast(1))
        .will(invoke(UbseGetNumaMemDebtInfoMock));   // 验证确实retry;

    auto &obj = mempooling::BorrowRecordHelper::Instance();
    auto ret = obj.GetValidDebtInfosWithRetry(debtInfos);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}