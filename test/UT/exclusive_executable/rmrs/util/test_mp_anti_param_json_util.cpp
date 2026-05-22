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
#include <gtest/gtest.h>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "iostream"
#include "securec.h"

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include "mp_anti_param_json_util.h"
#undef private
#include "mp_configuration.h"
#include "mp_json_util.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {
using namespace std;

class TestMpUpdateAntiNodeParam : public ::testing::Test {
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

std::vector<std::string> MockTestGetNodeIds()
{
    std::vector<std::string> allNodeIdList = {"1", "2"};
    return allNodeIdList;
}

bool MockRackMemConvertJsonStr2Map(const JSON_STR& jsonStr, JSON_MAP& strMap)
{
    strMap["1"] = "{2}";
    strMap["2"] = "{1}";
    return true;
}

std::vector<std::string> MockTestGetNodeIds1()
{
    std::vector<std::string> allNodeIdList = {"1"};
    return allNodeIdList;
}

bool MockRackMemConvertJsonStr2Map1(const JSON_STR& jsonStr, JSON_MAP& strMap)
{
    strMap["1"] = "{2}";
    return true;
}

bool MockRackMemConvertJsonStr2Vec1(const JSON_STR& jsonStr, JSON_VEC& strVec)
{
    strVec.emplace_back("2");
    return true;
}

TEST_F(TestMpUpdateAntiNodeParam, AntiFromJsonSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string> (*)()).stubs().will(invoke(MockTestGetNodeIds1));
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(invoke(MockRackMemConvertJsonStr2Map1));
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR& jsonStr, JSON_VEC& strVec))
        .stubs()
        .will(invoke(MockRackMemConvertJsonStr2Vec1));
    MpUpdateAntiNodeParam param;
    std::string jsonString = R"({"nodeAntiAffinityMap":{"1":["2"],"2":["1"]}})";
    EXPECT_EQ(param.FromJson(jsonString), true);
}

TEST_F(TestMpUpdateAntiNodeParam, AntiFromJsonFailed1)
{
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string> (*)()).stubs().will(invoke(MockTestGetNodeIds));
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(returnValue(false));
    MpUpdateAntiNodeParam param;
    std::string jsonString = R"({"nodeAntiAffinityMap":{"1":["2"],"2":["1"]}})";
    EXPECT_EQ(param.FromJson(jsonString), false);
}

TEST_F(TestMpUpdateAntiNodeParam, AntiToJsonSuccess)
{
    MOCKER_CPP(&MpConfiguration::GetNodeIds, std::vector<std::string> (*)()).stubs().will(invoke(MockTestGetNodeIds));
    MpUpdateAntiNodeParam param;
    param.nodeAntiAffinityMap = {{"1", {"2"}}, {"2", {"1"}}};
    param.ToJson();
}

} // namespace mempooling