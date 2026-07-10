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

#include "topo_cases.h"

#include <gtest/gtest.h>

#include "it_console_log.h"

namespace ubse::it::tests::topo {

void RunQueryNodeTopo001(ubse::it::infra::ItCluster& cluster, const std::string& nodeId)
{
    auto& cli = cluster.GetCliInvoker(nodeId);

    // 正确参数：display topo -t cpu 应返回完整拓扑信息
    std::vector<ubse::it::infra::ItTopoCpuLink> topoLinks;
    auto ret = cli.QueryTopoCpu(topoLinks);
    EXPECT_EQ(ret, UBS_SUCCESS) << "display topo -t cpu should succeed";
    EXPECT_FALSE(topoLinks.empty()) << "display topo -t cpu should return topology entries";
    IT_LOG_INFO << "display topo -t cpu returned " << topoLinks.size() << " links";

    // 统计有效linkId（非空且非"-"）的数量，预期为2
    int validLinkCount = 0;
    for (const auto& link : topoLinks) {
        if (!link.linkId.empty() && link.linkId != "-") {
            validLinkCount++;
        }
    }
    EXPECT_EQ(validLinkCount, 2) << "display topo -t cpu should have 2 valid links, but got " << validLinkCount;
    IT_LOG_INFO << "display topo -t cpu valid link count: " << validLinkCount;

    // 错误参数：display topo -t cpu1 应返回失败
    std::string badOutput = cli.ExecCli("display topo -t cpu1");
    bool isFailed = (badOutput.find("fail") != std::string::npos || badOutput.find("error") != std::string::npos ||
                     badOutput.find("Error") != std::string::npos || badOutput.find("invalid") != std::string::npos ||
                     badOutput.find("Invalid") != std::string::npos || badOutput.empty());
    EXPECT_TRUE(isFailed) << "display topo -t cpu1 should fail, but got: " << badOutput;
    IT_LOG_INFO << "display topo -t cpu1 returned: " << badOutput;
}

} // namespace ubse::it::tests::topo
