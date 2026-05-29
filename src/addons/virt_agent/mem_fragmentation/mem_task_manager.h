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

#ifndef MEM_ASYNC_TASK_MANAGER_H
#define MEM_ASYNC_TASK_MANAGER_H
#include <ubse_logger.h>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <ubse_api_server_def.h>
#include <vm_error.h>

#include "mem_fragmentation_msg.h"
#include "mempooling_def.h"

namespace vm {
enum class AsyncTaskStatus
{
    NOT_EXIST,
    RUNNING,
    SUCCESS,
    FAILED
};

struct AsyncTaskInfo {
    std::string taskId;                              // Task ID, e.g., "frag_memreturn_1", "frag_memborrow_2"
    AsyncTaskStatus status;                          // Current task status (e.g., PENDING, RUNNING, SUCCESS, FAILED)
    uint32_t resultCode;                             // Execution result code (0 for success, non-zero for error)
    std::string errorMessage;                        // Error message if task failed
    std::chrono::system_clock::time_point startTime; // Start time of the task
    std::chrono::system_clock::time_point endTime;   // End time of the task
    std::thread::id threadId;                        // Thread ID where the task is running

    uint8_t* msgRawData = nullptr; // Thread ID where the task is running
    uint32_t msgRawDataSize = 0;   // Size of the raw data buffer in bytes
};

class ThreadTaskManager {
public:
    // Singleton pattern access
    static ThreadTaskManager& GetInstance();

    // Disable copy and assignment
    ThreadTaskManager(const ThreadTaskManager&) = delete;
    ThreadTaskManager& operator=(const ThreadTaskManager&) = delete;

    // Add new task
    std::string AddTask(const std::string& taskType = "memreturn");
    void SetTaskThreadId(const std::string& taskId);

    // Update task status
    void UpdateTaskStatus(const std::string& taskId, AsyncTaskStatus status, uint32_t resultCode = 0,
                          const std::string& errorMsg = "");

    // Get task status
    AsyncTaskStatus GetTaskStatus(const std::string& taskId);

    // Get task details
    uint32_t GetTaskInfo(const std::string& taskId, AsyncTaskInfo& asyncTaskInfo);

    // Get all running tasks
    std::vector<AsyncTaskInfo> GetRunningTasks();

    // Get all tasks
    std::vector<AsyncTaskInfo> GetAllTasks();

    // Clean up completed tasks
    void CleanupCompletedTasks();

    // Force clean up all tasks
    void ClearAllTasks();

    uint32_t SetMemBorrowResult(const std::string& taskId, const mem_borrow_result_c& result);
    uint32_t PackMemBorrowResult(const mem_borrow_result_c& memBorrowExecuteResult, AsyncTaskInfo& asyncTaskInfo);

private:
    ThreadTaskManager() = default;
    ~ThreadTaskManager() = default;

    // Generate a unique task ID
    std::string GenerateTaskId(const std::string& taskType);

    // Check if there are any running tasks for nodeId.
    bool HasRunningTask();

private:
    std::mutex mutex_;
    std::unordered_map<std::string, AsyncTaskInfo> taskMap_;
    std::atomic<uint32_t> taskCounter_{0};
};
} // namespace vm

#endif // MEM_ASYNC_TASK_MANAGER_H
