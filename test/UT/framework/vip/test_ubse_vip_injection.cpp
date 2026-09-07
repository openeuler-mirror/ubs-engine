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

#include <cstring>

#include "ubse_vip_manager.h"
#include "ubse_vip_injection_handler.h"

#include "ubse_api_server.h"
#include "ubse_error.h"
#include "ubse_http_server.h"
#include "ubse_net_util.h"
#include "ubse_os_util.h"

using namespace ubse::vip;
using namespace ubse::http;
using namespace ubse::utils;
namespace ubse::ut::vip {

namespace {
// 192.168.100.200 的 host 序整数表示（与 IntToIpV4 约定一致）
constexpr uint32_t kTestAddr = 0xC0A864C8u;
constexpr const char *kTestAddrStr = "192.168.100.200";
constexpr uint16_t kTestPort = 10002;

UbseVipConfig MakeContainerModeConfig()
{
    UbseVipConfig cfg;
    cfg.enable = true;
    cfg.containerMode = true;
    return cfg;
}
} // namespace

class TestUbseVipInjection : public testing::Test {
public:
    void SetUp() override { UbseVipManager::GetInstance().Deinit(); }

    void TearDown() override
    {
        UbseVipManager::GetInstance().Deinit();
        GlobalMockObject::verify();
    }
};

/*
 * 用例描述：非容器模式下注入配置应被拒绝，返回 UBSE_ERR_INVALID_ARG
 */
TEST_F(TestUbseVipInjection, InjectConfig_NotContainerMode_ReturnsInvalidArg)
{
    UbseVipConfig cfg;
    cfg.enable = false;
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseVipManager::GetInstance().InjectConfig(kTestAddr, kTestPort, 24, "eth0"));
}

/*
 * 用例描述：非法 payload 字段（addr=0、端口越界、prefix 越界、空网卡、非法网卡名）应逐项校验失败
 */
TEST_F(TestUbseVipInjection, InjectConfig_InvalidFields_ReturnsInvalidArg)
{
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(MakeContainerModeConfig()));
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseVipManager::GetInstance().InjectConfig(0, kTestPort, 24, "eth0"));
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseVipManager::GetInstance().InjectConfig(kTestAddr, 1000, 24, "eth0"));
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseVipManager::GetInstance().InjectConfig(kTestAddr, kTestPort, 0, "eth0"));
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseVipManager::GetInstance().InjectConfig(kTestAddr, kTestPort, 33, "eth0"));
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseVipManager::GetInstance().InjectConfig(kTestAddr, kTestPort, 24, ""));
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseVipManager::GetInstance().InjectConfig(kTestAddr, kTestPort, 24, "eth0$bad"));
}

/*
 * 用例描述：非 master 节点注入合法配置，仅落盘 config_，不触发绑定
 */
TEST_F(TestUbseVipInjection, InjectConfig_NotMaster_StoresOnly)
{
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(MakeContainerModeConfig()));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().InjectConfig(kTestAddr, kTestPort, 24, "eth0"));
    EXPECT_EQ(std::string(kTestAddrStr), UbseVipManager::GetInstance().GetConfig().address);
    EXPECT_EQ(24u, UbseVipManager::GetInstance().GetConfig().prefix);
    EXPECT_EQ(kTestPort, UbseVipManager::GetInstance().GetConfig().listenPort);
    EXPECT_EQ(std::string("eth0"), UbseVipManager::GetInstance().GetConfig().interface);
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
}

/*
 * 用例描述：master 节点未绑定且注入新配置，应触发实际绑定（BindVipLocked 成功路径）
 */
TEST_F(TestUbseVipInjection, InjectConfig_MasterUnbound_Binds)
{
    UbseVipConfig cfg = MakeContainerModeConfig();
    cfg.arpCount = 1;
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    // BindVip 使 active_=true；容器模式且未注入，走延迟绑定分支（不真正绑定）
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());

    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().InjectConfig(kTestAddr, kTestPort, 24, "eth0"));
    EXPECT_TRUE(UbseVipManager::GetInstance().IsVipBound());
    EXPECT_EQ(std::string(kTestAddrStr), UbseVipManager::GetInstance().GetConfig().address);
}

/*
 * 用例描述：master 节点配置未变且仍处于未绑定状态，注入应触发心跳自愈补 bind
 */
TEST_F(TestUbseVipInjection, InjectConfig_MasterUnbound_Unchanged_HeartbeatRebind)
{
    UbseVipConfig cfg = MakeContainerModeConfig();
    cfg.arpCount = 1;
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));
    // BindVip 使 active_=true，容器模式未注入走延迟绑定分支
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().BindVip());

    // 首次注入但绑定失败（AddIpAddress 失败）：config_ 已落地（configured_=true），vipBound_ 仍为 false
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseVipManager::GetInstance().InjectConfig(kTestAddr, kTestPort, 24, "eth0"));
    EXPECT_FALSE(UbseVipManager::GetInstance().IsVipBound());
    EXPECT_EQ(std::string(kTestAddrStr), UbseVipManager::GetInstance().GetConfig().address);

    // mockcpp 对同一函数的二次 MOCKER_CPP 不会覆盖先前 stub,需先重置再重新设置成功行为
    GlobalMockObject::reset();
    // 心跳重推相同配置 → 配置未变且 master 未绑定 → 补 bind 成功
    MOCKER_CPP(&UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpServer::Start).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, UbseVipManager::GetInstance().InjectConfig(kTestAddr, kTestPort, 24, "eth0"));
    EXPECT_TRUE(UbseVipManager::GetInstance().IsVipBound());
}

// ==================== UbseVipInjectionHandler ====================

class TestUbseVipInjectionHandler : public testing::Test {
public:
    void SetUp() override { UbseVipManager::GetInstance().Deinit(); }

    void TearDown() override
    {
        UbseVipManager::GetInstance().Deinit();
        GlobalMockObject::verify();
    }

    // 构造 23 字节合法 payload（LittleEndian 内存布局，与 Go 侧逐字节对齐）
    static UbseVipCfgPushPayload MakeValidPayload()
    {
        UbseVipCfgPushPayload p{};
        p.addr = kTestAddr;
        p.port = kTestPort;
        p.prefix = 24;
        std::memcpy(p.iface, "eth0", sizeof("eth0"));
        return p;
    }
};

/*
 * 用例描述：空 buffer 应直接返回 UBSE_ERR_INVALID_ARG，不进入反序列化
 */
TEST_F(TestUbseVipInjectionHandler, Handle_NullBuffer_ReturnsInvalidArg)
{
    UbseVipInjectionHandler handler;
    api::server::UbseIpcMessage msg{nullptr, 0};
    api::server::UbseRequestContext ctx{};
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, handler.Handle(msg, ctx));
}

/*
 * 用例描述：payload 长度不等于 23 字节应返回 UBSE_ERR_INVALID_ARG
 */
TEST_F(TestUbseVipInjectionHandler, Handle_WrongLength_ReturnsInvalidArg)
{
    UbseVipInjectionHandler handler;
    uint8_t buf[8] = {0};
    api::server::UbseIpcMessage msg{buf, sizeof(buf)};
    api::server::UbseRequestContext ctx{};
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, handler.Handle(msg, ctx));
}

/*
 * 用例描述：合法结构但 manager 处于非容器模式，反序列化后转发被拒绝
 */
TEST_F(TestUbseVipInjectionHandler, Handle_ValidPayloadButNotContainerMode_Rejected)
{
    UbseVipConfig cfg;
    cfg.enable = false;
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(cfg));

    UbseVipInjectionHandler handler;
    auto payload = MakeValidPayload();
    api::server::UbseIpcMessage msg{reinterpret_cast<uint8_t *>(&payload), sizeof(payload)};
    api::server::UbseRequestContext ctx{};
    EXPECT_EQ(UBSE_ERR_INVALID_ARG, handler.Handle(msg, ctx));
}

/*
 * 用例描述：容器模式非 master 下合法注入，反序列化 + 转发成功并回 ACK
 */
TEST_F(TestUbseVipInjectionHandler, Handle_ValidPayload_Success)
{
    ASSERT_EQ(UBSE_OK, UbseVipManager::GetInstance().Init(MakeContainerModeConfig()));

    UbseVipInjectionHandler handler;
    auto payload = MakeValidPayload();
    api::server::UbseIpcMessage msg{reinterpret_cast<uint8_t *>(&payload), sizeof(payload)};
    api::server::UbseRequestContext ctx{};

    MOCKER(api::server::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, handler.Handle(msg, ctx));
    EXPECT_EQ(std::string(kTestAddrStr), UbseVipManager::GetInstance().GetConfig().address);
}

} // namespace ubse::ut::vip