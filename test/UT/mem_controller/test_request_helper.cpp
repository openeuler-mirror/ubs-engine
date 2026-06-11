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

#include "test_request_helper.h"

namespace ubse::mem_controller::ut {

void TestRequestHelper::SetUp()
{
    Test::SetUp();
}

void TestRequestHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestRequestHelper, CreateInstance_Wait)
{
    std::thread t([]() -> void {
        auto featureMgr = UbseFutureMgr::CreateInstance("resId");
        auto uintFeature = featureMgr->GetFuture<uint32_t>();
        // 延迟1s赋值，构造线程2等待场景
        sleep(1);
        UbseFutureMgr::SetResult("resId", 0);
    });
    std::thread t1([]() -> void {
        auto featureMgr = UbseFutureMgr::CreateInstance("resId");
        auto uintFeature = featureMgr->GetFuture<uint32_t>();
        UbseFutureMgr::SetResult("resId", 0);
    });
    t.join();
    t1.join();
}

TEST_F(TestRequestHelper, Feature_NotFound)
{
    EXPECT_EQ(false, UbseFutureMgr::Find("resId"));
}

TEST_F(TestRequestHelper, GetSize)
{
    auto featureMgr = UbseFutureMgr::CreateInstance("resId");
    auto uintFeature = featureMgr->GetFuture<uint32_t>();
    EXPECT_EQ(1, UbseFutureMgr::GetSize());
    UbseFutureMgr::SetResult("resId", 0);
}
} // namespace ubse::mem_controller::ut