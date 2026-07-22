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

#ifndef IT_URMA_QOS_CASES_H
#define IT_URMA_QOS_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::urma_qos {

// URMA QoS 创建测试：创建单优先级（pri=0, bw=100Gbps）配置
void RunP0UrmaQosCreateOk01(ubse::it::infra::ItCluster& cluster);

// URMA QoS 创建+查询测试：创建双优先级配置后查询验证
void RunP1UrmaQosCreateGetMatch01(ubse::it::infra::ItCluster& cluster);

// URMA QoS 全生命周期测试：创建→查询→删除→查询验证空
void RunP1UrmaQosLifecycle01(ubse::it::infra::ItCluster& cluster);

// --- CLI 交互测试 ---

// CLI 创建成功：create urma-qos --pri 0 --cir 100
void RunP0CliCreateQosOk01(ubse::it::infra::ItCluster& cluster);

// CLI 创建成功（双优先级）
void RunP1CliCreateQosParamVariant01(ubse::it::infra::ItCluster& cluster);

// CLI 参数验证：缺少必要参数
void RunP0CliCreateQosBadParam01(ubse::it::infra::ItCluster& cluster);

// CLI 参数验证：无效 priority（超出 0/1 范围）
void RunP0CliCreateQosInvalidVal01(ubse::it::infra::ItCluster& cluster);

// CLI 参数验证：重复 priority
void RunP0CliCreateQosDup01(ubse::it::infra::ItCluster& cluster);

// CLI 参数验证：count 超过上限(2)
void RunP0CliCreateQosOverCnt01(ubse::it::infra::ItCluster& cluster);

// CLI 参数验证：pri/cir 数量不匹配
void RunP0CliCreateQosBadParam02(ubse::it::infra::ItCluster& cluster);

// CLI 参数验证：bandwidth 为 0
void RunP2CliCreateQosInvalidVal01(ubse::it::infra::ItCluster& cluster);

// CLI 删除成功
void RunP1CliDelQosOk01(ubse::it::infra::ItCluster& cluster);

// CLI 查询空（未创建）
void RunP0CliDisplayQosOk01(ubse::it::infra::ItCluster& cluster);

// CLI 全生命周期：创建→display 表格→删除→display 空
void RunP1CliDisplayQosCrossConsist01(ubse::it::infra::ItCluster& cluster);

// P0-CliDelQos-NotReady-01: CLI delete urma-qos without create, expect error
void RunP0CliDelQosNotReady01(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::urma_qos

#endif // IT_URMA_QOS_CASES_H
