/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/Mulan PSL v2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "fault_cases.h"

#include <gtest/gtest.h>

#include "ubse_ras.h"
#include "it_assertion.h"
#include "it_console_log.h"
#include "it_xalarm_helper.h"

namespace ubse::it::tests::fault {

// OOM ack alarm ID = 原始事件ID + 1
static constexpr unsigned short OOM_ACK_ALARM_ID = ubse::ras::ALARM_OOM_EVENT + 1;

// 向指定节点注入OOM事件，验证RAS故障处理链路
void RunVmOomEscapeBorrowTest(ubse::it::infra::ItCluster& cluster, const std::string& nodeId,
                              const std::vector<int>& nids, int timeout)
{
    auto& node = cluster.GetNode(nodeId);
    auto fifoPath = node.GetXalarmFifoPath();
    auto workDir = node.GetWorkDir();
    int sync = 1;   // 同步标志（需要ack）
    int reason = 2; // 故障原因码（大页OOM）

    // 清理上次可能残留的ack文件
    ubse::it::infra::ItXalarmHelper::ClearAckResult(workDir, OOM_ACK_ALARM_ID);

    // 向FIFO写入OOM事件
    auto ret = ubse::it::infra::ItXalarmHelper::InjectOomEvent(fifoPath, 99999, nids, sync, timeout, reason);
    EXPECT_IT_OK(ret);

    // 等待RAS处理完成的ack（xalarm_stub写文件，反映RAS handler执行结果）
    uint32_t rasRetCode = 0;
    auto ackRet = ubse::it::infra::ItXalarmHelper::WaitForAckResult(workDir, OOM_ACK_ALARM_ID, 15000, rasRetCode);
    EXPECT_IT_OK(ackRet) << "Timeout waiting for RAS OOM ack";
    EXPECT_EQ(rasRetCode, 0) << "RAS OOM handler failed, retCode=" << rasRetCode;
}

} // namespace ubse::it::tests::fault
