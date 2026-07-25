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
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "mp_json_util.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
namespace mempooling {

class TestMpJsonUtil : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestMpJsonUtil SetUp Begin]" << endl;
        cout << "[TestMpJsonUtil SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestMpJsonUtil TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestMpJsonUtil TearDown End]" << endl;
    }
};

TEST_F(TestMpJsonUtil, RackMemConvertMap2JsonStrSucceed)
{
    JSON_MAP strMap1;
    JSON_STR jsonStr1;
    strMap1["a"] = "b";
    bool res = JsonUtil::RackMemConvertMap2JsonStr(strMap1, jsonStr1);
    EXPECT_EQ(res, true);
}

TEST_F(TestMpJsonUtil, RackMemConvertVector2JsonStrSucceed)
{
    JSON_VEC vec1;
    JSON_STR jsonStr1;
    vec1.push_back("a");
    bool res = JsonUtil::RackMemConvertVector2JsonStr(vec1, jsonStr1);
    EXPECT_EQ(res, true);
}

TEST_F(TestMpJsonUtil, RackMemConvertJsonStr2VecSucceed)
{
    JSON_VEC vec1;
    JSON_VEC vec2;
    JSON_STR jsonStr1;
    vec1.push_back("a");
    bool res = JsonUtil::RackMemConvertVector2JsonStr(vec1, jsonStr1);
    EXPECT_EQ(res, true);
    res = JsonUtil::RackMemConvertJsonStr2Vec(jsonStr1, vec2);
    EXPECT_EQ(res, true);
}

TEST_F(TestMpJsonUtil, RackMemConvertJsonStr2MapSucceed)
{
    JSON_MAP map1;
    JSON_STR jsonStr1;
    map1["a"] = "b";
    bool res = JsonUtil::RackMemConvertMap2JsonStr(map1, jsonStr1);
    EXPECT_EQ(res, true);
    res = JsonUtil::RackMemConvertJsonStr2Map(jsonStr1, map1);
    EXPECT_EQ(res, true);
}

// =====================================================================
// 1. GetJsonStringValueWithoutCheck testing// =====================================================================
TEST_F(TestMpJsonUtil, GetJsonStringValueWithoutCheck_SucceedAndFail)
{
    rapidjson::Document doc;
    doc.Parse(R"({"str_key":"hello", "num_key":1234})");
    std::string out;

    // Normal string retrieval
    EXPECT_TRUE(JsonUtil::GetJsonStringValueWithoutCheck(doc, "str_key", out));
    EXPECT_EQ(out, "hello");

    // Retrieval of non-string type (will be converted to string output by PrintJsonString)
    EXPECT_TRUE(JsonUtil::GetJsonStringValueWithoutCheck(doc, "num_key", out));
    EXPECT_EQ(out, "1234");

    // Exception branch: key is null or does not exist
    EXPECT_FALSE(JsonUtil::GetJsonStringValueWithoutCheck(doc, nullptr, out));
    EXPECT_FALSE(JsonUtil::GetJsonStringValueWithoutCheck(doc, "not_exist", out));
}

// =====================================================================
// 2. GetJsonStringValue (Document & Value) testing// =====================================================================
TEST_F(TestMpJsonUtil, GetJsonStringValue_SucceedAndFail)
{
    rapidjson::Document doc;
    doc.Parse(R"({"str_key":"hello", "num_key":1234})");
    std::string out;

    // Document interface
    EXPECT_TRUE(JsonUtil::GetJsonStringValue(doc, "str_key", out));
    EXPECT_EQ(out, "hello");
    EXPECT_FALSE(JsonUtil::GetJsonStringValue(doc, "num_key", out)); // Type error
    EXPECT_FALSE(JsonUtil::GetJsonStringValue(doc, nullptr, out));
    EXPECT_FALSE(JsonUtil::GetJsonStringValue(doc, "not_exist", out));

    // Value interface
    const rapidjson::Value& val = doc;
    EXPECT_TRUE(JsonUtil::GetJsonStringValue(val, "str_key", out));
    EXPECT_FALSE(JsonUtil::GetJsonStringValue(
        val, "num_key", out)); // Type error    EXPECT_FALSE(JsonUtil::GetJsonStringValue(val, nullptr, out));
    EXPECT_FALSE(JsonUtil::GetJsonStringValue(val, "not_exist", out));
}

// =====================================================================
// 3. GetJsonUint64Value testing// =====================================================================
TEST_F(TestMpJsonUtil, GetJsonUint64Value_SucceedAndFail)
{
    rapidjson::Document doc;
    doc.Parse(R"({"uint_key":123456789012345, "str_key":"123"})");
    uint64_t out = 0;

    EXPECT_TRUE(JsonUtil::GetJsonUint64Value(doc, "uint_key", out));
    EXPECT_EQ(out, 123456789012345);

    EXPECT_FALSE(JsonUtil::GetJsonUint64Value(
        doc, "str_key", out)); // Type error    EXPECT_FALSE(JsonUtil::GetJsonUint64Value(doc, nullptr, out));
    EXPECT_FALSE(JsonUtil::GetJsonUint64Value(doc, "not_exist", out));
}

// =====================================================================
// 4. Numeric type boundary test (Uint16, Int, Int16)
// =====================================================================
TEST_F(TestMpJsonUtil, NumberBoundary_SucceedAndFail)
{
    rapidjson::Document doc;
    // Construct data exactly at boundary and overflow
    // INT16_MAX = 32767, INT16_MIN = -32768, UINT16_MAX = 65535
    doc.Parse(R"({
        "valid_u16": 65535,
        "invalid_u16": 65536,
        "valid_i16": 32767,
        "invalid_i16_max": 32768,
        "invalid_i16_min": -32769,
        "valid_int": 2147483647,
        "str_key": "123"
    })");

    uint16_t out_u16;
    int16_t out_i16;
    int out_int;

    // Uint16
    EXPECT_TRUE(JsonUtil::GetJsonUint16Value(doc, "valid_u16", out_u16));
    EXPECT_FALSE(JsonUtil::GetJsonUint16Value(doc, "invalid_u16", out_u16)); // Out of range
    EXPECT_FALSE(JsonUtil::GetJsonUint16Value(doc, "str_key", out_u16));     // Type error
    // Int16
    EXPECT_TRUE(JsonUtil::GetJsonInt16Value(doc, "valid_i16", out_i16));
    EXPECT_FALSE(JsonUtil::GetJsonInt16Value(
        doc, "invalid_i16_max",
        out_i16)); // Upper limit overflow    EXPECT_FALSE(JsonUtil::GetJsonInt16Value(doc, "invalid_i16_min", out_i16)); // Lower limit overflow    EXPECT_FALSE(JsonUtil::GetJsonInt16Value(doc, "str_key", out_i16));         // Type error
    // Int
    EXPECT_TRUE(JsonUtil::GetJsonIntValue(doc, "valid_int", out_int));
    EXPECT_FALSE(JsonUtil::GetJsonIntValue(doc, "str_key", out_int));
    EXPECT_FALSE(JsonUtil::GetJsonIntValue(doc, nullptr, out_int));
}

// =====================================================================
// 5. RackMemGetJsonItemStr test// =====================================================================
TEST_F(TestMpJsonUtil, RackMemGetJsonItemStr_SucceedAndFail)
{
    rapidjson::Document doc;
    doc.Parse(R"({"obj_key":{"inner_key":"inner_val"}})");
    JSON_STR out;

    // Normal retrieval of sub-node JSON string
    EXPECT_TRUE(JsonUtil::RackMemGetJsonItemStr(doc, "obj_key", out));
    // Serialized result usually has no extra spaces, format like {"inner_key":"inner_val"}
    EXPECT_TRUE(out.find("inner_val") != std::string::npos);

    // Exception branch
    EXPECT_FALSE(JsonUtil::RackMemGetJsonItemStr(doc, nullptr, out));
    EXPECT_FALSE(JsonUtil::RackMemGetJsonItemStr(doc, "not_exist", out));
}

// =====================================================================
// 6. Convert function abnormal branch test// =====================================================================
TEST_F(TestMpJsonUtil, ConvertAbnormalCases)
{
    JSON_MAP map1;
    JSON_VEC vec1;

    // JsonStr2Map: Invalid JSON or not Object type
    EXPECT_FALSE(JsonUtil::RackMemConvertJsonStr2Map("invalid_json", map1));
    EXPECT_FALSE(JsonUtil::RackMemConvertJsonStr2Map("[]", map1));

    // JsonStr2Map: JSON doesn't have keys pre-filled in map1
    map1["must_exist_key"] = "";
    EXPECT_FALSE(JsonUtil::RackMemConvertJsonStr2Map(R"({"other_key":"val"})", map1));

    // JsonStr2Vec: Invalid JSON or not Array type
    EXPECT_FALSE(JsonUtil::RackMemConvertJsonStr2Vec("invalid_json", vec1));
    EXPECT_FALSE(JsonUtil::RackMemConvertJsonStr2Vec("{}", vec1));
}
} // namespace mempooling