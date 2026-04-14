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

#include "test_fragmentation_vm_collection.h"

#include <ubse_timer.h>
#include <mockcpp/mockcpp.hpp>
#include "fragmentation_vm_collection.h"
#include "resource_query.h"

using namespace vm;
namespace ubse::ut::vm {

void TestFragmentationVmCollection::SetUp()
{
    Test::SetUp();
}

void TestFragmentationVmCollection::TearDown()
{
    Test::TearDown();
}

TEST_F(TestFragmentationVmCollection, FragInit)
{
    MOCKER(ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(FragmentationVmCollection::GetInstance().FragInit(), VM_OK);
    MOCKER(ubse::timer::UbseTimerHandlerRegister).reset();
    MOCKER(ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(FragmentationVmCollection::GetInstance().FragInit(), VM_ERROR);
    MOCKER(ubse::timer::UbseTimerHandlerRegister).reset();
}

TEST_F(TestFragmentationVmCollection, FragTerminate)
{
    FragmentationVmCollection::GetInstance().FragTerminate();
}

TEST_F(TestFragmentationVmCollection, FragGetLocalHostVmCollectData)
{
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(FragmentationVmCollection::FragGetLocalHostVmCollectData(), VM_OK);
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).reset();

    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(FragmentationVmCollection::FragGetLocalHostVmCollectData(), VM_ERROR);
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).reset();
}

TEST_F(TestFragmentationVmCollection, GetInstance)
{
    FragmentationVmCollection *instance1 = &FragmentationVmCollection::GetInstance();
    FragmentationVmCollection *instance2 = &FragmentationVmCollection::GetInstance();
    EXPECT_EQ(instance1, instance2);
}

} // namespace ubse::ut::vm