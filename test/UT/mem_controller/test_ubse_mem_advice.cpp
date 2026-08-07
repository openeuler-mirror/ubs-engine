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

#include "test_ubse_mem_advice.h"

#include <sstream>
#include <string>

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_advice.h"
#include "ubse_mem_controller_addr_api.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_fd_api.h"
#include "ubse_mem_controller_numa_api.h"
#include "ubse_mem_controller_share_api.h"
#include "ubse_mem_scheduler_impl.h"
#include "ubse_mem_util.h"
#include "message/ubse_mem_simpo_types.h"

namespace ubse::mem::controller::agent {
std::chrono::seconds GetWaitTimeout();
} // namespace ubse::mem::controller::agent

namespace ubse::mem::controller {
bool IsHighSafety();
} // namespace ubse::mem::controller

namespace ubse::mem::controller::ut {
using namespace context;
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::agent;
using namespace ubse::election;
using ubse::com::UbseComModule;
using namespace ubse::mem::controller::message;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::util;
using ubse::mem::scheduler::SchedulerImpl;

// 全局变量：用于捕获 BorrowFailedAdvice 中 oss.str() 的输出
static std::string g_capturedAdviceMsg;
static bool g_logCaptured = false;

// Mock 函数：替代 UbseGetMasterNodeId，返回可控的 master 节点 ID
static uint32_t MockGetMasterNodeId(std::string& masterNodeId)
{
    masterNodeId = "masterNode1";
    return 0;
}

static uint32_t MockGetMasterNodeIdFailed(std::string& masterNodeId)
{
    return 1; // 模拟失败，masterNodeId 保持空字符串
}

static bool CaptureLogEntry(ubse::log::UbseLog*, ubse::log::UbseLoggerEntry& entry)
{
    std::ostringstream oss;
    entry.OutPutLog(oss);
    std::string fullLog = oss.str();

    // OutPutLog 格式: "<timestamp> [LEVEL][PID][TID][traceId][file:func:line] <message>\n"
    // 从 "[UBSE_MEM]" 开始提取消息内容
    size_t msgPos = fullLog.find("[UBSE_MEM]");
    if (msgPos != std::string::npos) {
        // 去掉末尾的换行符
        std::string msg = fullLog.substr(msgPos);
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) {
            msg.pop_back();
        }
        g_capturedAdviceMsg = msg;
    }
    g_logCaptured = true;
    return true;
}

void TestUbseMemAdvice::SetUp()
{
    g_capturedAdviceMsg.clear();
    g_logCaptured = false;
}

void TestUbseMemAdvice::TearDown()
{
    GlobalMockObject::verify();
}

void TestUbseMemAdvice::SetupDefaultMocks()
{
    g_capturedAdviceMsg.clear();
    g_logCaptured = false;
    MOCKER_CPP(ubse::election::UbseGetMasterNodeId, uint32_t(*)(std::string&))
        .stubs()
        .will(invoke(MockGetMasterNodeId));
    // UBSE_LOG_INFO 宏展开为 UbseIsLog(...) && (UbseLog() == (entry << msg))，
    // UT 环境中 UbseLoggerManager::gInstance 为空导致 UbseIsLog 返回 false，
    // && 短路求值使 operator== 永不执行。需 mock UbseIsLog 返回 true 以放行。
    MOCKER_CPP(ubse::log::UbseIsLog, bool (*)(ubse::log::UbseLogLevel)).stubs().will(returnValue(true));
    MOCKER_CPP(&ubse::log::UbseLog::operator==).stubs().will(invoke(CaptureLogEntry));
}

std::string TestUbseMemAdvice::BuildExpectedMsg(const char* processDesc, const std::string& name,
                                                const char* borrowTypeStr, size_t size, const std::string& exportNode,
                                                const std::string& importNode, const std::string& requestNode,
                                                const std::string& masterId, const std::string& errorCode,
                                                const char* errorInfo, uint32_t adviceCode, const char* adviceStr)
{
    std::ostringstream oss;
    oss << "[UBSE_MEM] " << processDesc << ". RequestName=" << name << ", BorrowType=" << borrowTypeStr
        << ", RequestSize=" << size << "byte, ExportNode=" << exportNode << ", ImportNode=" << importNode
        << ", RequestNode=" << requestNode << ", MasterNode=" << masterId << ", ErrorCode=" << errorCode
        << ", ErrorInfo=" << errorInfo << ", AdviceCode=" << adviceCode << ", Advice=" << adviceStr;
    return oss.str();
}

// ==================== 未知 faultCode 场景 ====================

TEST_F(TestUbseMemAdvice, UnknownFaultCode)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = static_cast<MemFault>(200); // 不存在的 faultCode
    ctx.name = "testBorrow";
    ctx.borrowType = MemType::FD;
    ctx.size = 1024;
    ctx.exportNode = "node1";
    ctx.importNode = "node2";
    ctx.requestNode = "node3";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    EXPECT_EQ(g_capturedAdviceMsg, "[UBSE_MEM] Unknown faultCode=200");
}

TEST_F(TestUbseMemAdvice, UnknownFaultCode_ZeroValue)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::UNKNOWN; // faultCode=0，不在映射表中
    ctx.name = "testBorrow";
    ctx.borrowType = MemType::FD;
    ctx.size = 0;
    ctx.requestNode = "node1";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    EXPECT_EQ(g_capturedAdviceMsg, "[UBSE_MEM] Unknown faultCode=0");
}

// ==================== BORROW_FAILED 场景 ====================

TEST_F(TestUbseMemAdvice, BorrowTimeOut_Numa)
{
    SetupDefaultMocks();

    MOCKER(&election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    UbseMemNumaBorrowReq req{};
    req.name = "timeoutBorrow";
    req.requestNodeId = "2";
    req.importNodeId = "2";
    req.size = 512;
    UbseMemOperationResp resp{};

    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(module));

    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemNumaBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(0);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()>& task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemNumaBorrow(req, resp), UBSE_ERROR);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Borrow Schedule failed", "timeoutBorrow", "APP_NUMA_BORROW", 512, "", "2",
                                            "2", "masterNode1", "ubse_borrow_0015", "the borrow operation timed out", 6,
                                            "please try again.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, BorrowTimeOut_Shm)
{
    SetupDefaultMocks();

    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    UbseMemShareBorrowReq req{};
    req.name = "timeoutBorrow";
    req.requestNodeId = "2";
    req.size = 512;
    UbseMemOperationResp resp{};
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(module));
    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemShareBorrowReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(0);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()>& task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareBorrow(req, resp), UBSE_ERROR);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Borrow Schedule failed", "timeoutBorrow", "SHARE_BORROW", 512, "", "", "2",
                                            "masterNode1", "ubse_borrow_0015", "the borrow operation timed out", 6,
                                            "please try again.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, BorrowTimeOut_SharedAttach)
{
    SetupDefaultMocks();
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    UbseMemShareAttachReq req{};
    req.name = "timeoutAttach";
    req.requestNodeId = "2";
    req.size = 512;
    UbseMemOperationResp resp{};
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(module));
    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemShareAttachReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(0);
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()>& task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));
    EXPECT_EQ(ubse::mem::controller::agent::UbseMemShareAttach(req, resp), UBSE_ERROR);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Borrow Schedule failed", "timeoutAttach", "SHARE_BORROW", 512, "", "", "2",
                                            "masterNode1", "ubse_borrow_0015", "the borrow operation timed out", 6,
                                            "please try again.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, BorrowImportInMaintenance_Fd)
{
    SetupDefaultMocks();

    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));

    UbseMemFdBorrowReq req;
    req.name = "test";
    req.importNodeId = "2";
    req.requestNodeId = "2";
    UbseMemOperationResp resp;

    EXPECT_EQ(ubse::mem::controller::UbseMemFdBorrow(req, resp), UBSE_OK);
    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Import failed", "test", "WATER_BORROW", 0, "", "2", "2", "masterNode1",
                                            "ubse_borrow_0012", "the import node is in maintenance", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, BorrowImportInMaintenance_Numa)
{
    SetupDefaultMocks();

    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));

    UbseMemNumaBorrowReq req;
    req.name = "test";
    req.size = 512;
    req.importNodeId = "2";
    req.requestNodeId = "2";
    UbseMemOperationResp resp;

    EXPECT_EQ(ubse::mem::controller::UbseMemNumaBorrow(req, resp), UBSE_OK);
    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Import failed", "test", "APP_NUMA_BORROW", 512, "", "2", "2",
                                            "masterNode1", "ubse_borrow_0012", "the import node is in maintenance", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, BorrowImportInMaintenance_SharedAttach)
{
    SetupDefaultMocks();

    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));

    UbseMemShareAttachReq req;
    req.name = "test";
    req.importNodeId = "2";
    req.requestNodeId = "2";
    req.size = 512;
    UbseMemOperationResp resp;

    EXPECT_EQ(ubse::mem::controller::UbseMemShareAttach(req, resp), UBSE_OK);
    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Import failed", "test", "SHARE_BORROW", 512, "", "2", "2", "masterNode1",
                                            "ubse_borrow_0012", "the import node is in maintenance", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, BorrowImportInMaintenance_Addr)
{
    SetupDefaultMocks();

    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));

    UbseMemAddrBorrowReq req;
    req.name = "test";
    req.importNodeId = "2";
    req.requestNodeId = "2";
    UbseMemAddrInfo addrInfo;
    addrInfo.size = 512;
    req.exportAddrList.push_back(addrInfo);
    UbseMemOperationResp resp;

    EXPECT_EQ(ubse::mem::controller::UbseMemAddrBorrow(req, resp), UBSE_OK);
    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Import failed", "test", "APP_PRI_BORROW", 512, "", "2", "2", "masterNode1",
                                            "ubse_borrow_0012", "the import node is in maintenance", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

// ==================== RETURN_FAILED 场景 ====================
TEST_F(TestUbseMemAdvice, ReturnTimeOut)
{
    SetupDefaultMocks();

    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    UbseMemReturnReq req{};
    req.name = "timeoutReturn";
    req.requestNodeId = "2";
    UbseMemOperationResp resp{};
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(module));
    const auto sendFunc =
        &UbseComModule::RpcSend<mem::controller::message::UbseMemReturnReqSimpoPtr, UbseBaseMessagePtr>;
    std::chrono::seconds timeout(0);
    Ref<ubse::task_executor::UbseTaskExecutor> taskExecutorPtr =
        new ubse::task_executor::UbseTaskExecutor("task", 0, 0);
    MOCKER_CPP(&ubse::mem::util::GetExecutor).stubs().will(returnValue(taskExecutorPtr));
    MOCKER(sendFunc).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&GetWaitTimeout).stubs().will(returnValue(timeout));
    bool (task_executor::UbseTaskExecutor::*func)(const std::function<void()>& task) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER(func).stubs().will(returnValue(true));

    // 测试NUMA内存类型
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::NUMA_RETURN, resp), UBSE_ERROR);
    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Return Schedule failed", "timeoutReturn", "APP_NUMA_BORROW", 0, "", "",
                                            "2", "masterNode1", "ubse_borrow_0036", "the return operation timed out", 6,
                                            "please try again.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);

    // 测试FD内存类型
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::FD_RETURN, resp), UBSE_ERROR);
    EXPECT_TRUE(g_logCaptured);
    expected = BuildExpectedMsg("Return Schedule failed", "timeoutReturn", "WATER_BORROW", 0, "", "", "2",
                                "masterNode1", "ubse_borrow_0036", "the return operation timed out", 6,
                                "please try again.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);

    // 测试SHM内存类型
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::SHARED_RETURN, resp), UBSE_ERROR);
    EXPECT_TRUE(g_logCaptured);
    expected = BuildExpectedMsg("Return Schedule failed", "timeoutReturn", "SHARE_BORROW", 0, "", "", "2",
                                "masterNode1", "ubse_borrow_0036", "the return operation timed out", 6,
                                "please try again.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);

    // 测试ADDR内存类型
    EXPECT_EQ(UbseMemReturn(req, MemOperationType::ADDR_RETURN, resp), UBSE_ERROR);
    EXPECT_TRUE(g_logCaptured);
    expected = BuildExpectedMsg("Return Schedule failed", "timeoutReturn", "APP_PRI_BORROW", 0, "", "", "2",
                                "masterNode1", "ubse_borrow_0036", "the return operation timed out", 6,
                                "please try again.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, ReturnImportInMaintenance)
{
    SetupDefaultMocks();

    UbseMemReturnReq req;
    req.name = "test";
    req.importNodeId = "2";
    req.requestNodeId = "2";
    UbseMemOperationResp resp;
    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(ubse::mem::controller::UbseMemNumaReturn(req, resp, req.requestNodeId), UBSE_OK);
    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("UnImport failed", "test", "APP_NUMA_BORROW", 0, "", "2", "2",
                                            "masterNode1", "ubse_borrow_0032", "the import node is in maintenance", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);

    EXPECT_EQ(ubse::mem::controller::UbseMemFdReturn(req, resp, req.requestNodeId), UBSE_OK);
    EXPECT_TRUE(g_logCaptured);
    expected = BuildExpectedMsg("UnImport failed", "test", "WATER_BORROW", 0, "", "2", "2", "masterNode1",
                                "ubse_borrow_0032", "the import node is in maintenance", 7,
                                "please wait for the node to join the cluster or complete the smoothing "
                                "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);

    EXPECT_EQ(ubse::mem::controller::UbseMemAddrReturn(req, resp, req.requestNodeId), UBSE_OK);
    EXPECT_TRUE(g_logCaptured);
    expected = BuildExpectedMsg("UnImport failed", "test", "APP_PRI_BORROW", 0, "", "2", "2", "masterNode1",
                                "ubse_borrow_0032", "the import node is in maintenance", 7,
                                "please wait for the node to join the cluster or complete the smoothing "
                                "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, ShareDetach_ImportInMaintenance)
{
    SetupDefaultMocks();

    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));

    UbseMemShareDetachReq req;
    req.name = "test";
    req.unImportNodeId = "2";
    req.requestNodeId = "2";
    UbseMemOperationResp resp;
    EXPECT_EQ(ubse::mem::controller::UbseMemShareDetach(req, resp, req.requestNodeId), UBSE_OK);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("UnImport failed", "test", "SHARE_BORROW", 0, "", "2", "2", "masterNode1",
                                            "ubse_borrow_0032", "the import node is in maintenance", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, ReturnExportInMaintenance_Fd)
{
    SetupDefaultMocks();

    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemFdBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "2";
    importObj.returnReq.requestNodeId = "2";
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemFdBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req.name = "test";
    AddFdExport(exportObj);
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "2";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemFdBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemFdBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubse::mem::controller::UbseMemFdBorrowImportObjCallback(importObj), UBSE_ERROR);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("UnExport failed", "test", "WATER_BORROW", 0, "1", "2", "2", "masterNode1",
                                            "ubse_borrow_0033", "the export node is in maintenance", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, ReturnExportInMaintenance_Numa)
{
    SetupDefaultMocks();

    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "2";
    importObj.returnReq.requestNodeId = "2";
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req.name = "test";
    AddNumaExport(exportObj);
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "2";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemNumaBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubse::mem::controller::UbseMemNumaBorrowImportObjCallback(importObj), UBSE_ERROR);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("UnExport failed", "test", "APP_NUMA_BORROW", 0, "1", "2", "2",
                                            "masterNode1", "ubse_borrow_0033", "the export node is in maintenance", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, ReturnExportInMaintenance_Addr)
{
    SetupDefaultMocks();

    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "1";
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    UbseMemAddrBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.exportNodeId = "1";
    importObj.req.importNodeId = "2";
    importObj.returnReq.requestNodeId = "2";
    importObj.algoResult.importNumaInfos = numaInfos;
    importObj.algoResult.exportNumaInfos = numaInfos;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemAddrBorrowExportObj exportObj;
    exportObj.algoResult = importObj.algoResult;
    exportObj.req.name = "test";
    AddAddrExport(exportObj);
    UbseRoleInfo currentInfo{};
    currentInfo.nodeId = "2";
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func1 = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&SchedulerImpl::MemoryObjChangeHandler<UbseMemAddrBorrowImportObj>).stubs().will(returnValue(UBSE_OK));
    const auto func2 = &UbseComModule::RpcSend<UbseMemAddrBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER(func2).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubse::mem::controller::UbseMemAddrBorrowImportObjCallback(importObj), UBSE_ERROR);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("UnExport failed", "test", "APP_PRI_BORROW", 0, "1", "2", "2",
                                            "masterNode1", "ubse_borrow_0033", "the export node is in maintenance", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

// ==================== INTERNAL_FAULT 场景 ====================

TEST_F(TestUbseMemAdvice, BorrowFaultInternal)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_FAULT_INTERNAL;
    ctx.name = "internalFault";
    ctx.borrowType = MemType::FD;
    ctx.size = 1024;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "2";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Borrow Schedule failed", "internalFault", "WATER_BORROW", 1024, "1", "2",
                                            "2", "masterNode1", "ubse_borrow_0248", "internal fault", 0, "");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, ReturnFaultInternal)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::RETURN_FAULT_INTERNAL;
    ctx.name = "returnInternal";
    ctx.borrowType = MemType::NUMA;
    ctx.size = 2048;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "2";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Return Schedule failed", "returnInternal", "APP_NUMA_BORROW", 2048, "1",
                                            "2", "2", "masterNode1", "ubse_borrow_0252", "internal fault", 0, "");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

// ==================== 边界条件测试 ====================

TEST_F(TestUbseMemAdvice, EmptyOptionalFields)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_CHECK_FAILED;
    ctx.name = "minimalBorrow";
    ctx.borrowType = MemType::ADDR;
    ctx.size = 0;
    ctx.exportNode = ""; // 空 exportNode
    ctx.importNode = ""; // 空 importNode
    ctx.requestNode = "1";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Borrow Schedule failed", "minimalBorrow", "APP_PRI_BORROW", 0, "", "", "1",
                                            "masterNode1", "ubse_borrow_0001", "the input parameter is not valid", 1,
                                            "please check and correct the input parameters.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, LargeSizeValue)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_SCHEDULE_FAILED;
    ctx.name = "largeBorrow";
    ctx.borrowType = MemType::SHM;
    ctx.size = 1099511627776; // 1TB
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "3";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg(
        "Borrow Schedule failed", "largeBorrow", "SHARE_BORROW", 1099511627776, "1", "2", "3", "masterNode1",
        "ubse_borrow_0004", "the borrow scheduler failed to allocate memory", 3, "failed to schedule resources.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, MasterNodeIdFailed)
{
    // 模拟 UbseGetMasterNodeId 失败，masterId 为空字符串
    MOCKER_CPP(ubse::election::UbseGetMasterNodeId, uint32_t(*)(std::string&))
        .stubs()
        .will(invoke(MockGetMasterNodeIdFailed));
    MOCKER_CPP(ubse::log::UbseIsLog, bool (*)(ubse::log::UbseLogLevel)).stubs().will(returnValue(true));
    MOCKER_CPP(&ubse::log::UbseLog::operator==).stubs().will(invoke(CaptureLogEntry));

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_CHECK_FAILED;
    ctx.name = "noMaster";
    ctx.borrowType = MemType::FD;
    ctx.size = 512;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "1";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    // masterId 应为空字符串
    std::string expected = BuildExpectedMsg("Borrow Schedule failed", "noMaster", "WATER_BORROW", 512, "1", "2", "1",
                                            "", "ubse_borrow_0001", "the input parameter is not valid", 1,
                                            "please check and correct the input parameters.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

// ==================== 错误码格式化验证 ====================

TEST_F(TestUbseMemAdvice, ErrorCodeFormatting_SingleDigit)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_CHECK_FAILED; // faultCode=1
    ctx.name = "fmt";
    ctx.borrowType = MemType::FD;
    ctx.size = 0;
    ctx.exportNode = "";
    ctx.importNode = "";
    ctx.requestNode = "";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    // 验证 errorCode 格式为 ubse_borrow_0001（4位补零）
    EXPECT_NE(g_capturedAdviceMsg.find("ErrorCode=ubse_borrow_0001"), std::string::npos);
}

} // namespace ubse::mem::controller::ut
