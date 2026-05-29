/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#include "mem_task_manager.h"
#include <ubse_api_server.h>
#include <ubse_logger.h>
#include <ubse_node.h>
#include <map>
#include <string>
#include <vector>
#include "hugepage_handler.h"
#include "mem_fragmentation_msg.h"
#include "mem_handler.h"
#include "mempooling_def.h"
#include "mempooling_module.h"
#include "ubs_virt_agent_object_def.h"
#include "vm_sdk_def.h"
#include "vm_system_util.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace api::server;
using namespace vm::mempooling;
using namespace ubse::log;
using namespace ubse::nodeController;

ThreadTaskManager& ThreadTaskManager::GetInstance()
{
    static ThreadTaskManager instance;
    return instance;
}

std::string ThreadTaskManager::GenerateTaskId(const std::string& taskType)
{
    uint32_t count = ++taskCounter_;
    if (taskType == "memborrow") {
        return "frag_memborrow_" + std::to_string(count);
    } else {
        return "frag_memreturn_" + std::to_string(count);
    }
}

std::string ThreadTaskManager::AddTask(const std::string& taskType)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string taskId = GenerateTaskId(taskType);
    AsyncTaskInfo taskInfo;
    taskInfo.taskId = taskId;
    taskInfo.status = AsyncTaskStatus::RUNNING;
    taskInfo.resultCode = 0;
    taskInfo.errorMessage = "";
    taskInfo.startTime = std::chrono::system_clock::now();
    taskInfo.endTime = std::chrono::system_clock::now();
    taskInfo.threadId = std::thread::id();

    taskMap_[taskId] = taskInfo;
    UBSE_LOG_INFO << "Add new async task, taskId=" << taskId << ", type=" << taskType;

    return taskId;
}

void ThreadTaskManager::SetTaskThreadId(const std::string& taskId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskMap_.find(taskId);
    if (it != taskMap_.end()) {
        it->second.threadId = std::this_thread::get_id();
    }
}

void ThreadTaskManager::UpdateTaskStatus(const std::string& taskId, AsyncTaskStatus status, uint32_t resultCode,
                                         const std::string& errorMsg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskMap_.find(taskId);
    if (it != taskMap_.end()) {
        it->second.status = status;
        it->second.resultCode = resultCode;
        it->second.errorMessage = errorMsg;

        if (status == AsyncTaskStatus::RUNNING) {
            it->second.threadId = std::this_thread::get_id();
        }

        if (status == AsyncTaskStatus::SUCCESS || status == AsyncTaskStatus::FAILED) {
            it->second.endTime = std::chrono::system_clock::now();
        }

        UBSE_LOG_INFO << "INFO: Update task status, taskId=" << taskId << ", status=" << static_cast<int>(status)
                      << ", resultCode: " << resultCode;
    } else {
        UBSE_LOG_WARN << "WARN: Update task status failed, task not found=" << taskId;
    }
}

AsyncTaskStatus ThreadTaskManager::GetTaskStatus(const std::string& taskId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskMap_.find(taskId);
    if (it != taskMap_.end()) {
        return it->second.status;
    }
    return AsyncTaskStatus::NOT_EXIST;
}

uint32_t ThreadTaskManager::GetTaskInfo(const std::string& taskId, AsyncTaskInfo& asyncTaskInfo)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskMap_.find(taskId);
    if (it != taskMap_.end()) {
        asyncTaskInfo = it->second;
        return VM_OK;
    }
    return VM_ERROR;
}

bool ThreadTaskManager::HasRunningTask()
{
    for (const auto& pair : taskMap_) {
        if (pair.second.status == AsyncTaskStatus::RUNNING) {
            return true;
        }
    }
    return false;
}

std::vector<AsyncTaskInfo> ThreadTaskManager::GetRunningTasks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AsyncTaskInfo> runningTasks;

    for (const auto& pair : taskMap_) {
        if (pair.second.status == AsyncTaskStatus::RUNNING) {
            runningTasks.push_back(pair.second);
        }
    }
    return runningTasks;
}

std::vector<AsyncTaskInfo> ThreadTaskManager::GetAllTasks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AsyncTaskInfo> allTasks;

    for (const auto& pair : taskMap_) {
        allTasks.push_back(pair.second);
    }

    std::sort(allTasks.begin(), allTasks.end(),
              [](const AsyncTaskInfo& a, const AsyncTaskInfo& b) { return a.startTime > b.startTime; });

    return allTasks;
}

void ThreadTaskManager::CleanupCompletedTasks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    auto it = taskMap_.begin();

    size_t cleanedCount = 0;

    while (it != taskMap_.end()) {
        if (it->second.status == AsyncTaskStatus::SUCCESS || it->second.status == AsyncTaskStatus::FAILED) {
            UBSE_LOG_INFO << "INFO: Cleanup completed task, taskId=" << it->first;
            SafeDeleteArray(it->second.msgRawData);
            it = taskMap_.erase(it);
            cleanedCount++;
            continue;
        }
        ++it;
    }

    if (cleanedCount > 0) {
        UBSE_LOG_INFO << "INFO: Cleanup " << cleanedCount << " completed tasks.";
    }
}

void ThreadTaskManager::ClearAllTasks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskMap_.begin();
    while (it != taskMap_.end()) {
        UBSE_LOG_INFO << "INFO: Cleanup all task, taskId=" << it->first;
        SafeDeleteArray(it->second.msgRawData);
        it = taskMap_.erase(it);
    }
}

uint32_t ThreadTaskManager::SetMemBorrowResult(const std::string& taskId, const mem_borrow_result_c& result)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = taskMap_.find(taskId);
    if (it == taskMap_.end()) {
        UBSE_LOG_WARN << "Set mem borrow result failed, task not found, taskId=" << taskId;
        return VM_ERROR;
    }
    auto ret = PackMemBorrowResult(result, it->second);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "PackMemBorrowResult failed for task=" << taskId << ", " << FormatRetCode(ret);
        return ret;
    }
    return VM_OK;
}

uint32_t ThreadTaskManager::PackMemBorrowResult(const mem_borrow_result_c& memBorrowExecuteResult,
                                                AsyncTaskInfo& asyncTaskInfo)
{
    MemBorrowExecuteResultMsg msg{memBorrowExecuteResult};
    auto ret = msg.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "MemBorrowExecuteResultMsg Serialize fail, " << FormatRetCode(ret);
        return ret;
    }

    uint32_t dataSize = msg.SerializedDataSize();
    if (dataSize == 0) {
        UBSE_LOG_ERROR << "Serialized data size is 0.";
        return VM_ERROR;
    }
    SafeDeleteArray(asyncTaskInfo.msgRawData);
    uint8_t* rawData = new (std::nothrow) uint8_t[dataSize];
    if (rawData == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate memory for serialized data.";
        return VM_ERROR;
    }

    errno_t copy_result = memcpy_s(rawData, dataSize, msg.SerializedData(), dataSize);
    if (copy_result != 0) {
        UBSE_LOG_ERROR << "memcpy_s failed for serialized data=" << copy_result;
        delete[] rawData;
        return VM_ERROR;
    }

    asyncTaskInfo.msgRawData = rawData;
    asyncTaskInfo.msgRawDataSize = dataSize;

    UBSE_LOG_INFO << "MemBorrow result packed successfully, dataSize=" << dataSize;
    return VM_OK;
}
} // namespace vm
