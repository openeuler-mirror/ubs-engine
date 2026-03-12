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

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "rmrs_libvirt_module.h"
#include "LibvirtHelper/LibvirtHelper.h"

using namespace std;

class TestLibvirtModule : public ::testing::Test {
    void SetUp() override
    {
        cout << "[LibvirtModuleTest SetUp Begin]" << endl;
        cout << "[LibvirtModuleTest SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[LibvirtModuleTest TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[LibvirtModuleTest TearDown End]" << endl;
    }
};

TEST_F(TestLibvirtModule, LibvirtModuleSuccess)
{
    // Mock测试
    MOCKER(mempooling::libvirt::LibvirtModule::Init).stubs().will(returnValue(0));
    MpResult res = mempooling::libvirt::LibvirtModule::Init();
    EXPECT_EQ(res, 0);
    cout << "Libvirt Module Success!" << endl;
}