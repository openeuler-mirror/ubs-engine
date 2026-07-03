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

#include "smoke_cases.h"

#include <gtest/gtest.h>

namespace ubse::it::tests::smoke {

// 验证节点"1"在集群启动后处于运行状态
void RunStartupTest(ubse::it::infra::ItCluster& cluster)
{
    EXPECT_TRUE(cluster.IsNodeRunning("1"));
}

} // namespace ubse::it::tests::smoke
