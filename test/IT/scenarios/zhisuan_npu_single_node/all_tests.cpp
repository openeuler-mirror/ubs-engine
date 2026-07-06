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

// NPU设备查询：验证设备列表查询返回预期的NPU和NIC_PFE设备
TEST_F(ZhisuanNpuSingleNodeScenario, DeviceListQuery)
{
    ubse::it::tests::npu::RunDeviceListQueryTest(Cluster());
}

// NPU设备生命周期：验证分配→使用→释放的完整生命周期
TEST_F(ZhisuanNpuSingleNodeScenario, DeviceAllocFreeLifecycle)
{
    ubse::it::tests::npu::RunDeviceAllocFreeLifecycleTest(Cluster());
}

// UBA/TID/Size查询：验证设备分配后UBA地址、TID和Size查询正确
TEST_F(ZhisuanNpuSingleNodeScenario, UbaTidSizeQueryAfterAlloc)
{
    ubse::it::tests::npu::RunUbaTidSizeQueryTest(Cluster());
}
