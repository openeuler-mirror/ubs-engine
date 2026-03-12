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
#include "mockcpp/mockcpp.hpp"
#include "ubse_storage_resp_simpo.h"
#include "ubse_error.h"

namespace ubse::ut::storage {
using namespace ubse::storage::message;
using namespace ubse::storage;
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
    UbseStorageResp resp{ kvs };
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
    data1[0] = NO_1;
    data1[1] = NO_2;
    auto data2 = new (std::nothrow) uint8_t[NO_2];
    data2[0] = NO_3;
    data2[1] = NO_4;
    std::vector<KV> kvs;
    kvs.emplace_back(KV{ "key1", data1, 2 });
    kvs.emplace_back(KV{ "key2", data2, 2 });

    UbseStorageResp resp{ kvs };
    UbseStorageRespSimpo respSimpo(resp);
    EXPECT_EQ(UBSE_OK, respSimpo.Serialize());
    UbseStorageRespSimpo newRespSimpo;
    newRespSimpo.SetInputRawData(respSimpo.SerializedData(), respSimpo.SerializedDataSize());
    EXPECT_EQ(UBSE_OK, newRespSimpo.Deserialize());
    auto respData = newRespSimpo.GetStorageResp();
    EXPECT_EQ(NO_2, respData.kvs.size());

    for (auto kv : kvs) {
        EXPECT_EQ(NO_2, kv.valueLen);
        if (kv.key == "key1") {
            EXPECT_EQ(NO_1, kv.value[0]);
            EXPECT_EQ(NO_2, kv.value[1]);
        }
        if (kv.key == "key2") {
            EXPECT_EQ(NO_3, kv.value[0]);
            EXPECT_EQ(NO_4, kv.value[1]);
        }
    }
}

/*
 * 用例描述：
 * 反序列化测试
 * 测试步骤：
 * 1.序列化resp数据
 * 2.反序列化
 * 3.构造key为空指针
 * 4.构造拷贝失败
 * 预期结果：
 * 反序列化失败
 */
TEST_F(TestUbseStorageRespSimpo, SerializeAndDeserializeWithInvalidData)
{
    auto data1 = new (std::nothrow) uint8_t[NO_2];
    data1[0] = NO_1;
    data1[1] = NO_2;
    std::vector<KV> kvs;
    kvs.emplace_back(KV{ "key1", data1, 2 });

    UbseStorageResp resp{ kvs };
    UbseStorageRespSimpo respSimpo(resp);
    EXPECT_EQ(UBSE_OK, respSimpo.Serialize());
    UbseStorageRespSimpo newRespSimpo;
    newRespSimpo.SetInputRawData(respSimpo.SerializedData(), respSimpo.SerializedDataSize());
    EXPECT_EQ(UBSE_OK, newRespSimpo.Deserialize());
}

TEST_F(TestUbseStorageRespSimpo, DeserializeFailWithVerifyStorageRespBufferFail)
{
    std::vector<KV> kvs;
    UbseStorageResp resp{ kvs };
    UbseStorageRespSimpo respSimpo(resp);
    UbseStorageRespSimpo newRespSimpo;
    newRespSimpo.SetInputRawData(respSimpo.SerializedData(), respSimpo.SerializedDataSize());
    EXPECT_EQ(UBSE_ERROR_NULLPTR, newRespSimpo.Deserialize());
}

TEST_F(TestUbseStorageRespSimpo, DeserializeFailWithGetStorageRespFail)
{
    std::vector<KV> kvs;
    UbseStorageResp resp{ kvs };
    UbseStorageRespSimpo respSimpo(resp);
    UbseStorageRespSimpo newRespSimpo;
    newRespSimpo.SetInputRawData(respSimpo.SerializedData(), respSimpo.SerializedDataSize());
    EXPECT_EQ(UBSE_ERROR_NULLPTR, newRespSimpo.Deserialize());
}
}