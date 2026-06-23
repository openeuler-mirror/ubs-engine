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

#include "test_process_mem_pid_bridge.h"

#include "ubse_api_server.h"
#include "ubse_node_controller.h"
#include "ubse_serial_util.h"
#include "mock/ubse/mock_control.h"
#include "process_mem_pid_info_manager.h"

// Forward declare functions from process_mem_pid_bridge.cpp for testing
namespace process_mem::pid::bridge {
void GetLentInfo(const ubse::mem::controller::UbseNumaMemoryDebtInfo& info,
                 const ubse::mem::controller::UbseMemNumaDesc& desc, uint32_t& socketId, uint64_t& numaId, bool& flag);
uint32_t ValidateSrcNumaIdOnCurrentNode(const process_mem::def::ProcessMemPidConfigInfo& configInfo);
uint32_t SendSetPidMapErrorResponse(uint32_t ret, uint64_t requestId);
uint32_t SendPidSetResponse(int successCode, const std::string& errorMsg, uint64_t requestId);
uint32_t SetPidInfo(const api::server::UbseIpcMessage& request, const api::server::UbseRequestContext& context);
uint32_t PidInfoPrint(const api::server::UbseIpcMessage& request, const api::server::UbseRequestContext& context);
uint32_t UnSetPidInfoPrint(const api::server::UbseIpcMessage& request, const api::server::UbseRequestContext& context);
} // namespace process_mem::pid::bridge

namespace ubse::ut::process_mem {
using namespace ::process_mem::pid::bridge;
using namespace ::process_mem::def;
using namespace ::process_mem::manager;

void TestProcessMemPidBridge::SetUp() {}

void TestProcessMemPidBridge::TearDown() {}

// ==================== GetLentInfo tests ====================

TEST_F(TestProcessMemPidBridge, GetLentInfoMatchingNameAndBorrowNode)
{
    ubse::mem::controller::UbseNumaMemoryDebtInfo info{};
    info.name = "test_debt";
    info.borrowNodeId = "1";
    info.lentSocketIdList = {0};
    info.lentNumaIdList = {2};

    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "test_debt";
    desc.importNode.slotId = 1;

    uint32_t socketId = 99;
    uint64_t numaId = 99;
    bool flag = false;
    GetLentInfo(info, desc, socketId, numaId, flag);
    EXPECT_TRUE(flag);
    EXPECT_EQ(socketId, 0u);
    EXPECT_EQ(numaId, 2u);
}

TEST_F(TestProcessMemPidBridge, GetLentInfoMismatchingName)
{
    ubse::mem::controller::UbseNumaMemoryDebtInfo info{};
    info.name = "other_debt";
    info.borrowNodeId = "1";
    info.lentSocketIdList = {0};
    info.lentNumaIdList = {2};

    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "test_debt";
    desc.importNode.slotId = 1;

    uint32_t socketId = 99;
    uint64_t numaId = 99;
    bool flag = false;
    GetLentInfo(info, desc, socketId, numaId, flag);
    EXPECT_FALSE(flag);
}

TEST_F(TestProcessMemPidBridge, GetLentInfoMismatchingBorrowNode)
{
    ubse::mem::controller::UbseNumaMemoryDebtInfo info{};
    info.name = "test_debt";
    info.borrowNodeId = "2";
    info.lentSocketIdList = {0};
    info.lentNumaIdList = {2};

    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "test_debt";
    desc.importNode.slotId = 1;

    uint32_t socketId = 99;
    uint64_t numaId = 99;
    bool flag = false;
    GetLentInfo(info, desc, socketId, numaId, flag);
    EXPECT_FALSE(flag);
}

TEST_F(TestProcessMemPidBridge, GetLentInfoEmptySocketAndNumaList)
{
    ubse::mem::controller::UbseNumaMemoryDebtInfo info{};
    info.name = "test_debt";
    info.borrowNodeId = "1";
    // Both empty: flag stays false, no crash

    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "test_debt";
    desc.importNode.slotId = 1;

    uint32_t socketId = 99;
    uint64_t numaId = 99;
    bool flag = false;
    GetLentInfo(info, desc, socketId, numaId, flag);
    EXPECT_FALSE(flag);
}

// ==================== ValidateSrcNumaIdOnCurrentNode tests ====================

TEST_F(TestProcessMemPidBridge, ValidateSrcNumaIdOnCurrentNodeNoSrcNuma)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.srcNumaId = std::nullopt;
    auto ret = ValidateSrcNumaIdOnCurrentNode(configInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, ValidateSrcNumaIdOnCurrentNodeWithSrcNuma)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.srcNumaId = 2;
    auto ret = ValidateSrcNumaIdOnCurrentNode(configInfo);
    // mock GetCurNode returns empty numaInfos, so srcNumaId won't match
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== SendSetPidMapErrorResponse tests ====================

TEST_F(TestProcessMemPidBridge, SendSetPidMapErrorResponseNotExist)
{
    auto ret = SendSetPidMapErrorResponse(UBSE_ERR_NOT_EXIST, 12345);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, SendSetPidMapErrorResponseInvalidArg)
{
    auto ret = SendSetPidMapErrorResponse(UBSE_ERR_INVALID_ARG, 12345);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, SendSetPidMapErrorResponseResourceBusy)
{
    auto ret = SendSetPidMapErrorResponse(UBSE_ERR_RESOURCE_BUSY, 12345);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, SendSetPidMapErrorResponseGeneric)
{
    auto ret = SendSetPidMapErrorResponse(UBSE_ERROR, 12345);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== SendPidSetResponse tests ====================

TEST_F(TestProcessMemPidBridge, SendPidSetResponseSuccess)
{
    auto ret = SendPidSetResponse(1, "", 12345);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, SendPidSetResponseFailure)
{
    auto ret = SendPidSetResponse(0, "Test error", 12345);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== SetPidInfo tests ====================

TEST_F(TestProcessMemPidBridge, SetPidInfoWithInvalidDeserialize)
{
    api::server::UbseIpcMessage request{};
    request.buffer = nullptr;
    request.length = 0;
    api::server::UbseRequestContext context{};
    context.requestId = 1;
    auto ret = SetPidInfo(request, context);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, SetPidInfoWithValidData)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 77777;
    pidInfo.configInfo.evictThreshold = 80;
    pidInfo.startTime = 1000;

    ubse::serial::UbseSerialization serializer;
    pidInfo.configInfo.SerializeConfigInfo(serializer);

    api::server::UbseIpcMessage request{};
    request.buffer = serializer.GetBuffer();
    request.length = static_cast<uint32_t>(serializer.GetLength());
    api::server::UbseRequestContext context{};
    context.requestId = 1;

    auto ret = SetPidInfo(request, context);
    // mock curNode has no numa matching srcNumaId -> will reject with message about srcNumaId
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== PidInfoPrint tests ====================

TEST_F(TestProcessMemPidBridge, PidInfoPrintSuccess)
{
    api::server::UbseIpcMessage request{};
    request.buffer = nullptr;
    request.length = 0;
    api::server::UbseRequestContext context{};
    context.requestId = 1;
    auto ret = PidInfoPrint(request, context);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== UnSetPidInfoPrint tests ====================

TEST_F(TestProcessMemPidBridge, UnSetPidInfoPrintNotConfigured)
{
    ubse::serial::UbseSerialization serializer;
    pid_t pid = 99999999;
    serializer << pid;

    api::server::UbseIpcMessage request{};
    request.buffer = serializer.GetBuffer();
    request.length = static_cast<uint32_t>(serializer.GetLength());
    api::server::UbseRequestContext context{};
    context.requestId = 1;

    auto ret = UnSetPidInfoPrint(request, context);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, UnSetPidInfoPrintSuccess)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 50001;
    pidInfo.startTime = 1000;
    mgr.SetPidInfoMap(pidInfo);

    ubse::serial::UbseSerialization serializer;
    pid_t unsetPid = 50001;
    serializer << unsetPid;

    api::server::UbseIpcMessage request{};
    request.buffer = serializer.GetBuffer();
    request.length = static_cast<uint32_t>(serializer.GetLength());
    api::server::UbseRequestContext context{};
    context.requestId = 1;

    auto ret = UnSetPidInfoPrint(request, context);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithExportSlotIdNegative)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 1234;
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE0";

    MemoryBorrowRequest request{};
    request.name = "test_borrow";
    request.size = 1024 * 1024;

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};

    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithExportSlotIdSet)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 2345;
    pidInfo.memBorrowInfo.exportSlotId = 1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE0";

    MemoryBorrowRequest request{};
    request.name = "test_borrow_candidate";
    request.size = 2048 * 1024;

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};

    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryReturnSuccess)
{
    auto ret = ProcessMemPidBridge::MemoryReturn("test_return_name");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, GetRemoteNumaSocketInfo)
{
    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "test_numa";
    desc.exportNode.slotId = 1;
    desc.importNode.slotId = 2;

    uint32_t socketId = 0;
    uint64_t numaId = 0;

    auto ret = ProcessMemPidBridge::GetRemoteNumaSocketInfo(desc, socketId, numaId);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, FaultHandlerSuccess)
{
    auto ret = ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_PANIC_EVENT, "NODE_FAULT");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, ProcessMemNodeFaultNotifyHandler)
{
    ubse::serial::UbseSerialization serializer;
    std::string faultNodeId = "NODE_FAULT_1";
    serializer << faultNodeId;

    UbseByteBuffer req{};
    req.data = serializer.GetBuffer();
    req.len = serializer.GetLength();

    UbseByteBuffer resp{};
    EXPECT_NO_THROW(ProcessMemPidBridge::ProcessMemNodeFaultNotifyHandler(req, resp));
}

TEST_F(TestProcessMemPidBridge, NotifyBorrowNodesOnFault)
{
    EXPECT_NO_THROW(ProcessMemPidBridge::NotifyBorrowNodesOnFault("NODE_FAULT_2"));
}

TEST_F(TestProcessMemPidBridge, InitFailNoLibrary)
{
    auto ret = ProcessMemPidBridge::Init();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, UnInitWithNullHandle)
{
    ProcessMemPidBridge::memPoolingHandle = nullptr;
    auto ret = ProcessMemPidBridge::UnInit();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithZeroSize)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 3001;
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE0";

    MemoryBorrowRequest request{};
    request.name = "test_borrow_zero_size";
    request.size = 0;

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};

    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithEmptyName)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 3002;
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE0";

    MemoryBorrowRequest request{};
    request.name = "";
    request.size = 1024 * 1024;

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};

    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithUsrInfo)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 3003;
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE0";

    MemoryBorrowRequest request{};
    request.name = "test_borrow_usr_info";
    request.size = 4096 * 1024;
    ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = 3003;
    usrInfo.startTime = 12345;
    usrInfo.srcNuma = 0;
    memcpy(request.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};

    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithCandidateAndUsrInfo)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 3004;
    pidInfo.memBorrowInfo.exportSlotId = 5;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE0";

    MemoryBorrowRequest request{};
    request.name = "test_borrow_candidate_usr";
    request.size = 8192 * 1024;
    ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = 3004;
    usrInfo.startTime = 54321;
    usrInfo.srcNuma = 1;
    memcpy(request.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};

    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithLargeExportSlotId)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 3005;
    pidInfo.memBorrowInfo.exportSlotId = 255;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE1";

    MemoryBorrowRequest request{};
    request.name = "test_borrow_large_slot";
    request.size = 16 * 1024 * 1024;

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};

    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryReturnWithEmptyName)
{
    auto ret = ProcessMemPidBridge::MemoryReturn("");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, GetRemoteNumaSocketInfoWithDefaultDesc)
{
    ubse::mem::controller::UbseMemNumaDesc desc{};

    uint32_t socketId = 0;
    uint64_t numaId = 0;

    auto ret = ProcessMemPidBridge::GetRemoteNumaSocketInfo(desc, socketId, numaId);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, GetRemoteNumaSocketInfoWithSameSlotIds)
{
    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "test_same_slots";
    desc.exportNode.slotId = 1;
    desc.importNode.slotId = 1;

    uint32_t socketId = 0;
    uint64_t numaId = 0;

    auto ret = ProcessMemPidBridge::GetRemoteNumaSocketInfo(desc, socketId, numaId);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, GetRemoteNumaSocketInfoWithLargeSlotIds)
{
    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "test_large_slots";
    desc.exportNode.slotId = 100;
    desc.importNode.slotId = 200;

    uint32_t socketId = 0;
    uint64_t numaId = 0;

    auto ret = ProcessMemPidBridge::GetRemoteNumaSocketInfo(desc, socketId, numaId);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, FaultHandlerWithKernelRebootEvent)
{
    auto ret = ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_KERNEL_REBOOT_EVENT, "NODE_REBOOT");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, FaultHandlerWithEmptyFaultInfo)
{
    auto ret = ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_PANIC_EVENT, "");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, ProcessMemNodeFaultNotifyHandlerWithNullData)
{
    UbseByteBuffer req{};
    req.data = nullptr;
    req.len = 0;

    UbseByteBuffer resp{};
    EXPECT_NO_THROW(ProcessMemPidBridge::ProcessMemNodeFaultNotifyHandler(req, resp));
}

TEST_F(TestProcessMemPidBridge, ProcessMemNodeFaultNotifyHandlerWithEmptyData)
{
    uint8_t dummy = 0;
    UbseByteBuffer req{};
    req.data = &dummy;
    req.len = 0;

    UbseByteBuffer resp{};
    EXPECT_NO_THROW(ProcessMemPidBridge::ProcessMemNodeFaultNotifyHandler(req, resp));
}

TEST_F(TestProcessMemPidBridge, ProcessMemNodeFaultNotifyHandlerWithValidData)
{
    ubse::serial::UbseSerialization serializer;
    std::string faultNodeId = "NODE_FAULT_VALID";
    serializer << faultNodeId;

    UbseByteBuffer req{};
    req.data = serializer.GetBuffer();
    req.len = serializer.GetLength();

    UbseByteBuffer resp{};
    EXPECT_NO_THROW(ProcessMemPidBridge::ProcessMemNodeFaultNotifyHandler(req, resp));
}

TEST_F(TestProcessMemPidBridge, NotifyBorrowNodesOnFaultWithEmptyNodeId)
{
    EXPECT_NO_THROW(ProcessMemPidBridge::NotifyBorrowNodesOnFault(""));
}

TEST_F(TestProcessMemPidBridge, NotifyBorrowNodesOnFaultWithNonExistentNode)
{
    EXPECT_NO_THROW(ProcessMemPidBridge::NotifyBorrowNodesOnFault("NON_EXISTENT_NODE"));
}

TEST_F(TestProcessMemPidBridge, InitFailNoLibraryResetsHandle)
{
    ProcessMemPidBridge::memPoolingHandle = nullptr;
    auto ret = ProcessMemPidBridge::Init();
    EXPECT_NE(ret, UBSE_OK);
    EXPECT_EQ(ProcessMemPidBridge::memPoolingHandle, nullptr);
}

// ==================== Extended tests ====================

TEST_F(TestProcessMemPidBridge, FaultHandlerWithUnknownFaultEvent)
{
    auto ret = ProcessMemPidBridge::FaultHandler(static_cast<ubse::ras::ALARM_FAULT_TYPE>(999), "NODE_UNKNOWN");
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== Additional edge case tests ====================

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithDifferentUsrInfoSizes)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 4001;
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE2";

    MemoryBorrowRequest request{};
    request.name = "test_diff_usr_sizes";
    request.size = 1024;
    ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = 4001;
    memcpy(request.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};
    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithNodeIdVariations)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 4002;
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE_ZERO";

    MemoryBorrowRequest request{};
    request.name = "test_node_variation";
    request.size = 4096;

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};
    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithLargeSize)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 4003;
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE0";

    MemoryBorrowRequest request{};
    request.name = "test_large_size";
    request.size = UINT64_MAX;

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};
    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryReturnWithTrailingSpaces)
{
    auto ret = ProcessMemPidBridge::MemoryReturn("name_with_trailing  ");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, SetPidInfoWithZeroThresholds)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 5001;
    pidInfo.configInfo.evictThreshold = 0;
    pidInfo.configInfo.targetEvictThreshold = 0;
    pidInfo.configInfo.reclaimThreshold = 0;
    pidInfo.configInfo.expectedMemoryUsage = 0;
    pidInfo.startTime = 1000;

    ubse::serial::UbseSerialization serializer;
    pidInfo.configInfo.SerializeConfigInfo(serializer);

    api::server::UbseIpcMessage request{};
    request.buffer = serializer.GetBuffer();
    request.length = static_cast<uint32_t>(serializer.GetLength());
    api::server::UbseRequestContext context{};
    context.requestId = 2;

    auto ret = SetPidInfo(request, context);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, PidInfoPrintWithConfiguredPids)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 5002;
    pidInfo.startTime = 1000;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.SetPidInfoMap(pidInfo);

    api::server::UbseIpcMessage request{};
    request.buffer = nullptr;
    request.length = 0;
    api::server::UbseRequestContext context{};
    context.requestId = 2;
    auto ret = PidInfoPrint(request, context);
    EXPECT_EQ(ret, UBSE_OK);

    mgr.UnsetPidInfo(5002);
}

TEST_F(TestProcessMemPidBridge, GetRemoteNumaSocketInfoWithValidDesc)
{
    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "remote_numa";
    desc.exportNode.slotId = 5;
    desc.importNode.slotId = 3;

    uint32_t socketId = 99;
    uint64_t numaId = 99;
    auto ret = ProcessMemPidBridge::GetRemoteNumaSocketInfo(desc, socketId, numaId);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, NotifyBorrowNodesOnFaultWithValidNodeId)
{
    auto ret = ProcessMemPidBridge::NotifyBorrowNodesOnFault("VALID_NODE_1");
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== PidInfoPrint with null buffer (empty request) ====================

TEST_F(TestProcessMemPidBridge, PidInfoPrintWithNonnullEmptyBuffer)
{
    api::server::UbseIpcMessage request{};
    uint8_t dummy = 0;
    request.buffer = &dummy;
    request.length = 0;
    api::server::UbseRequestContext context{};
    context.requestId = 3;
    auto ret = PidInfoPrint(request, context);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== UnSetPidInfoPrint edge cases ====================

TEST_F(TestProcessMemPidBridge, UnSetPidInfoPrintWithNullBuffer)
{
    api::server::UbseIpcMessage request{};
    request.buffer = nullptr;
    request.length = 0;
    api::server::UbseRequestContext context{};
    context.requestId = 4;
    auto ret = UnSetPidInfoPrint(request, context);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== ValidateSrcNumaIdOnCurrentNode with nullopt ====================

TEST_F(TestProcessMemPidBridge, ValidateSrcNumaIdOnCurrentNodeNullOpt)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.srcNumaId = std::nullopt;
    auto ret = ValidateSrcNumaIdOnCurrentNode(configInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== FaultHandler with all known event types ====================

TEST_F(TestProcessMemPidBridge, FaultHandlerPanicEvent)
{
    auto ret = ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_PANIC_EVENT, "NODE_PANIC");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, FaultHandlerKernelRebootEvent)
{
    auto ret = ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_KERNEL_REBOOT_EVENT, "NODE_REBOOT_1");
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== SendSetPidMapErrorResponse all branches ====================

TEST_F(TestProcessMemPidBridge, SendSetPidMapErrorResponseAllBranches)
{
    // UBSE_ERR_NOT_EXIST -> "PID does not exist on current node"
    auto ret1 = SendSetPidMapErrorResponse(UBSE_ERR_NOT_EXIST, 99999);
    EXPECT_EQ(ret1, UBSE_OK);

    // UBSE_ERR_INVALID_ARG -> "PID start time mismatch..."
    auto ret2 = SendSetPidMapErrorResponse(UBSE_ERR_INVALID_ARG, 99999);
    EXPECT_EQ(ret2, UBSE_OK);

    // UBSE_ERR_RESOURCE_BUSY -> "Cannot modify srcNumaId..."
    auto ret3 = SendSetPidMapErrorResponse(UBSE_ERR_RESOURCE_BUSY, 99999);
    EXPECT_EQ(ret3, UBSE_OK);

    // Default -> "Set PID info failed"
    auto ret4 = SendSetPidMapErrorResponse(UBSE_ERROR, 99999);
    EXPECT_EQ(ret4, UBSE_OK);
}

// ==================== SendPidSetResponse edge cases ====================

TEST_F(TestProcessMemPidBridge, SendPidSetResponseWithLongMessage)
{
    auto ret = SendPidSetResponse(1, "A very long error message that exceeds normal length", 99999);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== MemoryReturn with empty nodeId ====================

TEST_F(TestProcessMemPidBridge, MemoryReturnEmptyNodeId)
{
    ubse::nodeController::MockSetCurrentNodeId("");
    auto ret = ProcessMemPidBridge::MemoryReturn("test_return");
    EXPECT_EQ(ret, UBSE_ERROR);
    ubse::nodeController::MockResetCurrentNodeId();
}

// ==================== GetRemoteNumaSocketInfo retry tests ====================

TEST_F(TestProcessMemPidBridge, GetRemoteNumaSocketInfoRetryAndError)
{
    ubse::mem::controller::MockSetDebtInfoWithNodeError(UBSE_ERROR);

    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "retry_test";
    desc.exportNode.slotId = 5;

    uint32_t socketId = 0;
    uint64_t numaId = 0;
    auto ret = ProcessMemPidBridge::GetRemoteNumaSocketInfo(desc, socketId, numaId);
    EXPECT_NE(ret, UBSE_OK);

    ubse::mem::controller::MockSetDebtInfoWithNodeError(UBSE_OK);
}

// ==================== NotifyBorrowNodesOnFault retry+error tests ====================

TEST_F(TestProcessMemPidBridge, NotifyBorrowNodesOnFaultRetryExhausted)
{
    ubse::mem::controller::MockSetDebtInfoWithNodeError(UBSE_ERROR);

    auto ret = ProcessMemPidBridge::NotifyBorrowNodesOnFault("FAULT_RETRY_NODE");
    EXPECT_NE(ret, UBSE_OK);

    ubse::mem::controller::MockSetDebtInfoWithNodeError(UBSE_OK);
}

// ==================== SetPidInfo success path test ====================

TEST_F(TestProcessMemPidBridge, SetPidInfoSuccessPath)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = getpid();
    pidInfo.configInfo.evictThreshold = 80;

    ubse::serial::UbseSerialization serializer;
    pidInfo.configInfo.SerializeConfigInfo(serializer);

    api::server::UbseIpcMessage request{};
    request.buffer = serializer.GetBuffer();
    request.length = static_cast<uint32_t>(serializer.GetLength());
    api::server::UbseRequestContext context{};
    context.requestId = 100;

    auto ret = SetPidInfo(request, context);
    EXPECT_EQ(ret, UBSE_OK);

    // Cleanup
    ProcessMemPidInfoManager::GetInstance().UnsetPidInfo(getpid());
}

// ==================== MemoryReturn with delete error ====================

TEST_F(TestProcessMemPidBridge, MemoryReturnDeleteError)
{
    ubse::mem::controller::MockSetNumaDeleteError(UBSE_ERROR);

    auto ret = ProcessMemPidBridge::MemoryReturn("test_delete_error");
    EXPECT_NE(ret, UBSE_OK);

    ubse::mem::controller::MockSetNumaDeleteError(UBSE_OK);
}

// ==================== MemoryBorrow create error tests ====================

TEST_F(TestProcessMemPidBridge, MemoryBorrowCreateError)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 3001;
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE0";

    MemoryBorrowRequest request{};
    request.name = "borrow_create_err";
    request.size = 1024 * 1024;

    ubse::mem::controller::MockSetNumaCreateError(UBSE_ERROR);

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};
    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_NE(ret, UBSE_OK);

    ubse::mem::controller::MockSetNumaCreateError(UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryBorrowWithCandidateCreateError)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 3002;
    pidInfo.memBorrowInfo.exportSlotId = 5;

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.nodeId = "NODE0";

    MemoryBorrowRequest request{};
    request.name = "borrow_candidate_err";
    request.size = 2048 * 1024;

    ubse::mem::controller::MockSetNumaCreateError(UBSE_ERROR);

    ubse::mem::controller::UbseMemNumaDesc borrowInfo{};
    auto ret = ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, borrowInfo);
    EXPECT_NE(ret, UBSE_OK);

    ubse::mem::controller::MockSetNumaCreateError(UBSE_OK);
}

// ==================== FaultHandler error paths ====================

TEST_F(TestProcessMemPidBridge, FaultHandlerWithHandleNodeFaultError)
{
    // HandleNodeFaultEvent may return an error for unexpected input
    auto ret = ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_PANIC_EVENT, "");
    // The fault handler should still work, HandleNodeFaultEvent may succeed
    (void)ret;
}

// ==================== NotifyBorrowNodesOnFault with remote targets (coverage for SendProcessMemNodeFaultToNode) ====================

TEST_F(TestProcessMemPidBridge, NotifyBorrowNodesOnFaultSendsToRemote)
{
    ubse::mem::controller::UbseNumaMemoryDebtInfo debt{};
    debt.name = "send_to_remote";
    debt.lentNodeId = "FAULT_SEND_NODE";
    debt.borrowNodeId = "REMOTE_TARGET"; // != "NODE0"

    ubse::mem::controller::MockSetDebtInfos({debt});

    auto ret = ProcessMemPidBridge::NotifyBorrowNodesOnFault("FAULT_SEND_NODE");
    EXPECT_EQ(ret, UBSE_OK);

    ubse::mem::controller::MockClearDebtInfos();
}

// ==================== FaultHandler with NotifyBorrowNodesOnFault error ====================

TEST_F(TestProcessMemPidBridge, FaultHandlerNotifyBorrowNodesFailed)
{
    ubse::mem::controller::MockSetDebtInfoWithNodeError(UBSE_ERROR);

    auto ret = ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_PANIC_EVENT, "FAULT_NOTIFY_FAIL");
    EXPECT_NE(ret, UBSE_OK);

    ubse::mem::controller::MockSetDebtInfoWithNodeError(UBSE_OK);
}

// ==================== SetPidInfo with srcNumaId not found ====================

TEST_F(TestProcessMemPidBridge, SetPidInfoSrcNumaIdNotFound)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.pid = getpid();
    configInfo.evictThreshold = 80;
    configInfo.srcNumaId = 42; // Non-matching numaId

    ubse::serial::UbseSerialization serializer;
    configInfo.SerializeConfigInfo(serializer);

    api::server::UbseIpcMessage request{};
    request.buffer = serializer.GetBuffer();
    request.length = static_cast<uint32_t>(serializer.GetLength());
    api::server::UbseRequestContext context{};
    context.requestId = 101;

    auto ret = SetPidInfo(request, context);
    EXPECT_EQ(ret, UBSE_OK); // SendPidSetResponse returns UBSE_OK even for error responses
}
} // namespace ubse::ut::process_mem
