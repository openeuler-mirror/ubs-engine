/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <memory>
#include <string>

#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_module.h"
#include "ubse_service_registry.h"
#include "ubse_ssu_http_handler.h"
#include "ubse_ssu_rpc_processor.h"
#include "ubse_ssu_service.h"
#include "ubse_ssu_service_imp.h"
#include "ubse_thread_pool_module.h"

// 在包含controller_module.cpp前，将PLUGIN_MODULE_IMPL置空，
// 避免与ssu_plugin共享库中的constructor符号冲突
#undef PLUGIN_MODULE_IMPL
#define PLUGIN_MODULE_IMPL(CLASS, MODULE_LIST)
// 包含实现文件以使被测代码纳入UT覆盖率统计
#include "ubse_ssu_controller_module.cpp"
#undef PLUGIN_MODULE_IMPL

namespace ubse::ut::ssu {
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::task_executor;
using namespace ubse::service;
using ubse::ssu::controller::UbseSsuControllerModule;
using ubse::ssu::controller::UbseSsuRpcProcessor;
using ubse::ssu::service::UbseSsuServiceImp;
using ubse::ssu::http_handler::RegisterSsuHttpHandlers;
using ubse::plugin::service::ssu::UbseSsuService;

namespace {
// 清除注册表中的SSU服务条目，保证用例间隔离
void ClearSsuServiceInRegistry()
{
    UbseServiceRegistry::GetInstance().services_.erase(UbseSsuService::kServiceName);
}

// 安装Stop阶段依赖mock（void方法），避免调用真实实现
void MockStopDeps()
{
    MOCKER(&UbseSsuServiceImp::StopClearTimer).stubs();
    MOCKER(&UbseSsuServiceImp::StopCollecting).stubs();
}

// 安装所有依赖mock为成功返回，用于Start成功路径及Stop验证
void MockStartDepsSuccess()
{
    MOCKER(&UbseSsuServiceImp::StartCollecting).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseSsuServiceImp::RebuildLedgerFromDevList).stubs();
    MOCKER(&UbseSsuServiceImp::StartClearTimer).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseSsuRpcProcessor::RegHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(RegisterSsuHttpHandlers).stubs().will(returnValue(UBSE_OK));
    MockStopDeps();
}

// 安装Start各阶段的失败mock（按调用顺序），用于验证Start提前返回
void MockStartCollectingFail()
{
    MOCKER(&UbseSsuServiceImp::StartCollecting).stubs().will(returnValue(UBSE_ERROR));
    MockStopDeps();
}
void MockStartClearTimerFail()
{
    MOCKER(&UbseSsuServiceImp::StartCollecting).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseSsuServiceImp::RebuildLedgerFromDevList).stubs();
    MOCKER(&UbseSsuServiceImp::StartClearTimer).stubs().will(returnValue(UBSE_ERROR));
    MockStopDeps();
}
void MockRegHandlerFail()
{
    MOCKER(&UbseSsuServiceImp::StartCollecting).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseSsuServiceImp::RebuildLedgerFromDevList).stubs();
    MOCKER(&UbseSsuServiceImp::StartClearTimer).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseSsuRpcProcessor::RegHandler).stubs().will(returnValue(UBSE_ERROR));
    MockStopDeps();
}
void MockRegisterHttpHandlersFail()
{
    MOCKER(&UbseSsuServiceImp::StartCollecting).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseSsuServiceImp::RebuildLedgerFromDevList).stubs();
    MOCKER(&UbseSsuServiceImp::StartClearTimer).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseSsuRpcProcessor::RegHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(RegisterSsuHttpHandlers).stubs().will(returnValue(UBSE_ERROR));
    MockStopDeps();
}
} // namespace

class TestUbseSsuControllerModule : public testing::Test {
public:
    TestUbseSsuControllerModule() = default;
    void SetUp() override
    {
        Test::SetUp();
        ClearSsuServiceInRegistry();
        // 清理可能残留的TaskExecutorModule注册，保证Initialize用例间隔离
        UbseContext::GetInstance().moduleMap_.erase(UbseTaskExecutorModule::kModuleName);
    }
    void TearDown() override
    {
        ClearSsuServiceInRegistry();
        // 清理测试中注入的TaskExecutorModule，避免影响其他用例
        UbseContext::GetInstance().moduleMap_.erase(UbseTaskExecutorModule::kModuleName);
        GlobalMockObject::verify();
        Test::TearDown();
    }
};

// ===================== Initialize =====================

/*
 * 用例描述：Initialize 在获取不到TaskExecutorModule时返回NULLPTR错误
 * 测试步骤：不向context注入TaskExecutorModule（moduleMap_中无对应条目）
 * 预期结果：返回UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbseSsuControllerModule, Initialize_ExecutorModuleNull_ReturnError)
{
    UbseSsuControllerModule module;
    EXPECT_EQ(module.Initialize(), UBSE_ERROR_NULLPTR);
}

/*
 * 用例描述：Initialize 在Create成功时返回OK
 * 测试步骤：向context注入真实TaskExecutorModule，mock Create返回UBSE_OK
 * 预期结果：返回UBSE_OK
 */
TEST_F(TestUbseSsuControllerModule, Initialize_Success)
{
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    UbseContext::GetInstance().moduleMap_[UbseTaskExecutorModule::kModuleName] = executorModule;
    MOCKER(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    UbseSsuControllerModule module;
    EXPECT_EQ(module.Initialize(), UBSE_OK);
}

/*
 * 用例描述：Initialize 在Create失败时返回错误码
 * 测试步骤：向context注入真实TaskExecutorModule，mock Create返回UBSE_ERROR
 * 预期结果：返回UBSE_ERROR
 */
TEST_F(TestUbseSsuControllerModule, Initialize_CreateFail_ReturnError)
{
    auto executorModule = std::make_shared<UbseTaskExecutorModule>();
    UbseContext::GetInstance().moduleMap_[UbseTaskExecutorModule::kModuleName] = executorModule;
    MOCKER(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_ERROR));
    UbseSsuControllerModule module;
    EXPECT_EQ(module.Initialize(), UBSE_ERROR);
}

// ===================== Start =====================

/*
 * 用例描述：Start 全部依赖成功时返回OK，且服务注册到Registry
 * 测试步骤：mock所有依赖返回成功
 * 预期结果：返回UBSE_OK，Registry中存在SSU服务
 */
TEST_F(TestUbseSsuControllerModule, Start_Success)
{
    MockStartDepsSuccess();
    UbseSsuControllerModule module;
    EXPECT_EQ(module.Start(), UBSE_OK);
    EXPECT_TRUE(UbseServiceRegistry::GetInstance().HasService(UbseSsuService::kServiceName));
    // Stop以清理注册表
    module.Stop();
}

/*
 * 用例描述：Start 在StartCollecting失败时提前返回错误
 * 测试步骤：mock StartCollecting返回错误
 * 预期结果：返回非OK，不再调用后续依赖
 */
TEST_F(TestUbseSsuControllerModule, Start_CollectingFail_ReturnError)
{
    MockStartCollectingFail();
    UbseSsuControllerModule module;
    EXPECT_NE(module.Start(), UBSE_OK);
}

/*
 * 用例描述：Start 在StartClearTimer失败时提前返回错误
 * 测试步骤：mock StartClearTimer返回错误
 * 预期结果：返回非OK
 */
TEST_F(TestUbseSsuControllerModule, Start_ClearTimerFail_ReturnError)
{
    MockStartClearTimerFail();
    UbseSsuControllerModule module;
    EXPECT_NE(module.Start(), UBSE_OK);
}

/*
 * 用例描述：Start 在RegHandler失败时提前返回错误
 * 测试步骤：mock RegHandler返回错误
 * 预期结果：返回非OK
 */
TEST_F(TestUbseSsuControllerModule, Start_RegHandlerFail_ReturnError)
{
    MockRegHandlerFail();
    UbseSsuControllerModule module;
    EXPECT_NE(module.Start(), UBSE_OK);
}

/*
 * 用例描述：Start 在RegisterSsuHttpHandlers失败时提前返回错误
 * 测试步骤：mock RegisterSsuHttpHandlers返回错误
 * 预期结果：返回非OK
 */
TEST_F(TestUbseSsuControllerModule, Start_RegisterHttpHandlersFail_ReturnError)
{
    MockRegisterHttpHandlersFail();
    UbseSsuControllerModule module;
    EXPECT_NE(module.Start(), UBSE_OK);
}

// ===================== Stop =====================

/*
 * 用例描述：Stop 在Start后调用，应注销服务且不崩溃
 * 测试步骤：先Start成功，再Stop
 * 预期结果：Stop后Registry中不存在SSU服务
 */
TEST_F(TestUbseSsuControllerModule, Stop_AfterStart_UnregistersService)
{
    MockStartDepsSuccess();
    UbseSsuControllerModule module;
    ASSERT_EQ(module.Start(), UBSE_OK);
    EXPECT_TRUE(UbseServiceRegistry::GetInstance().HasService(UbseSsuService::kServiceName));
    module.Stop();
    EXPECT_FALSE(UbseServiceRegistry::GetInstance().HasService(UbseSsuService::kServiceName));
}

/*
 * 用例描述：Stop 在未Start时调用（ssuService_为空），应安全返回不崩溃
 * 测试步骤：直接调用Stop
 * 预期结果：正常返回
 */
TEST_F(TestUbseSsuControllerModule, Stop_WithoutStart_NoCrash)
{
    MockStopDeps();
    UbseSsuControllerModule module;
    EXPECT_NO_THROW(module.Stop());
}

/*
 * 用例描述：UnInitialize 为空实现，调用应安全返回
 * 预期结果：正常返回
 */
TEST_F(TestUbseSsuControllerModule, UnInitialize_NoOp)
{
    UbseSsuControllerModule module;
    EXPECT_NO_THROW(module.UnInitialize());
}

/*
 * 用例描述：Name 返回固定模块名
 * 预期结果：返回kModuleName
 */
TEST_F(TestUbseSsuControllerModule, Name_ReturnsModuleName)
{
    UbseSsuControllerModule module;
    EXPECT_EQ(module.Name(), UbseSsuControllerModule::kModuleName);
}
} // namespace ubse::ut::ssu
