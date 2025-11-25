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

#include "test_test_plugin.h"
#include "ubse_error.h"
#include "ubse_plugin.h"
#include "ubse_plugin_module.h"
#include "ubse_context.h"

namespace ubse::ut::plugin {
using namespace ubse::plugin;
using namespace ubse::context;

std::string PLUGIN_NAME = "test";

void TestUbsePlugin::SetUp()
{
    Test::SetUp();
}
void TestUbsePlugin::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}
TEST_F(TestUbsePlugin, GetPluginInitResSuccess)
{
    std::shared_ptr<UbsePluginModule> Conf = std::make_shared<UbsePluginModule>();
    MOCKER(&UbseContext::GetModule<UbsePluginModule>).stubs().will(returnValue(Conf));
    bool res = GetPluginInitRes(PLUGIN_NAME);
    EXPECT_FALSE(res);
}
TEST_F(TestUbsePlugin, GetPluginReadyStatusSuccess)
{
    std::shared_ptr<UbsePluginModule> Conf = std::make_shared<UbsePluginModule>();
    MOCKER(&UbseContext::GetModule<UbsePluginModule>).stubs().will(returnValue(Conf));
    auto res = GetPluginReadyStatus(PLUGIN_NAME);
    EXPECT_FALSE(res);
}
TEST_F(TestUbsePlugin, NotifyPluginReadyStatusSuccess)
{
    std::shared_ptr<UbsePluginModule> Conf = std::make_shared<UbsePluginModule>();
    MOCKER(&UbseContext::GetModule<UbsePluginModule>).stubs().will(returnValue(Conf));
    auto ret = NotifyPluginReadyStatus(PLUGIN_NAME, true);
    EXPECT_EQ(ret, UBSE_OK);
    bool res = GetPluginReadyStatus(PLUGIN_NAME);
    EXPECT_TRUE(res);
}
TEST_F(TestUbsePlugin, GetPluginInitResFailed)
{
    auto res = GetPluginInitRes(PLUGIN_NAME);
    EXPECT_FALSE(res);
}
TEST_F(TestUbsePlugin, GetPluginReadyStatusFailed)
{
    auto res = GetPluginReadyStatus(PLUGIN_NAME);
    EXPECT_FALSE(res);
}

TEST_F(TestUbsePlugin, NotifyPluginReadyStatusFailed)
{
    auto ret = NotifyPluginReadyStatus(PLUGIN_NAME, true);
    EXPECT_NE(ret, UBSE_OK);
}
}
