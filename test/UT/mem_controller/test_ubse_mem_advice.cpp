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

#include "ubse_election.h"
#include "ubse_logger.h"
#include "ubse_mem_advice.h"

namespace ubse::mem::controller::ut {

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

// Mock 函数：捕获 UbseLog::operator== 中的 UbseLoggerEntry，解码并提取消息内容
static bool CaptureLogEntry(ubse::log::UbseLoggerEntry& entry)
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
    } else {
        // 未知 faultCode 分支: "Unknown faultCode=..."
        size_t unknownPos = fullLog.find("Unknown faultCode=");
        if (unknownPos != std::string::npos) {
            std::string msg = fullLog.substr(unknownPos);
            while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) {
                msg.pop_back();
            }
            g_capturedAdviceMsg = msg;
        } else {
            g_capturedAdviceMsg = fullLog;
        }
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
    MOCKER_CPP(ubse::election::UbseGetMasterNodeId, uint32_t(*)(std::string&))
        .stubs()
        .will(invoke(MockGetMasterNodeId));
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

TEST_F(TestUbseMemAdvice, BorrowTimeOut)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_TIME_OUT;
    ctx.name = "timeoutBorrow";
    ctx.borrowType = MemType::FD;
    ctx.size = 512;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "2";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Borrow Schedule failed", "timeoutBorrow", "WATER_BORROW", 512, "1", "2",
                                            "2", "masterNode1", "ubse_borrow_0015", "the borrow operation timed out.",
                                            6, "please try again.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, BorrowDecodeFailed)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_DECODE_FAILED;
    ctx.name = "decodeFail";
    ctx.borrowType = MemType::NUMA;
    ctx.size = 128;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "2";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg(
        "Import failed", "decodeFail", "APP_NUMA_BORROW", 128, "1", "2", "2", "masterNode1", "ubse_borrow_0011",
        "the import node failed to add decoder table.", 12, "please check the decoder table.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, BorrowImportInMaintenance)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_IMPORT_IN_MAINTENANCE;
    ctx.name = "maintImport";
    ctx.borrowType = MemType::SHM;
    ctx.size = 64;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "2";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Import failed", "maintImport", "SHARE_BORROW", 64, "1", "2", "2",
                                            "masterNode1", "ubse_borrow_0012", "the import node is in maintenance.", 7,
                                            "please wait for the node to join the cluster or complete the smoothing "
                                            "stats.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

// ==================== RETURN_FAILED 场景 ====================
TEST_F(TestUbseMemAdvice, ReturnAuthFailed)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::RETURN_AUTH_FAILED;
    ctx.name = "authFail";
    ctx.borrowType = MemType::ADDR;
    ctx.size = 1024;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "2";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Return Schedule failed", "authFail", "APP_PRI_BORROW", 1024, "1", "2", "2",
                                            "masterNode1", "ubse_borrow_0031", "the return authentication failed.", 10,
                                            "please check whether the user has permission to operate this resource.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, ReturnTimeOut)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::RETURN_TIME_OUT;
    ctx.name = "returnTimeout";
    ctx.borrowType = MemType::NUMA;
    ctx.size = 65536;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "2";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Return Schedule failed", "returnTimeout", "APP_NUMA_BORROW", 65536, "1",
                                            "2", "2", "masterNode1", "ubse_borrow_0036",
                                            "the return operation timed out.", 6, "please try again.");
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
                                            "2", "masterNode1", "ubse_borrow_0248", "internal fault.", 0, "");
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
                                            "2", "2", "masterNode1", "ubse_borrow_0252", "internal fault.", 0, "");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

// ==================== SHARED 场景 ====================
TEST_F(TestUbseMemAdvice, SharedAttachAuthFailed)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::SHARED_ATTACH_AUTH_FAILED;
    ctx.name = "sharedAuth";
    ctx.borrowType = MemType::SHM;
    ctx.size = 4096;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "1";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Import failed", "sharedAuth", "SHARE_BORROW", 4096, "1", "2", "1",
                                            "masterNode1", "ubse_borrow_0018",
                                            "the shared attach authentication failed.", 10,
                                            "please check whether the user has permission to operate this resource.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}
// ==================== 边界条件测试 ====================

TEST_F(TestUbseMemAdvice, EmptyOptionalFields)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_CHECK_FAILED;
    ctx.name = "minimalBorrow";
    ctx.borrowType = MemType::FD;
    ctx.size = 0;
    ctx.exportNode = ""; // 空 exportNode
    ctx.importNode = ""; // 空 importNode
    ctx.requestNode = "1";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("Borrow Schedule failed", "minimalBorrow", "WATER_BORROW", 0, "", "", "1",
                                            "masterNode1", "ubse_borrow_0001", "the input parameter is not valid.", 1,
                                            "please check and correct the input parameters.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, LargeSizeValue)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_SCHEDULE_FAILED;
    ctx.name = "largeBorrow";
    ctx.borrowType = MemType::NUMA;
    ctx.size = 1099511627776; // 1TB
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "3";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg(
        "Borrow Schedule failed", "largeBorrow", "APP_NUMA_BORROW", 1099511627776, "1", "2", "3", "masterNode1",
        "ubse_borrow_0004", "the borrow scheduler failed to allocate memory.", 3, "failed to schedule resources.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, MasterNodeIdFailed)
{
    // 模拟 UbseGetMasterNodeId 失败，masterId 为空字符串
    MOCKER_CPP(ubse::election::UbseGetMasterNodeId, uint32_t(*)(std::string&))
        .stubs()
        .will(invoke(MockGetMasterNodeIdFailed));
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
                                            "", "ubse_borrow_0001", "the input parameter is not valid.", 1,
                                            "please check and correct the input parameters.");
    EXPECT_EQ(g_capturedAdviceMsg, expected);
}

TEST_F(TestUbseMemAdvice, AllMemTypes_Fd)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_TIME_OUT;
    ctx.name = "fdType";
    ctx.borrowType = MemType::FD;
    ctx.size = 100;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "1";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    EXPECT_NE(g_capturedAdviceMsg.find("BorrowType=WATER_BORROW"), std::string::npos);
}

TEST_F(TestUbseMemAdvice, AllMemTypes_Numa)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_TIME_OUT;
    ctx.name = "numaType";
    ctx.borrowType = MemType::NUMA;
    ctx.size = 200;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "1";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    EXPECT_NE(g_capturedAdviceMsg.find("BorrowType=APP_NUMA_BORROW"), std::string::npos);
}

TEST_F(TestUbseMemAdvice, AllMemTypes_Shm)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_TIME_OUT;
    ctx.name = "shmType";
    ctx.borrowType = MemType::SHM;
    ctx.size = 300;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "1";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    EXPECT_NE(g_capturedAdviceMsg.find("BorrowType=SHARE_BORROW"), std::string::npos);
}

TEST_F(TestUbseMemAdvice, AllMemTypes_Addr)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_TIME_OUT;
    ctx.name = "addrType";
    ctx.borrowType = MemType::ADDR;
    ctx.size = 400;
    ctx.exportNode = "1";
    ctx.importNode = "2";
    ctx.requestNode = "1";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    EXPECT_NE(g_capturedAdviceMsg.find("BorrowType=APP_PRI_BORROW"), std::string::npos);
}

// ==================== 最高 faultCode 值测试 ====================

TEST_F(TestUbseMemAdvice, MaxFaultCode_SharedFaultDetachInternal)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::SHARED_FAULT_DETACH_INTERNAL; // 255
    ctx.name = "maxFault";
    ctx.borrowType = MemType::SHM;
    ctx.size = 1;
    ctx.exportNode = "emax";
    ctx.importNode = "imax";
    ctx.requestNode = "rmax";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    std::string expected = BuildExpectedMsg("UnImport failed", "maxFault", "SHARE_BORROW", 1, "emax", "imax", "rmax",
                                            "masterNode1", "ubse_borrow_0255", "internal fault.", 0, "");
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

TEST_F(TestUbseMemAdvice, ErrorCodeFormatting_TripleDigit)
{
    SetupDefaultMocks();

    BorrowFailedAdviceCtx ctx;
    ctx.faultCode = MemFault::BORROW_FAULT_INTERNAL; // faultCode=248
    ctx.name = "fmt248";
    ctx.borrowType = MemType::FD;
    ctx.size = 0;
    ctx.exportNode = "";
    ctx.importNode = "";
    ctx.requestNode = "";

    BorrowFailedAdvice(ctx);

    EXPECT_TRUE(g_logCaptured);
    // 验证 errorCode 格式为 ubse_borrow_0248
    EXPECT_NE(g_capturedAdviceMsg.find("ErrorCode=ubse_borrow_0248"), std::string::npos);
}

} // namespace ubse::mem::controller::ut
