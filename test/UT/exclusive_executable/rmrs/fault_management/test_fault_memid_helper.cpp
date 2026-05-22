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

#include "fault_memid_helper.h"

#include <cstring>
#include <iostream>
#include <string>

#include <gmock/gmock.h>

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "ubse_com.h"
#include "ubse_common_def.h"
#include "mempooling_message.h"
#include "mp_error.h"
#include "mp_mem_json_util.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using ubse::common::def::UbseResult;
using namespace ubse::log;
using namespace std;

namespace mempooling {

class TestFaultMemidHelper : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(TestFaultMemidHelper, FaultMemIdManageHelper1)
{
    std::string importNodeId = "node1";
    uint64_t importMemId = 1;
    MOCKER_CPP(&FaultMemIdModule::MemIdFaultManage, MpResult (*)(std::string borrowInNid, uint64_t memId))
        .stubs()
        .will(returnValue(1));
    auto res = FaultMemIdHelper::Instance().FaultMemIdManageHelper(importNodeId, importMemId, false, false);
    ASSERT_EQ(res, 1);
}

TEST_F(TestFaultMemidHelper, FaultMemIdManageHelper2)
{
    std::string importNodeId = "node1";
    uint64_t importMemId = 1;
    MOCKER_CPP(&FaultMemIdModule::MemIdFaultManage, MpResult (*)(std::string borrowInNid, uint64_t memId))
        .stubs()
        .will(returnValue(0));
    auto res = FaultMemIdHelper::Instance().FaultMemIdManageHelper(importNodeId, importMemId, false, false);
    ASSERT_EQ(res, 0);
}

} // namespace mempooling