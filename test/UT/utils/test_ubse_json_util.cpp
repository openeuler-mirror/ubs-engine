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

#include "test_ubse_json_util.h"

#include "ubse_json_util.h"

namespace ubse::ut::utils {
using namespace ubse::utils;

void TestUbseJsonUtil::SetUp()
{
    Test::SetUp();
}

void TestUbseJsonUtil::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseJsonUtil, GetStrFromJsonPtr)
{
    std::string str = R"({"test": "test"})";
    std::string value{};
    EXPECT_EQ(UbseJsonUtil::GetStrFromJsonPtr(str, "test", value), UBSE_OK);
    EXPECT_EQ(value, "test");

    str = R"({"test": 1})";
    EXPECT_NE(UbseJsonUtil::GetStrFromJsonPtr(str, "test", value), UBSE_OK);
}

TEST_F(TestUbseJsonUtil, GetUintFromJsonPtr)
{
    std::string str = R"({"test": 1})";
    rapidjson::Document doc;
    doc.Parse(str.c_str());
    uint32_t value{};
    EXPECT_EQ(UbseJsonUtil::GetUintFromJsonPtr(doc, "test", value), UBSE_OK);
    EXPECT_EQ(value, 1);

    str = R"({"test": "1"})";
    doc.Parse(str.c_str());
    EXPECT_NE(UbseJsonUtil::GetUintFromJsonPtr(doc, "test", value), UBSE_OK);
}

TEST_F(TestUbseJsonUtil, GetIntFromJsonPtr)
{
    std::string str = R"({"test": 1})";
    rapidjson::Document doc;
    doc.Parse(str.c_str());
    int value{};
    EXPECT_EQ(UbseJsonUtil::GetIntFromJsonPtr(doc, "test", value), UBSE_OK);
    EXPECT_EQ(value, 1);

    str = R"({"test": "1"})";
    doc.Parse(str.c_str());
    EXPECT_NE(UbseJsonUtil::GetIntFromJsonPtr(doc, "test", value), UBSE_OK);
}

TEST_F(TestUbseJsonUtil, GetBoolFromJsonPtr)
{
    std::string str = R"({"test": true})";
    rapidjson::Document doc;
    doc.Parse(str.c_str());
    bool value{};
    EXPECT_EQ(UbseJsonUtil::GetBoolFromJsonPtr(doc, "test", value), UBSE_OK);
    EXPECT_TRUE(value);

    str = R"({"test": "1"})";
    doc.Parse(str.c_str());
    EXPECT_NE(UbseJsonUtil::GetBoolFromJsonPtr(doc, "test", value), UBSE_OK);
}

TEST_F(TestUbseJsonUtil, GetUint64FromJsonPtr)
{
    std::string str = R"({"test": 1})";
    rapidjson::Document doc;
    doc.Parse(str.c_str());
    uint64_t value{};
    EXPECT_EQ(UbseJsonUtil::GetUint64FromJsonPtr(doc, "test", value), UBSE_OK);
    EXPECT_EQ(value, 1);

    str = R"({"test": "1"})";
    doc.Parse(str.c_str());
    EXPECT_NE(UbseJsonUtil::GetUint64FromJsonPtr(doc, "test", value), UBSE_OK);
}

TEST_F(TestUbseJsonUtil, GetDoubleFromJsonPtr)
{
    std::string str = R"({"test": 1.0})";
    rapidjson::Document doc;
    doc.Parse(str.c_str());
    double value{};
    EXPECT_EQ(UbseJsonUtil::GetDoubleFromJsonPtr(doc, "test", value), UBSE_OK);
    EXPECT_DOUBLE_EQ(value, 1.0);

    str = R"({"test": "1"})";
    doc.Parse(str.c_str());
    EXPECT_NE(UbseJsonUtil::GetDoubleFromJsonPtr(doc, "test", value), UBSE_OK);
}

TEST_F(TestUbseJsonUtil, GetFloatFromJsonPtr)
{
    std::string str = R"({"test": 1.0})";
    rapidjson::Document doc;
    doc.Parse(str.c_str());
    float value{};
    EXPECT_EQ(UbseJsonUtil::GetFloatFromJsonPtr(doc, "test", value), UBSE_OK);
    EXPECT_FLOAT_EQ(value, 1.0);

    str = R"({"test": "1"})";
    doc.Parse(str.c_str());
    EXPECT_NE(UbseJsonUtil::GetFloatFromJsonPtr(doc, "test", value), UBSE_OK);
}

TEST_F(TestUbseJsonUtil, GetArrayFromJsonPtr)
{
    std::string str = R"({"test": ["test"]})";
    rapidjson::Document doc;
    doc.Parse(str.c_str());
    rapidjson::Document pJson(rapidjson::kObjectType);
    auto& allocator = pJson.GetAllocator();
    rapidjson::Value value{};
    EXPECT_EQ(UbseJsonUtil::GetArrayFromJsonPtr(doc, "test", allocator, value), UBSE_OK);
    EXPECT_TRUE(value.IsArray());

    str = R"({"test": {"test": "test"}})";
    doc.Parse(str.c_str());
    EXPECT_NE(UbseJsonUtil::GetArrayFromJsonPtr(doc, "test", allocator, value), UBSE_OK);
}

TEST_F(TestUbseJsonUtil, GetObjectFromJsonPtr)
{
    std::string str = R"({"test": {"test": "test"}})";
    rapidjson::Document doc;
    doc.Parse(str.c_str());
    rapidjson::Document pJson(rapidjson::kObjectType);
    auto& allocator = pJson.GetAllocator();
    rapidjson::Value value{};
    EXPECT_EQ(UbseJsonUtil::GetObjectFromJsonPtr(doc, "test", allocator, value), UBSE_OK);
    EXPECT_TRUE(value.IsObject());

    str = R"({"test": ["test"]})";
    doc.Parse(str.c_str());
    EXPECT_NE(UbseJsonUtil::GetObjectFromJsonPtr(doc, "test", allocator, value), UBSE_OK);
}

TEST_F(TestUbseJsonUtil, GetStrFromJsonPtrForce)
{
    std::string str = R"({"test": 1.1})";
    rapidjson::Document doc;
    doc.Parse(str.c_str());
    std::string value{};
    EXPECT_EQ(UbseJsonUtil::GetStrFromJsonPtrForce(doc, "test", value), UBSE_OK);
    EXPECT_EQ(value, "1.100000");
}

TEST_F(TestUbseJsonUtil, PrintJsonPtr)
{
    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("test", rapidjson::Value("test").Move(), doc.GetAllocator());
    std::string value{};
    EXPECT_EQ(UbseJsonUtil::PrintJsonPtr(doc, value), UBSE_OK);
    EXPECT_EQ(value, R"({"test":"test"})");
}

TEST_F(TestUbseJsonUtil, ConvertVector2JsonStr)
{
    std::vector<std::string> vectors{};
    vectors.emplace_back("test");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::ConvertVector2JsonStr(vectors, value));
    EXPECT_EQ(value, R"(["test"])");
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrStringValue)
{
    rapidjson::Document doc;
    doc.Parse(R"({"test": "hello world"})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
    EXPECT_EQ(value, "hello world");
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrNumberValue)
{
    rapidjson::Document doc;
    doc.Parse(R"({"test": 12345})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
    EXPECT_EQ(value, "12345");
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrDoubleValue)
{
    rapidjson::Document doc;
    doc.Parse(R"({"test": 3.14})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
    EXPECT_TRUE(value.find("3.14") != std::string::npos);
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrBoolValue)
{
    rapidjson::Document doc;
    doc.Parse(R"({"test": true})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
    EXPECT_EQ(value, "true");
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrNullValue)
{
    rapidjson::Document doc;
    doc.Parse(R"({"test": null})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
    EXPECT_EQ(value, "null");
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrObjectValue)
{
    rapidjson::Document doc;
    doc.Parse(R"({"test": {"inner": "value"}})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
    EXPECT_EQ(value, R"({"inner":"value"})");
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrArrayValue)
{
    rapidjson::Document doc;
    doc.Parse(R"({"test": [1, 2, 3]})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
    EXPECT_EQ(value, "[1,2,3]");
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrEmptyArray)
{
    rapidjson::Document doc;
    doc.Parse(R"({"test": []})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
    EXPECT_EQ(value, "[]");
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrStringArray)
{
    rapidjson::Document doc;
    doc.Parse(R"({"test": ["a", "b", "c"]})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
    EXPECT_EQ(value, R"(["a","b","c"])");
}

TEST_F(TestUbseJsonUtil, GetJsonItemStrFailedNoKey)
{
    rapidjson::Document doc;
    doc.Parse(R"({"other": "value"})");
    std::string value{};
    EXPECT_FALSE(UbseJsonUtil::GetJsonItemStr(doc, "test", value));
}

TEST_F(TestUbseJsonUtil, GetJsonItemStr_EmptyKey)
{
    rapidjson::Document doc;
    doc.Parse(R"({"": "empty_key_value"})");
    std::string value{};
    EXPECT_TRUE(UbseJsonUtil::GetJsonItemStr(doc, "", value));
    EXPECT_EQ(value, "empty_key_value");
}

TEST_F(TestUbseJsonUtil, ConvertMapAndJsonStr)
{
    std::map<std::string, std::string> originalMap;
    originalMap["key1"] = "value1";
    originalMap["key2"] = "value2";

    std::string jsonStr;
    EXPECT_TRUE(UbseJsonUtil::ConvertMap2JsonStr(originalMap, jsonStr));

    std::map<std::string, std::string> parsedMap;
    parsedMap["key1"] = "";
    parsedMap["key2"] = "";
    EXPECT_TRUE(UbseJsonUtil::ConvertJsonStr2Map(jsonStr, parsedMap));
    EXPECT_EQ(parsedMap["key1"], "value1");
    EXPECT_EQ(parsedMap["key2"], "value2");
}

TEST_F(TestUbseJsonUtil, ConvertVectorAndJsonStr)
{
    std::vector<std::string> originalVec = {"item1", "item2", "item3"};
    std::string jsonStr;
    EXPECT_TRUE(UbseJsonUtil::ConvertVector2JsonStr(originalVec, jsonStr));

    std::vector<std::string> parsedVec;
    EXPECT_TRUE(UbseJsonUtil::ConvertJsonStr2Vec(jsonStr, parsedVec));
    EXPECT_EQ(parsedVec.size(), originalVec.size());
    for (size_t i = 0; i < originalVec.size(); ++i) {
        EXPECT_EQ(parsedVec[i], originalVec[i]);
    }
}
} // namespace ubse::ut::utils
