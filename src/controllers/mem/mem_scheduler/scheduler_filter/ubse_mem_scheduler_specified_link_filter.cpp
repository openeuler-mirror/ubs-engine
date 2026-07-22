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

#include "ubse_mem_scheduler_specified_link_filter.h"

namespace ubse::mem::scheduler {

UbseResult SpecifiedLinkFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                            const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    (void)nodeInfo;
    (void)account;

    auto linkInfoOpt = request.GetParamOpt<adapter_plugins::mmi::UbseMemLenderLinkInfo>("linkInfo");
    if (!linkInfoOpt.has_value()) {
        return UBSE_OK;
    }
    const auto& linkInfo = linkInfoOpt.value();
    if (linkInfo.lenderNode.empty()) {
        RecordError("linkInfo lenderNode is empty");
        return UBSE_SCHEDULER_ERROR_INVAL;
    }

    // filter by nodeId
    EraseNodesIf(nodes, [&](const NodeInfo& node) {
        if (node.nodeId != linkInfo.lenderNode) {
            RecordReject(node.nodeId, "not the specified lender node");
            return true;
        }
        return false;
    });

    if (nodes.empty()) {
        return UBSE_OK;
    }

    // filter by socketId
    uint32_t targetSocketId = static_cast<uint32_t>(linkInfo.lenderSocketId);
    auto& socketInfos = nodes.front().socketInfos;
    EraseSocketsIf(socketInfos, [&](const SocketInfo& socketInfo) {
        if (socketInfo.socketId != targetSocketId) {
            RecordReject(nodes.front().nodeId, std::string("socket=") + std::to_string(socketInfo.socketId) +
                                                   " not the specified lender socket");
            return true;
        }
        return false;
    });

    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
