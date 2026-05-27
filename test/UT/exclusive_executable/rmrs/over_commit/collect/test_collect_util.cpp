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
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "ubse_com.h"
#define private public
#undef private
#include "collect_util.h"
#include "mp_json_util.h"
#include "rmrs_resource_query.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace mempooling::over_commit {
using namespace mempooling;
using namespace ubse::com;
// 测试类
class TestCollectUtil : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestCollectUtil, GetNumaMemInfosSuccess)
{
    MOCKER_CPP(&ResourceQuery::HelpGetNumaMemInfoCollect,
               uint32_t(*)(const std::string& srcNid, const int& numaId, mempooling::NumaMetaData& numaInfo))
        .stubs()
        .will(returnValue(0));
    std::set<int16_t> numaIds{1};
    std::map<int, mempooling::NumaMetaData> numaMemInfos;
    auto ret = CollectUtil::GetNumaMemInfos("node1", numaIds, numaMemInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestCollectUtil, GetNumaMemInfosFail)
{
    MOCKER_CPP(&ResourceQuery::HelpGetNumaMemInfoCollect,
               uint32_t(*)(const std::string& srcNid, const int& numaId, mempooling::NumaMetaData& numaInfo))
        .stubs()
        .will(returnValue(1));
    std::set<int16_t> numaIds{1};
    std::map<int, mempooling::NumaMetaData> numaMemInfos;
    auto ret = CollectUtil::GetNumaMemInfos("node1", numaIds, numaMemInfos);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

bool MockRackMemConvertJsonStr2Map(const JSON_STR& jsonStr, JSON_MAP& strMap)
{
    strMap["1"] = "{123}";
    return true;
}

TEST_F(TestCollectUtil, GetRemoteVmPidsFail)
{
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(invoke(MockRackMemConvertJsonStr2Map));
    std::vector<uint32_t> remoteNumaIds = {1};
    std::unordered_map<std::uint16_t, std::vector<pid_t>> res;
    auto ret = CollectUtil::GetRemoteVmPids("node1", remoteNumaIds, res);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

bool MockRackMemConvertJsonStr2Vec(const JSON_STR& jsonStr, JSON_VEC& strVec)
{
    strVec.emplace_back("123");
    return true;
}

TEST_F(TestCollectUtil, GetRemoteVmPidsSuccess)
{
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(invoke(MockRackMemConvertJsonStr2Map));
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR& jsonStr, JSON_VEC& strVec))
        .stubs()
        .will(invoke(MockRackMemConvertJsonStr2Vec));
    std::vector<uint32_t> remoteNumaIds = {1};
    std::unordered_map<std::uint16_t, std::vector<pid_t>> res;
    auto ret = CollectUtil::GetRemoteVmPids("node1", remoteNumaIds, res);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

} // namespace mempooling::over_commit