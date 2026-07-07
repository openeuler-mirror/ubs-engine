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

#include "scenario.h"
#include "tests/npu/npu_cases.h"

using ubse::it::infra::ZhisuanNpuSingleNodeScenario;

TEST_F(ZhisuanNpuSingleNodeScenario, DeviceListQuery)
{
    ubse::it::tests::npu::RunDeviceListQueryTest(Cluster());
}

TEST_F(ZhisuanNpuSingleNodeScenario, DeviceAllocFreeLifecycle)
{
    ubse::it::tests::npu::RunDeviceAllocFreeLifecycleTest(Cluster());
}

TEST_F(ZhisuanNpuSingleNodeScenario, UbaTidSizeQueryAfterAlloc)
{
    ubse::it::tests::npu::RunUbaTidSizeQueryTest(Cluster());
}

TEST_F(ZhisuanNpuSingleNodeScenario, RepeatAllocAndFree)
{
    ubse::it::tests::npu::RunRepeatAllocAndFreeTest(Cluster());
}

TEST_F(ZhisuanNpuSingleNodeScenario, PreemptDevice)
{
    ubse::it::tests::npu::RunPreemptDeviceTest(Cluster());
}

TEST_F(ZhisuanNpuSingleNodeScenario, RepeatDealloc)
{
    ubse::it::tests::npu::RunRepeatDeallocTest(Cluster());
}

TEST_F(ZhisuanNpuSingleNodeScenario, ConcurrentSuccess)
{
    ubse::it::tests::npu::RunConcurrentSuccessTest(Cluster());
}
