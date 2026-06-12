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

namespace ubse::ut::process_mem {
using namespace ::process_mem::pid::bridge;
using namespace ::process_mem::def;

void TestProcessMemPidBridge::SetUp() {}

void TestProcessMemPidBridge::TearDown() {}

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
} // namespace ubse::ut::process_mem
