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
#include <mockcpp/mokc.h>
#include <iostream>
#include <memory>

#include "event_listener.h"
#include "fault_node_module.h"
#include "mp_error.h"
#include "ubse_ras.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace mempooling::event::listener {

using namespace std;
// 测试类
class TestEventListener : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestEventListener SetUp Begin]" << endl;
        cout << "[TestEventListener SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestEventListener TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestEventListener TearDown End]" << endl;
    }
};

TEST_F(TestEventListener, InitSuccess)
{
    EventListener::Init();
}

TEST_F(TestEventListener, MpEventSubModuleInitSuccess)
{
    GTEST_SKIP();
    auto mp = make_unique<mempooling::event::MpEventSubModule>();
    auto ret = mp->Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestEventListener, MpEventSubModuleInitFailure)
{
    auto mp = make_unique<mempooling::event::MpEventSubModule>();
    using FuncPtrType = uint32_t (*)(
        ubse::ras::ALARM_FAULT_TYPE, std::string,
        ubse::ras::AlarmFaultHandler,
        ubse::ras::AlarmHandlerPriority
    );
    MOCKER_CPP(static_cast<FuncPtrType>(&ubse::ras::RegisterAlarmFaultHandler), FuncPtrType)
        .stubs().will(returnValue(1));
    auto ret = mp->Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

} // namespace mempooling::event::listener