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
#include "tests/mem_borrow/mem_borrow_cases.h"

using ubse::it::infra::Tongsuan1dFourNodesBoundaryHostnameScenario;

// ====================================================================
// 四节点边界主机名场景测试
//
// 节点1~4 分别上报主机名 a / 63个'a' / b / 63个'b'，均配置
// [ubse.memory] group=全部主机名、provider=节点3,节点4。
// 验证 group/provider 对 63 字符最长主机名按全名精确匹配：
// 节点1 指定节点3（provider）创建 FD 成功；节点2 指定节点4（provider）创建 FD 成功。
// ====================================================================

// P0-FdCreateLender-BoundaryHostname-01: 四节点边界主机名场景，
// 节点1指定节点3创建FD成功，节点2指定节点4创建FD成功
TEST_F(Tongsuan1dFourNodesBoundaryHostnameScenario, P0FdCreateLenderBoundaryHostname01)
{
    ubse::it::tests::mem_borrow::RunP0FdCreateLenderBoundaryHostname01(Cluster());
}
