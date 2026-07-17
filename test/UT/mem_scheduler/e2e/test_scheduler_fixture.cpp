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
#include "test_scheduler_fixture.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_mem_scheduler_impl.h"

namespace ubse::mem::scheduler::ut {

void TestSchedulerEndToEnd::SetUp()
{
    Test::SetUp();
    SetupDefaultConfig();
    SchedulerImpl::GetInstance().initialized_ = false;
    SchedulerImpl::GetInstance().Init();
}

void TestSchedulerEndToEnd::TearDown()
{
    SchedulerImpl::GetInstance().ClearCache();
    GlobalMockObject::verify();
    Test::TearDown();
}

void TestSchedulerEndToEnd::SetPageSize(const std::string& pageSize)
{
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>).reset();
    SetupMockConfig("", "host-1,host-2,host-3,host-4,host-5,host-6,host-7,host-8");
    // 用 64K 页覆盖默认的 4K 页 mock
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(pageSize))
        .will(returnValue(UBSE_OK));

    std::string radiusBorrow;
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.borrow")), outBound(radiusBorrow))
        .will(returnValue(UBSE_OK));

    std::string radiusLender;
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.lender")), outBound(radiusLender))
        .will(returnValue(UBSE_OK));

    SchedulerImpl::GetInstance().ClearCache();
    SchedulerImpl::GetInstance().initialized_ = false;
    SchedulerImpl::GetInstance().Init();
}

void TestSchedulerEndToEnd::SetupRadiusConfig(const std::string& radiusBorrow, const std::string& radiusLender)
{
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>).reset();

    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(std::string("")))
        .will(returnValue(UBSE_OK));

    std::string groupStr = "host-1,host-2,host-3,host-4,host-5,host-6,host-7,host-8";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupStr))
        .will(returnValue(UBSE_OK));

    std::string defaultPageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(defaultPageSize))
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.borrow")), outBound(radiusBorrow))
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.lender")), outBound(radiusLender))
        .will(returnValue(UBSE_OK));

    SchedulerImpl::GetInstance().ClearCache();
    SchedulerImpl::GetInstance().initialized_ = false;
    SchedulerImpl::GetInstance().Init();
}

void TestSchedulerEndToEnd::SetupLenderBalanceConfig(bool enabled)
{
    MOCKER(&UbseNodeController::GetAllNodes).reset();
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>).reset();
    MOCKER_CPP(&config::UbseConfModule::GetConf<bool>).reset();

    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(std::string("")))
        .will(returnValue(UBSE_OK));

    std::string groupStr = "host-1,host-2,host-3,host-4,host-5,host-6,host-7,host-8";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupStr))
        .will(returnValue(UBSE_OK));

    std::string defaultPageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(defaultPageSize))
        .will(returnValue(UBSE_OK));

    std::string radiusEmpty = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.borrow")), outBound(radiusEmpty))
        .will(returnValue(UBSE_OK));

    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.lender")), outBound(radiusEmpty))
        .will(returnValue(UBSE_OK));

    bool val = enabled;
    MOCKER_CPP(&config::UbseConfModule::GetConf<bool>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("lender.balance")), outBound(val))
        .will(returnValue(UBSE_OK));

    SchedulerImpl::GetInstance().ClearCache();
    SchedulerImpl::GetInstance().initialized_ = false;
    SchedulerImpl::GetInstance().Init();
}

void TestSchedulerEndToEnd::SetupFilterTestConfig(const std::string& providerStr, bool providerOk,
                                                  const std::string& groupStr, bool groupOk)
{
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>).reset();
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    std::string providerOut = providerStr;
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(providerOut))
        .will(returnValue(providerOk ? UBSE_OK : UBSE_ERROR));

    std::string groupOut = groupStr;
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupOut))
        .will(returnValue(groupOk ? UBSE_OK : UBSE_ERROR));

    std::string defaultPageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(defaultPageSize))
        .will(returnValue(UBSE_OK));

    std::string radiusBorrow;
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.borrow")), outBound(radiusBorrow))
        .will(returnValue(UBSE_OK));

    std::string radiusLender;
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("radius.lender")), outBound(radiusLender))
        .will(returnValue(UBSE_OK));

    bool lenderBalanceDefault = false;
    MOCKER_CPP(&config::UbseConfModule::GetConf<bool>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("lender.balance")), outBound(lenderBalanceDefault))
        .will(returnValue(UBSE_OK));
}

} // namespace ubse::mem::scheduler::ut
