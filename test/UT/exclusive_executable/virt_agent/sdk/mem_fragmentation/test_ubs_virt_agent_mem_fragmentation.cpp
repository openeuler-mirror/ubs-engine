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

#include "test_ubs_virt_agent_mem_fragmentation.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include <securec.h>
#include "ubse_ipc_client.h"
#include "ubs_virt_agent_mem_fragmentation.h"
#include "ubs_virt_agent_mem_fragmentation_helper.h"
#include "vm_serial_util.h"

using namespace vm;
namespace ubse::vm::ut {

constexpr uint32_t TEST_VIRT_MAX_NODE_ID_LENGTH = 48;

TestLibvirtAgentMemFragmentation::TestLibvirtAgentMemFragmentation() = default;

void TestLibvirtAgentMemFragmentation::SetUp()
{
    Test::SetUp();
}

void TestLibvirtAgentMemFragmentation::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_node_info_InvalidParameters)
{
    numa_info_t* node_list = nullptr;
    uint32_t node_cnt = 0;

    auto ret = ubs_virt_agent_mem_fragmentation_node_info(nullptr, &node_cnt);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);

    ret = ubs_virt_agent_mem_fragmentation_node_info(&node_list, nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_node_info_InvokeCallFailed)
{
    numa_info_t* node_list{};
    uint32_t node_cnt = 0;

    virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_node_info(&node_list, &node_cnt);
    EXPECT_EQ(ret, VA_ERROR_BASE);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_node_info_unpack_fail)
{
    numa_info_t* node_list{};
    uint32_t node_cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_node_info(&node_list, &node_cnt);
    EXPECT_EQ(ret, VA_ERROR_DESERIALIZE_FAILED);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_node_info)
{
    numa_info_t* node_list{};
    uint32_t node_cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    MOCKER(ubse_node_info_unpack).stubs().will(returnValue(0));
    virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_node_info(&node_list, &node_cnt);
    EXPECT_EQ(ret, VA_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_vm_info_InvalidParameters)
{
    vm_domain_info_t* node_list = nullptr;
    uint32_t node_cnt = 0;

    auto ret = ubs_virt_agent_mem_fragmentation_vm_info(nullptr, &node_cnt);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);

    ret = ubs_virt_agent_mem_fragmentation_vm_info(&node_list, nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_vm_info_fail)
{
    vm_domain_info_t* node_list{};
    uint32_t node_cnt = 0;
    virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_vm_info(&node_list, &node_cnt);
    EXPECT_EQ(ret, VA_ERROR_BASE);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_vm_info_unpack_fail)
{
    vm_domain_info_t* node_list{};
    uint32_t node_cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_vm_info(&node_list, &node_cnt);
    EXPECT_EQ(ret, VA_ERROR_DESERIALIZE_FAILED);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_vm_info)
{
    vm_domain_info_t* node_list{};
    uint32_t node_cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    MOCKER(ubse_vm_info_unpack).stubs().will(returnValue(0));
    virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_vm_info(&node_list, &node_cnt);
    EXPECT_EQ(ret, VA_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_node_anti_affinity_InvalidParameters)
{
    auto ret = ubs_virt_agent_mem_fragmentation_node_anti_affinity(nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_node_anti_affinity_fail)
{
    NodeAntiDictionary dict = {.entries = {{.key = "1", .value = {"2"}, .value_count = 1}}, .entry_count = 1};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(1));
    virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_node_anti_affinity(&dict);
    EXPECT_EQ(ret, VA_ERROR_BASE);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_fragmentation_node_anti_affinity_AllocateMemoryFailed)
{
    NodeAntiDictionary dict = {.entries = {{.key = "1", .value = {"2"}, .value_count = 1}}, .entry_count = 1};

    MOCKER(allocate_memory).stubs().will(returnValue(static_cast<uint8_t*>(nullptr)));
    virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_node_anti_affinity(&dict);
    EXPECT_EQ(ret, VA_ERROR_MEM_ALLOCATE_FAILED);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_borrow_strategy_InvalidParameters)
{
    virt_agent_ret_t ret = ubs_virt_agent_mem_borrow_strategy(nullptr, nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_borrow_strategy_fail)
{
    src_memory_borrow_param src_param{"0", 0, 0, 100};
    borrow_strategy_c borrow_strategy{"0", 0, 0, 100, {"0", 0, 0, {0}, {100}}, 100};
    virt_agent_ret_t ret = ubs_virt_agent_mem_borrow_strategy(&src_param, &borrow_strategy);
    EXPECT_EQ(ret, VA_ERROR_BASE);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_borrow_strategy_unpack_fail)
{
    src_memory_borrow_param src_param{"0", 0, 0, 100};
    borrow_strategy_c borrow_strategy{"0", 0, 0, 100, {"0", 0, 0, {0}, {100}}, 100};

    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    MOCKER(ubse_mem_borrow_strategy_msg_unpack).stubs().will(returnValue(1));
    virt_agent_ret_t ret = ubs_virt_agent_mem_borrow_strategy(&src_param, &borrow_strategy);
    EXPECT_EQ(ret, VA_ERROR_DESERIALIZE_FAILED);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_borrow_strategy)
{
    src_memory_borrow_param src_param{"0", 0, 0, 100};
    borrow_strategy_c borrow_strategy{"0", 0, 0, 100, {"0", 0, 0, {0}, {100}}, 100};

    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    MOCKER(ubse_mem_borrow_strategy_msg_unpack).stubs().will(returnValue(0));
    virt_agent_ret_t ret = ubs_virt_agent_mem_borrow_strategy(&src_param, &borrow_strategy);
    EXPECT_EQ(ret, VA_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_migrate_strategy_InvalidParameters)
{
    virt_agent_ret_t ret = ubs_virt_agent_mem_migrate_strategy(nullptr, nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_migrate_strategy)
{
    MemMigrateStrategySrcParam param = {.borrowInNode = "node0"};
    param.borrowSize = 0;
    param.vmInfoListSize = 2;
    param.vmInfoList[0].pid = 1234;
    param.vmInfoList[0].ratio = 80;
    param.vmInfoList[1].pid = 5678;
    param.vmInfoList[1].ratio = 60;
    MemMigrateStrategy strategy{};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    MOCKER(ubse_mem_migrate_strategy_msg_unpack).stubs().will(returnValue(0));
    virt_agent_ret_t ret = ubs_virt_agent_mem_migrate_strategy(&param, &strategy);
    EXPECT_EQ(ret, VA_SUCCESS);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_migrate_strategy_InvokeFail)
{
    MemMigrateStrategySrcParam param = {.borrowInNode = "node0"};
    param.borrowSize = 0;
    param.vmInfoListSize = 2;
    param.vmInfoList[0].pid = 1234;
    param.vmInfoList[0].ratio = 80;
    param.vmInfoList[1].pid = 5678;
    param.vmInfoList[1].ratio = 60;
    MemMigrateStrategy strategy{};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(1));
    virt_agent_ret_t ret = ubs_virt_agent_mem_migrate_strategy(&param, &strategy);
    EXPECT_EQ(ret, VA_ERROR_BASE);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_migrate_strategy_UnpackFail)
{
    MemMigrateStrategySrcParam param = {.borrowInNode = "node0"};
    param.borrowSize = 0;
    param.vmInfoListSize = 2;
    param.vmInfoList[0].pid = 1234;
    param.vmInfoList[0].ratio = 80;
    param.vmInfoList[1].pid = 5678;
    param.vmInfoList[1].ratio = 60;
    MemMigrateStrategy strategy{};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    MOCKER(ubse_mem_migrate_strategy_msg_unpack).stubs().will(returnValue(1));
    virt_agent_ret_t ret = ubs_virt_agent_mem_migrate_strategy(&param, &strategy);
    EXPECT_EQ(ret, VA_ERROR_BASE);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_return_InvalidParameters)
{
    char* task_id = nullptr;
    uint32_t task_id_len = 0;
    virt_agent_ret_t ret = ubs_virt_agent_mem_return(true, &task_id, &task_id_len);
    EXPECT_EQ(ret, VA_ERROR_BASE);

    task_id = nullptr;
    task_id_len = 0;
    ret = ubs_virt_agent_mem_return(true, &task_id, nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);

    task_id = nullptr;
    task_id_len = 0;
    ret = ubs_virt_agent_mem_return(true, &task_id, &task_id_len);
    EXPECT_EQ(ret, VA_ERROR_BASE);

    ret = ubs_virt_agent_mem_return(false, &task_id, &task_id_len);
    EXPECT_EQ(ret, VA_ERROR_BASE);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_return)
{
    char* task_id = nullptr;
    uint32_t task_id_len = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    virt_agent_ret_t ret = ubs_virt_agent_mem_return(true, &task_id, &task_id_len);
    EXPECT_EQ(ret, VA_ERROR_BASE);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_return_MencpyFail)
{
    char* task_id = nullptr;
    uint32_t task_id_len = 0;

    MOCKER(memcpy_s).stubs().will(returnValue(1));
    virt_agent_ret_t ret = ubs_virt_agent_mem_return(true, &task_id, &task_id_len);
    EXPECT_EQ(ret, VA_ERROR_MEM_COPY_FAILED);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_migrate_execute_InvalidParameters)
{
    virt_agent_ret_t ret = ubs_virt_agent_mem_migrate_execute(nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_migrate_execute)
{
    MemMigrateExecuteSrcParam param = {.borrowInNode = "node0", .borrowIds = {"id1", "id2"}};
    param.borrowIdsCount = 2;
    param.vmInfoListSize = 2;
    param.vmInfoList[0].destNumaId = 1;
    param.vmInfoList[0].memSize = 2048;
    param.vmInfoList[0].pid = 1234;
    param.vmInfoList[1].destNumaId = 0;
    param.vmInfoList[1].memSize = 4096;
    param.vmInfoList[1].pid = 5678;
    param.waitingTime = 1000;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    virt_agent_ret_t ret = ubs_virt_agent_mem_migrate_execute(&param);
    EXPECT_EQ(ret, VA_SUCCESS);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_migrate_execute_InvokeFail)
{
    MemMigrateExecuteSrcParam param = {.borrowInNode = "node0", .borrowIds = {"id1"}};
    param.borrowIdsCount = 1;
    param.vmInfoListSize = 2;
    param.vmInfoList[0].destNumaId = 1;
    param.vmInfoList[0].memSize = 2048;
    param.vmInfoList[0].pid = 1234;
    param.waitingTime = 1000;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(1));
    virt_agent_ret_t ret = ubs_virt_agent_mem_migrate_execute(&param);
    EXPECT_EQ(ret, VA_ERROR_BASE);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_rollback_InvalidParameters)
{
    RollbackSrcParam srcParam{.borrow_id_size = 0};
    virt_agent_ret_t ret = ubs_virt_agent_mem_rollback(&srcParam);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_rollback)
{
    RollbackSrcParam srcParam{.node_id = "test_node", .borrow_id_list = {"id1", "id2"}, .borrow_id_size = 2};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(0));
    virt_agent_ret_t ret = ubs_virt_agent_mem_rollback(&srcParam);
    EXPECT_EQ(ret, VA_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentation, ubs_virt_agent_mem_rollback_InvokeFail)
{
    RollbackSrcParam srcParam{.node_id = "test_node", .borrow_id_list = {"id1", "id2"}, .borrow_id_size = 2};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(1));
    virt_agent_ret_t ret = ubs_virt_agent_mem_rollback(&srcParam);
    EXPECT_EQ(ret, VA_ERROR_BASE);
    GlobalMockObject::verify();
}
} // namespace ubse::vm::ut