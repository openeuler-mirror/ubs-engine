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

#include "test_resource_query.h"

#include <mempooling_module.h>
#include "mempooling_def.h"

#include "mockcpp/mockcpp.hpp"
#include "resource_query.h"

using namespace vm;
using namespace ::vm::mempooling;
namespace ubse::ut::vm {
// 设置测试环境
void TestResourceQuery::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestResourceQuery::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestResourceQuery, GetLocalHostVmCollectData)
{
    HostVmDomainInfo mockHostVmDomainInfo{
        "0", "0", "metric",
        {{"uuid", "name", "state", 1, 1000, 0, 0, "nodeId", "host", 1, 1, {{1, {1, 1, 1, 1, 1}}}}}};
    HostNumaCpuInfo mockHostNumaCpuInfo{"0", "0", 1, "metric", {}};
    HostVmDomainInfo hostVmDomainInfo;
    HostNumaCpuInfo hostNumaCpuInfo;
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo)
        .stubs()
        .with(outBound(mockHostVmDomainInfo))
        .will(returnValue(VM_OK));
    MOCKER(ResourceQuery::GetLocalHostNumaInfo).stubs().with(outBound(mockHostNumaCpuInfo)).will(returnValue(VM_OK));
    auto ret = ResourceQuery::GetLocalHostVmCollectData(hostVmDomainInfo, hostNumaCpuInfo);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(hostVmDomainInfo.toString(), mockHostVmDomainInfo.toString());
    EXPECT_EQ(hostNumaCpuInfo.toString(), mockHostNumaCpuInfo.toString());
}

TEST_F(TestResourceQuery, GetLocalHostVmCollectDataFail)
{
    HostVmDomainInfo hostVmDomainInfo;
    HostNumaCpuInfo hostNumaCpuInfo;
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(returnValue(VM_ERROR));
    auto ret = ResourceQuery::GetLocalHostVmCollectData(hostVmDomainInfo, hostNumaCpuInfo);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).reset();
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(returnValue(VM_OK));
    MOCKER(ResourceQuery::GetLocalHostNumaInfo).stubs().will(returnValue(VM_ERROR));
    ret = ResourceQuery::GetLocalHostVmCollectData(hostVmDomainInfo, hostNumaCpuInfo);
    EXPECT_EQ(ret, VM_ERROR);
}

UBSRMRSGetVmInfoListOnNodeFunc MockUBSRMRSGetVmInfoListOnNode()
{
    return [](std::vector<::vm::mempooling::VmDomainInfo> &vmInfoList) {
        ::vm::mempooling::VmDomainInfo vmInfo{{}, {}, 0};
        vmInfoList.push_back(vmInfo);
        return VM_OK;
    };
}

UBSRMRSGetVmInfoListOnNodeFunc MockUBSRMRSGetVmInfoListOnNodeErr()
{
    return [](std::vector<::vm::mempooling::VmDomainInfo> &vmInfoList) {
        return VM_ERROR;
    };
}

TEST_F(TestResourceQuery, GetLocalHostVmDomainInfo)
{
    HostVmDomainInfo hostVmDomainInfo{
        "0", "0", "metric",
        {{"uuid", "name", "state", 1, 1000, 0, 0, "nodeId", "host", 1, 1, {{1, {1, 1, 1, 1, 1}}}}}};
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetVmInfoListOnNode));
    auto ret = ResourceQuery::GetLocalHostVmDomainInfo(hostVmDomainInfo);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestResourceQuery, GetLocalHostVmDomainInfoFail)
{
    HostVmDomainInfo hostVmDomainInfo{};
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode)
        .stubs()
        .will(returnValue(static_cast<UBSRMRSGetVmInfoListOnNodeFunc>(nullptr)));
    auto ret = ResourceQuery::GetLocalHostVmDomainInfo(hostVmDomainInfo);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).reset();
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetVmInfoListOnNodeErr));
    ret = ResourceQuery::GetLocalHostVmDomainInfo(hostVmDomainInfo);
    EXPECT_EQ(ret, VM_ERROR);
}

UBSRMRSGetNumaInfoListOnNodeFunc UBSRMRSGetNumaInfoListOnNode()
{
    return [](std::vector<::vm::mempooling::NumaInfo> &numaInfoList) {
        ::vm::mempooling::NumaInfo numaInfo{};
        numaInfoList.push_back(numaInfo);
        return VM_OK;
    };
}

UBSRMRSGetNumaInfoListOnNodeFunc UBSRMRSGetNumaInfoListOnNodeErr()
{
    return [](std::vector<::vm::mempooling::NumaInfo> &numaInfoList) {
        return VM_ERROR;
    };
}

TEST_F(TestResourceQuery, GetLocalHostNumaInfo)
{
    HostNumaCpuInfo hostNumaCpuInfo{"0", "0", 1, "metric", {}};
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).stubs().will(invoke(UBSRMRSGetNumaInfoListOnNode));
    auto ret = ResourceQuery::GetLocalHostNumaInfo(hostNumaCpuInfo);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestResourceQuery, GetLocalHostNumaInfoFail)
{
    HostNumaCpuInfo hostNumaCpuInfo{};
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode)
        .stubs()
        .will(returnValue(static_cast<UBSRMRSGetNumaInfoListOnNodeFunc>(nullptr)));
    auto ret = ResourceQuery::GetLocalHostNumaInfo(hostNumaCpuInfo);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).reset();
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).stubs().will(invoke(UBSRMRSGetNumaInfoListOnNodeErr));
    ret = ResourceQuery::GetLocalHostNumaInfo(hostNumaCpuInfo);
    EXPECT_EQ(ret, VM_ERROR);
}
}