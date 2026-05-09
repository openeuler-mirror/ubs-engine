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

#include "test_ubs_virt_agent_mem_fragmentation_helper.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include "mem_fragmentation_msg.h"
#include "mempooling_def.h"
#include "ubs_virt_agent_mem_fragmentation.h"
#include "ubs_virt_agent_mem_fragmentation_helper.h"
#include "ubse_ipc_client.h"
#include "vm_serial_util.h"

namespace ubse::vm::ut {
using namespace ::vm;
using namespace ::vm::mempooling;
TestLibvirtAgentMemFragmentationHelper::TestLibvirtAgentMemFragmentationHelper() = default;

void TestLibvirtAgentMemFragmentationHelper::SetUp()
{
    Test::SetUp();
}

void TestLibvirtAgentMemFragmentationHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentMemFragmentationHelper, ubse_mem_borrow_execute_msg_unpack_fail)
{
    uint8_t *buffer = nullptr;
    uint32_t len = 0;

    mem_borrow_result_c result{};

    virt_agent_ret_t ret = ubse_mem_borrow_execute_msg_unpack(buffer, len, &result);

    EXPECT_EQ(ret, VA_ERROR_DESERIALIZE_FAILED);

    EXPECT_EQ(result.borrow_ids_size, 0);
    EXPECT_EQ(result.present_numa_ids_size, 0);
}

TEST_F(TestLibvirtAgentMemFragmentationHelper, ubse_mem_borrow_execute_msg_unpack_failed)
{
    MemBorrowExecuteResult param{{"1", "2"}, {1, 2}};
    MemFragmentationMemBorrowExecuteOutputMsg msg{param};
    msg.Serialize();

    mem_borrow_result_c result{};
    memset_s(&result, sizeof(result), 0, sizeof(result));

    virt_agent_ret_t ret = ubse_mem_borrow_execute_msg_unpack(msg.SerializedData(), msg.SerializedDataSize(), &result);

    EXPECT_EQ(ret, VA_ERROR_DESERIALIZE_FAILED);
}

VmResult MockDeserialize()
{
    return VM_OK;
}

NodeAntiDictionary CreateTestNodeDict()
{
    NodeAntiDictionary dict;
    dict.entry_count = 2;
    KeyValuePair entry1;
    strcpy_s(entry1.key, VIRT_MEM_MAX_NODE_ID_LENGTH, "key1");
    entry1.value_count = 2;
    strcpy_s(entry1.value[0], VIRT_MEM_MAX_NODE_ID_LENGTH, "value1");
    strcpy_s(entry1.value[1], VIRT_MEM_MAX_NODE_ID_LENGTH, "value2");

    KeyValuePair entry2;
    strcpy_s(entry2.key, VIRT_MEM_MAX_NODE_ID_LENGTH, "key2");
    entry2.value_count = 1;
    strcpy_s(entry2.value[0], VIRT_MEM_MAX_NODE_ID_LENGTH, "value3");
    dict.entries[0] = entry1;
    dict.entries[1] = entry2;

    return dict;
}

TEST_F(TestLibvirtAgentMemFragmentationHelper, allocate_memory_Success)
{
    size_t buffer_size = 1024;
    uint8_t *buffer = allocate_memory(buffer_size);

    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(buffer_size, sizeof(uint8_t) * buffer_size);

    delete[] buffer;
}

TEST_F(TestLibvirtAgentMemFragmentationHelper, serialize_data_Fail_NullBuffer)
{
    NodeAntiDictionary node_dict = CreateTestNodeDict();
    serialize_data(node_dict, nullptr);
}

TEST_F(TestLibvirtAgentMemFragmentationHelper, serialize_data_Success)
{
    NodeAntiDictionary node_dict = CreateTestNodeDict();
    size_t buffer_size = 4096;
    uint8_t *buffer = allocate_memory(buffer_size);

    EXPECT_NE(buffer, nullptr);

    serialize_data(node_dict, buffer);

    uint8_t *ptr = buffer;
    uint32_t entries_count = 0;
    memcpy_s(&entries_count, sizeof(uint32_t), ptr, sizeof(uint32_t));
    EXPECT_EQ(entries_count, node_dict.entry_count);
    ptr += sizeof(uint32_t);

    uint32_t key_length = 0;
    memcpy_s(&key_length, sizeof(uint32_t), ptr, sizeof(uint32_t));
    EXPECT_EQ(key_length, strlen(node_dict.entries[0].key) + 1);
    ptr += sizeof(uint32_t);

    char *key = new char[key_length];
    memcpy_s(key, key_length, ptr, key_length);
    EXPECT_STREQ(key, node_dict.entries[0].key);
    ptr += key_length;

    uint32_t value_count = 0;
    memcpy_s(&value_count, sizeof(uint32_t), ptr, sizeof(uint32_t));
    EXPECT_EQ(value_count, node_dict.entries[0].value_count);
    ptr += sizeof(uint32_t);

    for (size_t j = 0; j < value_count; ++j) {
        uint32_t value_length = 0;
        memcpy_s(&value_length, sizeof(uint32_t), ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        char *value = new char[value_length];
        memcpy_s(value, value_length, ptr, value_length);
        EXPECT_STREQ(value, node_dict.entries[0].value[j]);
        ptr += value_length;
    }

    delete[] key;
    delete[] buffer;
}

TEST_F(TestLibvirtAgentMemFragmentationHelper, ubse_mem_migrate_strategy_msg_unpack)
{
    MemMigrateStrategy strategy = {0};
    strategy.vmInfoListSize = 2;
    strategy.vmInfoList = (VmMigrateStrategy *)malloc(strategy.vmInfoListSize * sizeof(VmMigrateStrategy));
    strategy.vmInfoList[0].destNumaId = 1;
    strategy.vmInfoList[0].memSize = 2048;
    strategy.vmInfoList[0].pid = 1234;
    strategy.vmInfoList[1].destNumaId = 0;
    strategy.vmInfoList[1].memSize = 4096;
    strategy.vmInfoList[1].pid = 5678;
    strategy.waitingTime = 500;
    MemFragmentationMemMigrateStrategyOutputMsg msg{strategy};
    msg.Serialize();
    MemMigrateStrategy memMigrateStrategy;
    auto ret =
        ubse_mem_migrate_strategy_msg_unpack(msg.SerializedData(), msg.SerializedDataSize(), &memMigrateStrategy);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestLibvirtAgentMemFragmentationHelper, ubse_mem_borrow_strategy_msg_unpack)
{
    borrow_strategy_c borrowStrategyC{};
    borrow_strategy_c outputMsg{};
    MemFragmentationMemBorrowStrategyOutputMsg msg{borrowStrategyC};
    ASSERT_EQ(msg.Serialize(), VM_OK);
    virt_agent_ret_t ret =
        ubse_mem_borrow_strategy_msg_unpack(msg.SerializedData(), msg.SerializedDataSize(), &outputMsg);
    EXPECT_EQ(ret, VA_SUCCESS);
}
} // namespace ubse::vm::ut