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

#include "test_ubse_event_thread_pool.h"
#include "src/framework/event/ubse_event_thread_pool.h"

namespace ubse::event::ut {
using namespace ubse::event;
void TestUbseEventThreadPool::SetUp()
{
    Test::SetUp();
}
void TestUbseEventThreadPool::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}
/*
 * 用例描述：
 * 测试Init失败的情况
 * 测试步骤：
 * 1.MCOK pthread_barrier_init,返回失败
 * 预期结果：
 * 1.返回UBSE_ERROR
 */
TEST_F(TestUbseEventThreadPool, pthread_barrier_initError)
{
    uint32_t numsHighThs{ 10 };
    uint32_t numsMidThs{ 5 };
    uint32_t numsLowThs{ 2 };
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
    MOCKER(pthread_barrier_init).stubs().will(returnValue((int)UBSE_ERROR));
    auto ret = pool.Init();
    EXPECT_EQ(ret, UBSE_ERROR);
}
/*
 * 用例描述：
 * 测试Init失败的情况
 * 测试步骤：
 * 1.MCOK pthread_create,返回失败
 * 预期结果：
 * 1.返回UBSE_ERROR
 */
TEST_F(TestUbseEventThreadPool, pthread_createError)
{
    uint32_t numsHighThs{ 10 };
    uint32_t numsMidThs{ 5 };
    uint32_t numsLowThs{ 2 };
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
    MOCKER(pthread_create).stubs().will(returnValue((int)UBSE_ERROR));
    auto ret = pool.Init();
    EXPECT_EQ(ret, UBSE_ERROR);
}
/*
 * 用例描述：
 * 测试Init失败的情况
 * 测试步骤：
 * 1.MOCK pthread_barrier_wait,返回失败
 * 预期结果：
 * 1.返回UBSE_ERROR
 */
TEST_F(TestUbseEventThreadPool, pthread_barrier_waitError)
{
    uint32_t numsHighThs{ 10 };
    uint32_t numsMidThs{ 5 };
    uint32_t numsLowThs{ 2 };
    UbseEventThreadPool pool(numsHighThs, numsMidThs, numsLowThs);
    MOCKER(pthread_barrier_init).stubs().will(returnValue((int)UBSE_OK));
    MOCKER(pthread_create).stubs().will(returnValue((int)UBSE_OK));
    MOCKER(pthread_setschedparam).stubs().will(returnValue((int)UBSE_OK));
    MOCKER(pthread_barrier_wait).stubs().will(returnValue((int)UBSE_ERROR));
    auto ret = pool.Init();
    EXPECT_EQ(ret, UBSE_ERROR);
}
}