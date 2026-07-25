/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_storage_resp_simpo.h"
#include "ubse_error.h"
#include "ubse_storage_resp_simpo.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::ut::storage {
using namespace ubse::storage::message;
using namespace ubse::storage;
using namespace ubse::common::def;

void TestUbseStorageRespSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseStorageRespSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试构造和获取响应数据
 * 测试步骤：
 * 1. 构造响应对象
 * 2. 获取响应数据
 * 预期结果：
 * 响应数据与构造参数一致
 */
TEST_F(TestUbseStorageRespSimpo, ConstructAndGetStorageResp)
{
    std::vector<KV> kvs;
    UbseStorageResp resp{kvs};
    UbseStorageRespSimpo respSimpo(resp);

    auto respData = respSimpo.GetStorageResp();
    EXPECT_TRUE(respData.kvs.empty());
}

/*
 * 用例描述：
 * 测试设置和获取响应数据
 * 测试步骤：
 * 1. 设置响应对象
 * 2. 获取响应数据
 * 预期结果：
 * 响应数据与设置参数一致
 */
TEST_F(TestUbseStorageRespSimpo, SetAndGetStorageResp)
{
    std::vector<KV> kvs;
    UbseStorageResp resp{kvs};
    UbseStorageRespSimpo respSimpo;

    respSimpo.SetStorageResp(resp);
    auto respData = respSimpo.GetStorageResp();
    EXPECT_TRUE(respData.kvs.empty());
}

/*
 * 用例描述：
 * 序列化测试
 * 测试步骤：
 * 1.序列化resp数据,数据为空数组
 * 2.反序列化
 * 3.对比反序列化后的数据是否与序列化的数据一致
 * 预期结果：
 * 反序列化后的数据与序列化的数据一致
 */
TEST_F(TestUbseStorageRespSimpo, SerializeAndDeserializeEmptyData)
{
    std::vector<KV> kvs;
    UbseStorageResp resp{kvs};
    UbseStorageRespSimpo respSimpo(resp);
    EXPECT_EQ(UBSE_OK, respSimpo.Serialize());

    UbseStorageRespSimpo newRespSimpo;
    newRespSimpo.SetInputRawData(respSimpo.SerializedData(), respSimpo.SerializedDataSize());
    EXPECT_EQ(UBSE_OK, newRespSimpo.Deserialize());

    auto respData = newRespSimpo.GetStorageResp();
    EXPECT_TRUE(respData.kvs.empty());
}

/*
 * 用例描述：
 * 序列化测试
 * 测试步骤：
 * 1.序列化resp数据
 * 2.反序列化
 * 3.对比反序列化后的数据是否与序列化的数据一致
 * 预期结果：
 * 反序列化后的数据与序列化的数据一致
 */
TEST_F(TestUbseStorageRespSimpo, SerializeAndDeserializeNotEmptyData)
{
    auto data1 = new (std::nothrow) uint8_t[NO_2];
    ASSERT_NE(nullptr, data1);
    data1[0] = NO_1;
    data1[1] = NO_2;

    auto data2 = new (std::nothrow) uint8_t[NO_2];
    ASSERT_NE(nullptr, data2);
    data2[0] = NO_3;
    data2[1] = NO_4;

    std::vector<KV> kvs;
    kvs.emplace_back(KV{"key1", data1, 2});
    kvs.emplace_back(KV{"key2", data2, 2});

    UbseStorageResp resp{kvs};
    UbseStorageRespSimpo respSimpo(resp);
    EXPECT_EQ(UBSE_OK, respSimpo.Serialize());

    UbseStorageRespSimpo newRespSimpo;
    newRespSimpo.SetInputRawData(respSimpo.SerializedData(), respSimpo.SerializedDataSize());
    EXPECT_EQ(UBSE_OK, newRespSimpo.Deserialize());

    auto respData = newRespSimpo.GetStorageResp();
    ASSERT_EQ(NO_2, respData.kvs.size());

    EXPECT_EQ("key1", respData.kvs[0].key);
    EXPECT_EQ(NO_2, respData.kvs[0].valueLen);
    ASSERT_NE(nullptr, respData.kvs[0].value);
    EXPECT_EQ(NO_1, respData.kvs[0].value[0]);
    EXPECT_EQ(NO_2, respData.kvs[0].value[1]);

    EXPECT_EQ("key2", respData.kvs[1].key);
    EXPECT_EQ(NO_2, respData.kvs[1].valueLen);
    ASSERT_NE(nullptr, respData.kvs[1].value);
    EXPECT_EQ(NO_3, respData.kvs[1].value[0]);
    EXPECT_EQ(NO_4, respData.kvs[1].value[1]);
}

/*
 * 用例描述：
 * 反序列化测试
 * 测试步骤：
 * 1. 不设置输入数据
 * 2. 调用反序列化
 * 预期结果：
 * 反序列化失败
 */
TEST_F(TestUbseStorageRespSimpo, DeserializeFailWithVerifyStorageRespBufferFail)
{
    UbseStorageRespSimpo newRespSimpo;
    EXPECT_EQ(UBSE_ERROR_NULLPTR, newRespSimpo.Deserialize());
}

/*
 * 用例描述：
 * 反序列化测试
 * 测试步骤：
 * 1. 设置非法输入数据
 * 2. 调用反序列化
 * 预期结果：
 * 反序列化失败
 */
TEST_F(TestUbseStorageRespSimpo, DeserializeFailWithInvalidBuffer)
{
    uint8_t invalidData[1] = {0};
    UbseStorageRespSimpo newRespSimpo;
    newRespSimpo.SetInputRawData(invalidData, sizeof(invalidData));

    EXPECT_EQ(UBSE_ERROR, newRespSimpo.Deserialize());
}

/*
 * 用例描述：
 * 反序列化测试
 * 测试步骤：
 * 1. 第一次反序列化成功
 * 2. 第二次反序列化
 * 预期结果：
 * 第二次反序列化失败
 */
TEST_F(TestUbseStorageRespSimpo, DeserializeTwiceReturnError)
{
    std::vector<KV> kvs;
    UbseStorageResp resp{kvs};
    UbseStorageRespSimpo respSimpo(resp);
    EXPECT_EQ(UBSE_OK, respSimpo.Serialize());

    UbseStorageRespSimpo newRespSimpo;
    newRespSimpo.SetInputRawData(respSimpo.SerializedData(), respSimpo.SerializedDataSize());
    EXPECT_EQ(UBSE_OK, newRespSimpo.Deserialize());
    EXPECT_EQ(UBSE_ERROR, newRespSimpo.Deserialize());
}

/*
 * 用例描述：
 * 字符串转换测试
 * 测试步骤：
 * 1. 调用ToString
 * 预期结果：
 * 不抛出异常
 */
TEST_F(TestUbseStorageRespSimpo, ToString)
{
    UbseStorageRespSimpo respSimpo;
    EXPECT_NO_THROW(respSimpo.ToString());
}
} // namespace ubse::ut::storage