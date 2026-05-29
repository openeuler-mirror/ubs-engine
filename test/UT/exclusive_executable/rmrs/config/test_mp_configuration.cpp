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
#include "ubse_conf.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "mp_error.h"

#define private public
#include "mp_configuration.h"
#undef private
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;

namespace mempooling {

class TestMpConfigration : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestResourceQuery SetUp Begin]" << endl;
        cout << "[TestResourceQuery SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestResourceQuery TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestResourceQuery TearDown End]" << endl;
    }
};

uint32_t MockUbseGetUIntForLoadUCacheOK(const std::string& section, const std::string& configKey, uint32_t& configValue)
{
    configValue = 25;
    return 0;
}

uint32_t MockUbseGetUIntForLoadUCacheOutOfRange(const std::string& section, const std::string& configKey,
                                                uint32_t& configValue)
{
    configValue = -1;
    return 0;
}

TEST_F(TestMpConfigration, GetInstance)
{
    auto& instance1 = MpConfiguration::GetInstance();
    auto& instance2 = MpConfiguration::GetInstance();

    // 断言两个实例相同
    ASSERT_EQ(&instance1, &instance2);

    MpConfiguration mpConfiguration;
    mpConfiguration.nodeId = "nodeId0";
    mpConfiguration.nodeIds = "nodeId0,nodeId1";

    std::string str1 = mpConfiguration.GetNodeId();
    std::vector<std::string> str2 = mpConfiguration.GetNodeIds();

    uint16_t modCode = 0;
    MOCKER(ubse::config::UbseGetStr).stubs().will(returnValue(1));
    MOCKER(ubse::config::UbseGetUInt).stubs().will(returnValue(1));
    uint32_t result = mpConfiguration.Initialize(modCode);
}

uint32_t UbseGetUIntZero(const std::string& section, const std::string& configKey, uint32_t& configValue)
{
    configValue = 0;
    return 0;
}

uint32_t UbseGetUIntReturnError(const std::string& section, const std::string& configKey, uint32_t& configValue)
{
    configValue = 0;
    return 1;
}

uint32_t UbseGetBoolReturnError(const std::string& section, const std::string& configKey, bool& configValue)
{
    configValue = false;
    return 0;
}

TEST_F(TestMpConfigration, InitializeWithVIRTUAL_SCENE)
{
    MpConfiguration mpConfiguration;
    mpConfiguration.nodeId = "0";
    mpConfiguration.nodeIds = "0,1";

    uint16_t modCode = 0;
    MOCKER(ubse::config::UbseGetUInt).stubs().will(invoke(UbseGetUIntZero));
    uint32_t result = mpConfiguration.Initialize(modCode);
    EXPECT_EQ(result, MEM_POOLING_OK);
}
} // namespace mempooling
