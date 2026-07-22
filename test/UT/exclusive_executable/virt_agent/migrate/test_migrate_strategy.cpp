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

#include "test_migrate_strategy.h"

#include <ubse_api_server.h>
#include <ubse_conf.h>
#include <ubse_mem_controller.h>
#include <ubse_node.h>
#include "mockcpp/mockcpp.hpp"

#include "ham_make_decision_msg.h"
#include "migrate_info_utils.h"
#include "migrate_strategy.h"
#include "vm_error.h"

using namespace vm;
using namespace api::server;
using namespace ubse::config;
using namespace ubse::nodeController;
using namespace ubse::mem::controller;
using namespace ubse::election;

namespace ubse::vm::ut {

void TestMigrateStrategy::SetUp()
{
    Test::SetUp();
}

void TestMigrateStrategy::TearDown()
{
    Test::TearDown();
}

TEST_F(TestMigrateStrategy, Register)
{
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    EXPECT_EQ(VirtMigrateStrategy::Register(), VM_ERROR);
    EXPECT_EQ(VirtMigrateStrategy::Register(), VM_OK);
}

TEST_F(TestMigrateStrategy, GetMigrateStrategy_ShouldReturnError_WhenMakeMigrateStrategyDecisionFailed)
{
    InputParams inputParams{.vmMemoryMB = 1, .uuid = "1", .destHostName = "1", .destNumaId = 0};
    HamMakeDecisionMsg hamMakeDecisionMsg{inputParams};
    EXPECT_EQ(hamMakeDecisionMsg.Serialize(), VM_OK);
    UbseIpcMessage req{hamMakeDecisionMsg.SerializedData(), hamMakeDecisionMsg.SerializedDataSize()};
    UbseRequestContext context{};
    MOCKER(VirtMigrateStrategy::MakeMigrateStrategyDecision).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateStrategy(req, context), VM_ERROR);
    MOCKER(VirtMigrateStrategy::MakeMigrateStrategyDecision).reset();
}

uint32_t MockMakeMigrateStrategyDecisionOK(uint32_t vmMemoryMB, const std::string& uuid,
                                           const std::string& destHostName, uint32_t destNumaId,
                                           uint32_t* migrateStrategy)
{
    return VM_OK;
}

TEST_F(TestMigrateStrategy, GetMigrateStrategy_ShouldReturnError_WhenMemcpyFailed)
{
    InputParams inputParams{.vmMemoryMB = 1, .uuid = "1", .destHostName = "1", .destNumaId = 0};
    HamMakeDecisionMsg hamMakeDecisionMsg{inputParams};
    EXPECT_EQ(hamMakeDecisionMsg.Serialize(), VM_OK);
    UbseIpcMessage req{hamMakeDecisionMsg.SerializedData(), hamMakeDecisionMsg.SerializedDataSize()};
    UbseRequestContext context{};
    MOCKER(VirtMigrateStrategy::MakeMigrateStrategyDecision).stubs().will(invoke(MockMakeMigrateStrategyDecisionOK));
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateStrategy(req, context), VM_ERROR);
    MOCKER(memcpy_s).reset();
    MOCKER(VirtMigrateStrategy::MakeMigrateStrategyDecision).reset();
}

TEST_F(TestMigrateStrategy, GetMigrateStrategy_ShouldReturnError_WhenSendResponseFailed)
{
    InputParams inputParams{.vmMemoryMB = 1, .uuid = "1", .destHostName = "1", .destNumaId = 0};
    HamMakeDecisionMsg hamMakeDecisionMsg{inputParams};
    EXPECT_EQ(hamMakeDecisionMsg.Serialize(), VM_OK);
    UbseIpcMessage req{hamMakeDecisionMsg.SerializedData(), hamMakeDecisionMsg.SerializedDataSize()};
    UbseRequestContext context{};
    MOCKER(VirtMigrateStrategy::MakeMigrateStrategyDecision).stubs().will(invoke(MockMakeMigrateStrategyDecisionOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateStrategy(req, context), VM_ERROR);
    MOCKER(SendResponse).reset();
}

TEST_F(TestMigrateStrategy, GetMigrateStrategy_ShouldReturnOK_WhenEverythingIsOk)
{
    InputParams inputParams{.vmMemoryMB = 1, .uuid = "1", .destHostName = "1", .destNumaId = 0};
    HamMakeDecisionMsg hamMakeDecisionMsg{inputParams};
    EXPECT_EQ(hamMakeDecisionMsg.Serialize(), VM_OK);
    UbseIpcMessage req{hamMakeDecisionMsg.SerializedData(), hamMakeDecisionMsg.SerializedDataSize()};
    UbseRequestContext context{};
    MOCKER(VirtMigrateStrategy::MakeMigrateStrategyDecision).stubs().will(invoke(MockMakeMigrateStrategyDecisionOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateStrategy(req, context), VM_OK);
    MOCKER(VirtMigrateStrategy::MakeMigrateStrategyDecision).reset();
    MOCKER(SendResponse).reset();
}

constexpr uint32_t bigVal = 8192;
constexpr uint32_t smallVal = 100;
constexpr uint32_t validVal = 2048;

uint32_t MockUbseGetUIntError(const std::string& section, const std::string& configKey, uint32_t& configVal)
{
    return VM_ERROR;
}

uint32_t MockUbseGetUIntBig(const std::string& section, const std::string& configKey, uint32_t& configVal)
{
    configVal = bigVal;
    return VM_OK;
}

uint32_t MockUbseGetUIntSmall(const std::string& section, const std::string& configKey, uint32_t& configVal)
{
    configVal = smallVal;
    return VM_OK;
}

uint32_t MockUbseGetUIntValid(const std::string& section, const std::string& configKey, uint32_t& configVal)
{
    configVal = validVal;
    return VM_OK;
}

TEST_F(TestMigrateStrategy, GetMigrateOneCopyMemoryBound)
{
    MOCKER(UbseGetUInt).stubs().will(invoke(MockUbseGetUIntError));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateOneCopyMemoryBound(), UB_VM_MEMORY_BOUNDARY);
    MOCKER(UbseGetUInt).reset();

    MOCKER(UbseGetUInt).stubs().will(invoke(MockUbseGetUIntBig));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateOneCopyMemoryBound(), UB_VM_MEMORY_BOUNDARY);
    MOCKER(UbseGetUInt).reset();

    MOCKER(UbseGetUInt).stubs().will(invoke(MockUbseGetUIntSmall));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateOneCopyMemoryBound(), UB_VM_MEMORY_BOUNDARY);
    MOCKER(UbseGetUInt).reset();

    MOCKER(UbseGetUInt).stubs().will(invoke(MockUbseGetUIntValid));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateOneCopyMemoryBound(), validVal);
    MOCKER(UbseGetUInt).reset();
}

pid_t MockGetPidByVmUUIDFailed(const string& uuid)
{
    return -1;
}

pid_t MockGetPidByVmUUIDSuccess(const string& uuid)
{
    return 1;
}

VmResult MockGetNumaIdAndPageSizeByPidFailed(const pid_t pid, MigrateInfoBase& numaIdAndPageSize)
{
    return VM_ERROR;
}

VmResult MockGetNumaIdAndPageSizeByPidOK(const pid_t pid, MigrateInfoBase& numaIdAndPageSize)
{
    return VM_OK;
}

VmResult MockGetLocalMigrateInfo(MigrateInfoBase& migrateInfoLocal, const std::string& uuid)
{
    return VM_OK;
}

VmResult MockGetRemoteMigrateInfo(MigrateInfoBase& migrateInfoRemote, const std::string& destHostName,
                                  uint32_t destNumaId, const std::string& dstNid)
{
    return VM_OK;
}

VmResult MockGetSocketIdByNumaIdFailed(const uint32_t numaId, uint32_t* socketId)
{
    return VM_ERROR;
}

VmResult MockGetSocketIdByNumaIdOK(const uint32_t numaId, uint32_t* socketId)
{
    return VM_OK;
}

uint32_t UbseVmGetNodeTopologyInfoFailed(const JumpCount& jump,
                                         std::unordered_map<std::string, std::vector<VmNodeData>>& nodeData)
{
    return VM_ERROR;
}

uint32_t UbseVmGetNodeTopologyInfoOK(const JumpCount& jump,
                                     std::unordered_map<std::string, std::vector<VmNodeData>>& nodeData)
{
    std::vector<VmNodeData> nodeDataList;
    TelemetrySocketData telemetrySocketData{.nodeId = "1", .hostname = "node"};
    VmNodeData vmNodeData(std::move(telemetrySocketData));
    nodeDataList.push_back(vmNodeData);
    nodeData.emplace("node", nodeDataList);
    return VM_OK;
}

TEST_F(TestMigrateStrategy, GetMigrateInfo_ShouldReturnError_WhenGetPidByVmUUIDFailed)
{
    MigrateInfoBase migrateInfoLocal{};
    MigrateInfoBase migrateInfoRemote{};
    std::string uuid{};
    std::string destHostName = "node";
    uint32_t destNumaId{};
    MOCKER(MigrateInfoUtil::GetPidByVmUUID).stubs().will(invoke(MockGetPidByVmUUIDFailed));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateInfo(migrateInfoLocal, migrateInfoRemote, uuid, destHostName, destNumaId),
              VM_ERROR);
    MOCKER(MigrateInfoUtil::GetPidByVmUUID).reset();
}

TEST_F(TestMigrateStrategy, GetMigrateInfo_ShouldReturnError_WhenGetNumaIdAndPageSizeByPidFailed)
{
    MigrateInfoBase migrateInfoLocal{};
    MigrateInfoBase migrateInfoRemote{};
    std::string uuid{};
    std::string destHostName = "node";
    uint32_t destNumaId{};
    MOCKER(MigrateInfoUtil::GetPidByVmUUID).stubs().will(invoke(MockGetPidByVmUUIDSuccess));
    MOCKER(MigrateInfoUtil::GetNumaIdAndPageSizeByPid).stubs().will(invoke(MockGetNumaIdAndPageSizeByPidFailed));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateInfo(migrateInfoLocal, migrateInfoRemote, uuid, destHostName, destNumaId),
              VM_ERROR);
    MOCKER(MigrateInfoUtil::GetNumaIdAndPageSizeByPid).reset();
}

TEST_F(TestMigrateStrategy, GetMigrateInfo_ShouldReturnError_WhenGetSocketIdByNumaIdFailed)
{
    MigrateInfoBase migrateInfoLocal{};
    MigrateInfoBase migrateInfoRemote{};
    std::string uuid{};
    std::string destHostName = "node";
    uint32_t destNumaId{};
    MOCKER(MigrateInfoUtil::GetPidByVmUUID).stubs().will(invoke(MockGetPidByVmUUIDSuccess));
    MOCKER(MigrateInfoUtil::GetNumaIdAndPageSizeByPid).stubs().will(invoke(MockGetNumaIdAndPageSizeByPidOK));
    MOCKER(MigrateInfoUtil::GetSocketIdByNumaId).stubs().will(invoke(MockGetSocketIdByNumaIdFailed));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateInfo(migrateInfoLocal, migrateInfoRemote, uuid, destHostName, destNumaId),
              VM_ERROR);
    MOCKER(MigrateInfoUtil::GetSocketIdByNumaId).reset();
}

TEST_F(TestMigrateStrategy, GetMigrateInfo_ShouldReturnError_WhenUbseVmGetNodeTopologyInfoFailed)
{
    MigrateInfoBase migrateInfoLocal{};
    MigrateInfoBase migrateInfoRemote{};
    std::string uuid{};
    std::string destHostName = "node";
    uint32_t destNumaId{};
    MOCKER(MigrateInfoUtil::GetPidByVmUUID).stubs().will(invoke(MockGetPidByVmUUIDSuccess));
    MOCKER(MigrateInfoUtil::GetNumaIdAndPageSizeByPid).stubs().will(invoke(MockGetNumaIdAndPageSizeByPidOK));
    MOCKER(MigrateInfoUtil::GetSocketIdByNumaId).stubs().will(invoke(MockGetSocketIdByNumaIdOK));
    MOCKER(UbseVmGetNodeTopologyInfo).stubs().will(invoke(UbseVmGetNodeTopologyInfoFailed));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateInfo(migrateInfoLocal, migrateInfoRemote, uuid, destHostName, destNumaId),
              static_cast<uint32_t>(UbseVmResult::VM_GET_NODE_TOPOLOGY_INFO_FAILED));
    MOCKER(UbseVmGetNodeTopologyInfo).reset();
}

TEST_F(TestMigrateStrategy, GetMigrateInfo_ShouldReturnOK_WhenEverythingIsOk)
{
    MigrateInfoBase migrateInfoLocal{};
    MigrateInfoBase migrateInfoRemote{};
    std::string uuid{};
    std::string destHostName = "node";
    uint32_t destNumaId{};
    MOCKER(MigrateInfoUtil::GetPidByVmUUID).stubs().will(invoke(MockGetPidByVmUUIDSuccess));
    MOCKER(MigrateInfoUtil::GetNumaIdAndPageSizeByPid).stubs().will(invoke(MockGetNumaIdAndPageSizeByPidOK));
    MOCKER(MigrateInfoUtil::GetSocketIdByNumaId).stubs().will(invoke(MockGetSocketIdByNumaIdOK));
    MOCKER(UbseVmGetNodeTopologyInfo).stubs().will(invoke(UbseVmGetNodeTopologyInfoOK));
    MOCKER(VirtMigrateStrategy::GetLocalMigrateInfo).stubs().will(invoke(MockGetLocalMigrateInfo));
    MOCKER(VirtMigrateStrategy::GetRemoteMigrateInfo).stubs().will(invoke(MockGetRemoteMigrateInfo));
    EXPECT_EQ(VirtMigrateStrategy::GetMigrateInfo(migrateInfoLocal, migrateInfoRemote, uuid, destHostName, destNumaId),
              VM_OK);
    MOCKER(UbseVmGetNodeTopologyInfo).reset();
}

uint32_t MockUbseGetNodeInfos(std::vector<NodeInfo>& nodeInfos)
{
    NodeInfo nodeInfo{.hostName = "node"};
    nodeInfos.push_back(nodeInfo);
    return VM_OK;
}

TEST_F(TestMigrateStrategy, MakeMigrateStrategyDecision_ShouldReturnError_ParamIsInvalid)
{
    uint32_t vmMemoryMB{};
    std::string uuid{};
    std::string destHostName{};
    uint32_t destNumaId{};
    uint32_t migrateStrategy = static_cast<uint32_t>(MigrateStrategy::MULTICOPY_MIGRATE_POLICY);
    EXPECT_EQ(VirtMigrateStrategy::MakeMigrateStrategyDecision(vmMemoryMB, uuid, destHostName, destNumaId, nullptr),
              static_cast<uint32_t>(UbseVmResult::VM_MIGRATE_STRATEGY_NULL_POINTER));
    EXPECT_EQ(
        VirtMigrateStrategy::MakeMigrateStrategyDecision(vmMemoryMB, uuid, destHostName, destNumaId, &migrateStrategy),
        static_cast<uint32_t>(UbseVmResult::VM_DEST_UUID_EMPTY));
    uuid = "123";
    EXPECT_EQ(
        VirtMigrateStrategy::MakeMigrateStrategyDecision(vmMemoryMB, uuid, destHostName, destNumaId, &migrateStrategy),
        static_cast<uint32_t>(UbseVmResult::VM_DEST_HOST_NAME_EMPTY));
}

TEST_F(TestMigrateStrategy, MakeMigrateStrategyDecision_ShouldReturnOnecopy_WhenGetConfigFailed)
{
    uint32_t vmMemoryMB{};
    std::string uuid = "123";
    std::string destHostName = "node";
    uint32_t destNumaId{};
    uint32_t migrateStrategy = static_cast<uint32_t>(MigrateStrategy::MULTICOPY_MIGRATE_POLICY);
    MOCKER(UbseGetNodeInfos).stubs().will(invoke(MockUbseGetNodeInfos));
    MOCKER(UbseGetBool).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(
        VirtMigrateStrategy::MakeMigrateStrategyDecision(vmMemoryMB, uuid, destHostName, destNumaId, &migrateStrategy),
        VM_OK);
    EXPECT_EQ(migrateStrategy, static_cast<uint32_t>(MigrateStrategy::ONECOPY_MIGRATE_POLICY));
    MOCKER(UbseGetBool).reset();
}

uint32_t MockUbseGetBool(const std::string& section, const std::string& configKey, bool& configVal)
{
    configVal = true;
    return VM_OK;
}

TEST_F(TestMigrateStrategy, MakeMigrateStrategyDecision_ShouldReturnOnecopy_WhenVmMemoryIsSmall)
{
    uint32_t vmMemoryMB{};
    std::string uuid = "123";
    std::string destHostName = "node";
    uint32_t destNumaId{};
    uint32_t migrateStrategy = static_cast<uint32_t>(MigrateStrategy::MULTICOPY_MIGRATE_POLICY);
    MOCKER(UbseGetNodeInfos).stubs().will(invoke(MockUbseGetNodeInfos));
    MOCKER(UbseGetBool).stubs().will(invoke(MockUbseGetBool));
    EXPECT_EQ(
        VirtMigrateStrategy::MakeMigrateStrategyDecision(vmMemoryMB, uuid, destHostName, destNumaId, &migrateStrategy),
        VM_OK);
    EXPECT_EQ(migrateStrategy, static_cast<uint32_t>(MigrateStrategy::ONECOPY_MIGRATE_POLICY));
    MOCKER(UbseGetBool).reset();
}

uint32_t MockUbseGetCurrentNodeInfo(UbseRoleInfo& currentNode)
{
    currentNode.nodeId = "1";
    currentNode.nodeRole = ELECTION_ROLE_MASTER;
    return VM_OK;
}

uint32_t MockUbseMemDebtCircleCheck(const std::string& srcNodeId, const std::string& dstNodeId, bool& isCircle)
{
    isCircle = false;
    return VM_OK;
}

TEST_F(TestMigrateStrategy, MakeMigrateStrategyDecision_ShouldReturnOnecopy_WhenVmMemoryIsBig)
{
    uint32_t vmMemoryMB = bigVal;
    std::string uuid = "123";
    std::string destHostName = "node";
    uint32_t destNumaId{};
    uint32_t migrateStrategy = static_cast<uint32_t>(MigrateStrategy::MULTICOPY_MIGRATE_POLICY);
    MOCKER(UbseGetNodeInfos).stubs().will(invoke(MockUbseGetNodeInfos));
    MOCKER(UbseGetBool).stubs().will(invoke(MockUbseGetBool));
    MOCKER(UbseVmGetNodeTopologyInfo).stubs().will(invoke(UbseVmGetNodeTopologyInfoOK));
    MOCKER(UbseMemDebtCircleCheck).stubs().will(invoke(MockUbseMemDebtCircleCheck));
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(invoke(MockUbseGetCurrentNodeInfo));
    MOCKER(VirtMigrateStrategy::GetMigrateInfo).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(
        VirtMigrateStrategy::MakeMigrateStrategyDecision(vmMemoryMB, uuid, destHostName, destNumaId, &migrateStrategy),
        VM_OK);
    EXPECT_EQ(migrateStrategy, static_cast<uint32_t>(MigrateStrategy::MULTICOPY_MIGRATE_POLICY));
    MOCKER(UbseGetBool).reset();
    MOCKER(UbseVmGetNodeTopologyInfo).reset();
    MOCKER(UbseMemDebtCircleCheck).reset();
    MOCKER(UbseGetCurrentNodeInfo).reset();
    MOCKER(VirtMigrateStrategy::GetMigrateInfo).reset();
}

} // namespace ubse::vm::ut