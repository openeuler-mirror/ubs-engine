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

#ifndef UBS_ENGINE_UBSE_MEM_SCHEDULER_DATA_H
#define UBS_ENGINE_UBSE_MEM_SCHEDULER_DATA_H

#include <map>
#include <memory>
#include <string>

#include "ubse_logger.h"
#include "ubse_math_util.h"
#include "ubse_mem_scheduler_socket_info.h"
#include "ubse_node_controller.h"
#include "../ubse_mem_types.h"

namespace ubse::mem::scheduler {

class SchedulerNodeInfo {
public:
    explicit SchedulerNodeInfo(const UbseNodeInfo& nodeInfo);
    [[nodiscard]] NodeInfo GetNodeInfo() const;

    [[nodiscard]] SchedulerSocketInfo* GetSocketInfo(SocketId socketId) const;
    [[nodiscard]] SchedulerNumaInfo* GetNumaInfo(SocketId socketId, NumaId numaId) const;
    [[nodiscard]] const std::map<SocketId, std::unique_ptr<SchedulerSocketInfo>>& GetAllSocketInfo() const
    {
        return socketInfoMap_;
    }

    void UpdateNodeClusterState(const UbseNodeClusterState clusterState)
    {
        clusterState_ = clusterState;
    }

    void UpdateHostName(const std::string& hostName)
    {
        hostName_ = hostName;
    }

    void UpdateIsLender(bool isLender)
    {
        isLender_ = isLender;
    }

    ~SchedulerNodeInfo() = default;

    [[nodiscard]] bool IsLender() const
    {
        return isLender_;
    }
    [[nodiscard]] UbseNodeClusterState GetClusterState() const
    {
        return clusterState_;
    }
    [[nodiscard]] UbseAllocator GetAllocator() const
    {
        return allocator_;
    }
    [[nodiscard]] uint32_t GetBlockSize() const
    {
        return blockSize_;
    }
    [[nodiscard]] uint64_t GetBorrowSize() const
    {
        uint64_t borrowSize = 0;
        for (const auto& [_, socketInfo] : socketInfoMap_) {
            for (const auto& [__, numaInfo] : socketInfo->GetAllNumaInfos()) {
                uint64_t result = 0;
                if (ubse::utils::SafeAdd(borrowSize, numaInfo->GetMemBorrowSize(), result)) {
                    borrowSize = result;
                }
            }
        }
        return borrowSize;
    }
    [[nodiscard]] uint64_t GetLentSize() const
    {
        uint64_t lentSize = 0;
        for (const auto& [_, socketInfo] : socketInfoMap_) {
            for (const auto& [__, numaInfo] : socketInfo->GetAllNumaInfos()) {
                uint64_t result = 0;
                if (ubse::utils::SafeAdd(lentSize, numaInfo->GetMemLentSize(), result)) {
                    lentSize = result;
                }
            }
        }
        return lentSize;
    }
    [[nodiscard]] const NodeId& GetNodeId() const
    {
        return nodeId_;
    }

private:
    NodeId nodeId_;
    std::string hostName_;
    UbseAllocator allocator_{UbseAllocator::BUDDY_HIGHMEM};
    uint32_t pmdMapping_{100};
    uint32_t blockSize_{128};
    bool isLender_{true};
    UbseNodeClusterState clusterState_{UbseNodeClusterState::UBSE_NODE_INIT};
    std::map<SocketId, std::unique_ptr<SchedulerSocketInfo>> socketInfoMap_;
};

} // namespace ubse::mem::scheduler

#endif //UBS_ENGINE_UBSE_MEM_SCHEDULER_DATA_H
