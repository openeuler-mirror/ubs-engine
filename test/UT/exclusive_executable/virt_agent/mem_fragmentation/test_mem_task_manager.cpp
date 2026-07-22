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

#include "test_mem_task_manager.h"
#include <mockcpp/mockcpp.hpp>
#include "mem_fragmentation_msg.h"
#include "mem_task_manager.h"

using namespace vm;
namespace ubse::ut::vm {
void TestMemAsyncTaskManager::SetUp()
{
    Test::SetUp();
}

void TestMemAsyncTaskManager::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestMemAsyncTaskManager, MemborrowTaskCreate)
{
    std::string taskId1 = ThreadTaskManager::GetInstance().AddTask("memborrow");
    EXPECT_NE(taskId1, "");
    auto ret = ThreadTaskManager::GetInstance().HasRunningTask();
    EXPECT_TRUE(ret);
    std::vector<AsyncTaskInfo> runningTasks = ThreadTaskManager::GetInstance().GetRunningTasks();
    std::vector<AsyncTaskInfo> allTasks = ThreadTaskManager::GetInstance().GetAllTasks();
    ThreadTaskManager::GetInstance().SetTaskThreadId(taskId1);

    ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId1, AsyncTaskStatus::FAILED, 1, "StringToC failed");

    ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId1, AsyncTaskStatus::SUCCESS, 0, "");
    mem_borrow_result_c borrowResult{};
    ThreadTaskManager::GetInstance().SetMemBorrowResult(taskId1, borrowResult);
    AsyncTaskStatus status = ThreadTaskManager::GetInstance().GetTaskStatus(taskId1);
    EXPECT_EQ(status, AsyncTaskStatus::SUCCESS);

    AsyncTaskInfo taskInfo{};
    auto ret2 = ThreadTaskManager::GetInstance().GetTaskInfo(taskId1, taskInfo);
    ThreadTaskManager::GetInstance().CleanupCompletedTasks();
    ThreadTaskManager::GetInstance().ClearAllTasks();
    EXPECT_EQ(ret2, VM_OK);
    EXPECT_EQ(taskInfo.taskId, taskId1);
}

TEST_F(TestMemAsyncTaskManager, MemReturnTaskCreate)
{
    std::string taskId1 = ThreadTaskManager::GetInstance().AddTask("memreturn");
    EXPECT_NE(taskId1, "");
    auto ret = ThreadTaskManager::GetInstance().HasRunningTask();
    EXPECT_TRUE(ret);
    std::vector<AsyncTaskInfo> runningTasks = ThreadTaskManager::GetInstance().GetRunningTasks();
    std::vector<AsyncTaskInfo> allTasks = ThreadTaskManager::GetInstance().GetAllTasks();
    ThreadTaskManager::GetInstance().SetTaskThreadId(taskId1);

    ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId1, AsyncTaskStatus::FAILED, 1, "StringToC failed");

    ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId1, AsyncTaskStatus::SUCCESS, 0, "");
    mem_borrow_result_c borrowResult{};
    ThreadTaskManager::GetInstance().SetMemBorrowResult(taskId1, borrowResult);
    AsyncTaskStatus status = ThreadTaskManager::GetInstance().GetTaskStatus(taskId1);
    EXPECT_EQ(status, AsyncTaskStatus::SUCCESS);

    AsyncTaskInfo taskInfo{};
    auto ret2 = ThreadTaskManager::GetInstance().GetTaskInfo(taskId1, taskInfo);
    ThreadTaskManager::GetInstance().CleanupCompletedTasks();
    ThreadTaskManager::GetInstance().ClearAllTasks();
    EXPECT_EQ(ret2, VM_OK);
    EXPECT_EQ(taskInfo.taskId, taskId1);
}

TEST_F(TestMemAsyncTaskManager, MemborrowTaskCreate2)
{
    std::string taskId1 = ThreadTaskManager::GetInstance().AddTask("memborrow");
    EXPECT_NE(taskId1, "");
    auto ret = ThreadTaskManager::GetInstance().HasRunningTask();
    EXPECT_TRUE(ret);
    std::vector<AsyncTaskInfo> runningTasks = ThreadTaskManager::GetInstance().GetRunningTasks();
    std::vector<AsyncTaskInfo> allTasks = ThreadTaskManager::GetInstance().GetAllTasks();
    ThreadTaskManager::GetInstance().SetTaskThreadId(taskId1);
    ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId1, AsyncTaskStatus::SUCCESS, 0, "");
    ThreadTaskManager::GetInstance().UpdateTaskStatus(taskId1, AsyncTaskStatus::FAILED, 1, "test failed");

    mem_borrow_result_c borrowResult{};
    ThreadTaskManager::GetInstance().SetMemBorrowResult(taskId1, borrowResult);
    AsyncTaskStatus status = ThreadTaskManager::GetInstance().GetTaskStatus(taskId1);
    EXPECT_EQ(status, AsyncTaskStatus::FAILED);

    AsyncTaskInfo taskInfo{};
    auto ret2 = ThreadTaskManager::GetInstance().GetTaskInfo(taskId1, taskInfo);
    ThreadTaskManager::GetInstance().CleanupCompletedTasks();
    ThreadTaskManager::GetInstance().ClearAllTasks();
    EXPECT_EQ(ret2, VM_OK);
    EXPECT_EQ(taskInfo.taskId, taskId1);
}

} // namespace ubse::ut::vm