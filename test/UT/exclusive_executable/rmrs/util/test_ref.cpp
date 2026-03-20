/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more
 */

#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "mp_error.h"
#include "ref.h"

#define private public
#undef private
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
 
namespace mempooling::common {
 
class TestRef : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestRef SetUp Begin]" << endl;
        cout << "[TestRef SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestRef TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestRef TearDown End]" << endl;
    }
};

TEST_F(TestRef, SetSameNonnullObject)
{
    Referable *objA = new Referable();
    Ref<Referable> refA(objA);
    refA.Set(objA);
    ASSERT_EQ(refA.Get(), objA);
}

TEST_F(TestRef, SetWithNotNullAnother)
{
    Referable *objA = nullptr;
    Referable *objB = new Referable();
    Ref<Referable> refA(objA);
    refA.Set(objB);
    ASSERT_EQ(refA.Get(), objB);
}

TEST_F(TestRef, SetWithNullAnother)
{
    Referable *objA = new Referable();
    Referable *objB = nullptr;
    Ref<Referable> refA(objA);
    refA.Set(objB);
    ASSERT_EQ(refA.Get(), nullptr);
}

TEST_F(TestRef, NeverInit)
{
    Ref<Referable> refA;
    ASSERT_EQ(refA.Get(), nullptr);
}

TEST_F(TestRef, CopyWithNull)
{
    Ref<Referable> refA;
    Ref<Referable> refB(refA);
    ASSERT_EQ(refB.Get(), nullptr);
}

}