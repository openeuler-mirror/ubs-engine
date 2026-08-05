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

#ifndef IT_SEI_DEGRADE_CASES_H
#define IT_SEI_DEGRADE_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::sei_degrade {
// 节点2(借入方)依次执行: 借入FD → 借入NUMA → 归还FD → 归还NUMA(末次)
// 验证 SEI mock 文件状态在各阶段的正确性
void RunITSeiDegradeLifecycle(ubse::it::infra::ItCluster& cluster);

// 配置 sei.enable=false，验证整个借用→归还流程中 SEI mock 文件始终不变
void RunITSeiDegradeDisabled(ubse::it::infra::ItCluster& cluster);
} // namespace ubse::it::tests::sei_degrade

#endif // IT_SEI_DEGRADE_CASES_H
