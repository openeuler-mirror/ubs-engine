/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "test_mem_fragmentation_msg.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include <algorithm>
#include <iostream>
#include <string>

#include "mem_fragmentation_msg.h"
#include "mempooling_def.h"
#include "vm_serial_util.h"

using namespace vm;
using namespace vm::mempooling;
namespace ubse::vm::ut {

std::string HugePagesText(const std::unordered_map<uint64_t, NumaPageData>& pages)
{
    std::string text = "[";
    bool first = true;
    for (const auto& entry : pages) {
        if (!first) {
            text += "; ";
        }
        text += "pageSize=" + std::to_string(entry.second.pageSize) +
                "kB,total=" + std::to_string(entry.second.hugePageTotal) +
                ",free=" + std::to_string(entry.second.hugePageFree);
        first = false;
    }
    return text + "]";
}

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
    std::vector<vm_domain_info_for_c> info;
    auto ret = msg.GetVmInfo(info);
    EXPECT_EQ(ret, VM_OK);
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

TEST_F(TestMemFragmentationMsg, MemBorrowSettingMsg_PageSize_Serialize_Deserialize)
{
    borrow_setting_c setting{};
    strcpy_s(setting.borrow_strategy.src_host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "test_host_1");
    setting.borrow_strategy.src_socket_id = 0;
    setting.borrow_strategy.src_numa_id = 1;
    setting.borrow_strategy.borrow_size = 1024 * 1024;
    setting.borrow_strategy.dest_numa_infos_size = 1;
    strcpy_s(setting.borrow_strategy.dest_numa_infos[0].host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "dest_host_1");
    setting.borrow_strategy.dest_numa_infos[0].socket_id = 0;
    setting.borrow_strategy.dest_numa_infos[0].numa_nums = 1;
    setting.borrow_strategy.dest_numa_infos[0].numa_ids[0] = 10;
    setting.borrow_strategy.dest_numa_infos[0].mem_sizes[0] = 1024 * 1024;
    setting.page_size = HUGE_PAGE_SIZE_1G_KB;

    MemBorrowSettingMsg msg(setting);
    EXPECT_EQ(msg.Serialize(), VM_OK);
    MemBorrowSettingMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    EXPECT_EQ(dmsg.Deserialize(), VM_OK);
    EXPECT_EQ(dmsg.GetMemBorrowSettingMsg().page_size, HUGE_PAGE_SIZE_1G_KB);
}

// Tests for MemFragmentationNodeInfoListMsg
TEST_F(TestMemFragmentationMsg, MemFragmentationNodeInfoListMsg_Serialize_Deserialize)
{
    using namespace ::vm::mem_fragmentation;

    std::vector<NodeInfo> nodeInfoList;
    NodeInfo node1;
    node1.nodeId = "node1";
    node1.isCurrent = true;
    node1.isMaster = true;
    node1.podId = "pod-1";
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
    NumaInfo numa1b;
    numa1b.timestamp = 1500;
    numa1b.metaData.nodeId = "node1";
    numa1b.metaData.hostName = "host1";
    numa1b.metaData.numaId = 1;
    numa1b.metaData.socketId = 1;
    numa1b.metaData.isLocal = true;
    numa1b.metaData.memTotal = 16777216;
    numa1b.metaData.memFree = 8388608;
    numa1b.metaData.numaPageInfo[1048576] = {1048576, 8, 4};
    node1.numaInfos.push_back(numa1b);
    nodeInfoList.push_back(node1);

    NodeInfo node2;
    node2.nodeId = "node2";
    node2.isCurrent = false;
    node2.isMaster = false;
    NumaInfo numa2;
    numa2.timestamp = 2000;
    numa2.metaData.nodeId = "node2";
    numa2.metaData.hostName = "host2";
    numa2.metaData.numaId = 1;
    numa2.metaData.socketId = 1;
    numa2.metaData.isLocal = false;
    numa2.metaData.memTotal = 4194304;
    numa2.metaData.memFree = 2097152;
    numa2.metaData.numaPageInfo[2048] = {2048, 200, 100};
    numa2.metaData.numaPageInfo[1048576] = {1048576, 2, 1};
    node2.numaInfos.push_back(numa2);
    NumaInfo numa3;
    numa3.timestamp = 3000;
    numa3.metaData.nodeId = "node2";
    numa3.metaData.hostName = "host2";
    numa3.metaData.numaId = 2;
    numa3.metaData.socketId = 2;
    numa3.metaData.isLocal = false;
    numa3.metaData.memTotal = 8388608;
    numa3.metaData.memFree = 4194304;
    numa3.metaData.numaPageInfo[1048576] = {1048576, 4, 3};
    node2.numaInfos.push_back(numa3);
    nodeInfoList.push_back(node2);

    MemFragmentationNodeInfoListMsg msg{nodeInfoList};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    const auto* raw = reinterpret_cast<const char*>(msg.SerializedData());
    const auto rawSize = static_cast<size_t>(msg.SerializedDataSize());
    ASSERT_NE(raw, nullptr);
    ASSERT_GT(rawSize, 0UL);
    auto bytesContain = [&raw, &rawSize](const std::string& needle) {
        return std::search(raw, raw + rawSize, needle.begin(), needle.end()) != raw + rawSize;
    };
    EXPECT_TRUE(bytesContain("node1"));
    EXPECT_TRUE(bytesContain("node2"));
    EXPECT_TRUE(bytesContain("host1"));
    EXPECT_TRUE(bytesContain("host2"));
    EXPECT_TRUE(bytesContain("pod-1"));
    std::cout << "rawBytes=" << rawSize << " node1/node2/host1/host2/pod-1 all found in serialized stream" << std::endl;

    MemFragmentationNodeInfoListMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    auto result = dmsg.GetNodeInfoList();
    for (const auto& node : result) {
        std::cout << "nodeId=" << node.nodeId << " isCurrent=" << node.isCurrent << " isMaster=" << node.isMaster
                  << " podId=" << node.podId << " numaCount=" << node.numaInfos.size() << std::endl;
        for (const auto& numa : node.numaInfos) {
            std::cout << "  numaId=" << numa.metaData.numaId << " socketId=" << numa.metaData.socketId
                      << " isLocal=" << numa.metaData.isLocal << " nodeId=" << numa.metaData.nodeId
                      << " hostName=" << numa.metaData.hostName << " timestamp=" << numa.timestamp
                      << " memTotal=" << numa.metaData.memTotal << "kB memFree=" << numa.metaData.memFree
                      << "kB hugePages=" << HugePagesText(numa.metaData.numaPageInfo) << std::endl;
        }
    }
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].nodeId, "node1");
    EXPECT_EQ(result[0].isCurrent, true);
    EXPECT_EQ(result[0].isMaster, true);
    EXPECT_EQ(result[0].podId, "pod-1");
    EXPECT_EQ(result[1].nodeId, "node2");
    EXPECT_EQ(result[1].isCurrent, false);
    EXPECT_EQ(result[1].isMaster, false);
    EXPECT_EQ(result[1].podId, "");

    ASSERT_EQ(result[0].numaInfos.size(), 2);
    const auto& rn1 = result[0].numaInfos[0];
    EXPECT_EQ(rn1.metaData.nodeId, "node1");
    EXPECT_EQ(rn1.metaData.hostName, "host1");
    EXPECT_EQ(rn1.metaData.numaId, 0);
    EXPECT_EQ(rn1.metaData.socketId, 0);
    EXPECT_TRUE(rn1.metaData.isLocal);
    EXPECT_EQ(rn1.metaData.memTotal, 100000);
    EXPECT_EQ(rn1.metaData.memFree, 50000);
    EXPECT_EQ(rn1.timestamp, 1000);
    ASSERT_EQ(rn1.metaData.numaPageInfo.size(), 1);
    auto page1 = rn1.metaData.numaPageInfo.find(4096);
    ASSERT_NE(page1, rn1.metaData.numaPageInfo.end());
    EXPECT_EQ(page1->second.pageSize, 4096);
    EXPECT_EQ(page1->second.hugePageTotal, 100);
    EXPECT_EQ(page1->second.hugePageFree, 50);

    const auto& rn1b = result[0].numaInfos[1];
    EXPECT_EQ(rn1b.metaData.nodeId, "node1");
    EXPECT_EQ(rn1b.metaData.hostName, "host1");
    EXPECT_EQ(rn1b.metaData.numaId, 1);
    EXPECT_EQ(rn1b.metaData.socketId, 1);
    EXPECT_TRUE(rn1b.metaData.isLocal);
    EXPECT_EQ(rn1b.metaData.memTotal, 16777216);
    EXPECT_EQ(rn1b.metaData.memFree, 8388608);
    EXPECT_EQ(rn1b.timestamp, 1500);
    ASSERT_EQ(rn1b.metaData.numaPageInfo.size(), 1);
    auto page1b = rn1b.metaData.numaPageInfo.find(1048576);
    ASSERT_NE(page1b, rn1b.metaData.numaPageInfo.end());
    EXPECT_EQ(page1b->second.pageSize, 1048576);
    EXPECT_EQ(page1b->second.hugePageTotal, 8);
    EXPECT_EQ(page1b->second.hugePageFree, 4);

    ASSERT_EQ(result[1].numaInfos.size(), 2);
    const auto& rn2 = result[1].numaInfos[0];
    EXPECT_EQ(rn2.metaData.nodeId, "node2");
    EXPECT_EQ(rn2.metaData.hostName, "host2");
    EXPECT_EQ(rn2.metaData.numaId, 1);
    EXPECT_EQ(rn2.metaData.socketId, 1);
    EXPECT_FALSE(rn2.metaData.isLocal);
    EXPECT_EQ(rn2.metaData.memTotal, 4194304);
    EXPECT_EQ(rn2.metaData.memFree, 2097152);
    EXPECT_EQ(rn2.timestamp, 2000);
    ASSERT_EQ(rn2.metaData.numaPageInfo.size(), 2);
    auto page2048 = rn2.metaData.numaPageInfo.find(2048);
    ASSERT_NE(page2048, rn2.metaData.numaPageInfo.end());
    EXPECT_EQ(page2048->second.pageSize, 2048);
    EXPECT_EQ(page2048->second.hugePageTotal, 200);
    EXPECT_EQ(page2048->second.hugePageFree, 100);
    auto page1g = rn2.metaData.numaPageInfo.find(1048576);
    ASSERT_NE(page1g, rn2.metaData.numaPageInfo.end());
    EXPECT_EQ(page1g->second.pageSize, 1048576);
    EXPECT_EQ(page1g->second.hugePageTotal, 2);
    EXPECT_EQ(page1g->second.hugePageFree, 1);

    const auto& rn3 = result[1].numaInfos[1];
    EXPECT_EQ(rn3.metaData.nodeId, "node2");
    EXPECT_EQ(rn3.metaData.hostName, "host2");
    EXPECT_EQ(rn3.metaData.numaId, 2);
    EXPECT_EQ(rn3.metaData.socketId, 2);
    EXPECT_FALSE(rn3.metaData.isLocal);
    EXPECT_EQ(rn3.metaData.memTotal, 8388608);
    EXPECT_EQ(rn3.metaData.memFree, 4194304);
    EXPECT_EQ(rn3.timestamp, 3000);
    ASSERT_EQ(rn3.metaData.numaPageInfo.size(), 1);
    auto page3 = rn3.metaData.numaPageInfo.find(1048576);
    ASSERT_NE(page3, rn3.metaData.numaPageInfo.end());
    EXPECT_EQ(page3->second.pageSize, 1048576);
    EXPECT_EQ(page3->second.hugePageTotal, 4);
    EXPECT_EQ(page3->second.hugePageFree, 3);
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

TEST_F(TestMemFragmentationMsg, MemFragmentationVmInfoListMsg_Serialize_Deserialize)
{
    using namespace ::vm::mem_fragmentation;

    VmDomainInfo vmDomainInfo1;
    vmDomainInfo1.metaData.nodeId = "node1";
    vmDomainInfo1.metaData.hostName = "host1";
    vmDomainInfo1.metaData.uuid = "uuid1";
    vmDomainInfo1.metaData.name = "vm1";
    vmDomainInfo1.metaData.state = "running";
    vmDomainInfo1.metaData.vmCreateTime = 1000;
    vmDomainInfo1.metaData.maxMem = 1024;
    vmDomainInfo1.metaData.pid = 123;
    mempooling::VmDomainNumaInfo vmNumaInfo1;
    vmNumaInfo1.numaId = 0;
    vmNumaInfo1.pageSize = 2048;
    vmNumaInfo1.usedMem = 100;
    vmNumaInfo1.socketId = 0;
    vmNumaInfo1.isLocal = true;
    vmDomainInfo1.numaInfo[0] = vmNumaInfo1;
    mempooling::VmDomainNumaInfo vmNumaInfo1b;
    vmNumaInfo1b.numaId = 1;
    vmNumaInfo1b.pageSize = 1048576;
    vmNumaInfo1b.usedMem = 1024;
    vmNumaInfo1b.socketId = 1;
    vmNumaInfo1b.isLocal = false;
    vmDomainInfo1.numaInfo[1] = vmNumaInfo1b;

    VmNodeInfo node1;
    node1.nodeId = "node1";
    node1.isCurrent = true;
    node1.isMaster = true;
    node1.podId = "pod-1";
    node1.vmInfos.push_back(vmDomainInfo1);

    VmDomainInfo vmDomainInfo2;
    vmDomainInfo2.metaData.nodeId = "node2";
    vmDomainInfo2.metaData.hostName = "host2";
    vmDomainInfo2.metaData.uuid = "uuid2";
    vmDomainInfo2.metaData.name = "vm2";
    vmDomainInfo2.metaData.state = "running";
    vmDomainInfo2.metaData.vmCreateTime = 2000;
    vmDomainInfo2.metaData.maxMem = 2048;
    vmDomainInfo2.metaData.pid = 456;
    mempooling::VmDomainNumaInfo vmNumaInfo2;
    vmNumaInfo2.numaId = 1;
    vmNumaInfo2.pageSize = 2048;
    vmNumaInfo2.usedMem = 200;
    vmNumaInfo2.socketId = 1;
    vmNumaInfo2.isLocal = false;
    vmDomainInfo2.numaInfo[1] = vmNumaInfo2;
    mempooling::VmDomainNumaInfo vmNumaInfo2b;
    vmNumaInfo2b.numaId = 2;
    vmNumaInfo2b.pageSize = 1048576;
    vmNumaInfo2b.usedMem = 2048;
    vmNumaInfo2b.socketId = 2;
    vmNumaInfo2b.isLocal = false;
    vmDomainInfo2.numaInfo[2] = vmNumaInfo2b;

    VmNodeInfo node2;
    node2.nodeId = "node2";
    node2.isCurrent = false;
    node2.isMaster = false;
    node2.vmInfos.push_back(vmDomainInfo2);

    std::vector<VmNodeInfo> vmNodeInfoList;
    vmNodeInfoList.push_back(node1);
    vmNodeInfoList.push_back(node2);

    MemFragmentationVmInfoListMsg msg{vmNodeInfoList};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    const auto* raw = reinterpret_cast<const char*>(msg.SerializedData());
    const auto rawSize = static_cast<size_t>(msg.SerializedDataSize());
    ASSERT_NE(raw, nullptr);
    ASSERT_GT(rawSize, 0UL);
    auto bytesContain = [&raw, &rawSize](const std::string& needle) {
        return std::search(raw, raw + rawSize, needle.begin(), needle.end()) != raw + rawSize;
    };
    EXPECT_TRUE(bytesContain("node1"));
    EXPECT_TRUE(bytesContain("node2"));
    EXPECT_TRUE(bytesContain("vm1"));
    EXPECT_TRUE(bytesContain("vm2"));
    EXPECT_TRUE(bytesContain("uuid1"));
    std::cout << "rawBytes=" << rawSize << " node1/node2/vm1/vm2/uuid1 all found in serialized stream" << std::endl;

    MemFragmentationVmInfoListMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    auto result = dmsg.GetVmNodeInfoList();
    for (const auto& node : result) {
        std::cout << "nodeId=" << node.nodeId << " isCurrent=" << node.isCurrent << " isMaster=" << node.isMaster
                  << " podId=" << node.podId << " vmCount=" << node.vmInfos.size() << std::endl;
        for (const auto& vmInfo : node.vmInfos) {
            std::cout << "  name=" << vmInfo.metaData.name << " uuid=" << vmInfo.metaData.uuid
                      << " state=" << vmInfo.metaData.state << " hostName=" << vmInfo.metaData.hostName
                      << " vmCreateTime=" << vmInfo.metaData.vmCreateTime << " maxMem=" << vmInfo.metaData.maxMem
                      << "kB pid=" << vmInfo.metaData.pid << " timestamp=" << vmInfo.timestamp << std::endl;
            for (const auto& entry : vmInfo.numaInfo) {
                const auto& numa = entry.second;
                std::cout << "    numaId=" << numa.numaId << " pageSize=" << numa.pageSize << "kB"
                          << " socketId=" << numa.socketId << " isLocal=" << numa.isLocal << " usedMem=" << numa.usedMem
                          << "kB" << std::endl;
            }
        }
    }
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].nodeId, "node1");
    EXPECT_EQ(result[0].isCurrent, true);
    EXPECT_EQ(result[0].isMaster, true);
    EXPECT_EQ(result[0].podId, "pod-1");
    ASSERT_EQ(result[0].vmInfos.size(), 1);
    const auto& rvm1 = result[0].vmInfos[0];
    EXPECT_EQ(rvm1.metaData.uuid, "uuid1");
    EXPECT_EQ(rvm1.metaData.name, "vm1");
    EXPECT_EQ(rvm1.metaData.state, "running");
    EXPECT_EQ(rvm1.metaData.hostName, "host1");
    EXPECT_EQ(rvm1.metaData.vmCreateTime, 1000);
    EXPECT_EQ(rvm1.metaData.maxMem, 1024);
    EXPECT_EQ(rvm1.metaData.pid, 123);
    ASSERT_EQ(rvm1.numaInfo.size(), 2);
    auto vmNuma1 = rvm1.numaInfo.find(0);
    ASSERT_NE(vmNuma1, rvm1.numaInfo.end());
    EXPECT_EQ(vmNuma1->second.numaId, 0);
    EXPECT_EQ(vmNuma1->second.pageSize, 2048);
    EXPECT_EQ(vmNuma1->second.usedMem, 100);
    EXPECT_EQ(vmNuma1->second.socketId, 0);
    EXPECT_TRUE(vmNuma1->second.isLocal);
    auto vmNuma1b = rvm1.numaInfo.find(1);
    ASSERT_NE(vmNuma1b, rvm1.numaInfo.end());
    EXPECT_EQ(vmNuma1b->second.numaId, 1);
    EXPECT_EQ(vmNuma1b->second.pageSize, 1048576);
    EXPECT_EQ(vmNuma1b->second.usedMem, 1024);
    EXPECT_EQ(vmNuma1b->second.socketId, 1);
    EXPECT_FALSE(vmNuma1b->second.isLocal);
    EXPECT_EQ(result[1].nodeId, "node2");
    EXPECT_EQ(result[1].isCurrent, false);
    EXPECT_EQ(result[1].isMaster, false);
    EXPECT_EQ(result[1].podId, "");
    ASSERT_EQ(result[1].vmInfos.size(), 1);
    const auto& rvm2 = result[1].vmInfos[0];
    EXPECT_EQ(rvm2.metaData.uuid, "uuid2");
    EXPECT_EQ(rvm2.metaData.name, "vm2");
    EXPECT_EQ(rvm2.metaData.state, "running");
    EXPECT_EQ(rvm2.metaData.hostName, "host2");
    EXPECT_EQ(rvm2.metaData.vmCreateTime, 2000);
    EXPECT_EQ(rvm2.metaData.maxMem, 2048);
    EXPECT_EQ(rvm2.metaData.pid, 456);
    ASSERT_EQ(rvm2.numaInfo.size(), 2);
    auto vmNuma2 = rvm2.numaInfo.find(1);
    ASSERT_NE(vmNuma2, rvm2.numaInfo.end());
    EXPECT_EQ(vmNuma2->second.numaId, 1);
    EXPECT_EQ(vmNuma2->second.pageSize, 2048);
    EXPECT_EQ(vmNuma2->second.usedMem, 200);
    EXPECT_EQ(vmNuma2->second.socketId, 1);
    EXPECT_FALSE(vmNuma2->second.isLocal);
    auto vmNuma2b = rvm2.numaInfo.find(2);
    ASSERT_NE(vmNuma2b, rvm2.numaInfo.end());
    EXPECT_EQ(vmNuma2b->second.numaId, 2);
    EXPECT_EQ(vmNuma2b->second.pageSize, 1048576);
    EXPECT_EQ(vmNuma2b->second.usedMem, 2048);
    EXPECT_EQ(vmNuma2b->second.socketId, 2);
    EXPECT_FALSE(vmNuma2b->second.isLocal);
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

    param.pageSize = HUGE_PAGE_SIZE_1G_KB;

    MemFragmentationMemBorrowParamMsg msg{param, true};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);

    MemFragmentationMemBorrowParamMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);

    auto resultParam = dmsg.GetBorrowParam();
    EXPECT_EQ(resultParam.nodeId, "node1");
    EXPECT_EQ(resultParam.borrowSize, 1024);
    EXPECT_EQ(resultParam.pageSize, HUGE_PAGE_SIZE_1G_KB);
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

/*
 * 用例描述：
 * 测试三个反序列化接口对线路上超限计数值的拒绝能力（防数组越界索引）
 * 构造方法：用 VmSerialization 手工构造首字段为超限 count 的恶意流，模拟被篡改的 IPC 响应
 * 预期结果：均返回 VM_ERROR_INVAL，且不进入循环（不发生越界访问）
 */
TEST_F(TestMemFragmentationMsg, MemBorrowExecuteResultMsg_Deserialize_RejectOversizedCount)
{
    VmSerialization out;
    const uint32_t oversizedCount = MAX_BORROW_ID_COUNT + 1;
    out << oversizedCount;
    ASSERT_TRUE(out.Check());
    MemBorrowExecuteResultMsg dmsg{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR_INVAL);
}

TEST_F(TestMemFragmentationMsg, MemTaskResultQueryMsg_Deserialize_RejectOversizedCount)
{
    VmSerialization out;
    const std::string taskId = "task";
    out << taskId;
    const uint32_t statusValue = 0;
    out << statusValue;
    const uint32_t resultCode = 0;
    out << resultCode;
    const uint32_t oversizedCount = MAX_BORROW_ID_COUNT + 1;
    out << oversizedCount;
    ASSERT_TRUE(out.Check());
    MemTaskResultQueryMsg dmsg{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR_INVAL);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowResultMsg_Deserialize_RejectOversizedCount)
{
    VmSerialization out;
    const size_t oversizedCount = static_cast<size_t>(MAX_BORROW_ID_COUNT) + 1;
    out << oversizedCount;
    ASSERT_TRUE(out.Check());
    mem_fragmentation::MemFragmentationMemBorrowResultMsg dmsg{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR_INVAL);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationNodeInfoListMsg_Deserialize_RejectHugeNodeCount)
{
    using namespace ::vm::mem_fragmentation;
    VmSerialization out;
    const size_t hugeCount = static_cast<size_t>(MAX_NODE_NUM) + 1;
    out << hugeCount;
    ASSERT_TRUE(out.Check());
    MemFragmentationNodeInfoListMsg dmsg{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR_INVAL);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationNodeInfoListMsg_Deserialize_RejectHugeNumaCount)
{
    using namespace ::vm::mem_fragmentation;
    VmSerialization out;
    const size_t nodeCount = 1;
    out << nodeCount;
    out << std::string("n");
    const size_t hugeNumaCount = static_cast<size_t>(MAX_NUMA_NUM) + 1;
    out << hugeNumaCount;
    ASSERT_TRUE(out.Check());
    MemFragmentationNodeInfoListMsg dmsg{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR_INVAL);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationMemBorrowParamMsg_Deserialize_RejectHugeNumaMetaCount)
{
    using namespace ::vm::mem_fragmentation;
    VmSerialization out;
    out << std::string("n");
    const uint32_t hugeCount = MAX_NUMA_NUM + 1;
    out << hugeCount;
    ASSERT_TRUE(out.Check());
    MemFragmentationMemBorrowParamMsg dmsg{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR_INVAL);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationPageSwapEnableMsg_Deserialize_RejectHugePairsCount)
{
    using namespace ::vm::mem_fragmentation;
    VmSerialization out;
    const pid_t pid = 12345;
    out << pid;
    const uint32_t hugeCount = MAX_NUMA_NUM + 1;
    out << hugeCount;
    ASSERT_TRUE(out.Check());
    MemFragmentationPageSwapEnableMsg dmsg{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR_INVAL);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationPageSwapEnableMsg_Deserialize_RejectHugeLocalQuotaCount)
{
    using namespace ::vm::mem_fragmentation;
    VmSerialization out;
    const pid_t pid = 12345;
    out << pid;
    const uint32_t pairsCount = 1;
    out << pairsCount;
    const uint32_t hugeCount = MAX_NUMA_NUM + 1;
    out << hugeCount;
    ASSERT_TRUE(out.Check());
    MemFragmentationPageSwapEnableMsg dmsg{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR_INVAL);
}

TEST_F(TestMemFragmentationMsg, MemFragmentationPageSwapEnableMsg_Deserialize_RejectHugeRemoteQuotaCount)
{
    using namespace ::vm::mem_fragmentation;
    VmSerialization out;
    const pid_t pid = 12345;
    out << pid;
    const uint32_t pairsCount = 1;
    out << pairsCount;
    const uint32_t localCount = 0;
    out << localCount;
    const uint32_t hugeCount = MAX_NUMA_NUM + 1;
    out << hugeCount;
    ASSERT_TRUE(out.Check());
    MemFragmentationPageSwapEnableMsg dmsg{out.GetBuffer(), static_cast<uint32_t>(out.GetLength())};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR_INVAL);
}

TEST_F(TestMemFragmentationMsg, MemMigrateStrategyOutputMsg_Deserialize_TruncatedWaitingTime)
{
    MemMigrateStrategy srcParam{};
    srcParam.vmInfoListSize = 0;
    srcParam.vmInfoList = nullptr;
    srcParam.waitingTime = 999;
    MemFragmentationMemMigrateStrategyOutputMsg msg{srcParam};
    ASSERT_EQ(msg.Serialize(), VM_OK);
    uint32_t truncLen = msg.SerializedDataSize() - sizeof(uint64_t);
    MemFragmentationMemMigrateStrategyOutputMsg dmsg{msg.SerializedData(), truncLen};
    EXPECT_EQ(dmsg.Deserialize(), VM_ERROR);
}

} // namespace ubse::vm::ut