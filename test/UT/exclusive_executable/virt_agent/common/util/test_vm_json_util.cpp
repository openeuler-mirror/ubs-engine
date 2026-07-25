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

#include "test_vm_json_util.h"
#include "mockcpp/mockcpp.hpp"

using namespace vm;

namespace ubse::ut::vm {
// 设置测试环境
void TestVmJsonUtil::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestVmJsonUtil::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

using testing::_;
using testing::Return;

/**
 * @tc.name  : VMGetJsonItemStr_ShouldReturnTrue_WhenJsonItemGetSucceeds
 * @tc.number: VMJsonUtilTest_VMGetJsonItemStr_001
 * @tc.desc  : Test scenario for VMGetJsonItemStr when DOCUE_JsonItemGet succeeds
 */
TEST_F(TestVmJsonUtil, VMGetJsonItemStr_ShouldReturnTrue_WhenJsonItemGetSucceeds)
{
    JSON_STR jsonStr = R"({"key":"value"})";
    Document pstJson;
    pstJson.Parse(jsonStr.c_str());

    JSON_STR jsonStrRes;
    EXPECT_TRUE(VMJsonUtil::VMGetJsonItemStr(pstJson, "key", jsonStrRes));
    EXPECT_EQ(jsonStrRes, "\"value\"");
}

/**
 * @tc.name  : VMGetJsonItemStr_ShouldReturnFalse_WhenJsonItemGetFails
 * @tc.number: VMJsonUtilTest_VMGetJsonItemStr_002
 * @tc.desc  : Test scenario for VMGetJsonItemStr when DOCUE_JsonItemGet fails
 */
TEST_F(TestVmJsonUtil, VMGetJsonItemStr_ShouldReturnFalse_WhenJsonItemGetFails)
{
    Document pstJson;
    pstJson.SetObject();

    JSON_STR jsonStr;
    EXPECT_FALSE(VMJsonUtil::VMGetJsonItemStr(pstJson, "key", jsonStr));
    EXPECT_EQ(jsonStr, "");
}

/**
 * @tc.name  : VMConvertMap2JsonStr_ShouldReturnTrue_WhenMapIsEmpty
 * @tc.number: VMJsonUtilTest_VMConvertMap2JsonStr_001
 * @tc.desc  : Test VMConvertMap2JsonStr function when input map is empty.
 */
TEST_F(TestVmJsonUtil, VMConvertMap2JsonStr_ShouldReturnTrue_WhenMapIsEmpty)
{
    JSON_MAP strMap;
    JSON_STR jsonStr;
    EXPECT_TRUE(VMJsonUtil::VMConvertMap2JsonStr(strMap, jsonStr));
    EXPECT_EQ(jsonStr, "{}");
}

/**
 * @tc.name  : VMConvertMap2JsonStr_ShouldReturnTrue_WhenMapContainsString
 * @tc.number: VMJsonUtilTest_VMConvertMap2JsonStr_002
 * @tc.desc  : Test VMConvertMap2JsonStr function when input map contains string.
 */
TEST_F(TestVmJsonUtil, VMConvertMap2JsonStr_ShouldReturnTrue_WhenMapContainsString)
{
    JSON_MAP strMap = {{"key1", "value1"}, {"key2", "value2"}};
    JSON_STR jsonStr;
    EXPECT_TRUE(VMJsonUtil::VMConvertMap2JsonStr(strMap, jsonStr));
    EXPECT_EQ(jsonStr, "{\n    \"key1\": \"value1\",\n    \"key2\": \"value2\"\n}");
}

/**
 * @tc.name  : VMConvertMap2JsonStr_ShouldReturnTrue_WhenMapContainsJsonString
 * @tc.number: VMJsonUtilTest_VMConvertMap2JsonStr_003
 * @tc.desc  : Test VMConvertMap2JsonStr function when input map contains json string.
 */
TEST_F(TestVmJsonUtil, VMConvertMap2JsonStr_ShouldReturnTrue_WhenMapContainsJsonString)
{
    JSON_MAP strMap = {{"key1", R"({"subKey1":"subValue1"})"}, {"key2", R"({"subKey2":"subValue2"})"}};
    JSON_STR jsonStr;
    EXPECT_TRUE(VMJsonUtil::VMConvertMap2JsonStr(strMap, jsonStr));
    EXPECT_EQ(jsonStr, "{\n    \"key1\": {\n        \"subKey1\": \"subValue1\"\n    },"
                       "\n    \"key2\": {\n        \"subKey2\": \"subValue2\"\n    }\n}");
}

/**
 * @tc.name  : VMConvertJsonStr2Map_ShouldReturnFalse_WhenJsonParseFail
 * @tc.number: VMJsonUtilTest_VMConvertJsonStr2Map_001
 * @tc.desc  : Test for DOCUE_JsonParse error.
 */
TEST_F(TestVmJsonUtil, JsonStrParseError_Test)
{
    JSON_STR jsonStr = "invalid json string";
    JSON_MAP strMap;
    EXPECT_FALSE(VMJsonUtil::VMConvertJsonStr2Map(jsonStr, strMap));
}

/**
 * @tc.name  : VMConvertJsonStr2Map_ShouldReturnFalse_WhenJsonIsObjectFail
 * @tc.number: VMJsonUtilTest_VMConvertJsonStr2Map_002
 * @tc.desc  : Test for JsonStr is not Object.
 */
TEST_F(TestVmJsonUtil, JsonStrIsNotObject_Test)
{
    JSON_STR jsonStr = "[]";
    JSON_MAP strMap;
    EXPECT_FALSE(VMJsonUtil::VMConvertJsonStr2Map(jsonStr, strMap));
}

/**
 * @tc.name  : VMConvertJsonStr2Map_ShouldReturnTrue_WhenJsonStrIsObject
 * @tc.number: VMJsonUtilTest_VMConvertJsonStr2Map_003
 * @tc.desc  : Test for JsonStr is Object.
 */
TEST_F(TestVmJsonUtil, JsonStrIsObject_Test)
{
    JSON_STR jsonStr = R"({"key":"value"})";
    JSON_MAP strMap;
    strMap["key"] = "";
    EXPECT_TRUE(VMJsonUtil::VMConvertJsonStr2Map(jsonStr, strMap));
    EXPECT_EQ(strMap["key"], "value");
}

/**
 * @tc.name  : VMConvertJsonStr2Map_ShouldReturnFalse_WhenVMGetJsonItemStrFail
 * @tc.number: VMJsonUtilTest_VMConvertJsonStr2Map_004
 * @tc.desc  : Test for VMGetJsonItemStr error.
 */
TEST_F(TestVmJsonUtil, VMGetJsonItemStrError_Test)
{
    JSON_STR jsonStr = R"({"key":"value"})";
    JSON_MAP strMap;
    strMap["invalid_key"] = "";
    EXPECT_FALSE(VMJsonUtil::VMConvertJsonStr2Map(jsonStr, strMap));
}

/**
 * @tc.name  : VMConvertVector2JsonStr_ShouldReturnTrue_WhenVectorIsEmpty
 * @tc.number: VMJsonUtilTest_VMConvertVector2JsonStr_001
 * @tc.desc  : Test if VMConvertVector2JsonStr returns true when input vector is empty.
 */
TEST_F(TestVmJsonUtil, VMConvertVector2JsonStr_ShouldReturnTrue_WhenVectorIsEmpty)
{
    JSON_VEC strVec;
    JSON_STR jsonStr;
    EXPECT_TRUE(VMJsonUtil::VMConvertVector2JsonStr(strVec, jsonStr));
    EXPECT_EQ(jsonStr, "[]");
}

/**
 * @tc.name  : VMConvertVector2JsonStr_ShouldReturnTrue_WhenVectorContainsValidJsonStrings
 * @tc.number: VMJsonUtilTest_VMConvertVector2JsonStr_002
 * @tc.desc  : Test if VMConvertVector2JsonStr returns true when input vector contains valid json strings.
 */
TEST_F(TestVmJsonUtil, VMConvertVector2JsonStr_ShouldReturnTrue_WhenVectorContainsValidJsonStrings)
{
    JSON_VEC strVec = {R"({"key":"value"})", R"({"key2":"value2"})", "value3"};
    JSON_STR jsonStr;
    EXPECT_TRUE(VMJsonUtil::VMConvertVector2JsonStr(strVec, jsonStr));
    EXPECT_EQ(jsonStr, "[\n    {\n        \"key\": \"value\"\n    },"
                       "\n    {\n        \"key2\": \"value2\"\n    },\n    \"value3\"\n]");
}

/**
 * @tc.name  : JsonStr2Vec_ShouldReturnTrue_WhenJsonStrIsArray
 * @tc.number: VMJsonUtilTest_VMConvertJsonStr2Vec_001
 * @tc.desc  : Test if VMConvertJsonStr2Vec returns true when jsonStr is an array.
 */
TEST_F(TestVmJsonUtil, JsonStr2Vec_ShouldReturnTrue_WhenJsonStrIsArray)
{
    JSON_STR jsonStr = R"(["value1", "value2"])";
    JSON_VEC strVec;
    EXPECT_TRUE(VMJsonUtil::VMConvertJsonStr2Vec(jsonStr, strVec));
    EXPECT_EQ(strVec.size(), 2);
    EXPECT_EQ(strVec[0], "value1");
    EXPECT_EQ(strVec[1], "value2");
}

/**
 * @tc.name  : JsonStr2Vec_ShouldReturnFalse_WhenJsonStrIsNotArray
 * @tc.number: VMJsonUtilTest_VMConvertJsonStr2Vec_002
 * @tc.desc  : Test if VMConvertJsonStr2Vec returns false when jsonStr is not an array.
 */
TEST_F(TestVmJsonUtil, JsonStr2Vec_ShouldReturnFalse_WhenJsonStrIsNotArray)
{
    JSON_STR jsonStr = R"({"key":"value"})";
    JSON_VEC strVec;
    EXPECT_FALSE(VMJsonUtil::VMConvertJsonStr2Vec(jsonStr, strVec));
}

/**
 * @tc.name  : JsonStr2Vec_ShouldReturnFalse_WhenJsonStrIsInvalid
 * @tc.number: VMJsonUtilTest_VMConvertJsonStr2Vec_003
 * @tc.desc  : Test if VMConvertJsonStr2Vec returns false when jsonStr is invalid.
 */
TEST_F(TestVmJsonUtil, JsonStr2Vec_ShouldReturnFalse_WhenJsonStrIsInvalid)
{
    JSON_STR jsonStr = "invalid json";
    JSON_VEC strVec;
    EXPECT_FALSE(VMJsonUtil::VMConvertJsonStr2Vec(jsonStr, strVec));
}

/**
 * @tc.name  : JsonStr2Vec_ShouldReturnFalse_WhenJsonStrIsEmpty
 * @tc.number: VMJsonUtilTest_VMConvertJsonStr2Vec_004
 * @tc.desc  : Test if VMConvertJsonStr2Vec returns false when jsonStr is empty.
 */
TEST_F(TestVmJsonUtil, JsonStr2Vec_ShouldReturnFalse_WhenJsonStrIsEmpty)
{
    JSON_STR jsonStr = "";
    JSON_VEC strVec;
    EXPECT_FALSE(VMJsonUtil::VMConvertJsonStr2Vec(jsonStr, strVec));
}

/**
 * @tc.name  : JsonStr2Vec_ShouldReturnTrue_WhenJsonStrIsArrayWithNestedJson
 * @tc.number: VMJsonUtilTest_VMConvertJsonStr2Vec_005
 * @tc.desc  : Test if VMConvertJsonStr2Vec returns true when jsonStr is an array with nested json.
 */
TEST_F(TestVmJsonUtil, JsonStr2Vec_ShouldReturnTrue_WhenJsonStrIsArrayWithNestedJson)
{
    JSON_STR jsonStr = R"(["value1", {"key":"value"}])";
    JSON_VEC strVec;
    EXPECT_TRUE(VMJsonUtil::VMConvertJsonStr2Vec(jsonStr, strVec));
    EXPECT_EQ(strVec.size(), 2);
    EXPECT_EQ(strVec[0], "value1");
    EXPECT_EQ(strVec[1], "{\n    \"key\": \"value\"\n}");
}

/**
 * @tc.name  : GetJsonString_ShouldReturnCorrectValue_WhenKeyExists
 * @tc.number: VMJsonUtilTest_GetJsonString_001
 * @tc.desc  : Test GetJsonString function when key exists in the json object
 */
TEST_F(TestVmJsonUtil, GetJsonString_ShouldReturnCorrectValue_WhenKeyExists)
{
    JSON_STR jsonStr = R"({"key":"value"})";
    Document jsonObject;
    jsonObject.Parse(jsonStr.c_str());

    JSON_STR result;
    uint32_t ret = VMJsonUtil::GetJsonString(jsonObject, "key", result);

    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(result, "value");
}

/**
 * @tc.name  : GetJsonString_ShouldReturnError_WhenKeyNotExists
 * @tc.number: VMJsonUtilTest_GetJsonString_002
 * @tc.desc  : Test GetJsonString function when key does not exist in the json object
 */
TEST_F(TestVmJsonUtil, GetJsonString_ShouldReturnError_WhenKeyNotExists)
{
    JSON_STR jsonStr = R"({"key":"value"})";
    Document jsonObject;
    jsonObject.Parse(jsonStr.c_str());

    JSON_STR result;
    uint32_t ret = VMJsonUtil::GetJsonString(jsonObject, "notExistKey", result);

    EXPECT_EQ(ret, VM_ERROR);
    EXPECT_EQ(result, "");
}

/**
 * @tc.name  : GetJsonString_ShouldReturnCorrectValue_WhenKeyIsNotString
 * @tc.number: VMJsonUtilTest_GetJsonString_003
 * @tc.desc  : Test GetJsonString function when key is not a string type
 */
TEST_F(TestVmJsonUtil, GetJsonString_ShouldReturnCorrectValue_WhenKeyIsNotString)
{
    JSON_STR jsonStr = R"({"key":123456})";
    Document jsonObject;
    jsonObject.Parse(jsonStr.c_str());

    JSON_STR result;
    uint32_t ret = VMJsonUtil::GetJsonString(jsonObject, "key", result);

    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(result, "123456");
}

/**
 * @tc.name  : GetJsonStringNotValid_ShouldReturnTrue_WhenJsonStringIsInvalid
 * @tc.number: VMJsonUtilTest_GetJsonStringNotValid_001
 * @tc.desc  : Test GetJsonStringNotValid function when json string is invalid
 */
TEST_F(TestVmJsonUtil, GetJsonStringNotValid_ShouldReturnTrue_WhenJsonStringIsInvalid)
{
    Value jsonBody(kObjectType);
    std::string value;

    EXPECT_TRUE(VMJsonUtil::GetJsonStringNotValid(jsonBody, "key", value));
    EXPECT_EQ(value, "");
}

/**
 * @tc.name  : GetJsonStringNotValid_ShouldReturnTrue_WhenJsonStringIsEmpty
 * @tc.number: VMJsonUtilTest_GetJsonStringNotValid_002
 * @tc.desc  : Test GetJsonStringNotValid function when json string is empty
 */
TEST_F(TestVmJsonUtil, GetJsonStringNotValid_ShouldReturnTrue_WhenJsonStringIsEmpty)
{
    JSON_STR jsonStr = R"({"key":""})";
    Document jsonBody;
    jsonBody.Parse(jsonStr.c_str());

    std::string value;
    EXPECT_TRUE(VMJsonUtil::GetJsonStringNotValid(jsonBody, "key", value));
}

/**
 * @tc.name  : GetJsonStringNotValid_ShouldReturnFalse_WhenJsonStringIsValid
 * @tc.number: VMJsonUtilTest_GetJsonStringNotValid_003
 * @tc.desc  : Test GetJsonStringNotValid function when json string is valid
 */
TEST_F(TestVmJsonUtil, GetJsonStringNotValid_ShouldReturnFalse_WhenJsonStringIsValid)
{
    JSON_STR jsonStr = R"({"key":"value"})";
    Document jsonBody;
    jsonBody.Parse(jsonStr.c_str());

    std::string value;
    EXPECT_FALSE(VMJsonUtil::GetJsonStringNotValid(jsonBody, "key", value));
    EXPECT_EQ(value, "value");
}
} // namespace ubse::ut::vm