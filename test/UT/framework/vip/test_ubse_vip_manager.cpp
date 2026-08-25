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

#include "ubse_vip_manager.h"

#include <fstream>

#include "ubse_error.h"
#include "ubse_http_server.h"
#include "ubse_net_util.h"
#include "ubse_os_util.h"
#include "ubse_cert_def.h"
#include "ubse_cert_validator.h"
#include "framework/vip/ubse_vip_http_server.h"

using namespace ubse::vip;
using namespace ubse::http;
using namespace ubse::utils;
using namespace ubse::cert;
namespace ubse::ut::vip {

namespace {
constexpr const char *kTestIface = "eth0";
constexpr const char *kTestIfaceFilePath = "/var/run/ubse/ubse_iface";

// 创建 iface 文件以便 ResolveInterface 能成功读取，需 root 权限写 /var/run/ubse/
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

UbseVipConfig MakeValidConfig()
{
    UbseVipConfig cfg;
    cfg.enable = true;
    cfg.listenIp = "192.168.100.200/24";
    cfg.listenPort = 10002;
    cfg.arpCount = 5;
    cfg.arpInterval = 200;
    return cfg;
}
} // namespace

class TestUbseVipManager : public testing::Test {
public:
    TestUbseVipManager() = default;

    void SetUp() override
    {
        // 每个用例前重置单例状态，避免上个用例遗留状态影响
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
 * 用例描述：VIP 管理未启用时，Init 应直接返回 UBSE_OK
 */
TEST_F(TestUbseVipManager, Init_Disabled_ReturnsOK)
{
    UbseVipConfig cfg;
    cfg.enable = false;
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：启用 VIP 但未配置 listenIp，Init 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, Init_EmptyListenIp_ReturnsError)
{
    UbseVipConfig cfg = MakeValidConfig();
    cfg.listenIp.clear();
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：listenIp 不含 '/'，CIDR 格式非法，Init 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, Init_InvalidCIDR_NoSlash_ReturnsError)
{
    UbseVipConfig cfg = MakeValidConfig();
    cfg.listenIp = "192.168.100.200";
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：listenIp 的 prefix 为空，CIDR 格式非法，Init 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, Init_InvalidCIDR_EmptyPrefix_ReturnsError)
{
    UbseVipConfig cfg = MakeValidConfig();
    cfg.listenIp = "192.168.100.200/";
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：prefix 非数字，Init 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, Init_InvalidCIDR_NonNumericPrefix_ReturnsError)
{
    UbseVipConfig cfg = MakeValidConfig();
    cfg.listenIp = "192.168.100.200/abc";
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：address 不是合法 IPv4，Init 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, Init_InvalidIpv4_ReturnsError)
{
    UbseVipConfig cfg = MakeValidConfig();
    cfg.listenIp = "999.168.100.200/24";
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(false));
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：prefix 为 0，Init 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, Init_PrefixZero_ReturnsError)
{
    UbseVipConfig cfg = MakeValidConfig();
    cfg.listenIp = "192.168.100.200/0";
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：prefix 超过 32，Init 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, Init_PrefixTooLarge_ReturnsError)
{
    UbseVipConfig cfg = MakeValidConfig();
    cfg.listenIp = "192.168.100.200/33";
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：ResolveInterface 失败（iface 文件不存在），Init 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, Init_IfaceFileMissing_ReturnsError)
{
    UbseVipConfig cfg = MakeValidConfig();
    CleanupIfaceFile();
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：证书校验失败，Init 应返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, Init_CertValidationFails_ReturnsError)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(false));
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：证书校验成功，Init 应返回 UBSE_OK（ForceCleanup 内部允许失败）
 */
TEST_F(TestUbseVipManager, Init_Success)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_EQ(cfg.listenIp, UbseVipManager::GetInstance().GetConfig().listenIp);
    EXPECT_EQ(std::string("192.168.100.200"), UbseVipManager::GetInstance().GetConfig().address);
    EXPECT_EQ(24u, UbseVipManager::GetInstance().GetConfig().prefix);
    EXPECT_EQ(std::string("eth0"), UbseVipManager::GetInstance().GetConfig().interface);
}

/*
 * 用例描述：VIP 未启用时 BindVip 应直接返回 UBSE_OK 且不修改绑定状态
 */
TEST_F(TestUbseVipManager, BindVip_Disabled_ReturnsOK)
{
    UbseVipConfig cfg;
    cfg.enable = false;
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：VIP 已绑定时再次调用 BindVip 应直接返回 UBSE_OK
 */
TEST_F(TestUbseVipManager, BindVip_AlreadyBound_ReturnsOK)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());
    EXPECT_TRUE(UbseVipManager::GetInstance().IsVipBound());
    // 再次绑定应直接返回
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());
}

/*
 * 用例描述：BindVipL2 失败（AddIpAddress 失败）时，BindVip 应返回 UBSE_ERROR 且不绑定
 */
TEST_F(TestUbseVipManager, BindVip_BindL2Fails_ReturnsError)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    cfg.arpCount = 1;
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    // ForceCleanup 检查返回 UBSE_OK（无残留），AddIpAddress 失败
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().BindVip());
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：BindVipL2 成功但 HTTP server 启动失败，应回滚（调用 UnbindVipL2）并返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, BindVip_StartHttpFails_RollsBack)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    cfg.arpCount = 1;
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(false));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().BindVip());
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：BindVip 成功场景，vipBound_ 应为 true
 */
TEST_F(TestUbseVipManager, BindVip_Success)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    cfg.arpCount = 1;
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());
    EXPECT_TRUE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：VIP 未启用时 UnbindVip 应直接返回 UBSE_OK
 */
TEST_F(TestUbseVipManager, UnbindVip_Disabled_ReturnsOK)
{
    UbseVipConfig cfg;
    cfg.enable = false;
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().UnbindVip());
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：UnbindVip 始终执行 DelIpAddress，即使 vipBound_=false
 */
TEST_F(TestUbseVipManager, UnbindVip_NotBound_StillCallsDel)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().UnbindVip());
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：UnbindVip 在 DelIpAddress 失败时仍应重置 vipBound_ 并返回 UBSE_OK
 */
TEST_F(TestUbseVipManager, UnbindVip_DelFails_StillReturnsOK)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    cfg.arpCount = 1;
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());
    // 之后让 DelIpAddress 失败
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().UnbindVip());
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：HTTP server 未运行时 RegisterRoute 仅缓存到 pendingRoutes_
 */
TEST_F(TestUbseVipManager, RegisterRoute_WhenServerNotRunning_StoresPending)
{
    UbseVipConfig cfg;
    cfg.enable = false;
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    auto handler = [](const UbseHttpRequest &, UbseHttpResponse &) { return UBSE_OK; };
    EXPECT_NO_THROW(UbseVipManager::GetInstance().RegisterRoute("/test", UbseHttpMethod::UBSE_HTTP_METHOD_GET, handler));
}

/*
 * 用例描述：HTTP server 已运行时 RegisterRoute 应直接注册到运行实例
 */
TEST_F(TestUbseVipManager, RegisterRoute_WhenServerRunning_RegistersDirectly)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    cfg.arpCount = 1;
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());
    auto handler = [](const UbseHttpRequest &, UbseHttpResponse &) { return UBSE_OK; };
    EXPECT_NO_THROW(UbseVipManager::GetInstance().RegisterRoute("/test", UbseHttpMethod::UBSE_HTTP_METHOD_GET, handler));
}

/*
 * 用例描述：Deinit 在已绑定状态下应停止 HTTP server 并解绑 VIP
 */
TEST_F(TestUbseVipManager, Deinit_WhenBound_UnbindsVip)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    cfg.arpCount = 1;
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());
    EXPECT_TRUE(UbseVipManager::GetInstance().IsVipBound());
    EXPECT_NO_THROW(UbseVipManager::GetInstance().Deinit());
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：Deinit 在 UnbindVipL2 失败时应触发 ForceCleanup 兜底重试
 */
TEST_F(TestUbseVipManager, Deinit_UnbindFails_TriggersForceCleanup)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    cfg.arpCount = 1;
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    // Init 时 ForceCleanup 走 UBSE_OK 路径
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());
    // Deinit 时 UnbindVipL2 失败，ForceCleanup 兜底
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NO_THROW(UbseVipManager::GetInstance().Deinit());
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：ForceCleanup 在网卡上无残留 VIP 时直接返回 UBSE_OK
 */
TEST_F(TestUbseVipManager, ForceCleanup_NoStaleVip_ReturnsOK)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    // 第一次 Exec（ip addr show | grep）返回空结果
    std::string emptyResult;
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().with(mockcpp::any(), outBound(emptyResult)).will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
}

/*
 * 用例描述：ForceCleanup 在网卡上有残留 VIP 且 del 成功时返回 UBSE_OK
 *          通过直接调用私有方法 ForceCleanup 验证（-fno-access-control 启用）
 */
TEST_F(TestUbseVipManager, ForceCleanup_StaleVipDelSuccess_ReturnsOK)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    // Init 时 Exec 全部失败，ForceCleanup 仍返回 UBSE_OK（grep 失败即跳过 cleanup）
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    // 重新设置 mock：grep 返回 UBSE_OK 且 result 非空（检测到残留），随后 DelIpAddress 也成功
    std::string nonEmpty = "192.168.100.200/24 dev eth0";
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().with(mockcpp::any(), outBound(nonEmpty)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().ForceCleanup());
}

/*
 * 用例描述：SendGratuitousArp 所有发送均失败时返回 UBSE_ERROR
 */
TEST_F(TestUbseVipManager, SendGratuitousArp_AllFail_ReturnsError)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    cfg.arpCount = 2;
    cfg.arpInterval = 0;
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    // 直接调用私有方法 SendGratuitousArp，所有 arping 均失败
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().SendGratuitousArp());
}

/*
 * 用例描述：SendGratuitousArp 部分成功时返回 UBSE_OK
 */
TEST_F(TestUbseVipManager, SendGratuitousArp_PartialSuccess_ReturnsOK)
{
    if (!PrepareIfaceFile(kTestIface)) {
        GTEST_SKIP() << "无法创建 iface 文件，跳过该用例（需要 root 权限写 /var/run/ubse/）";
    }
    UbseVipConfig cfg = MakeValidConfig();
    cfg.arpCount = 3;
    cfg.arpInterval = 0;
    MOCKER_CPP(&UbseNetUtil::ValidIpv4Addr).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    // 单次 mock 序列覆盖所有 Exec 调用：
    //   #1 Init 的 ForceCleanup grep（失败 -> 跳过清理，Init 仍返回 UBSE_OK）
    //   #2 SendGratuitousArp 第 1 次（失败）
    //   #3 SendGratuitousArp 第 2 次（成功 -> anySuccess=true）
    //   #4 SendGratuitousArp 第 3 次（失败）
    // 只要 anySuccess 即返回 UBSE_OK
    MOCKER_CPP(&UbseOsUtil::Exec)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK))
        .then(returnValue(UBSE_ERROR));
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().SendGratuitousArp());
}

/*
 * 用例描述：RegVipHttpService 全局函数应正常调用 RegisterRoute，无异常
 */
TEST_F(TestUbseVipManager, RegVipHttpService_Normal)
{
    UbseVipConfig cfg;
    cfg.enable = false;
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    auto handler = [](const UbseHttpRequest &, UbseHttpResponse &) { return UBSE_OK; };
    EXPECT_EQ(UBSE_OK, RegVipHttpService(UbseHttpMethod::UBSE_HTTP_METHOD_GET, "/health", handler));
}

/*
 * 用例描述：IsVipBound 初始应为 false
 */
TEST_F(TestUbseVipManager, IsVipBound_InitiallyFalse)
{
    UbseVipConfig cfg;
    cfg.enable = false;
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

} // namespace ubse::ut::vip
