/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "test_mem_container_msg.h"

#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>
#include "mem_container_msg.h"
#include "mempooling_def.h"
#include "vm_serial_util.h"

using namespace vm;
using namespace vm::mempooling;
namespace ubse::vm::ut {

TestMemContainerMsg::TestMemContainerMsg() = default;

void TestMemContainerMsg::SetUp()
{
    Test::SetUp();
}

void TestMemContainerMsg::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestMemContainerMsg, MemContainerPidMemInfoInputMsg_Serialize)
{
    pid_param_fo_c param = {.srcNid = "node01", .pids = {1234, 5678}, .pid_size = 2};
    MemContainerPidMemInfoInputMsg msg{param};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemContainerMsg, MemContainerPidMemInfoInputMsg_Deserialize)
{
    pid_param_fo_c param = {.srcNid = "node01", .pids = {1234, 5678}, .pid_size = 2};
    MemContainerPidMemInfoInputMsg msg{param};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemContainerPidMemInfoInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemContainerMsg, MemContainerPidMemInfoInputMsg_GetBorrowParam)
{
    pid_param_fo_c param = {.srcNid = "node01", .pids = {1234, 5678}, .pid_size = 2};
    MemContainerPidMemInfoInputMsg msg{param};
    auto borrowParam = msg.GetBorrowParam();
    EXPECT_EQ(borrowParam.srcNid, "node01");
    GlobalMockObject::verify();
}

TEST_F(TestMemContainerMsg, MemContainerPidMemInfoInputMsg_GetPids)
{
    pid_param_fo_c param = {.srcNid = "node01", .pids = {1234, 5678}, .pid_size = 2};
    MemContainerPidMemInfoInputMsg msg{param};
    auto pids = msg.GetPids();
    EXPECT_EQ(pids.size(), 2);
}

TEST_F(TestMemContainerMsg, MemContainerPidMemInfoOutputMsg_Serialize_Deserialize)
{
    std::vector<PidInfo> pidInfos{};
    pidInfos.push_back({0, 0, {0, 1}, 100, 0});
    pidInfos.push_back({1, 1, {2, 3}, 200, 1});

    MemContainerPidMemInfoOutputMsg msg{pidInfos};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemContainerPidMemInfoOutputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
    auto infos = dmsg.GetPidInfos();
    EXPECT_EQ(infos[0].pid, pidInfos[0].pid);
    EXPECT_EQ(infos[0].localUsedMem, pidInfos[0].localUsedMem);
    EXPECT_EQ(infos[0].localNumaCount, pidInfos[0].localNumaIds.size());
    EXPECT_EQ(infos[0].remoteUsedMem, pidInfos[0].remoteUsedMem);
}

TEST_F(TestMemContainerMsg, MemContainerPidMemInfoOutputMsg_GetPidInfos)
{
    std::vector<PidInfo> pidInfos{};
    pidInfos.push_back({0, 0, {0, 1}, 100, 0});
    pidInfos.push_back({1, 1, {2, 3}, 200, 1});

    MemContainerPidMemInfoOutputMsg msg{pidInfos};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    auto pidInfosGet = msg.GetPidInfos();
    EXPECT_EQ(pidInfosGet.size(), 2);
}

TEST_F(TestMemContainerMsg, UpdateWaterLineForCInputMsg_Serialize_Deserialize)
{
    WaterMark waterMark{85, 80};
    UpdateWaterLineForCInputMsg msg{waterMark};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    UpdateWaterLineForCInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemContainerMsg, UpdateWaterLineForCInputMsg_GetWaterMark)
{
    WaterMark waterMark{88, 70};
    UpdateWaterLineForCInputMsg msg{waterMark};
    auto info = msg.GetWaterMark();
    EXPECT_EQ(info.highWaterMark, 88);
    EXPECT_EQ(info.lowWaterMark, 70);
}

TEST_F(TestMemContainerMsg, ContainerIdListForCInputMsg_Serialize_Deserialize)
{
    container_id_list_for_c containerIdListForC{{"0", "1"}, 2};
    ContainerIdListForCInputMsg msg{containerIdListForC};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    ContainerIdListForCInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemContainerMsg, ContainerIdListForCInputMsg_GetContainerPidInfo)
{
    container_id_list_for_c containerIdListForC{{"0", "1"}, 2};
    ContainerIdListForCInputMsg msg{containerIdListForC};
    auto info = msg.GetContainerPidInfo();
    EXPECT_EQ(info.containerIdSize, 2);
}

TEST_F(TestMemContainerMsg, ContainerPidsForCInputMsg_Serialize_Deserialize)
{
    std::vector<container_pid_info_for_c> containerPidInfos;
    char cId[10] = "01";
    containerPidInfos.push_back({{0, 1}, 2, cId});
    ContainerPidsForCInputMsg msg{containerPidInfos};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    ContainerPidsForCInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemContainerMsg, ContainerPidsForCInputMsg_GetContainerPidInfos)
{
    std::vector<container_pid_info_for_c> containerPidInfos;
    containerPidInfos.push_back({{0, 1}, 2, nullptr});
    ContainerPidsForCInputMsg msg{containerPidInfos};
    auto info = msg.GetContainerPidInfos();
    EXPECT_EQ(info.size(), 1);
}

TEST_F(TestMemContainerMsg, MemContainerWaterLineMemBorrowInputMsg_Serialize_Deserialize)
{
    MemBorrowRequestC param{{"0", {0, 0}, 0}, {100, 200}, 2, {80, 20}};
    MemContainerWaterLineMemBorrowInputMsg msg{param};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemContainerWaterLineMemBorrowInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemContainerMsg, MemContainerWaterLineMemBorrowInputMsg_GetParams)
{
    MemBorrowRequestC param{{"0", {0, 0}, 0}, {100, 200}, 2, {80, 20}};
    MemContainerWaterLineMemBorrowInputMsg msg{param};
    NodeLocInfo nodeLocInfo;
    std::vector<uint64_t> borrowSizes;
    WaterMark waterMark;
    auto ret = msg.GetParams(nodeLocInfo, borrowSizes, waterMark);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(waterMark.highWaterMark, 80);
    EXPECT_EQ(waterMark.lowWaterMark, 20);
}

TEST_F(TestMemContainerMsg, MemContainerWaterLineMemBorrowOutputMsg_Serialize_Deserialize)
{
    std::vector<std::string> param{"0", "1"};
    MemContainerWaterLineMemBorrowOutputMsg msg{param};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemContainerWaterLineMemBorrowOutputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemContainerMsg, MemContainerWaterLineMemMigrateInputMsg_Serialize_Deserialize)
{
    MemMigrateRequestC param{{"0", {{0, 0}, {1, 1}}, 0}, {"1", "2"}, 2, {{0, 10}, {1, 20}}, 2};
    MemContainerWaterLineMemMigrateInputMsg msg{param};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemContainerWaterLineMemMigrateInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemContainerMsg, MemContainerWaterLineMemMigrateInputMsg_GetParams)
{
    MemMigrateRequestC param{{"0", {{0, 0}, {1, 1}}, 0}, {"1", "2"}, 2, {{0, 10}, {1, 20}}, 2};
    MemContainerWaterLineMemMigrateInputMsg msg{param};
    NodeLocInfo nodeLocInfo;
    std::unordered_set<std::string> borrowIdSet;
    std::vector<VMPresetParam> vmPresetParamList;
    auto ret = msg.GetParams(nodeLocInfo, borrowIdSet, vmPresetParamList);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(vmPresetParamList[0].pid, param.containerParam[0].pid);
    EXPECT_EQ(vmPresetParamList[0].ratio, param.containerParam[0].ratio);
}

TEST_F(TestMemContainerMsg, MemContainerWaterLineMemReturnInputMsg_Serialize_Deserialize)
{
    MemReturnRequestC param{{"0", {{0, 0}, {1, 1}, 2}}, {"0", "1"}, 0, {0, 1}, 0};
    MemContainerWaterLineMemReturnInputMsg msg{param};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemContainerWaterLineMemReturnInputMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemContainerMsg, MemContainerWaterLineMemReturnInputMsg_GetParams)
{
    MemReturnRequestC param{{"0", {{0, 0}, {1, 1}, 2}}, {"0", "1"}, 0, {0, 1}, 0};
    MemContainerWaterLineMemReturnInputMsg msg{param};
    NodeLocInfo nodeLocInfo;
    std::vector<std::string> borrowIds;
    std::vector<pid_t> pids;
    auto ret = msg.GetParams(nodeLocInfo, borrowIds, pids);
    EXPECT_EQ(ret, 0);
}

} // namespace ubse::vm::ut