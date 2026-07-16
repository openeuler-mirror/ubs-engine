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

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "ubse_mem_scheduler_specified_lender_filter.h"

namespace ubse::mem::scheduler {

UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

void SpecifiedLenderFilter::FilterByNodeId(std::vector<NodeInfo>& nodes, const NodeId& targetNodeId)
{
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                               [&](const NodeInfo& node) {
                                   if (node.nodeId != targetNodeId) {
                                       RecordReject(node.nodeId, "not the specified lender node");
                                       return true;
                                   }
                                   return false;
                               }),
                nodes.end());
}

void SpecifiedLenderFilter::FilterBySocketId(std::vector<NodeInfo>& nodes, SocketId targetSocketId,
                                             const NodeId& lenderNodeId)
{
    if (targetSocketId == UINT32_MAX || nodes.empty()) {
        return;
    }
    auto& socketInfos = nodes.front().socketInfos;
    socketInfos.erase(std::remove_if(socketInfos.begin(), socketInfos.end(),
                                     [&](const SocketInfo& socketInfo) {
                                         if (socketInfo.socketId != targetSocketId) {
                                             RecordReject(lenderNodeId, std::string("socket=") +
                                                                            std::to_string(socketInfo.socketId) +
                                                                            " not the specified lender socket");
                                             return true;
                                         }
                                         return false;
                                     }),
                      socketInfos.end());
}

void SpecifiedLenderFilter::FilterByNumaIds(std::vector<NodeInfo>& nodes, const std::set<NumaId>& targetNumaIds,
                                            const NodeId& lenderNodeId)
{
    if (targetNumaIds.empty() || nodes.empty()) {
        return;
    }
    for (auto& socketInfo : nodes.front().socketInfos) {
        socketInfo.numaInfos.erase(std::remove_if(socketInfo.numaInfos.begin(), socketInfo.numaInfos.end(),
                                                  [&](const NumaId& numaId) {
                                                      if (targetNumaIds.find(numaId) == targetNumaIds.end()) {
                                                          RecordReject(lenderNodeId,
                                                                       std::string("numa=") + std::to_string(numaId) +
                                                                           " not in specified lender numas");
                                                          return true;
                                                      }
                                                      return false;
                                                  }),
                                   socketInfo.numaInfos.end());
    }
}

UbseResult SpecifiedLenderFilter::CheckNumaCapacity(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                                    const std::unordered_map<NumaId, uint64_t>& numaToSize,
                                                    const SchedulerRequest& request)
{
    if (nodes.empty() || numaToSize.empty()) {
        return UBSE_OK;
    }
    auto* nodePtr = nodeInfo.GetNodeInfo(nodes.front().nodeId);
    if (nodePtr == nullptr) {
        RecordWarning(std::string("GetNodeInfo failed, node=") + nodes.front().nodeId);
        return UBSE_ERROR;
    }
    uint64_t blockSize = nodePtr->GetBlockSize() * ONE_M;
    auto highWatermarkOpt = request.GetParamOpt<size_t>("highWatermark");
    uint64_t highWatermark = static_cast<uint64_t>(highWatermarkOpt.value_or(MAX_PERCENT));

    for (auto& socketInfo : nodes.front().socketInfos) {
        socketInfo.numaInfos.erase(
            std::remove_if(socketInfo.numaInfos.begin(), socketInfo.numaInfos.end(),
                           [&](const NumaId& numaId) {
                               auto it = numaToSize.find(numaId);
                               if (it == numaToSize.end()) {
                                   RecordReject(nodes.front().nodeId, std::string("numa=") + std::to_string(numaId) +
                                                                          " not in specified lender numas");
                                   return true;
                               }
                               uint64_t requiredSize = it->second;
                               auto* numaInfo = nodeInfo.GetNumaInfo(nodes.front().nodeId, numaId);
                               if (numaInfo == nullptr) {
                                   RecordWarning(std::string("GetNumaInfo failed, node=") + nodes.front().nodeId +
                                                 ", numa=" + std::to_string(numaId));
                                   return true;
                               }
                               uint64_t available = numaInfo->GetAvailableLendSize(highWatermark, blockSize);
                               if (available < requiredSize) {
                                   RecordReject(nodes.front().nodeId,
                                                std::string("numa=") + std::to_string(numaId) +
                                                    " available=" + std::to_string(available) +
                                                    " < lender_size=" + std::to_string(requiredSize));
                                   return true;
                               }
                               return false;
                           }),
            socketInfo.numaInfos.end());
    }
    return UBSE_OK;
}

void SpecifiedLenderFilter::CleanupEmptyNodes(std::vector<NodeInfo>& nodes)
{
    for (auto& n : nodes) {
        n.socketInfos.erase(std::remove_if(n.socketInfos.begin(), n.socketInfos.end(),
                                           [](const SocketInfo& s) { return s.numaInfos.empty(); }),
                            n.socketInfos.end());
    }
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [](const NodeInfo& n) { return n.socketInfos.empty(); }),
                nodes.end());
}

UbseResult SpecifiedLenderFilter::ValidatePortId(const adapter_plugins::mmi::UbseMemLenderInfo& lender,
                                                 const SchedulerNodeManager& nodeInfo, SocketId socketId)
{
    auto* socketInfo = nodeInfo.GetSocketInfo(lender.nodeId, socketId);
    if (socketInfo == nullptr) {
        RecordWarning(std::string("GetSocketInfo failed, node=") + lender.nodeId +
                      ", socket=" + std::to_string(socketId));
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    if (socketInfo->GetPorts().count(lender.portId) == 0) {
        RecordError(std::string("lender portId=") + std::to_string(lender.portId) +
                    " is not valid or not UP on node=" + lender.nodeId + ", socketId=" + std::to_string(socketId));
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult SpecifiedLenderFilter::ValidateLenderConsistency(
    const std::vector<adapter_plugins::mmi::UbseMemLenderInfo>& lenderInfos, const SchedulerNodeManager& nodeInfo,
    SocketId& outSocketId, std::set<NumaId>& outTargetNumaIds, std::unordered_map<NumaId, uint64_t>& outNumaToSize)
{
    if (lenderInfos.empty()) {
        RecordError("lenderInfos is empty, can not validate consistency");
        return UBSE_SCHEDULER_ERROR_INVAL;
    }
    NodeId firstNodeId = lenderInfos[0].nodeId;
    outSocketId = static_cast<SocketId>(lenderInfos[0].socketId);
    if (outSocketId == UINT32_MAX && lenderInfos[0].numaId != UINT32_MAX) {
        outSocketId = nodeInfo.GetSocketIdByNumaId(firstNodeId, static_cast<NumaId>(lenderInfos[0].numaId));
    }

    for (size_t i = 0; i < lenderInfos.size(); ++i) {
        const auto& lender = lenderInfos[i];

        if (lender.portId != UINT32_MAX && lender.socketId == UINT32_MAX && lender.numaId == UINT32_MAX) {
            RecordError(std::string("lender portId=") + std::to_string(lender.portId) +
                        " specified but socketId and numaId is invalid");
            return UBSE_SCHEDULER_ERROR_INVAL;
        }

        SocketId currentSocketId = static_cast<SocketId>(lender.socketId);
        if (currentSocketId == UINT32_MAX && lender.numaId != UINT32_MAX) {
            currentSocketId = nodeInfo.GetSocketIdByNumaId(lender.nodeId, static_cast<NumaId>(lender.numaId));
        }

        if (lender.nodeId != firstNodeId) {
            RecordError(std::string("lender nodeId=") + lender.nodeId +
                        " inconsistent with first lender nodeId=" + firstNodeId);
            return UBSE_SCHEDULER_ERROR_INVAL;
        }
        if (currentSocketId != outSocketId) {
            RecordError(std::string("lender socketId=") + std::to_string(currentSocketId) +
                        " inconsistent with first lender socketId=" + std::to_string(outSocketId));
            return UBSE_SCHEDULER_ERROR_INVAL;
        }

        if (lender.portId != UINT32_MAX) {
            auto ret = ValidatePortId(lender, nodeInfo, currentSocketId);
            if (ret != UBSE_OK) {
                return ret;
            }
        }

        if (lender.numaId != UINT32_MAX) {
            NumaId numaId = lender.numaId;
            if (currentSocketId != UINT32_MAX &&
                nodeInfo.GetSocketIdByNumaId(lender.nodeId, numaId) != currentSocketId) {
                RecordError(std::string("lender numaId=") + std::to_string(numaId) +
                            " does not belong to socketId=" + std::to_string(currentSocketId));
                return UBSE_SCHEDULER_ERROR_INVAL;
            }
            outTargetNumaIds.insert(numaId);
            outNumaToSize[numaId] = lender.lender_size;
        }
    }
    return UBSE_OK;
}

UbseResult SpecifiedLenderFilter::FilterNodes(std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                              const SchedulerAccountManager& account, const SchedulerRequest& request)
{
    auto lenderInfosOpt = request.GetParamOpt<std::vector<adapter_plugins::mmi::UbseMemLenderInfo>>("lenderInfos");
    if (!lenderInfosOpt.has_value() || lenderInfosOpt.value().empty()) {
        return UBSE_OK;
    }
    const auto& lenderInfos = lenderInfosOpt.value();

    NodeId nodeId = lenderInfos[0].nodeId;
    SocketId socketId = UINT32_MAX;
    std::set<NumaId> targetNumaIds;
    std::unordered_map<NumaId, uint64_t> numaToSize;

    auto ret = ValidateLenderConsistency(lenderInfos, nodeInfo, socketId, targetNumaIds, numaToSize);
    if (ret != UBSE_OK) {
        nodes.clear();
        return ret;
    }

    FilterByNodeId(nodes, nodeId);
    FilterBySocketId(nodes, socketId, nodeId);
    FilterByNumaIds(nodes, targetNumaIds, nodeId);

    ret = CheckNumaCapacity(nodes, nodeInfo, numaToSize, request);
    if (ret != UBSE_OK) {
        return ret;
    }

    CleanupEmptyNodes(nodes);
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
