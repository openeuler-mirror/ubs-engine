/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_agent_task_manager.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_agent_task_manager.h"

namespace ubse::mem::controller::ut {
using namespace ubse::mem::controller;

void TestUbseMemAgentTaskManager::SetUp()
{
    Test::SetUp();
}

void TestUbseMemAgentTaskManager::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemAgentTaskManager, AddFdExportTask)
{
    UbseMemFdBorrowExportObj obj{};
    obj.req.name = "test_fd_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::AddFdExportTask(taskId, obj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::AddFdExportTask(taskId, obj)); // Should fail as task already exists
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, DeleteFdExportTask)
{
    UbseMemFdBorrowExportObj obj{};
    obj.req.name = "test_fd_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddFdExportTask(taskId, obj);
    EXPECT_TRUE(UbseMemAgentTaskManager::DeleteFdExportTask(taskId));
    EXPECT_FALSE(UbseMemAgentTaskManager::DeleteFdExportTask(taskId)); // Should fail as task does not exist
}

TEST_F(TestUbseMemAgentTaskManager, GetFdExportTaskObj)
{
    UbseMemFdBorrowExportObj obj{};
    obj.req.name = "test_fd_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddFdExportTask(taskId, obj);
    UbseMemFdBorrowExportObj returnObj{};
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::GetFdExportTaskObj("test_fd_export", "", returnObj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::GetFdExportTaskObj("nonexistent", "", returnObj)); // Should fail
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, AddNumaExportTask)
{
    UbseMemNumaBorrowExportObj obj{};
    obj.req.name = "test_numa_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::AddNumaExportTask(taskId, obj));
    EXPECT_EQ(UBSE_ERROR,
              UbseMemAgentTaskManager::AddNumaExportTask(taskId, obj)); // Should fail as task already exists
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, DeleteNumaExportTask)
{
    UbseMemNumaBorrowExportObj obj{};
    obj.req.name = "test_numa_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddNumaExportTask(taskId, obj);
    EXPECT_TRUE(UbseMemAgentTaskManager::DeleteNumaExportTask(taskId));
    EXPECT_FALSE(UbseMemAgentTaskManager::DeleteNumaExportTask(taskId)); // Should fail as task does not exist
}

TEST_F(TestUbseMemAgentTaskManager, GetNumaExportTaskObj)
{
    UbseMemNumaBorrowExportObj obj{};
    obj.req.name = "test_numa_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddNumaExportTask(taskId, obj);
    UbseMemNumaBorrowExportObj returnObj{};
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::GetNumaExportTaskObj("test_numa_export", "", returnObj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::GetNumaExportTaskObj("nonexistent", "", returnObj)); // Should fail
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, AddShareExportTask)
{
    UbseMemShareBorrowExportObj obj{};
    obj.req.name = "test_share_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::AddShareExportTask(taskId, obj));
    EXPECT_EQ(UBSE_ERROR,
              UbseMemAgentTaskManager::AddShareExportTask(taskId, obj)); // Should fail as task already exists
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, DeleteShareExportTask)
{
    UbseMemShareBorrowExportObj obj{};
    obj.req.name = "test_share_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddShareExportTask(taskId, obj);
    EXPECT_TRUE(UbseMemAgentTaskManager::DeleteShareExportTask(taskId));
    EXPECT_FALSE(UbseMemAgentTaskManager::DeleteShareExportTask(taskId)); // Should fail as task does not exist
}

TEST_F(TestUbseMemAgentTaskManager, GetShareExportTaskObj)
{
    UbseMemShareBorrowExportObj obj{};
    obj.req.name = "test_share_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddShareExportTask(taskId, obj);
    UbseMemShareBorrowExportObj returnObj{};
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::GetShareExportTaskObj("test_share_export", returnObj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::GetShareExportTaskObj("nonexistent", returnObj)); // Should fail
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, AddAddrExportTask)
{
    UbseMemAddrBorrowExportObj obj{};
    obj.req.name = "test_addr_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::AddAddrExportTask(taskId, obj));
    EXPECT_EQ(UBSE_ERROR,
              UbseMemAgentTaskManager::AddAddrExportTask(taskId, obj)); // Should fail as task already exists
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, DeleteAddrExportTask)
{
    UbseMemAddrBorrowExportObj obj{};
    obj.req.name = "test_addr_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddAddrExportTask(taskId, obj);
    EXPECT_TRUE(UbseMemAgentTaskManager::DeleteAddrExportTask(taskId));
    EXPECT_FALSE(UbseMemAgentTaskManager::DeleteAddrExportTask(taskId)); // Should fail as task does not exist
}

TEST_F(TestUbseMemAgentTaskManager, GetAddrExportTaskObj)
{
    UbseMemAddrBorrowExportObj obj{};
    obj.req.name = "test_addr_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddAddrExportTask(taskId, obj);
    UbseMemAddrBorrowExportObj returnObj{};
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::GetAddrExportTaskObj("test_addr_export", "", returnObj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::GetAddrExportTaskObj("nonexistent", "", returnObj)); // Should fail
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, AddFdImportTask)
{
    UbseMemFdBorrowImportObj obj{};
    obj.req.name = "test_fd_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::AddFdImportTask(taskId, obj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::AddFdImportTask(taskId, obj)); // Should fail as task already exists
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, DeleteFdImportTask)
{
    UbseMemFdBorrowImportObj obj{};
    obj.req.name = "test_fd_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddFdImportTask(taskId, obj);
    EXPECT_TRUE(UbseMemAgentTaskManager::DeleteFdImportTask(taskId));
    EXPECT_FALSE(UbseMemAgentTaskManager::DeleteFdImportTask(taskId)); // Should fail as task does not exist
}

TEST_F(TestUbseMemAgentTaskManager, GetFdImportTaskObj)
{
    UbseMemFdBorrowImportObj obj{};
    obj.req.name = "test_fd_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddFdImportTask(taskId, obj);
    UbseMemFdBorrowImportObj returnObj{};
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::GetFdImportTaskObj("test_fd_import", returnObj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::GetFdImportTaskObj("nonexistent", returnObj)); // Should fail
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, AddNumaImportTask)
{
    UbseMemNumaBorrowImportObj obj{};
    obj.req.name = "test_numa_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::AddNumaImportTask(taskId, obj));
    EXPECT_EQ(UBSE_ERROR,
              UbseMemAgentTaskManager::AddNumaImportTask(taskId, obj)); // Should fail as task already exists
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, DeleteNumaImportTask)
{
    UbseMemNumaBorrowImportObj obj{};
    obj.req.name = "test_numa_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddNumaImportTask(taskId, obj);
    EXPECT_TRUE(UbseMemAgentTaskManager::DeleteNumaImportTask(taskId));
    EXPECT_FALSE(UbseMemAgentTaskManager::DeleteNumaImportTask(taskId)); // Should fail as task does not exist
}

TEST_F(TestUbseMemAgentTaskManager, GetNumaImportTaskObj)
{
    UbseMemNumaBorrowImportObj obj{};
    obj.req.name = "test_numa_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddNumaImportTask(taskId, obj);
    UbseMemNumaBorrowImportObj returnObj{};
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::GetNumaImportTaskObj("test_numa_import", returnObj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::GetNumaImportTaskObj("nonexistent", returnObj)); // Should fail
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, AddShareImportTask)
{
    UbseMemShareBorrowImportObj obj{};
    obj.req.name = "test_share_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::AddShareImportTask(taskId, obj));
    EXPECT_EQ(UBSE_ERROR,
              UbseMemAgentTaskManager::AddShareImportTask(taskId, obj)); // Should fail as task already exists
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, DeleteShareImportTask)
{
    UbseMemShareBorrowImportObj obj{};
    obj.req.name = "test_share_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddShareImportTask(taskId, obj);
    EXPECT_TRUE(UbseMemAgentTaskManager::DeleteShareImportTask(taskId));
    EXPECT_FALSE(UbseMemAgentTaskManager::DeleteShareImportTask(taskId)); // Should fail as task does not exist
}

TEST_F(TestUbseMemAgentTaskManager, GetShareImportTaskObj)
{
    UbseMemShareBorrowImportObj obj{};
    obj.req.name = "test_share_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddShareImportTask(taskId, obj);
    UbseMemShareBorrowImportObj returnObj{};
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::GetShareImportTaskObj("test_share_import", returnObj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::GetShareImportTaskObj("nonexistent", returnObj)); // Should fail
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, AddAddrImportTask)
{
    UbseMemAddrBorrowImportObj obj{};
    obj.req.name = "test_addr_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::AddAddrImportTask(taskId, obj));
    EXPECT_EQ(UBSE_ERROR,
              UbseMemAgentTaskManager::AddAddrImportTask(taskId, obj)); // Should fail as task already exists
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, DeleteAddrImportTask)
{
    UbseMemAddrBorrowImportObj obj{};
    obj.req.name = "test_addr_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddAddrImportTask(taskId, obj);
    EXPECT_TRUE(UbseMemAgentTaskManager::DeleteAddrImportTask(taskId));
    EXPECT_FALSE(UbseMemAgentTaskManager::DeleteAddrImportTask(taskId)); // Should fail as task does not exist
}

TEST_F(TestUbseMemAgentTaskManager, GetAddrImportTaskObj)
{
    UbseMemAddrBorrowImportObj obj{};
    obj.req.name = "test_addr_import";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddAddrImportTask(taskId, obj);
    UbseMemAddrBorrowImportObj returnObj{};
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::GetAddrImportTaskObj("test_addr_import", returnObj));
    EXPECT_EQ(UBSE_ERROR, UbseMemAgentTaskManager::GetAddrImportTaskObj("nonexistent", returnObj)); // Should fail
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, DeleteTaskObj)
{
    UbseMemFdBorrowExportObj obj{};
    obj.req.name = "test_fd_export";
    uint32_t taskId = UbseMemAgentTaskManager::GetTaskId();
    UbseMemAgentTaskManager::AddFdExportTask(taskId, obj);
    EXPECT_EQ(UBSE_OK, UbseMemAgentTaskManager::DeleteTaskObj(taskId));
}

TEST_F(TestUbseMemAgentTaskManager, GetTaskId)
{
    EXPECT_EQ(0, UbseMemAgentTaskManager::GetTaskId());
    uint32_t taskId = UbseMemAgentTaskManager::GenerateTaskId();
    EXPECT_EQ(taskId + 1, UbseMemAgentTaskManager::GetTaskId());
}

} // namespace ubse::mem::controller::ut