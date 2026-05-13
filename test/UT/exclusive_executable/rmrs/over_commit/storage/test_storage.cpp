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

#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "securec.h"

#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "ubse_storage.h"
#include "mempooling_interface.h"
#include "mp_sync_data_helper.h"

#define private public
#include "over_commit_storage.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {
MpResult ClearData(const std::vector<std::string>& keyList);
}

namespace mempooling::over_commit {
using namespace mempooling;
using namespace ubse::storage;
using namespace mempooling::outinterface;
const std::string HIGH_WATER_MAKR = "/rack.mp.over_commit.high_water";
const std::string LOW_WATER_MAKR = "/rack.mp.over_commit.low_water";
// 测试类
class TestOverCommitStorageModule : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

uint32_t RackStorageQueryAllDataForWaterMark(const std::string& keyPrefix, const std::string& key, void* ctx,
                                             UbseStorageDealDataFunc func)
{
    std::unordered_map<std::string, uint16_t>& map = *(static_cast<std::unordered_map<std::string, uint16_t>*>(ctx));
    map["high_water"] = 90;
    map["low_water"] = 60;
    return 0;
}

uint32_t RackStorageQueryAllDataForWaterMarkFail1(const std::string& keyPrefix, const std::string& key, void* ctx,
                                                  UbseStorageDealDataFunc func)
{
    std::unordered_map<std::string, uint16_t>& map = *(static_cast<std::unordered_map<std::string, uint16_t>*>(ctx));
    map[HIGH_WATER_MAKR] = 92;
    return 0;
}

uint32_t RackStorageQueryAllDataForWaterMarkFail2(const std::string& keyPrefix, const std::string& key, void* ctx,
                                                  UbseStorageDealDataFunc func)
{
    std::unordered_map<std::string, uint16_t>& map = *(static_cast<std::unordered_map<std::string, uint16_t>*>(ctx));
    map[LOW_WATER_MAKR] = 80;
    return 0;
}

uint32_t RackStorageQueryAllDataForKeyList(const std::string& keyPrefix, const std::string& key, void* ctx,
                                           UbseStorageDealDataFunc func)
{
    std::unordered_map<std::string, uint16_t>& map = *(static_cast<std::unordered_map<std::string, uint16_t>*>(ctx));
    map["high_water"] = 90;
    map["low_water"] = 60;
    return 0;
}

TEST_F(TestOverCommitStorageModule, GetWaterMarkSuccess)
{
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryAllDataForWaterMark));
    struct WaterMark waterMark;
    auto ret = OverCommitStorage::Instance().GetWaterMark(waterMark.highWaterMark, waterMark.lowWaterMark);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitStorageModule, GetWaterMarkFail)
{
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryAllDataForWaterMarkFail1));
    struct WaterMark waterMark;
    auto ret = OverCommitStorage::Instance().GetWaterMark(waterMark.highWaterMark, waterMark.lowWaterMark);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitStorageModule, GetWaterMarkFail2)
{
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(invoke(RackStorageQueryAllDataForWaterMarkFail2));
    struct WaterMark waterMark;
    auto ret = OverCommitStorage::Instance().GetWaterMark(waterMark.highWaterMark, waterMark.lowWaterMark);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitStorageModule, GetWaterMarkFail3)
{
    MOCKER_CPP(ubse::storage::UbseStorageQueryData, uint32_t(*)(const std::string& keyPrefix, const std::string& key,
                                                                void* ctx, ubse::storage::UbseStorageDealDataFunc func))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    struct WaterMark waterMark;
    auto ret = OverCommitStorage::Instance().GetWaterMark(waterMark.highWaterMark, waterMark.lowWaterMark);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitStorageModule, PutNumaBindTypeRawDataSuccessWhenNoData)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(0));
    UbseByteBuffer buffer;
    auto ret = OverCommitStorage::Instance().PutNumaBindTypeRawData(buffer);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestOverCommitStorageModule, PutNumaBindTypeRawDataSuccessWithData)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(0));
    UbseByteBuffer buffer;
    buffer.data = new uint8_t[2]; // 分配数据
    buffer.len = 2;
    auto ret = OverCommitStorage::Instance().PutNumaBindTypeRawData(buffer);
    delete[] buffer.data;
    EXPECT_EQ(ret, 0);
}

TEST_F(TestOverCommitStorageModule, PutNumaBindTypeRawDataFail1)
{
    MOCKER_CPP(UbseStoragePutData, uint32_t(*)(const std::string&, void* ctx, UbseStorageDealDataFunc))
        .stubs()
        .will(returnValue(1));
    UbseByteBuffer buffer;
    buffer.data = new uint8_t[2]; // 分配数据
    buffer.len = 2;
    auto ret = OverCommitStorage::Instance().PutNumaBindTypeRawData(buffer);
    delete[] buffer.data;
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

uint32_t RackStorageQueryAllDataForBindType(const std::string& keyPrefix, void* ctx, UbseStorageDealDataFunc func)
{
    UbseByteBuffer buff;
    std::vector<std::string>& vec = *(static_cast<std::vector<std::string>*>(ctx));
    vec.emplace_back("BIND_MULTIPLE");
    return 0;
}

TEST_F(TestOverCommitStorageModule, GetNumaBindTypeRawDataSuccessWithData)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(0));
    UbseByteBuffer buffer;
    buffer.data = new uint8_t[2]; // 分配数据
    buffer.len = 2;
    auto ret = OverCommitStorage::Instance().GetNumaBindTypeRawData(buffer, true);
    delete[] buffer.data;
    EXPECT_EQ(ret, 0);
}

TEST_F(TestOverCommitStorageModule, GetNumaBindTypeRawDataSuccessWithoutData)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(0));
    UbseByteBuffer buffer;
    buffer.data = new uint8_t[2]; // 分配数据
    buffer.len = 2;
    auto ret = OverCommitStorage::Instance().GetNumaBindTypeRawData(buffer, false);
    delete[] buffer.data;
    EXPECT_EQ(ret, 0);
}

TEST_F(TestOverCommitStorageModule, GetNumaBindTypeRawDataFail1)
{
    MOCKER_CPP(UbseStorageQueryData,
               uint32_t(*)(const std::string&, const std::string&, void* ctx, UbseStorageDealDataFunc))
        .stubs()
        .will(returnValue(1));
    UbseByteBuffer buffer;
    buffer.data = new uint8_t[2]; // 分配数据
    buffer.len = 2;
    auto ret = OverCommitStorage::Instance().GetNumaBindTypeRawData(buffer, false);
    delete[] buffer.data;
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitStorageModule, UpdateBindTypeDBFail2)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto bindType = NumaBindType::BIND_SINGLE;
    auto ret = OverCommitStorage::Instance().UpdateBindTypeDB("node128", bindType);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitStorageModule, UpdateBindTypeDBFail3)
{
    UbseByteBuffer syncData;
    bool needLock = false;
    MOCKER_CPP(&OverCommitStorage::GetNumaBindTypeRawData, uint32_t(*)(UbseByteBuffer & data, bool needLock))
        .stubs()
        .will(returnValue(1));
    auto bindType = NumaBindType::BIND_MULTIPLE;
    auto ret = OverCommitStorage::Instance().UpdateBindTypeDB("node128", bindType);
    delete[] syncData.data;
    EXPECT_EQ(ret, 1);
}

TEST_F(TestOverCommitStorageModule, SelectBindTypeDBSuccess)
{
    auto ret = OverCommitStorage::Instance().SelectBindTypeDB();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestOverCommitStorageModule, UpdateWaterMarkSuccess)
{
    MOCKER_CPP(&OverCommitStorage::UpdateUint16,
               MpResult(*)(const std::string& keyPrefix, const std::string& key, const uint16_t& value))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(0));
    auto ret = OverCommitStorage::Instance().UpdateWaterMark(92, 80);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestOverCommitStorageModule, UpdateWaterMarkFail1)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(1));
    auto ret = OverCommitStorage::Instance().UpdateWaterMark(92, 80);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestOverCommitStorageModule, UpdateWaterMarkFail2)
{
    MOCKER_CPP(&memcpy_s, int (*)(void* dest, size_t destMax, const void* src, size_t count))
        .stubs()
        .will(returnValue(1));
    auto ret = OverCommitStorage::Instance().UpdateWaterMark(92, 80);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestOverCommitStorageModule, UpdateUint16Fail1)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(1));

    auto ret = OverCommitStorage::Instance().UpdateUint16("/key", "/key", 92);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestOverCommitStorageModule, UpdateUint16Fail2)
{
    MOCKER_CPP(&memcpy_s, int (*)(void* dest, size_t destMax, const void* src, size_t count))
        .stubs()
        .will(returnValue(1));

    auto ret = OverCommitStorage::Instance().UpdateUint16("/key", "/key", 92);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestOverCommitStorageModule, UpdateUint16Success)
{
    MOCKER_CPP(UbseStoragePutData,
               uint32_t(*)(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data))
        .stubs()
        .will(returnValue(0));

    auto ret = OverCommitStorage::Instance().UpdateUint16("/key", "/key", 92);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestOverCommitStorageModule, BindTypeMapToStrTest)
{
    std::string str = "";
    OverCommitStorage::Instance().BindTypeMapToStr(str);
}

TEST_F(TestOverCommitStorageModule, ClearDataTestSucessAndFailed)
{
    std::vector<std::string> keyList = {"0"};
    MOCKER(&UbseStorageDeleteData).stubs().will(returnValue(MEM_POOLING_OK));
    uint32_t ret = ClearData(keyList);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();

    MOCKER(&UbseStorageDeleteData).stubs().will(returnValue(MEM_POOLING_ERROR));
    ret = ClearData(keyList);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

} // namespace mempooling::over_commit