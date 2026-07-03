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

#ifndef IT_NPU_CASES_H
#define IT_NPU_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::npu {

// NPU设备列表查询测试：验证查询返回预期的NPU和NIC_PFE设备
void RunDeviceListQueryTest(ubse::it::infra::ItCluster& cluster);

// NPU设备分配+释放生命周期测试：验证完整的分配→使用→释放流程
void RunDeviceAllocFreeLifecycleTest(ubse::it::infra::ItCluster& cluster);

// UBA/TID/Size查询测试：验证设备分配后UBA地址、TID和Size查询结果正确
void RunUbaTidSizeQueryTest(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::npu

#endif // IT_NPU_CASES_H
