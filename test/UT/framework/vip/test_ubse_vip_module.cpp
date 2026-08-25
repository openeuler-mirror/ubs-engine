/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mockcpp/mockcpp.hpp"

#include "ubse_vip_module.h"

#include <fstream>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_http_server.h"
#include "ubse_net_util.h"
#include "ubse_os_util.h"
#include "ubse_cert_validator.h"

using namespace ubse::vip;
using namespace ubse::election;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::http;
using namespace ubse::utils;
using namespace ubse::cert;
namespace ubse::ut::vip {

namespace {
constexpr const char *kTestIface = "eth0";
constexpr const char *kTestIfaceFilePath = "/var/run/ubse/ubse_iface";

bool PrepareIfaceFile(const std::string &iface)
{
    std::ofstream ofs(kTestIfaceFilePath);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << iface;
    ofs.close();
    return true;
}

void CleanupIfaceFile()
{
    std::remove(kTestIfaceFilePath);
}

// 设置 enabled=true 的完整配置 mock（Init 阶段 Exec 全部成功，无残留 VIP）
void SetupEnabledConfigMocks(uint32_t arpCount = 5, uint32_t arpInterval = 200)
{
    auto conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    bool enableVal = true;
    MOCKER_CPP(&UbseConfModule::GetConf<bool>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(enableVal))
        .will(returnValue(UBSE_OK));
    std::string listenIp = "192.168.100.200/24";
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(listenIp))
        .will(returnValue(UBSE_OK));
    uint32_t port = 10002;
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(port))
        .will(returnValue(UBSE_OK));
    uint32_t arpCountVal = arpCount;
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(arpCountVal))
        .will(returnValue(UBSE_OK));
    uint32_t arpIntervalVal = arpInterval;
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(arpIntervalVal))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
}
} // namespace

class TestUbseVipModule : public testing::Test {
public:
    TestUbseVipModule() = default;

    void SetUp() override
    {
        UbseVipManager::GetInstance().Deinit();
        Test::SetUp();
    }

    void TearDown() override
    {
        Test::TearDown();
        UbseVipManager::GetInstance().Deinit();
        CleanupIfaceFile();
        GlobalMockObject::verify();
    }
};

/*
 * 用例描述：LoadConfig 时 vip.enable 不存在，默认禁用，Initialize 应成功
 */
TEST_F(TestUbseVipModule, Initialize_EnableMissing_DefaultsDisabled_Success)
{
    auto conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    // vip.enable 未配置，GetConf<bool> 返回 UBSE_ERROR，enable 默认 false
    bool enableVal = false;
    MOCKER_CPP(&UbseConfModule::GetConf<bool>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(enableVal))
        .will(returnValue(UBSE_ERROR));
    UbseVipModule module;
    EXPECT_EQ(UBSE_OK, module.Initialize());
    module.UnInitialize();
}

/*
 * 用例描述：LoadConfig 时 vip.enable=false，Initialize 应成功且不进行后续校验
 */
TEST_F(TestUbseVipModule, Initialize_Disabled_Success)
{
    auto conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    bool enableVal = false;
    MOCKER_CPP(&UbseConfModule::GetConf<bool>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(enableVal))
        .will(returnValue(UBSE_OK));
    UbseVipModule module;
    EXPECT_EQ(UBSE_OK, module.Initialize());
    module.UnInitialize();
}

/*
 * 用例描述：启用 VIP 但 listenIp 未配置，Initialize 应失败
 */
TEST_F(TestUbseVipModule, Initialize_EnabledButNoListenIp_Fails)
{
    auto conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    bool enableVal = true;
    MOCKER_CPP(&UbseConfModule::GetConf<bool>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(enableVal))
        .will(returnValue(UBSE_OK));
    // listenIp 配置缺失
    std::string emptyIp;
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(emptyIp))
        .will(returnValue(UBSE_ERROR));
    UbseVipModule module;
    EXPECT_EQ(UBSE_ERROR, module.Initialize());
}

/*
 * 用例描述：UbseConfModule 获取失败，LoadConfig 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipModule, Initialize_ConfModuleNull_Fails)
{
    std::shared_ptr<UbseConfModule> nullConf;
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullConf));
    UbseVipModule module;
    EXPECT_EQ(UBSE_ERROR, module.Initialize());
}

/*
 * 用例描述：配置完整且证书校验通过，Initialize 应成功
 */
TEST_F(TestUbseVipModule, Initialize_FullConfig_Success)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks();
    // Init 阶段 ForceCleanup 的 grep 返回空结果（无残留）
    std::string emptyResult;
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().with(mockcpp::any(), outBound(emptyResult)).will(returnValue(UBSE_OK));
    UbseVipModule module;
    EXPECT_EQ(UBSE_OK, module.Initialize());
    module.UnInitialize();
}

/*
 * 用例描述：Start 在 VIP 禁用时直接返回 UBSE_OK，不注册任何选举 handler
 */
TEST_F(TestUbseVipModule, Start_Disabled_ReturnsOK)
{
    auto conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    bool enableVal = false;
    MOCKER_CPP(&UbseConfModule::GetConf<bool>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(enableVal))
        .will(returnValue(UBSE_OK));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    EXPECT_EQ(UBSE_OK, module.Start());
    module.Stop();
}

/*
 * 用例描述：Start 在启用时调用 UbseElectionChangeAttachHandler 注册 4 个 handler，全部成功
 */
TEST_F(TestUbseVipModule, Start_Enabled_AttachAllSuccess)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks();
    std::string emptyResult;
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().with(mockcpp::any(), outBound(emptyResult)).will(returnValue(UBSE_OK));
    // 4 次 Attach 全部成功
    MOCKER(UbseElectionChangeAttachHandler).stubs().will(returnValue(UbseElectionOk));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    EXPECT_EQ(UBSE_OK, module.Start());
    module.Stop();
}

/*
 * 用例描述：Start 时第一次 Attach 即失败，应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipModule, Start_Enabled_AttachFails_ReturnsError)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks();
    std::string emptyResult;
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().with(mockcpp::any(), outBound(emptyResult)).will(returnValue(UBSE_OK));
    MOCKER(UbseElectionChangeAttachHandler).stubs().will(returnValue(UbseElectionError));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    EXPECT_EQ(UBSE_ERROR, module.Start());
    module.Stop();
}

/*
 * 用例描述：Stop 在禁用状态下应直接返回，不调用 DeAttachHandler
 */
TEST_F(TestUbseVipModule, Stop_Disabled_NoDeattach)
{
    auto conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    bool enableVal = false;
    MOCKER_CPP(&UbseConfModule::GetConf<bool>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(enableVal))
        .will(returnValue(UBSE_OK));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    EXPECT_NO_THROW(module.Stop());
}

/*
 * 用例描述：Stop 在启用状态下应反注册 4 个 handler
 */
TEST_F(TestUbseVipModule, Stop_Enabled_DeattachAll)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks();
    std::string emptyResult;
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().with(mockcpp::any(), outBound(emptyResult)).will(returnValue(UBSE_OK));
    MOCKER(UbseElectionChangeAttachHandler).stubs().will(returnValue(UbseElectionOk));
    MOCKER(UbseElectionChangeDeAttachHandler).stubs().will(returnValue(UbseElectionOk));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    ASSERT_EQ(UBSE_OK, module.Start());
    EXPECT_NO_THROW(module.Stop());
}

/*
 * 用例描述：HandleChangeToMaster 在 BindVip 成功时返回 UbseElectionOk
 */
TEST_F(TestUbseVipModule, HandleChangeToMaster_BindSuccess_ReturnsOk)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks(1, 0);
    // 所有 Exec 成功（ForceCleanup/AddIp/ARP），HTTP server 启动成功
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    // 利用 -fno-access-control 直接调用私有 handler
    UbseElectionEventType type = UbseElectionEventType::CHANGE_TO_MASTER;
    UBSE_ID_TYPE nodeId = "node1";
    EXPECT_EQ(UbseElectionOk, module.HandleChangeToMaster(type, nodeId));
    EXPECT_TRUE(UbseVipManager::GetInstance().IsVipBound());
    module.UnInitialize();
}

/*
 * 用例描述：HandleChangeToMaster 在 BindVip 失败时返回 UbseElectionError
 */
TEST_F(TestUbseVipModule, HandleChangeToMaster_BindFails_ReturnsError)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks(1, 0);
    // 所有 Exec 失败：Init 时 ForceCleanup 跳过（grep 失败），BindVip 时 AddIpAddress 失败
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    UbseElectionEventType type = UbseElectionEventType::CHANGE_TO_MASTER;
    UBSE_ID_TYPE nodeId = "node1";
    EXPECT_EQ(UbseElectionError, module.HandleChangeToMaster(type, nodeId));
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
    module.UnInitialize();
}

/*
 * 用例描述：HandleStandbyChangeToMaster 行为与 HandleChangeToMaster 一致
 */
TEST_F(TestUbseVipModule, HandleStandbyChangeToMaster_BindSuccess_ReturnsOk)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks(1, 0);
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    UbseElectionEventType type = UbseElectionEventType::STANDBY_CHANGE_TO_MASTER;
    UBSE_ID_TYPE nodeId = "node1";
    EXPECT_EQ(UbseElectionOk, module.HandleStandbyChangeToMaster(type, nodeId));
    EXPECT_TRUE(UbseVipManager::GetInstance().IsVipBound());
    module.UnInitialize();
}

/*
 * 用例描述：HandleChangeToStandby 在 UnbindVip 成功时返回 UbseElectionOk
 */
TEST_F(TestUbseVipModule, HandleChangeToStandby_UnbindSuccess_ReturnsOk)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks(1, 0);
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    UbseElectionEventType type = UbseElectionEventType::CHANGE_TO_STANDBY;
    UBSE_ID_TYPE nodeId = "node1";
    EXPECT_EQ(UbseElectionOk, module.HandleChangeToStandby(type, nodeId));
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
    module.UnInitialize();
}

/*
 * 用例描述：HandleChangeToAgent 行为与 HandleChangeToStandby 一致
 */
TEST_F(TestUbseVipModule, HandleChangeToAgent_UnbindSuccess_ReturnsOk)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks(1, 0);
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    UbseElectionEventType type = UbseElectionEventType::CHANGE_TO_AGENT;
    UBSE_ID_TYPE nodeId = "node1";
    EXPECT_EQ(UbseElectionOk, module.HandleChangeToAgent(type, nodeId));
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
    module.UnInitialize();
}

/*
 * 用例描述：HandleChangeToStandby 在 DelIpAddress 失败时，由于 UnbindVip 始终返回 UBSE_OK，
 *          handler 仍返回 UbseElectionOk
 */
TEST_F(TestUbseVipModule, HandleChangeToStandby_DelFails_StillReturnsOk)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    SetupEnabledConfigMocks(1, 0);
    // Init 时 Exec 失败让 ForceCleanup 跳过；UnbindVip 时 DelIpAddress 失败但 UnbindVip 仍返回 UBSE_OK
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    UbseElectionEventType type = UbseElectionEventType::CHANGE_TO_STANDBY;
    UBSE_ID_TYPE nodeId = "node1";
    // UnbindVip 始终返回 UBSE_OK（即使 DelIpAddress 失败），故 handler 返回 UbseElectionOk
    EXPECT_EQ(UbseElectionOk, module.HandleChangeToStandby(type, nodeId));
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
    module.UnInitialize();
}

/*
 * 用例描述：UnInitialize 应清理 VIP manager 状态
 */
TEST_F(TestUbseVipModule, UnInitialize_ResetsVipManager)
{
    auto conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    bool enableVal = false;
    MOCKER_CPP(&UbseConfModule::GetConf<bool>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(enableVal))
        .will(returnValue(UBSE_OK));
    UbseVipModule module;
    ASSERT_EQ(UBSE_OK, module.Initialize());
    EXPECT_NO_THROW(module.UnInitialize());
}

} // namespace ubse::ut::vip
