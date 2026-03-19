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

#include "over_commit_serializer.h"
#include "rmrs_serialize.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::over_commit {
using namespace mempooling;
using namespace rmrs::serialize;

// 测试类
class TestOverCommitSerializerModule : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestOverCommitSerializerModule, PidNumaInfoCollectParam_Serialize_Succeed)
{
    std::vector<pid_t> pidList{1, 2};

    PidNumaInfoCollectParam param1(pidList);
    RmrsOutStream outBuilder;
    outBuilder << param1;
    RmrsInStream inBuilder(outBuilder.GetBufferPointer(), outBuilder.GetSize());
    PidNumaInfoCollectParam param2;
    inBuilder >> param2;
    EXPECT_EQ(param1.pidList[0], param2.pidList[0]);
    EXPECT_EQ(param1.pidList[1], param2.pidList[1]);
}

TEST_F(TestOverCommitSerializerModule, PidNumaInfoCollectResult_Serialize_Succeed)
{
    using mempooling::RmrsPidInfo;
    using mempooling::MetaNumaInfo;

    std::vector<mempooling::RmrsPidInfo> pidInfoList;

    // -------- pid1 --------
    mempooling::RmrsPidInfo p1;
    p1.pid = 1;
    p1.localNumaIds = {0};
    p1.totalLocalUsedMem = 1024;
    p1.totalRemoteUsedMem = 0;
    p1.remoteNumaId = 1;
    p1.metaNumaInfos = {
        MetaNumaInfo{0, 512, true, 0},
        MetaNumaInfo{1, 512, false, 1},
    };

    // -------- pid2 --------
    mempooling::RmrsPidInfo p2;
    p2.pid = 2;
    p2.localNumaIds = {0};
    p2.totalLocalUsedMem = 2048;
    p2.totalRemoteUsedMem = 0;
    p2.remoteNumaId = 1;
    p2.metaNumaInfos = {
        MetaNumaInfo{0, 2048, true, 0},
    };

    pidInfoList.push_back(p1);
    pidInfoList.push_back(p2);

    // ---------- Serialize ----------
    PidNumaInfoCollectResult param1(pidInfoList);

    RmrsOutStream outBuilder;
    outBuilder << param1;

    // ---------- Deserialize ----------
    RmrsInStream inBuilder(outBuilder.GetBufferPointer(),
                           outBuilder.GetSize());

    PidNumaInfoCollectResult param2;
    inBuilder >> param2;

    // ---------- Verify basic fields ----------
    ASSERT_EQ(param1.pidInfoList.size(), param2.pidInfoList.size());

    for (size_t i = 0; i < param1.pidInfoList.size(); ++i) {
        const auto &a = param1.pidInfoList[i];
        const auto &b = param2.pidInfoList[i];

        EXPECT_EQ(a.pid, b.pid);
        EXPECT_EQ(a.localNumaIds, b.localNumaIds);
        EXPECT_EQ(a.totalLocalUsedMem, b.totalLocalUsedMem);
        EXPECT_EQ(a.totalRemoteUsedMem, b.totalRemoteUsedMem);
        EXPECT_EQ(a.remoteNumaId, b.remoteNumaId);

        // ---------- Verify metaNumaInfos ----------
        ASSERT_EQ(a.metaNumaInfos.size(), b.metaNumaInfos.size());

        for (size_t j = 0; j < a.metaNumaInfos.size(); ++j) {
            const auto &ma = a.metaNumaInfos[j];
            const auto &mb = b.metaNumaInfos[j];

            EXPECT_EQ(ma.numaId, mb.numaId);
            EXPECT_EQ(ma.numaUsedMem, mb.numaUsedMem);
            EXPECT_EQ(ma.isLocalNuma, mb.isLocalNuma);
            EXPECT_EQ(ma.socketId, mb.socketId);
        }
    }
}

} // namespace mempooling::over_commit