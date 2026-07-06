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

#include "fault_cases.h"

#include <thread>

#include <gtest/gtest.h>

#include "it_assertion.h"
#include "it_xalarm_helper.h"

namespace ubse::it::tests::fault {

// 向指定节点注入OOM事件，验证RAS故障处理链路
void RunOomEventTest(ubse::it::infra::ItCluster& cluster, const std::string& nodeId, const std::vector<int>& nids,
                     int timeout)
{
    // 获取目标节点的xalarm FIFO路径
    auto fifoPath = cluster.GetNodeProcess(nodeId).GetXalarmFifoPath();
    int sync = 1;   // 同步标志
    int reason = 2; // 故障原因码

    // 向FIFO写入OOM事件
    auto ret = ubse::it::infra::ItXalarmHelper::InjectOomEvent(fifoPath, 99999, nids, sync, 30, reason);
    EXPECT_IT_OK(ret);
}

} // namespace ubse::it::tests::fault
