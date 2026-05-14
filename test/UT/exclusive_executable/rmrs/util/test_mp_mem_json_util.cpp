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

#include "mp_mem_json_util.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
namespace mempooling {

class TestMpMemJsonUtil : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestMpMemJsonUtil SetUp Begin]" << endl;
        cout << "[TestMpMemJsonUtil SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestMpMemJsonUtil TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestMpMemJsonUtil TearDown End]" << endl;
    }
};

TEST_F(TestMpMemJsonUtil, MpFaultMemIdParam_FromJson_Succeed)
{
    std::string jsonString = R"({
        "importNodeId": "Node0",
        "importMemId": 0
    })";
    MpFaultMemIdParam obj;
    auto ret = obj.FromJson(jsonString);
    ASSERT_EQ(ret, true);
    ASSERT_EQ(obj.importNodeId, "Node0");
    ASSERT_EQ(obj.importMemId, 0);
}

TEST_F(TestMpMemJsonUtil, MpFaultMemIdParam_FromJson_Fail_IsNotObject)
{
    std::string jsonString = R"([0, 1])";
    MpFaultMemIdParam obj;
    auto ret = obj.FromJson(jsonString);
    ASSERT_EQ(ret, false);
}

} // namespace mempooling