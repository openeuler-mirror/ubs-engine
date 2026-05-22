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

#include "mp_sync_data_helper.h"
#include "securec.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::sync {

// 测试类
class TestSync : public ::testing::Test {
public:
    void SetUp() override
    {
        std::cout << "[TestSync SetUp Begin]" << std::endl;
        std::cout << "[TestSync SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[TestSync TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        std::cout << "[TestSync TearDown End]" << std::endl;
    }
};

TEST_F(TestSync, InitSucceed)
{
    auto ret = MpSyncDataHelper::Instance().Init();
    EXPECT_EQ(ret, 0);
}

uint32_t TestNotify(UbseByteBuffer& buffer)
{
    return 0;
}

uint32_t TestGetData(UbseByteBuffer& buffer)
{
    return 0;
}

TEST_F(TestSync, DeInitFailed1)
{
    MpResult ret = MpSyncDataHelper::Instance().DeInit();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestSync, DeInitSucceed)
{
    auto ret = MpSyncDataHelper::Instance().DeInit();
    EXPECT_EQ(ret, 0);
}

uint32_t MockGenerateIndexGet1(std::string serviceName, uint64_t& index)
{
    index = 1;
    return 0;
}

uint32_t MockNotify(UbseByteBuffer& buffer)
{
    return 0;
}

uint32_t MockGetData(UbseByteBuffer& buffer)
{
    return 0;
}

TEST_F(TestSync, SubModuleInitFailed1)
{
    MOCKER_CPP(&MpSyncDataHelper::Init, uint32_t (*)(MpSyncDataHelper*)).stubs().will(returnValue(1));
    MpSyncDataSubModule obj;
    auto ret = obj.Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestSync, SubModuleInitSucceed)
{
    MOCKER_CPP(&MpSyncDataHelper::Init, uint32_t (*)(MpSyncDataHelper*)).stubs().will(returnValue(0));
    MpSyncDataSubModule obj;
    auto ret = obj.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

} // namespace mempooling::sync