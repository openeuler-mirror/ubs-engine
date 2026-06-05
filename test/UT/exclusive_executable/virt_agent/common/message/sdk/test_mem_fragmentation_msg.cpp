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
    RollbackSrcParam rollBackSrcParam{.node_id = "0", .borrow_id_list = {"1", "2"}, .borrow_id_size = 2};
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
    RollbackSrcParam rollBackSrcParam{.node_id = "0", .borrow_id_list = {"1", "2"}, .borrow_id_size = 2};
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
    setting.borrow_strategy.borrow_size = 1024 * 1024; // 1MB

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
    setting.borrow_strategy.borrow_size = 1024 * 1024; // 1MB

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

// Tests for MemFragmentationNodeInfoListMsg
TEST_F(TestMemFragmentationMsg, MemFragmentationNodeInfoListMsg_Serialize_Deserialize)
{
    using namespace ::vm::mem_fragmentation;

    std::vector<NodeInfo> nodeInfoList;
    NodeInfo node1;
    node1.nodeId = "node1";
    node1.isCurrent = true;
    NumaInfo numa1;
    numa1.timestamp = 1000;
    numa1.metaData.nodeId = "node1";
    numa1.metaData.hostName = "host1";
    numa1.metaData.numaId = 0;
    numa1.metaData.socketId = 0;
    numa1.metaData.isLocal = true;
    numa1.metaData.memTotal = 100000;
    numa1.metaData.memFree = 50000;
    numa1.metaData.numaPageInfo[4096] = {4096, 100, 50};
    node1.numaInfos.push_back(numa1);
    nodeInfoList.push_back(node1);

    NodeInfo node2;
    node2.nodeId = "node2";
    node2.isCurrent = false;
    NumaInfo numa2;
    numa2.timestamp = 2000;
    numa2.metaData.nodeId = "node2";
    numa2.metaData.hostName = "host2";
    numa2.metaData.numaId = 1;
    numa2.metaData.socketId = 1;
    numa2.metaData.isLocal = false;
    numa2.metaData.memTotal = 200000;
    numa2.metaData.memFree = 100000;
    numa2.metaData.numaPageInfo[2048] = {2048, 200, 100};
    node2.numaInfos.push_back(numa2);
    nodeInfoList.push_back(node2);

    MemFragmentationNodeInfoListMsg msg{nodeInfoList};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationNodeInfoListMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    auto result = dmsg.GetNodeInfoList();
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].nodeId, "node1");
    EXPECT_EQ(result[0].isCurrent, true);
    EXPECT_EQ(result[1].nodeId, "node2");
    EXPECT_EQ(result[1].isCurrent, false);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationNodeInfoListMsg_Empty_List)
{
    using namespace ::vm::mem_fragmentation;

    std::vector<NodeInfo> nodeInfoList;
    MemFragmentationNodeInfoListMsg msg{nodeInfoList};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationNodeInfoListMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    auto result = dmsg.GetNodeInfoList();
    EXPECT_EQ(result.size(), 0);
}

// Tests for MemFragmentationMemBorrowParamMsg
TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowParamMsg_Serialize_Deserialize)
{
    using namespace ::vm::mem_fragmentation;

    BorrowParam param;
    param.nodeId = "node1";
    param.borrowSize = 1024; // 1GB

    NumaMetaInfo numaMeta1;
    numaMeta1.numaId = 0;
    param.numaMetaInfos.push_back(numaMeta1);

    NumaMetaInfo numaMeta2;
    numaMeta2.numaId = 1;
    param.numaMetaInfos.push_back(numaMeta2);

    MemFragmentationMemBorrowParamMsg msg{param, true};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationMemBorrowParamMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    auto resultParam = dmsg.GetBorrowParam();
    EXPECT_EQ(resultParam.nodeId, "node1");
    EXPECT_EQ(resultParam.borrowSize, 1024);
    EXPECT_EQ(resultParam.numaMetaInfos.size(), 2);
    EXPECT_EQ(dmsg.GetIsAsync(), true);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowParamMsg_Sync_Mode)
{
    using namespace ::vm::mem_fragmentation;

    BorrowParam param;
    param.nodeId = "node2";
    param.borrowSize = 2048;

    MemFragmentationMemBorrowParamMsg msg{param, false};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationMemBorrowParamMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(dmsg.GetIsAsync(), false);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowParamMsg_Empty_NumaMetaInfos)
{
    using namespace ::vm::mem_fragmentation;

    BorrowParam param;
    param.nodeId = "node3";
    param.borrowSize = 512;

    MemFragmentationMemBorrowParamMsg msg{param, true};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationMemBorrowParamMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    auto resultParam = dmsg.GetBorrowParam();
    EXPECT_EQ(resultParam.numaMetaInfos.size(), 0);
}

// Tests for MemFragmentationMemBorrowResultMsg
TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowResultMsg_Serialize_Deserialize)
{
    using namespace ::vm::mem_fragmentation;

    std::vector<mem_borrow_result_c> results;
    mem_borrow_result_c result1;
    result1.task_id[0] = '\0';
    result1.borrow_ids_size = 2;
    result1.present_numa_ids_size = 1;
    strcpy_s(result1.task_id, MEM_TASK_ID_MAX, "task1");
    strcpy_s(result1.borrow_ids_ptr[0], MAX_BORROW_ID_LENGTH, "borrow1");
    strcpy_s(result1.borrow_ids_ptr[1], MAX_BORROW_ID_LENGTH, "borrow2");
    result1.present_numa_ids_ptr[0] = 0;
    results.push_back(result1);

    MemFragmentationMemBorrowResultMsg msg{results};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationMemBorrowResultMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowResultMsg_Empty_Result)
{
    using namespace ::vm::mem_fragmentation;

    std::vector<mem_borrow_result_c> results;
    MemFragmentationMemBorrowResultMsg msg{results};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationMemBorrowResultMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

// Tests for MemFragmentationPageSwapEnableMsg
TEST_F(TestMemFragmentationMsg, MemFragmentationPageSwapEnableMsg_Serialize_Deserialize)
{
    using namespace ::vm::mem_fragmentation;

    pid_t pid = 12345;
    std::vector<::mem_fragmentation::PageSwapPair> pageSwapPairs;

    ::mem_fragmentation::PageSwapPair pair1;
    ::mem_fragmentation::NumaQuota localQuota1;
    localQuota1.numaId = 0;
    localQuota1.quota = 1024;
    pair1.localNumaQuotas.push_back(localQuota1);

    ::mem_fragmentation::NumaQuota remoteQuota1;
    remoteQuota1.numaId = 1;
    remoteQuota1.quota = 2048;
    pair1.remoteNumaQuotas.push_back(remoteQuota1);
    pageSwapPairs.push_back(pair1);

    ::mem_fragmentation::PageSwapPair pair2;
    ::mem_fragmentation::NumaQuota localQuota2;
    localQuota2.numaId = 2;
    localQuota2.quota = 4096;
    pair2.localNumaQuotas.push_back(localQuota2);

    ::mem_fragmentation::NumaQuota remoteQuota2;
    remoteQuota2.numaId = 3;
    remoteQuota2.quota = 8192;
    pair2.remoteNumaQuotas.push_back(remoteQuota2);
    pageSwapPairs.push_back(pair2);

    MemFragmentationPageSwapEnableMsg msg{pid, pageSwapPairs};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationPageSwapEnableMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    EXPECT_EQ(dmsg.GetPid(), pid);
    auto resultPairs = dmsg.GetPageSwapPair();
    EXPECT_EQ(resultPairs.size(), 2);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationPageSwapEnableMsg_Empty_Pairs)
{
    using namespace ::vm::mem_fragmentation;

    pid_t pid = 67890;
    std::vector<::mem_fragmentation::PageSwapPair> pageSwapPairs;

    MemFragmentationPageSwapEnableMsg msg{pid, pageSwapPairs};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationPageSwapEnableMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    EXPECT_EQ(dmsg.GetPid(), pid);
    EXPECT_EQ(dmsg.GetPageSwapPair().size(), 0);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationPageSwapEnableMsg_Multiple_Quotas_Per_Pair)
{
    using namespace ::vm::mem_fragmentation;

    pid_t pid = 11111;
    std::vector<::mem_fragmentation::PageSwapPair> pageSwapPairs;

    ::mem_fragmentation::PageSwapPair pair1;
    // Multiple local quotas
    for (uint32_t i = 0; i < 3; i++) {
        ::mem_fragmentation::NumaQuota localQuota;
        localQuota.numaId = i;
        localQuota.quota = 1024 * (i + 1);
        pair1.localNumaQuotas.push_back(localQuota);
    }

    // Multiple remote quotas
    for (uint32_t i = 0; i < 2; i++) {
        ::mem_fragmentation::NumaQuota remoteQuota;
        remoteQuota.numaId = i + 10;
        remoteQuota.quota = 2048 * (i + 1);
        pair1.remoteNumaQuotas.push_back(remoteQuota);
    }
    pageSwapPairs.push_back(pair1);

    MemFragmentationPageSwapEnableMsg msg{pid, pageSwapPairs};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationPageSwapEnableMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    auto resultPairs = dmsg.GetPageSwapPair();
    EXPECT_EQ(resultPairs.size(), 1);
    EXPECT_EQ(resultPairs[0].localNumaQuotas.size(), 3);
    EXPECT_EQ(resultPairs[0].remoteNumaQuotas.size(), 2);
}

} // namespace ubse::vm::ut