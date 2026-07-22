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

#ifndef IT_SMOKE_CASES_H
#define IT_SMOKE_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::smoke {

// 冒烟启动测试：验证单节点集群启动后进程存活
void RunStartupTest(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::smoke

#endif // IT_SMOKE_CASES_H
