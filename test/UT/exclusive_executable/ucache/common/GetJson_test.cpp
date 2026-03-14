/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "ucache_json_util.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using namespace ucache;

class JsonUtilTest : public ::testing::Test {
protected:
    JsonUtil jsonUtil;
    rapidjson::Document doc;
    rapidjson::Value value;
    const char* key = "testKey";
    std::string output;
    uint64_t output2;
    uint16_t output3;
    int output4;
};

/*
 * Case Description: 测试Json对象中不存在指定key的情况
 * Preset Condition: 当Json对象中不存在指定key
 * Test Steps: 1.调用GetJsonStringValue方法
 * Expected Result: 结果应为UCACHE_ERR
 */
TEST_F(JsonUtilTest, ShouldReturnUCACHE_ERR_WhenKeyNotExists2)
{
    const char* key = "nonExistentKey";
    doc.SetObject();
    uint32_t result = jsonUtil.GetJsonStringValue(doc, key, output);
    EXPECT_EQ(result, 1);
}

/*
 * Case Description: 测试Json对象中存在指定key但值为整数的情况
 * Preset Condition: 当Json对象中存在指定key但值为整数
 * Test Steps: 1.调用GetJsonStringValue方法
 * Expected Result: 结果应为UCACHE_ERR
 */
TEST_F(JsonUtilTest, ShouldReturnUCACHE_ERR_WhenKeyExistsAndValueIsInteger)
{
    const char* key = "testKey";
    doc.SetObject();
    doc.AddMember(rapidjson::StringRef(key), 123, doc.GetAllocator());
    uint32_t result = jsonUtil.GetJsonStringValue(doc, key, output);
    EXPECT_EQ(result, 1);
}

/*
 * Case Description: 测试Json对象中存在指定key且值为字符串的情况
 * Preset Condition: 当Json对象中存在指定key且值为字符串
 * Test Steps: 1.调用GetJsonStringValue方法
 * Expected Result: 结果应为UCACHE_OK，输出应为字符串值
 */
TEST_F(JsonUtilTest, ShouldReturnUCACHE_OK_WhenKeyExistsAndValueIsString2)
{
    const char* key = "testKey";
    doc.SetObject();
    doc.AddMember(rapidjson::StringRef(key), "testValue", doc.GetAllocator());
    uint32_t result = jsonUtil.GetJsonStringValue(doc, key, output);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(output, "testValue");
}

/*
 * Case Description: 测试Json对象中键值对应的值不是字符串的情况
 * Preset Condition: 当Json对象中键值对应的值不是字符串
 * Test Steps: 1.调用GetJsonStringValue方法，传入键值对应值不是字符串的Json对象和键值
 * Expected Result: 结果返回UCACHE_ERR
 */
TEST_F(JsonUtilTest, ShouldReturnErr_WhenJsonValueIsNotString)
{
    const char* key = "non_string_key";
    value.SetInt(123);
    doc.SetObject();
    doc.AddMember(rapidjson::StringRef(key), value, doc.GetAllocator());
    uint32_t result = jsonUtil.GetJsonStringValue(doc, key, output);
    EXPECT_EQ(result, 1);
}

/*
 * Case Description: 测试Json对象中键值对应的值是字符串的情况
 * Preset Condition: 当Json对象中键值对应的值是字符串
 * Test Steps: 1.调用GetJsonStringValue方法，传入键值对应值是字符串的Json对象和键值
 * Expected Result: 结果返回UCACHE_OK，并且output中包含键值对应的字符串
 */
TEST_F(JsonUtilTest, ShouldReturnOkAndOutputContainsString_WhenJsonValueIsString)
{
    const char* key = "string_key";
    const char* str = "test_string";
    value.SetObject();
    doc.SetObject();
    value.SetString(str, doc.GetAllocator());
    doc.AddMember(rapidjson::StringRef(key), value, doc.GetAllocator());
    uint32_t result = jsonUtil.GetJsonStringValue(doc, key, output);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(output, str);
}
