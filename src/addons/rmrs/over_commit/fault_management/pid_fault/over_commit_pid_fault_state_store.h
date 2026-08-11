/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MEMPOOLING_OVER_COMMIT_PID_FAULT_STATE_STORE_H
#define MEMPOOLING_OVER_COMMIT_PID_FAULT_STATE_STORE_H

#include <mutex>
#include <string>
#include <unordered_map>
#include "mp_error.h"
#include "over_commit_pid_fault_context.h"

namespace mempooling {

/**
 * @brief PID粒度故障处理状态持久化管理器
 *
 * 职责:
 * - 管理故障处理过程中的task状态持久化（BORROWED/MIGRATED/COMPLETED）
 * - 支持断点恢复：ubse重启后从存储中加载未完成状态
 * - 保证不扩大故障：已借用的NUMA不会因故障处理失败而泄漏
 *
 * 存储key: "mempooling" / "_oc_fault_pid_state"
 */
class PidFaultStateStore {
public:
    static PidFaultStateStore& Instance()
    {
        static PidFaultStateStore instance;
        return instance;
    }

    /**
     * @brief 初始化：从UBSE存储加载历史状态到内存
     * 在DataReloadInit中调用，仅加载不自动执行
     */
    MpResult Init();

    /**
     * @brief 查询指定故障节点的历史处理状态
     * @param faultNodeId 故障借出节点ID
     * @param state 输出: 历史状态（若不存在则faultNodeId为空）
     */
    MpResult Query(const std::string& faultNodeId, FaultProcessState& state);

    /**
     * @brief 保存/更新task状态（即时持久化）
     * @param faultNodeId 故障节点ID
     * @param taskState 待持久化的task状态
     */
    MpResult SaveTaskState(const std::string& faultNodeId, const TaskPersistState& taskState);

    /**
     * @brief 更新task阶段（BORROWED→MIGRATED→COMPLETED）
     */
    MpResult UpdateTaskPhase(const std::string& faultNodeId, const std::string& taskId, TaskPhase newPhase);

    /**
     * @brief 记录已归还的borrowId
     */
    MpResult AddFreedBorrowId(const std::string& faultNodeId, const std::string& taskId,
                              const std::string& freedBorrowId);

    /**
     * @brief 删除已完成的task状态
     */
    MpResult RemoveCompletedTask(const std::string& faultNodeId, const std::string& taskId);

    /**
     * @brief 清除指定故障节点的所有状态（全部完成后调用）
     */
    MpResult ClearFaultState(const std::string& faultNodeId);

    /**
     * @brief 检查指定task是否有历史状态（用于RESUME判断）
     */
    bool HasTaskState(const std::string& faultNodeId, const std::string& taskId);

    /**
     * @brief 获取指定task的历史状态
     */
    MpResult GetTaskState(const std::string& faultNodeId, const std::string& taskId, TaskPersistState& taskState);

private:
    PidFaultStateStore() = default;
    PidFaultStateStore(const PidFaultStateStore&) = delete;
    PidFaultStateStore& operator=(const PidFaultStateStore&) = delete;
    ~PidFaultStateStore() = default;

    MpResult PersistToStorage();
    MpResult LoadFromStorage();

    std::mutex mtx_;
    // faultNodeId → FaultProcessState
    std::unordered_map<std::string, FaultProcessState> stateMap_;
    bool initialized_ = false;
};

} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_PID_FAULT_STATE_STORE_H
