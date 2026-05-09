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

#include "test_mem_handler.h"

#include <ubse_error.h>
#include <ubse_event.h>
#include <ubse_timer.h>
#include <mockcpp/mockcpp.hpp>
#include "alarm_handler.h"
#include "mem_handler.h"
#include "resource_query.h"
#include "vm_configuration.h"
#include "vm_error.h"

using namespace vm;
using namespace ubse::event;
namespace ubse::ut::vm {
const uint64_t BORROW_SIZE = 1073741824;
const uint16_t REMOTE_NUMA_ID = 5;
// 设置测试环境
void TestMemHandler::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestMemHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestMemHandler, InitAndTerminate)
{
    MOCKER(ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(MemHandler::GetInstance().Init(), VM_OK);
    EXPECT_NO_THROW(MemHandler::GetInstance().Terminate());
    MOCKER(ubse::timer::UbseTimerHandlerRegister).reset();
}

const int ERR_WATERMARK = 123;

/*
 * 用例描述：
 * 测试Init方法
 * 测试步骤：
 * 1.GetDataFromMem获取数据失败
 * 2.GetDataFromMem获取成功，更新NumaInfo失败
 * 3.GetDataFromMem获取成功，更新NumaInfo成功，事件通知失败
 * 3.GetDataFromMem获取成功，更新NumaInfo成功，事件通知成功
 * 预期结果：
 * 1.返回VM_ERROR
 * 2.返回VM_ERROR
 * 3.返回VM_ERROR
 * 4.返回VM_OK
 */
TEST_F(TestMemHandler, NotifyVm)
{
    WatermarkWarningType warningType = WatermarkWarningType::LOW_WATERMARK;
    Notify notify;

    MOCKER(AlarmHandler::MemNotifyEventHandler).stubs().will(returnValue(VM_OK));
    VmResult result = MemHandler::NotifyVm(warningType, notify);
    EXPECT_EQ(result, VM_OK);
    MOCKER(AlarmHandler::MemNotifyEventHandler).reset();

    warningType = WatermarkWarningType::CLEAR_BORROW;
    MOCKER(AlarmHandler::MemNotifyEventHandler).stubs().will(returnValue(VM_ERROR));
    result = MemHandler::NotifyVm(warningType, notify);
    EXPECT_EQ(result, VM_ERROR);
}

/*
 * 用例描述：
 * 测试ToString方法
 * 测试步骤：
 * 1.传入高水位枚举
 * 2.传入低水位枚举
 * 3.传入正常水位枚举
 * 4.传入非法值
 * 预期结果：
 * 1.返回高水位字符串
 * 1.返回低水位字符串
 * 1.返回非告警字符串
 * 1.返回空字符串
 */
TEST_F(TestMemHandler, ToString)
{
    std::string lowWaterMark = "LOW_WATERMARK";
    std::string highWaterMark = "HIGH_WATERMARK";
    std::string clearBorrow = "CLEAR_BORROW";
    std::string noWarn = "NO_WARN";
    std::string errStr;

    EXPECT_EQ(MemHandler::ToString(WatermarkWarningType::LOW_WATERMARK), lowWaterMark);
    EXPECT_EQ(MemHandler::ToString(WatermarkWarningType::HIGH_WATERMARK), highWaterMark);
    EXPECT_EQ(MemHandler::ToString(WatermarkWarningType::CLEAR_BORROW), clearBorrow);
    EXPECT_EQ(MemHandler::ToString(WatermarkWarningType::NO_WARN), noWarn);
    EXPECT_EQ(MemHandler::ToString((WatermarkWarningType)ERR_WATERMARK), errStr);
}

VmResult MockGetVmCollectData(HostVmDomainInfo &hostVmDomainInfo, HostNumaCpuInfo &hostNumaCpuInfo)
{
    NumaCpuInfo numaCpuInfo1 = {.socketId = 0, .nodeId = "Node0", .numaId = 0, .nrHugePage = 0};
    NumaCpuInfo numaCpuInfo2 = {.socketId = 0, .nodeId = "Node0", .numaId = 1, .nrHugePage = 1};
    hostNumaCpuInfo.numaCpuInfos.emplace_back(numaCpuInfo1);
    hostNumaCpuInfo.numaCpuInfos.emplace_back(numaCpuInfo2);
    return VM_OK;
}

TEST_F(TestMemHandler, IsVmExists)
{
    NumaCpuInfo numaCpuInfo{};
    HostVmDomainInfo hostVmDomainInfo{};
    EXPECT_FALSE(MemHandler::IsVmExists(numaCpuInfo, hostVmDomainInfo));

    numaCpuInfo.numaId = 1;
    numaCpuInfo.socketId = 0;
    VmDomainInfo vmDomainInfo;
    mempooling::VmDomainNumaInfo vmDomainNumaInfo{1, 0, 0, 0, 0};
    vmDomainInfo.numaMemInfo.emplace(1, vmDomainNumaInfo);
    hostVmDomainInfo.vmDomainInfos.push_back(vmDomainInfo);

    vmDomainInfo.numaMemInfo.clear();
    hostVmDomainInfo.vmDomainInfos.clear();
    vmDomainNumaInfo.numaId = 0;
    vmDomainInfo.numaMemInfo.emplace(0, vmDomainNumaInfo);
    hostVmDomainInfo.vmDomainInfos.push_back(vmDomainInfo);
    EXPECT_FALSE(MemHandler::IsVmExists(numaCpuInfo, hostVmDomainInfo));
}

TEST_F(TestMemHandler, WaterNotifyEvents)
{
    NumaCpuInfo numaCpuInfo{};
    constexpr uint64_t totalHugePage = 1000;
    constexpr uint64_t HighFreeHugePage = 80;
    constexpr uint64_t LowFreeHugePage = 400;
    constexpr uint64_t ErrorFreeHugePage = 2000;
    numaCpuInfo.nrHugePage = totalHugePage;
    numaCpuInfo.freeHugePage = ErrorFreeHugePage;
    size_t borrowInfoSize = 1;
    Notify notify{};
    MOCKER_CPP(&VmConfiguration::GetBorrowWatermark, std::string(VmConfiguration::*)())
        .stubs()
        .will(returnValue(std::string("92")));
    MOCKER_CPP(&VmConfiguration::GetLowWatermark, std::string(VmConfiguration::*)())
        .stubs()
        .will(returnValue(std::string("80")));
    auto ret = MemHandler::WaterNotifyEvent(numaCpuInfo, borrowInfoSize, notify);
    EXPECT_EQ(ret, WatermarkWarningType::NO_WARN);
    numaCpuInfo.freeHugePage = HighFreeHugePage;
    ret = MemHandler::WaterNotifyEvent(numaCpuInfo, borrowInfoSize, notify);
    EXPECT_EQ(ret, WatermarkWarningType::HIGH_WATERMARK);
    numaCpuInfo.freeHugePage = LowFreeHugePage;
    ret = MemHandler::WaterNotifyEvent(numaCpuInfo, borrowInfoSize, notify);
    EXPECT_EQ(ret, WatermarkWarningType::LOW_WATERMARK);
    borrowInfoSize = 0;
    ret = MemHandler::WaterNotifyEvent(numaCpuInfo, borrowInfoSize, notify);
    EXPECT_EQ(ret, WatermarkWarningType::NO_WARN);
}

TEST_F(TestMemHandler, NotifyReturnMemTest)
{
    NumaCpuInfo numaCpuInfo = {.socketId = 0, .nodeId = "Node0", .numaId = 0};
    MOCKER(MemHandler::NotifyVm).stubs().will(returnValue(VM_OK));
    MemHandler::NotifyReturnMem(numaCpuInfo);
}

TEST_F(TestMemHandler, TransNotifyTest)
{
    Notify notify = Notify{
        .percent = 1,
        .highWaterMark = 1,
        .lowWaterMark = 1,
        .nodeId = "Node0",
        .socketId = 0,
        .numaId = 0,
        .memTotal = 1,
        .memUsed = 1,
        .memFree = 0,
    };
    std::string json = notify.ToJson();

    Notify notify1;

    EXPECT_EQ(MemHandler::TransNotify(json, notify1), VM_OK);
}

TEST_F(TestMemHandler, MemNotifyEventHandlerTest)
{
    VmConfiguration::GetInstance().Initialize(1);
    MOCKER(&ResourceQuery::GetLocalHostVmCollectData).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(MemHandler::CheckNumaWaterLine(), VM_ERROR);
    MOCKER(&ResourceQuery::GetLocalHostVmCollectData).reset();

    MOCKER(&ResourceQuery::GetLocalHostVmCollectData).stubs().will(invoke(MockGetVmCollectData));
    MOCKER(&MemHandler::GetMemoryBorrowInfo).stubs().will(returnValue(VM_OK));
    MOCKER(MemHandler::IsVmExists).stubs().will(returnValue(true));
    MOCKER(MemHandler::NotifyReturnMem).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(MemHandler::CheckNumaWaterLine(), VM_OK);
    MOCKER(MemHandler::IsVmExists).reset();

    MOCKER(MemHandler::IsVmExists).stubs().will(returnValue(false));
    MOCKER(MemHandler::WaterNotifyEvent).stubs().will(returnValue(WatermarkWarningType::HIGH_WATERMARK));
    MOCKER(MemHandler::NotifyVm).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(MemHandler::CheckNumaWaterLine(), VM_OK);
}

UbseResult MockUbseGetNumaMemDebtInfoWithNode(std::vector<UbseNumaMemoryImportDebtInfo> &debtInfos)
{
    UbseNumaMemoryImportDebtInfo debtInfo{};
    return UBSE_OK;
}

TEST_F(TestMemHandler, GetMemoryBorrowInfoTest)
{
    std::unordered_map<unsigned int, unsigned int> borrowInfo;
    MOCKER(UbseGetNumaMemImportDebtInfoWithLocalNode).stubs().will(returnValue(UBSE_ERR_AUTH_FAILED));
    EXPECT_EQ(MemHandler::GetMemoryBorrowInfo(borrowInfo), VM_ERROR);
    MOCKER(UbseGetNumaMemImportDebtInfoWithLocalNode).reset();
    MOCKER(UbseGetNumaMemImportDebtInfoWithLocalNode).stubs().will(invoke(MockUbseGetNumaMemDebtInfoWithNode));
    EXPECT_EQ(MemHandler::GetMemoryBorrowInfo(borrowInfo), VM_OK);
}

} // namespace ubse::ut::vm
