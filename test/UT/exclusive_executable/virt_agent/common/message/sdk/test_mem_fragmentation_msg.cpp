/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "test_mem_fragmentation_msg.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include "mem_fragmentation_msg.h"
#include "mempooling_def.h"
#include "vm_serial_util.h"

using namespace vm;
using namespace vm::mempooling;
namespace ubse::vm::ut {

TestMemFragmentationMsg::TestMemFragmentationMsg() = default;

void TestMemFragmentationMsg::SetUp()
{
    Test::SetUp();
}

void TestMemFragmentationMsg::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMsg_Serialize_Deserialize)
{
    std::vector<NumaInfo> numaInfos;
    numaInfos.push_back({0, {"0", "0", 0, 0, true, 100, 0, {{0, {0, 0, 0}}}}});
    numaInfos.push_back({0, {"1", "1", 1, 1, false, 1000, 10, {{1, {1, 1, 1}}}}});
    MemFragmentationMsg msg{numaInfos};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemFragmentationMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMsg_GetNumaInfo)
{
    std::vector<NumaInfo> numaInfos;
    numaInfos.push_back({0, {"0", "0", 0, 0, true, 100, 0, {{0, {0, 0, 0}}}}});
    numaInfos.push_back({0, {"1", "1", 1, 1, false, 1000, 10, {{1, {1, 1, 1}}}}});
    MemFragmentationMsg msg{numaInfos};
    std::vector<numa_info_t> numaInfo{};
    auto ret = msg.GetNumaInfo(numaInfo);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(numaInfo.size(), 2);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationVmInfoMsg_Serialize_Deserialize)
{
    std::vector<VmDomainInfo> vmInfoList;
    vmInfoList.push_back({{"0", "0", "0", "0", "0", 0, 1000, 0}, {{0, {0, 0, 0, 0, 0}}}});
    vmInfoList.push_back({{"1", "1", "1", "1", "1", 1, 2000, 1}, {{1, {1, 1, 1, 1, 1}}}});
    MemFragmentationVmInfoMsg msg{vmInfoList};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemFragmentationVmInfoMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationVmInfoMsg_GetVmInfo)
{
    std::vector<VmDomainInfo> vmInfoList;
    vmInfoList.push_back({{"0", "0", "0", "0", "0", 0, 1000, 0}, {{0, {0, 0, 0, 0, 0}}}});
    vmInfoList.push_back({{"1", "1", "1", "1", "1", 1, 2000, 1}, {{1, {1, 1, 1, 1, 1}}}});
    MemFragmentationVmInfoMsg msg{vmInfoList};
    auto info = msg.GetVmInfo();
    EXPECT_EQ(info.size(), 2);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowStrategyInputMsg_Serialize_Deserialize)
{
    src_memory_borrow_param srcMemoryBorrowParam{
        .src_nid = "0", .src_socket_id = 0, .src_numa_id = 0, .borrow_size = 0};
    MemFragmentationMemBorrowStrategyInputMsg msg{srcMemoryBorrowParam};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemFragmentationMemBorrowStrategyInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowStrategyInputMsg_GetInputMsg)
{
    src_memory_borrow_param srcMemoryBorrowParam{
        .src_nid = "0", .src_socket_id = 0, .src_numa_id = 0, .borrow_size = 0};
    MemFragmentationMemBorrowStrategyInputMsg msg{srcMemoryBorrowParam};
    auto info = msg.GetInputMsg();
    EXPECT_STREQ(info.src_nid, "0");
    EXPECT_EQ(info.src_socket_id, 0);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowStrategyOutputMsg_Serialize_Deserialize)
{
    borrow_strategy_c outputMsg{"0", 0, 0, 0, {{"1", 1, 1, {1}, {100}}, {"1", 1, 1, {1}, {100}}}, 0};
    MemFragmentationMemBorrowStrategyOutputMsg msg{outputMsg};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemFragmentationMemBorrowStrategyOutputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowStrategyOutputMsg_GetMemBorrowStrategyOutputMsg)
{
    MemBorrowStrategyResult borrowStrategyResult{{"0", 0, 0}, 100, {{"1", 1, 1, {0}, {100}}}};
    MemFragmentationMemBorrowStrategyOutputMsg msg{};
    auto ret = msg.SetOutputMsg(borrowStrategyResult);
    ASSERT_EQ(ret, VM_OK);
    auto info = msg.GetMemBorrowStrategyOutputMsg();
    EXPECT_STREQ(info.src_host_id, "0");
    EXPECT_EQ(info.src_socket_id, 0);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowExecuteOutputMsg_Serialize_Deserialize)
{
    MemBorrowExecuteResult outputMsg{{"0", "1"}, {0, 1}};
    MemFragmentationMemBorrowExecuteOutputMsg msg{outputMsg};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemFragmentationMemBorrowExecuteOutputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowExecuteOutputMsg_GetMemBorrowExecuteOutputMsg)
{
    MemBorrowExecuteResult outputMsg{{"0", "1"}, {0, 1}};
    MemFragmentationMemBorrowExecuteOutputMsg msg{outputMsg};
    auto info = msg.GetMemBorrowExecuteOutputMsg();
    EXPECT_EQ(info.borrowIds.size(), 2);
    EXPECT_EQ(info.presentNumaIds.size(), 2);
}

TEST_F(TestMemFragmentationMsg, MemRollbackMsg_Serialize_Deserialize)
{
    RollbackSrcParam rollBackSrcParam{
        .node_id = "0",
        .borrow_id_list = {"1", "2"},
        .borrow_id_size = 2
    };
    RollbackParams rollbackParams(&rollBackSrcParam);
    MemRollbackMsg msg{rollbackParams};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemRollbackMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemFragmentationMsg, MemRollbackMsg_GetRollbackParams)
{
    RollbackSrcParam rollBackSrcParam{
        .node_id = "0",
        .borrow_id_list = {"1", "2"},
        .borrow_id_size = 2
    };
    RollbackParams rollbackParams(&rollBackSrcParam);
    MemRollbackMsg msg{rollbackParams};
    auto info = msg.GetRollbackParams();
    EXPECT_EQ(info.node_id, "0");
    EXPECT_EQ(info.borrow_id_list.size(), 2);
    EXPECT_EQ(info.borrow_id_size, 2);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemMigrateStrategyInputMsg_Serialize_Deserialize)
{
    MemMigrateStrategyVmInfo vmInfo1 = {708680, 30};
    MemMigrateStrategyVmInfo vmInfo2 = {708857, 30};
    MemMigrateStrategySrcParam srcParam;
    srcParam.borrowSize = 1048576; // 1MB
    strcpy_s(srcParam.borrowInNode, VIRT_MEM_MAX_NODE_ID_LENGTH, "Node1");
    srcParam.vmInfoListSize = 2;
    srcParam.vmInfoList[0] = vmInfo1;
    srcParam.vmInfoList[1] = vmInfo2;
    MemFragmentationMemMigrateStrategyInputMsg msg{};
    msg.SetInputMsg(srcParam);
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemFragmentationMemMigrateStrategyInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemMigrateStrategyInputMsg_GetInputMsg)
{
    MemMigrateStrategyVmInfo vmInfo1 = {708680, 30};
    MemMigrateStrategyVmInfo vmInfo2 = {708857, 30};
    MemMigrateStrategySrcParam srcParam;
    srcParam.borrowSize = 1048576; // 1MB
    strcpy_s(srcParam.borrowInNode, VIRT_MEM_MAX_NODE_ID_LENGTH, "Node1");
    srcParam.vmInfoListSize = 2;
    srcParam.vmInfoList[0] = vmInfo1;
    srcParam.vmInfoList[1] = vmInfo2;
    MemFragmentationMemMigrateStrategyInputMsg msg{};
    msg.SetInputMsg(srcParam);
    auto info = msg.GetInputMsg();
    EXPECT_EQ(info.borrowSize, 1048576);
    EXPECT_EQ(info.vmInfoListSize, 2);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemMigrateStrategyOutputMsg_Serialize_Deserialize)
{
    VmMigrateStrategy vmInfo1 = {0, 1048576, 708680}; // destNumaId=0, memSize=1MB, pid=708680
    MemMigrateStrategy srcParam;
    srcParam.vmInfoListSize = 1;
    srcParam.vmInfoList = new VmMigrateStrategy[srcParam.vmInfoListSize];
    srcParam.vmInfoList[0] = vmInfo1;
    srcParam.waitingTime = 0;
    MemFragmentationMemMigrateStrategyOutputMsg msg{srcParam};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemFragmentationMemMigrateStrategyOutputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemMigrateStrategyOutputMsg_GetOutputMsg)
{
    MigrateStrategyResult param{0, {{0, 100, 1}}, 0};
    MemFragmentationMemMigrateStrategyOutputMsg msg{};
    msg.SetOutputMsg(param);
    auto info = msg.GetOutputMsg();
    EXPECT_EQ(info.waitingTime, 0);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemMigrateExecuteInputMsg_Serialize_Deserialize)
{
    VmMigrateStrategy vmInfo1 = {0, 1048576, 708680};
    MemMigrateExecuteSrcParam srcParam;
    strcpy_s(srcParam.borrowInNode, VIRT_MEM_MAX_NODE_ID_LENGTH, "Node1");
    srcParam.borrowIdsCount = 2;
    strcpy_s(srcParam.borrowIds[0], MAX_BORROW_ID_LENGTH, "borrow1");
    strcpy_s(srcParam.borrowIds[1], MAX_BORROW_ID_LENGTH, "borrow2");
    srcParam.vmInfoListSize = 1;
    srcParam.vmInfoList[0] = vmInfo1;
    srcParam.waitingTime = 1000;
    MemFragmentationMemMigrateExecuteInputMsg msg{};
    msg.SetInputMsg(srcParam);
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemFragmentationMemMigrateExecuteInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemMigrateExecuteInputMsg_GetInputMsg)
{
    VmMigrateStrategy vmInfo1 = {0, 1048576, 708680};
    MemMigrateExecuteSrcParam srcParam;
    strcpy_s(srcParam.borrowInNode, VIRT_MEM_MAX_NODE_ID_LENGTH, "Node1");
    srcParam.borrowIdsCount = 2;
    strcpy_s(srcParam.borrowIds[0], MAX_BORROW_ID_LENGTH, "borrow1");
    strcpy_s(srcParam.borrowIds[1], MAX_BORROW_ID_LENGTH, "borrow2");
    srcParam.vmInfoListSize = 1;
    srcParam.vmInfoList[0] = vmInfo1;
    srcParam.waitingTime = 1000;
    MemFragmentationMemMigrateExecuteInputMsg msg{};
    msg.SetInputMsg(srcParam);
    auto info = msg.GetInputMsg();
    EXPECT_EQ(info.borrowIdsCount, 2);
    EXPECT_EQ(info.waitingTime, 1000);
}

TEST_F(TestMemFragmentationMsg, MemTaskResultQueryMsg_Serialize_Deserialize)
{
    async_task_info_c taskInfo{};
    taskInfo.task_id[0] = '\0';
    taskInfo.status = ASYNC_TASK_RUNNING;
    taskInfo.resultCode = 0;
    taskInfo.memBorrowResult.borrow_ids_size = 2;
    taskInfo.memBorrowResult.present_numa_ids_size = 1;
    strcpy_s(taskInfo.memBorrowResult.borrow_ids_ptr[0], MAX_BORROW_ID_LENGTH, "id1");
    strcpy_s(taskInfo.memBorrowResult.borrow_ids_ptr[1], MAX_BORROW_ID_LENGTH, "id2");
    taskInfo.memBorrowResult.present_numa_ids_ptr[0] = 100;
    strcpy_s(taskInfo.memBorrowResult.task_id, MEM_TASK_ID_MAX, "test_task_123");

    MemTaskResultQueryMsg msg{};
    msg.SetInputInfos(taskInfo);
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemTaskResultQueryMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemFragmentationMsg, MemBorrowSettingMsg_Serialize_Deserialize_Failed)
{
    borrow_setting_c setting{};
    setting.borrow_strategy.src_host_id[0] = '\0';
    strcpy_s(setting.borrow_strategy.src_host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "test_host_1");
    setting.borrow_strategy.src_socket_id = 0;
    setting.borrow_strategy.src_numa_id = 1;
    setting.borrow_strategy.borrow_size = 1024 * 1024;  // 1MB

    setting.borrow_strategy.dest_numa_infos_size = 2;
    for (uint32_t i = 0; i < 2; i++) {
        strcpy_s(setting.borrow_strategy.dest_numa_infos[i].host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "dest_host_1");
        setting.borrow_strategy.dest_numa_infos[i].socket_id = 0;
        setting.borrow_strategy.dest_numa_infos[i].numa_nums = 2;
        for (uint16_t j = 0; j < 2; j++) {
            setting.borrow_strategy.dest_numa_infos[i].numa_ids[j] = j + 10;
            setting.borrow_strategy.dest_numa_infos[i].mem_sizes[j] = 1024 * 1024 * (j + 1);
        }
    }

    MemBorrowSettingMsg msg(setting);
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_ERROR_INVAL);
    MemBorrowSettingMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_ERROR_NULLPTR);
}

TEST_F(TestMemFragmentationMsg, MemBorrowSettingMsg_Serialize_Deserialize2)
{
    borrow_setting_c setting{};
    setting.borrow_strategy.src_host_id[0] = '\0';
    strcpy_s(setting.borrow_strategy.src_host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "test_host_1");
    setting.borrow_strategy.src_socket_id = 0;
    setting.borrow_strategy.src_numa_id = 1;
    setting.borrow_strategy.borrow_size = 1024 * 1024;  // 1MB

    setting.borrow_strategy.dest_numa_infos_size = 2;
    for (uint32_t i = 0; i < 2; i++) {
        strcpy_s(setting.borrow_strategy.dest_numa_infos[i].host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "dest_host_1");
        setting.borrow_strategy.dest_numa_infos[i].socket_id = 0;
        setting.borrow_strategy.dest_numa_infos[i].numa_nums = 2;
        for (uint16_t j = 0; j < 2; j++) {
            setting.borrow_strategy.dest_numa_infos[i].numa_ids[j] = j + 10;
            setting.borrow_strategy.dest_numa_infos[i].mem_sizes[j] = 1024 * 1024 * (j + 1);
        }
    }

    MemBorrowSettingMsg msg(setting);
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_ERROR_INVAL);
    MemBorrowSettingMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_ERROR_NULLPTR);
}

} // namespace ubse::vm::ut