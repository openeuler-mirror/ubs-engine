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

#include "test_ubse_storage_req_simpo.h"
#include "ubse_error.h"
#include "ubse_storage_req_simpo.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::ut::storage {
const int WRONG_CODE = 3;

using namespace ubse::storage::message;
void TestUbseStorageReqSimpo::SetUp()
{
    Test::SetUp();
}

void TestUbseStorageReqSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试构造和获取请求数据
 * 测试步骤：
 * 1. 构造请求对象
 * 2. 获取请求内容
 * 预期结果：
 * 请求内容与构造参数一致
 */
TEST_F(TestUbseStorageReqSimpo, ConstructAndGetStorageReq)
{
    UbseStorageReq req{UbseStorageReqCmdType::GET, "default", "/key"};
    UbseStorageReqSimpo reqSimpo(req);

    auto reqData = reqSimpo.GetStorageReq();
    EXPECT_EQ(UbseStorageReqCmdType::GET, reqData.cmdType);
    EXPECT_EQ("default", reqData.dbName);
    EXPECT_EQ("/key", reqData.key);
}

/*
 * 用例描述：
 * 序列化测试
 * 测试步骤：
 * 1.序列化req数据
 * 2.反序列化
 * 3.对比反序列化后的数据是否与序列化的数据一致
 * 预期结果：
 * 反序列化后的数据与序列化的数据一致
 */
TEST_F(TestUbseStorageReqSimpo, SerializeAndDeserialize)
{
    UbseStorageReq req{UbseStorageReqCmdType::GET, "default", "/key"};
    UbseStorageReqSimpo reqSimpo(req);
    EXPECT_EQ(UBSE_OK, reqSimpo.Serialize());

    UbseStorageReqSimpo newReqSimpo;
    newReqSimpo.SetInputRawData(reqSimpo.SerializedData(), reqSimpo.SerializedDataSize());
    EXPECT_EQ(UBSE_OK, newReqSimpo.Deserialize());

    auto reqData = newReqSimpo.GetStorageReq();
    EXPECT_EQ(UbseStorageReqCmdType::GET, reqData.cmdType);
    EXPECT_EQ("/key", reqData.key);
    EXPECT_EQ("default", reqData.dbName);
    EXPECT_EQ("UbseStorageReqSimpo(Get,default,/key)", newReqSimpo.ToString());
}

/*
 * 用例描述：
 * 序列化测试
 * 测试步骤：
 * 1. 构造GET_WITH_PREFIX请求
 * 2. 调用ToString
 * 预期结果：
 * 字符串内容符合预期
 */
TEST_F(TestUbseStorageReqSimpo, ToStringGetWithPrefix)
{
    UbseStorageReq req{UbseStorageReqCmdType::GET_WITH_PREFIX, "default", "/prefix"};
    UbseStorageReqSimpo reqSimpo(req);

    EXPECT_EQ("UbseStorageReqSimpo(GetWithPrefix,default,/prefix)", reqSimpo.ToString());
}

/*
 * 用例描述：
 * 序列化测试
 * 测试步骤：
 * 1. 构造未知请求类型
 * 2. 调用ToString
 * 预期结果：
 * 字符串内容符合预期
 */
TEST_F(TestUbseStorageReqSimpo, ToStringUnknown)
{
    UbseStorageReq req{static_cast<UbseStorageReqCmdType>(WRONG_CODE), "default", "/key"};
    UbseStorageReqSimpo reqSimpo(req);

    EXPECT_EQ("UbseStorageReqSimpo(Unknown,default,/key)", reqSimpo.ToString());
}

TEST_F(TestUbseStorageReqSimpo, DeserializeFailWithVerifyStorageReqBufferFail)
{
    UbseStorageReqSimpo newReqSimpo;
    EXPECT_EQ(UBSE_ERROR_NULLPTR, newReqSimpo.Deserialize());
}

TEST_F(TestUbseStorageReqSimpo, DeserializeFailWithInvalidBuffer)
{
    uint8_t invalidData[1] = {0};
    UbseStorageReqSimpo newReqSimpo;
    newReqSimpo.SetInputRawData(invalidData, sizeof(invalidData));

    EXPECT_EQ(UBSE_ERROR, newReqSimpo.Deserialize());
}

TEST_F(TestUbseStorageReqSimpo, ReqCmdTypeToString)
{
    EXPECT_EQ("Get", ReqCmdTypeToString(UbseStorageReqCmdType::GET));
    EXPECT_EQ("GetWithPrefix", ReqCmdTypeToString(UbseStorageReqCmdType::GET_WITH_PREFIX));
    EXPECT_EQ("Unknown", ReqCmdTypeToString(static_cast<UbseStorageReqCmdType>(WRONG_CODE)));
}
} // namespace ubse::ut::storage