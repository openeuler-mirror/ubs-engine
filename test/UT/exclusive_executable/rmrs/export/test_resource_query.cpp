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
#include <mockcpp/mockcpp.hpp>
#include "gtest/gtest.h"

#include <rmrs_serialize.h>
#include <securec.h>
#include "event_handler.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_json_util.h"
#include "mp_string_util.h"
#include "numa_memInfo_send.h"
#include "over_commit_msg_handler.h"
#include "over_commit_serializer.h"
#include "ubse_pointer_process.h"
#include "ubse_com.h"
#include "ubse_event.h"

#define private public
#include "rmrs_resource_query.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {
using namespace ubse::log;
using namespace mempooling::event;

MpResult FillNumaInfo(mempooling::NumaMetaData &numaInfo, JSON_MAP numaInfoStrMap);

// 测试类
class TestResourceQuery : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestResourceQuery, FillNumaInfo_EmptyMap)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMapEmpty{};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMapEmpty);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_OnlyMemTotal)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {{"MemTotal", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_MemTotalMemFree)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {{"MemTotal", "0"}, {"MemFree", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_WithHugePagesTotal)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {
        {"MemTotal", "0"}, {"MemFree", "0"}, {"HugePages_Total", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_WithHugePagesFree)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {
        {"MemTotal", "0"}, {"MemFree", "0"}, {"HugePages_Total", "0"}, {"HugePages_Free", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_WithSocketId)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {
        {"MemTotal", "0"}, {"MemFree", "0"}, {"HugePages_Total", "0"},
        {"HugePages_Free", "0"}, {"SocketId", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestResourceQuery, HelpGetNumaMemInfoCollect_SendFail)
{
    std::string srcNid = "0";
    int numaId = 0;
    NumaMetaData numaInfo{};

    MOCKER_CPP(&over_commit::NumaMemInfoSend::SendMsg, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_ERROR));

    uint32_t ret = ResourceQuery::HelpGetNumaMemInfoCollect(srcNid, numaId, numaInfo);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, HelpGetNumaMemInfoCollect_SendOK_JsonFail)
{
    std::string srcNid = "0";
    int numaId = 0;
    NumaMetaData numaInfo{};

    MOCKER_CPP(&over_commit::NumaMemInfoSend::SendMsg, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(&JsonUtil::RackMemConvertJsonStr2Map).stubs().will(returnValue(false));

    uint32_t ret = ResourceQuery::HelpGetNumaMemInfoCollect(srcNid, numaId, numaInfo);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, HelpGetNumaMemInfoCollect_SendOK_JsonOK_FillFail)
{
    std::string srcNid = "0";
    int numaId = 0;
    NumaMetaData numaInfo{};

    MOCKER_CPP(&over_commit::NumaMemInfoSend::SendMsg, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(&JsonUtil::RackMemConvertJsonStr2Map).stubs().will(returnValue(true));
    MOCKER(&FillNumaInfo).stubs().will(returnValue(MEM_POOLING_ERROR));

    uint32_t ret = ResourceQuery::HelpGetNumaMemInfoCollect(srcNid, numaId, numaInfo);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, HelpGetNumaMemInfoCollect_Success)
{
    std::string srcNid = "0";
    int numaId = 0;
    NumaMetaData numaInfo{};

    MOCKER_CPP(&over_commit::NumaMemInfoSend::SendMsg, MpResult(*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(&JsonUtil::RackMemConvertJsonStr2Map).stubs().will(returnValue(true));
    MOCKER(&FillNumaInfo).stubs().will(returnValue(MEM_POOLING_OK));

    uint32_t ret = ResourceQuery::HelpGetNumaMemInfoCollect(srcNid, numaId, numaInfo);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}
} // namespace mempooling