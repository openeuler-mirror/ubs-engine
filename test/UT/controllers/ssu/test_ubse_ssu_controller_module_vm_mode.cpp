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

// UbseSsuControllerModule 在 vm_mode 配置下的分流逻辑 UT
// 注:本测试不验证 UbseVsockServer / UbseSsuVmCache 的实现

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <memory>
#include <string>
#include <vector>

#include "config/ubse_conf.h"
#include "test_ubse_ssu_controller_module_vm_mock.h"
#include "ubse_error.h"
#include "ubse_ssu_controller_module.h"
#include "ubse_ssu_service_imp.h"

namespace ubse::ssu::controller::ut {

using ubse::ssu::controller::UbseSsuControllerModule;
using ubse::ssu::service::UbseSsuServiceImp;

// config mock(MockUbseGetBool/GetStr/GetUInt 及 g_vmModeForTest 等状态)见共享头 test_ubse_ssu_controller_module_vm_mock.h

// ============================================================================
// 测试夹具
// ============================================================================

class TestUbseSsuControllerModuleVmMode : public testing::Test {
public:
    void SetUp() override
    {
        g_vmModeForTest = false;
        g_enableTlsForTest = false;
        g_vmModeConfigRetForTest = UBSE_OK;
        SetupVmConfigMocks();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        // 恢复 ServiceImp 单例状态,避免污染其他测试
        UbseSsuServiceImp::GetInstance().SetVmCache(nullptr);
        UbseSsuServiceImp::GetInstance().SetVmMode(false);
        g_vmModeForTest = false;
        g_enableTlsForTest = false;
        g_vmModeConfigRetForTest = UBSE_OK;
    }

protected:
    // 设置下一次 Initialize 读取的 vm_mode 值
    // 保持原语义:VM 模式下 enable_tls=true,默认 certDir(/var/lib/ubse/lcne_cert/)使配置合法
    static void SetConfigVmMode(bool enabled)
    {
        g_vmModeForTest = enabled;
        g_enableTlsForTest = enabled;
    }

    // 检查 ServiceImp 是否被标记为 VM 模式
    static bool ServiceImpInVmMode() { return UbseSsuServiceImp::GetInstance().IsVmMode(); }

    // UT 目标启用 -fno-access-control，用于验证 controller 未遗留 ServiceImp 缓存裸指针
    static UbseSsuVmCache *ServiceImpVmCache() { return UbseSsuServiceImp::GetInstance().vmCache_; }
};

// ============================================================================
// A. VM 模式初始化
// ============================================================================

/*
 * 用例描述:BuildVsockConfig 返回单工作线程配置,保证推送消息按接收顺序串行处理
 * 测试步骤:
 * 1、构造 UbseSsuControllerModule 实例
 * 2、调用 BuildVsockConfig() 读取 threadPoolSize
 * 预期结果:
 * 1、threadPoolSize 等于 1(单工作线程保证同会话消息串行处理)
 */
TEST_F(TestUbseSsuControllerModuleVmMode, BuildVsockConfig_UsesSingleRequestWorker)
{
    UbseSsuControllerModule module;
    EXPECT_EQ(module.BuildVsockConfig().threadPoolSize, 1);
}

/*
 * 用例描述:VM 模式下插件依赖集为空,避免误激活 node/election/com/vip
 * 测试步骤:
 * 1、SetConfigVmMode(true)
 * 2、调用 GetSsuModuleDependencies()
 * 预期结果:
 * 1、返回的依赖列表为空
 */
TEST_F(TestUbseSsuControllerModuleVmMode, ModuleDependencies_VmMode_AreEmpty)
{
    SetConfigVmMode(true);
    EXPECT_TRUE(GetSsuModuleDependencies().empty());
}

/*
 * 用例描述:host 模式下插件依赖集保持现有四个模块不变
 * 测试步骤:
 * 1、SetConfigVmMode(false)
 * 2、调用 GetSsuModuleDependencies()
 * 预期结果:
 * 1、返回 Node/Election/Com/Vip 四个依赖模块
 */
TEST_F(TestUbseSsuControllerModuleVmMode, ModuleDependencies_HostMode_PreserveExistingDependencies)
{
    SetConfigVmMode(false);
    const std::vector<ubse::module::UbseOptionModule> expected{
        ubse::module::UbseOptionModule::UbseNodeControllerModule,
        ubse::module::UbseOptionModule::UbseElectionModule,
        ubse::module::UbseOptionModule::UbseComModule,
        ubse::module::UbseOptionModule::UbseVipModule,
    };
    EXPECT_EQ(GetSsuModuleDependencies(), expected);
}

/*
 * 用例描述:配置读取失败时按默认 host 模式返回完整依赖集,保持现有启动行为
 * 测试步骤:
 * 1、设置 g_vmModeConfigRetForTest 为 UBSE_ERR_OPERATION_FAILED
 * 2、调用 GetSsuModuleDependencies()
 * 预期结果:
 * 1、返回 Node/Election/Com/Vip 四个依赖模块(回退到 host 模式)
 */
TEST_F(TestUbseSsuControllerModuleVmMode, ModuleDependencies_ConfigReadFails_PreserveExistingDependencies)
{
    g_vmModeConfigRetForTest = UBSE_ERR_OPERATION_FAILED;
    const std::vector<ubse::module::UbseOptionModule> expected{
        ubse::module::UbseOptionModule::UbseNodeControllerModule,
        ubse::module::UbseOptionModule::UbseElectionModule,
        ubse::module::UbseOptionModule::UbseComModule,
        ubse::module::UbseOptionModule::UbseVipModule,
    };
    EXPECT_EQ(GetSsuModuleDependencies(), expected);
}

/*
 * 用例描述:VM 模式 Initialize 后 ServiceImp 单例被创建并标记 vmMode_=true
 * 测试步骤:
 * 1、SetConfigVmMode(true)
 * 2、UbseSsuControllerModule 实例 Initialize
 * 预期结果:
 * 1、Initialize 返回 UBSE_OK
 * 2、ServiceImp::IsVmMode() 返回 true
 */
TEST_F(TestUbseSsuControllerModuleVmMode, Init_VmMode_SetsVmModeTrue)
{
    SetConfigVmMode(true);
    UbseSsuControllerModule module;
    auto ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(ServiceImpInVmMode());
    module.UnInitialize();
}

/*
 * 用例描述:VM 模式 Initialize 在默认配置下成功(enableTls=true 但 certDir 有默认值)
 * 测试步骤:
 * 1、SetConfigVmMode(true)
 * 2、UbseSsuControllerModule Initialize
 * 预期结果:
 * 1、返回 UBSE_OK(默认 certDir="/var/lib/ubse/lcne_cert/" 使 cfg.IsValid()=true)
 * 注:UbseVsockConfig 有默认 certDir,UbseGetStr 返回空时保留默认值
 */
TEST_F(TestUbseSsuControllerModuleVmMode, Init_VmMode_DefaultConfig_Succeeds)
{
    SetConfigVmMode(true);
    UbseSsuControllerModule module;
    auto ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_OK);
    module.UnInitialize();
}

/*
 * 用例描述:VM 模式 UnInitialize 后 ServiceImp 状态与 controller 成员全部清空
 * 测试步骤:
 * 1、SetConfigVmMode(true) 后 Initialize 成功
 * 2、调用 UnInitialize()
 * 预期结果:
 * 1、ServiceImp::IsVmMode() 返回 false
 * 2、ServiceImp 的 vmCache_ 指针为 nullptr
 * 3、controller 的 vmCache_ 与 vsockServer_ 为 nullptr,vmMode_ 为 false
 */
TEST_F(TestUbseSsuControllerModuleVmMode, UnInitialize_VmMode_ClearsServiceState)
{
    SetConfigVmMode(true);
    UbseSsuControllerModule module;
    ASSERT_EQ(module.Initialize(), UBSE_OK);
    ASSERT_TRUE(ServiceImpInVmMode());
    ASSERT_EQ(ServiceImpVmCache(), module.vmCache_.get());

    module.UnInitialize();

    EXPECT_FALSE(ServiceImpInVmMode());
    EXPECT_EQ(ServiceImpVmCache(), nullptr);
    EXPECT_EQ(module.vmCache_, nullptr);
    EXPECT_FALSE(module.vmMode_);
}

/*
 * 用例描述:VM 模式 Initialize 中 vsock handler 注册失败时回滚所有已初始化状态
 * 测试步骤:
 * 1、SetConfigVmMode(true)
 * 2、Mock UbseVsockServer::RegisterHandler 返回 UBSE_ERR_OPERATION_FAILED
 * 3、调用 Initialize()
 * 预期结果:
 * 1、Initialize 返回 UBSE_ERR_OPERATION_FAILED
 * 2、ServiceImp 的 vmMode_ 为 false、vmCache_ 为 nullptr
 * 3、controller 的 vmCache_、vsockServer_ 为 nullptr,vmMode_ 为 false
 */
TEST_F(TestUbseSsuControllerModuleVmMode, Init_VmMode_HandlerRegistrationFails_RollsBackServiceState)
{
    SetConfigVmMode(true);
    MOCKER(&ubse::vsock::UbseVsockServer::RegisterHandler)
        .stubs()
        .will(returnValue(UBSE_ERR_OPERATION_FAILED));
    UbseSsuControllerModule module;

    EXPECT_EQ(module.Initialize(), UBSE_ERR_OPERATION_FAILED);

    EXPECT_FALSE(ServiceImpInVmMode());
    EXPECT_EQ(ServiceImpVmCache(), nullptr);
    EXPECT_EQ(module.vmCache_, nullptr);
    EXPECT_EQ(module.vsockServer_, nullptr);
    EXPECT_FALSE(module.vmMode_);
}

// ============================================================================
// D. host 模式向后兼容
// ============================================================================

/*
 * 用例描述:host 模式(vm_mode=false)Initialize 时 ServiceImp 不被标记为 VM 模式,
 *          行为与特性启用前一致(向后兼容)
 * 测试步骤:
 * 1、SetConfigVmMode(false)
 * 2、UbseSsuControllerModule Initialize(host 模式需要 task executor,本用例仅验证 vmMode_ 标记)
 * 3、UbseSsuServiceImp::IsVmMode() 检查
 * 预期结果:
 * 1、ServiceImp::IsVmMode() 返回 false
 * 注:host 模式 Initialize 会尝试获取 task executor 模块,可能失败,但不影响 vmMode_ 标记验证
 */
TEST_F(TestUbseSsuControllerModuleVmMode, Init_HostMode_BehaviorUnchanged)
{
    SetConfigVmMode(false);
    UbseSsuControllerModule module;
    // host 模式 Initialize 可能因 task executor 缺失而返回错误,这里只关注 vmMode_ 标记
    (void)module.Initialize();
    EXPECT_FALSE(ServiceImpInVmMode());
    module.UnInitialize();
}

} // namespace ubse::ssu::controller::ut
