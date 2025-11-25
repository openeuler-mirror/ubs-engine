/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_timer.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_timer.h"

namespace ubse::ut::timer {
using namespace common::def;
using namespace ubse::timer;
const int INTERVAL = 1000;

void TestUbseTimer::SetUp()
{
    Test::SetUp();
}

void TestUbseTimer::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * Start返回成功
 * 测试步骤：
 * 1.构造测试函数
 * 2.调用Start
 * 3.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 */
TEST(TestUbseTimer, TestStartSucess)
{
    auto callback = [this]() {
        std::cout << "test" << std::endl;
        return UBSE_OK;
    };
    UbseTimer timer;
    UbseResult ret = timer.Start(INTERVAL, callback);
    EXPECT_EQ(ret, UBSE_OK);
    timer.Stop();
}

/*
 * 用例描述：
 * Start因计时器已启动而失败
 * 测试步骤：
 * 1.构造测试函数
 * 2.调用Start
 * 3.再次调用Start
 * 4.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 * 返回失败
 */
TEST(TestUbseTimer, TestStartFailedWhenStartAgain)
{
    auto callback = [this]() {
        std::cout << "test" << std::endl;
        return UBSE_OK;
    };
    UbseTimer timer;
    timer.Start(INTERVAL, callback);
    sleep(2);
    UbseResult ret = timer.Start(INTERVAL, callback);
    EXPECT_EQ(ret, UBSE_ERROR);
    sleep(2);
    timer.Stop();
}

/*
 * 用例描述：
 * Start因计时器创建失败而失败
 * 测试步骤：
 * 1.构造测试函数
 * 2.Mock timer_create
 * 3.调用Start
 * 4.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 * 返回失败
 */
TEST(TestUbseTimer, TestStartFailedWhenCreatTimer)
{
    auto callback = [this]() {
        std::cout << "test" << std::endl;
        return UBSE_OK;
    };
    UbseTimer timer;
    MOCKER(&timer_create).stubs().will(returnValue(-1));
    UbseResult ret = timer.Start(INTERVAL, callback);
    EXPECT_EQ(ret, UBSE_ERROR);
    timer.Stop();
    MOCKER(&timer_create).reset();
}

/*
 * 用例描述：
 * Start因设置时间失败而失败
 * 测试步骤：
 * 1.构造测试函数
 * 2.Mock timer_settime
 * 3.调用Start
 * 4.调用Stop
 * 预期结果：
 * 1. 返回UBSE_OK
 * 返回失败
 */
TEST(TestUbseTimer, TestStartFailedWhenSetTime)
{
    auto callback = [this]() {
        std::cout << "test" << std::endl;
        return UBSE_OK;
    };
    UbseTimer timer;
    MOCKER(&timer_settime).stubs().then(returnValue(-1));
    UbseResult ret = timer.Start(INTERVAL, callback);
    EXPECT_EQ(ret, UBSE_ERROR);
    timer.Stop();
}
}