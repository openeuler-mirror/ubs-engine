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

}