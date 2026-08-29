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

#include "over_commit_pid_fault_state_store.h"
#include <algorithm>
#include <cstring>
#include "ubse_logger.h"
#include "ubse_storage.h"
#include "mp_configuration.h"

namespace mempooling {

using namespace ubse::storage;

static const std::string TAG = "[OverCommit][PidFault][StateStore] ";
static const std::string KEY_PREFIX = "mempooling";
static const std::string KEY_NAME = "_oc_fault_pid_state";
// 持久化数据中故障节点数量的防御上限: 存储数据可能因写入时崩溃而损坏，
// 损坏的超大count会导致反序列化循环空转挂死，超上限直接判损坏
static constexpr size_t MAX_FAULT_NODE_STATES = 1024;

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << TAG

static void LoadStateCallback(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                              void* ctx)
{
    if (ctx == nullptr || buff.data == nullptr || buff.len == 0) {
        return;
    }
    UbseByteBuffer& value = *(static_cast<UbseByteBuffer*>(ctx));
    value.len = buff.len;
    value.data = new (std::nothrow) uint8_t[value.len];
    if (value.data == nullptr) {
        value.len = 0;
        return;
    }
    if (memcpy_s(value.data, value.len, buff.data, buff.len) != 0) {
        delete[] value.data;
        value.data = nullptr;
        value.len = 0;
    }
}

MpResult PidFaultStateStore::Init()
{
    std::lock_guard<std::mutex> lock(mtx_);
    MpResult ret = LoadFromStorage();
    if (ret != MEM_POOLING_OK) {
        LOG_WARN << "No historical state found or load failed, starting fresh.";
        // 无历史状态不算错误，首次启动时存储为空
    }
    initialized_ = true;
    LOG_INFO << "PidFaultStateStore initialized, stateMap size=" << stateMap_.size() << ".";
    return MEM_POOLING_OK;
}

MpResult PidFaultStateStore::LoadFromStorage()
{
    UbseByteBuffer buffer{};
    MpResult ret = UbseStorageQueryData(KEY_PREFIX, KEY_NAME, &buffer, LoadStateCallback);
    if (ret != MEM_POOLING_OK) {
        // 存储层对"无数据"与"I/O错误"返回同样的非OK码无法区分，按首次启动从头开始处理;
        // 记WARN便于排障时区分静默启动与存储异常
        LOG_WARN << "LoadFromStorage query failed, ret=" << ret << ", starting fresh.";
        if (buffer.data != nullptr) {
            delete[] buffer.data;
        }
        return MEM_POOLING_OK; // 无数据时返回OK
    }
    if (buffer.len == 0 || buffer.data == nullptr) {
        return MEM_POOLING_OK; // 空数据
    }

    // 反序列化: 先读state数量，再逐个读取
    rmrs::serialize::RmrsInStream in(buffer.data, buffer.len);
    size_t stateCount = 0;
    in >> stateCount;
    // stateCount来自持久化数据（可能因写入时进程崩溃而损坏）: 超上限直接判损坏，
    // 防止超大count导致循环空转挂死；反序列化失败同样判损坏，丢弃全部状态从头开始
    if (!in.IsValid() || stateCount > MAX_FAULT_NODE_STATES) {
        LOG_ERROR << "LoadFromStorage corrupted, stateCount=" << stateCount << ", drop all states.";
        delete[] buffer.data;
        return MEM_POOLING_ERROR;
    }
    // 先加载到临时map: 反序列化中途失败时不残留部分状态，整体判损坏从头开始
    std::unordered_map<std::string, FaultProcessState> loadedMap;
    for (size_t i = 0; i < stateCount; ++i) {
        FaultProcessState state;
        if (FaultProcessStateDeserialization(in, state) != MEM_POOLING_OK) {
            LOG_ERROR << "LoadFromStorage deserialization failed at index=" << i << ", drop all states.";
            delete[] buffer.data;
            return MEM_POOLING_ERROR;
        }
        if (!state.faultNodeId.empty()) {
            loadedMap[state.faultNodeId] = state;
        }
    }
    stateMap_.insert(loadedMap.begin(), loadedMap.end());

    delete[] buffer.data;
    LOG_INFO << "Loaded " << stateMap_.size() << " fault states from storage.";
    return MEM_POOLING_OK;
}

MpResult PidFaultStateStore::PersistToStorage()
{
    rmrs::serialize::RmrsOutStream out;
    size_t stateCount = stateMap_.size();
    out << stateCount;
    for (const auto& [faultNodeId, state] : stateMap_) {
        FaultProcessStateSerialization(out, state);
    }

    UbseByteBuffer buffer{};
    buffer.data = out.GetBufferPointer();
    buffer.len = out.GetSize();
    buffer.freeFunc = nullptr;

    MpResult ret = UbseStoragePutData(KEY_PREFIX, KEY_NAME, &buffer);
    delete[] buffer.data;
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "UbseStoragePutData failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult PidFaultStateStore::Query(const std::string& faultNodeId, FaultProcessState& state)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = stateMap_.find(faultNodeId);
    if (it == stateMap_.end()) {
        // 无历史状态返回空对象，调用方据此走全新流程（非RESUME）
        LOG_DEBUG << "Query: no state for faultNodeId=" << faultNodeId << ", fresh start.";
        state = FaultProcessState{};
        return MEM_POOLING_OK;
    }
    state = it->second;
    LOG_DEBUG << "Query: found state for faultNodeId=" << faultNodeId << ", taskStates=" << state.taskStates.size()
              << ".";
    return MEM_POOLING_OK;
}

MpResult PidFaultStateStore::SaveTaskState(const std::string& faultNodeId, const TaskPersistState& taskState)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto& state = stateMap_[faultNodeId];
    if (state.faultNodeId.empty()) {
        state.faultNodeId = faultNodeId;
        state.processStartTime = static_cast<uint64_t>(time(nullptr));
    }

    // 查找已有task或新增（同taskId重复保存视为覆盖更新，保证幂等）
    auto it = std::find_if(state.taskStates.begin(), state.taskStates.end(),
                           [&taskState](const TaskPersistState& t) { return t.taskId == taskState.taskId; });
    if (it != state.taskStates.end()) {
        LOG_DEBUG << "SaveTaskState update: faultNode=" << faultNodeId << ", taskId=" << taskState.taskId
                  << ", phase=" << static_cast<uint32_t>(taskState.phase) << ".";
        *it = taskState;
    } else {
        LOG_DEBUG << "SaveTaskState insert: faultNode=" << faultNodeId << ", taskId=" << taskState.taskId
                  << ", phase=" << static_cast<uint32_t>(taskState.phase) << ", total=" << (state.taskStates.size() + 1)
                  << ".";
        state.taskStates.push_back(taskState);
    }

    return PersistToStorage();
}

MpResult PidFaultStateStore::UpdateTaskPhase(const std::string& faultNodeId, const std::string& taskId,
                                             TaskPhase newPhase)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto stateIt = stateMap_.find(faultNodeId);
    if (stateIt == stateMap_.end()) {
        LOG_ERROR << "FaultNodeId not found: " << faultNodeId << ".";
        return MEM_POOLING_ERROR;
    }

    auto& taskStates = stateIt->second.taskStates;
    auto taskIt = std::find_if(taskStates.begin(), taskStates.end(),
                               [&taskId](const TaskPersistState& t) { return t.taskId == taskId; });
    if (taskIt == taskStates.end()) {
        LOG_ERROR << "TaskId not found: " << taskId << ".";
        return MEM_POOLING_ERROR;
    }

    taskIt->phase = newPhase;
    LOG_DEBUG << "UpdateTaskPhase: faultNode=" << faultNodeId << ", taskId=" << taskId
              << ", newPhase=" << static_cast<uint32_t>(newPhase) << ".";
    return PersistToStorage();
}

MpResult PidFaultStateStore::AddFreedBorrowId(const std::string& faultNodeId, const std::string& taskId,
                                              const std::string& freedBorrowId)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto stateIt = stateMap_.find(faultNodeId);
    if (stateIt == stateMap_.end()) {
        return MEM_POOLING_ERROR;
    }

    auto& taskStates = stateIt->second.taskStates;
    auto taskIt = std::find_if(taskStates.begin(), taskStates.end(),
                               [&taskId](const TaskPersistState& t) { return t.taskId == taskId; });
    if (taskIt == taskStates.end()) {
        return MEM_POOLING_ERROR;
    }

    taskIt->freedOldBorrowIds.insert(freedBorrowId);
    LOG_DEBUG << "AddFreedBorrowId: faultNode=" << faultNodeId << ", taskId=" << taskId
              << ", freedBorrowId=" << freedBorrowId << ", freedTotal=" << taskIt->freedOldBorrowIds.size() << ".";
    return PersistToStorage();
}

MpResult PidFaultStateStore::RemoveCompletedTask(const std::string& faultNodeId, const std::string& taskId)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto stateIt = stateMap_.find(faultNodeId);
    if (stateIt == stateMap_.end()) {
        return MEM_POOLING_OK;
    }

    auto& taskStates = stateIt->second.taskStates;
    taskStates.erase(std::remove_if(taskStates.begin(), taskStates.end(),
                                    [&taskId](const TaskPersistState& t) { return t.taskId == taskId; }),
                     taskStates.end());
    LOG_DEBUG << "RemoveCompletedTask: faultNode=" << faultNodeId << ", taskId=" << taskId
              << ", remaining=" << taskStates.size() << ".";

    // 如果所有task都完成了，清除整个fault状态（本次故障处理完整结束）
    if (taskStates.empty()) {
        LOG_INFO << "All tasks completed for faultNode=" << faultNodeId << ", clear fault state.";
        stateMap_.erase(stateIt);
    }

    return PersistToStorage();
}

MpResult PidFaultStateStore::ClearFaultState(const std::string& faultNodeId)
{
    std::lock_guard<std::mutex> lock(mtx_);
    LOG_DEBUG << "ClearFaultState: faultNode=" << faultNodeId << ".";
    stateMap_.erase(faultNodeId);
    return PersistToStorage();
}

bool PidFaultStateStore::HasTaskState(const std::string& faultNodeId, const std::string& taskId)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto stateIt = stateMap_.find(faultNodeId);
    if (stateIt == stateMap_.end()) {
        return false;
    }
    return std::any_of(stateIt->second.taskStates.begin(), stateIt->second.taskStates.end(),
                       [&taskId](const TaskPersistState& t) { return t.taskId == taskId; });
}

MpResult PidFaultStateStore::GetTaskState(const std::string& faultNodeId, const std::string& taskId,
                                          TaskPersistState& taskState)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto stateIt = stateMap_.find(faultNodeId);
    if (stateIt == stateMap_.end()) {
        return MEM_POOLING_ERROR;
    }

    auto taskIt = std::find_if(stateIt->second.taskStates.begin(), stateIt->second.taskStates.end(),
                               [&taskId](const TaskPersistState& t) { return t.taskId == taskId; });
    if (taskIt == stateIt->second.taskStates.end()) {
        return MEM_POOLING_ERROR;
    }

    taskState = *taskIt;
    return MEM_POOLING_OK;
}

} // namespace mempooling
