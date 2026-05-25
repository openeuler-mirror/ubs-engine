/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef HAM_MIGRATE_VM_INFO_H
#define HAM_MIGRATE_VM_INFO_H

#include <chrono>
#include <sstream>
#include <string>

namespace vm {
using namespace std::chrono;
enum class VmState
{
    NOPE = 0,               // No state
    BORROWED_MIGRATED = 1,  // Memory was borrowed and migration succeeded
    BORROWED_NOMIGRATE = 2, // Memory was borrowed and migration failed or did not occur
    NOBORROW_MIGRATED = 3,  // No memory borrowed and migration succeeded
    NOBORROW_NOMIGRATE = 4, // No memory borrowed and migration failed or did not occur
};

enum class VmOpState
{
    NOPE = 0,                    // No state
    DISABLE_PROCESS_MIGRATE = 1, // Disable hot/cold page migration
    PROCESS_TRACKING = 2,        // Perform hot/cold page scanning
    BORROWED_ADDRESS = 3,        // Borrow memory addresses
};

enum class OpState
{
    NOPE = 0,  // No state
    START = 1, // Operation started
    END = 2,   // Operation finished
};

enum class NodeState
{
    NOPE = 0,  // No state
    PANIC = 1, // Panic occurred
};

struct HamMigrateVmInfo {
    HamMigrateVmInfo() = default;

    std::string nodeId{};
    int socketId{};
    int numaId{};
    int pid{};
    std::string dstNodeId{};
    std::string uuid{};
    std::string borrowName{};
    VmState vmState{VmState::NOPE};
    VmOpState vmOpState{VmOpState::NOPE};
    OpState opState{OpState::NOPE};
    NodeState dstNodeState{NodeState::NOPE};

    time_point<system_clock> timeout;
    int count = 1;

    bool operator<(const HamMigrateVmInfo& hamMigrateVmInfo) const
    {
        return timeout > hamMigrateVmInfo.timeout;
    }

    bool operator>(const HamMigrateVmInfo& hamMigrateVmInfo) const
    {
        return timeout < hamMigrateVmInfo.timeout;
    }

    bool operator==(const HamMigrateVmInfo& hamMigrateVmInfo) const
    {
        return nodeId == hamMigrateVmInfo.nodeId && pid == hamMigrateVmInfo.pid;
    }

    static std::string VmStateToString(const VmState& vmState)
    {
        std::ostringstream oss;
        if (vmState == VmState::NOPE) {
            oss << "\"NOPE\"";
        } else if (vmState == VmState::BORROWED_MIGRATED) {
            oss << "\"BORROWED_MIGRATED\"";
        } else if (vmState == VmState::BORROWED_NOMIGRATE) {
            oss << "\"BORROWED_NOMIGRATE\"";
        } else if (vmState == VmState::NOBORROW_MIGRATED) {
            oss << "\"NOBORRO_MIGRATED\"";
        } else if (vmState == VmState::NOBORROW_NOMIGRATE) {
            oss << "\"NOBORROW_NOMIGRATE\"";
        }
        return oss.str();
    }

    static std::string VmOpStateToString(const VmOpState& vmOpState)
    {
        std::ostringstream oss;
        if (vmOpState == VmOpState::NOPE) {
            oss << "\"NOPE\"";
        } else if (vmOpState == VmOpState::DISABLE_PROCESS_MIGRATE) {
            oss << "\"DISABLE_PROCESS_MIGRATE\"";
        } else if (vmOpState == VmOpState::PROCESS_TRACKING) {
            oss << "\"PROCESS_TRACKING\"";
        } else if (vmOpState == VmOpState::BORROWED_ADDRESS) {
            oss << "\"BORROWED_ADDRESS\"";
        }
        return oss.str();
    }

    static std::string OpStateToString(const OpState& opState)
    {
        std::ostringstream oss;
        if (opState == OpState::NOPE) {
            oss << "\"NOPE\"";
        } else if (opState == OpState::START) {
            oss << "\"START\"";
        } else if (opState == OpState::END) {
            oss << "\"END\"";
        }
        return oss.str();
    }

    static std::string NodeStateToString(const NodeState& nodeState)
    {
        std::ostringstream oss;
        if (nodeState == NodeState::NOPE) {
            oss << "\"NOPE\"";
        } else if (nodeState == NodeState::PANIC) {
            oss << "\"PANIC\"";
        }
        return oss.str();
    }

    std::string GetNumaLoc() const
    {
        return nodeId + "/" + std::to_string(socketId) + "/" + std::to_string(numaId);
    }

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "nodeId: " << nodeId << ", ";
        oss << "socketId: " << socketId << ", ";
        oss << "numaId: " << numaId << ", ";
        oss << "pid: " << pid << ", ";
        oss << "dstNodeId: " << dstNodeId << ", ";
        oss << "uuid: " << uuid << ", ";
        oss << "borrowName: " << borrowName << ", ";
        oss << "vmState: " << VmStateToString(vmState) << ", ";
        oss << "vmOpState: " << VmOpStateToString(vmOpState) << ", ";
        oss << "opState: " << OpStateToString(opState) << ", ";
        oss << "dstNodeState: " << NodeStateToString(dstNodeState) << ", ";
        oss << "timeout: " << timeout.time_since_epoch().count();
        return oss.str();
    }
};
} // namespace vm

#endif // HAM_MIGRATE_VM_INFO_H
