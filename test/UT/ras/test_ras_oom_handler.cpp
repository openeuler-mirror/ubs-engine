// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "test_ras_oom_handler.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_ras_oom_handler.h"
#include "securec.h"
#include "ubse_ras_oom_handler.cpp"

namespace ubse::ras::ut {
using namespace ubse::ras;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::election;

void TestUbseRasOomHandler::SetUp()
{
    Test::SetUp();
}
void TestUbseRasOomHandler::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseRasOomHandler, TestOomHandler)
{
    int type = 1;
    std::string faultInfo = "1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:2,"
                            "timesec:1741057335,timeusec:469389}";
    MOCKER(ForwardOomEventToManager).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(0, OomHandler(type, faultInfo));
    MOCKER(ForwardOomEventToManager).reset();
}

TEST_F(TestUbseRasOomHandler, TestForwardOomEventToManager)
{
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    auto func1 = &UbseComModule::RpcSend<UbseRasOomMessagePtr, UbseRasOomMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, ForwardOomEventToManager(0, TWO_MB_HUGE_PAGE));
    MOCKER(UbseGetCurrentNodeInfo).reset();
    MOCKER(UbseGetMasterInfo).reset();
    MOCKER(&UbseContext::GetModule<UbseComModule>).reset();
    MOCKER(func1).reset();
}

TEST_F(TestUbseRasOomHandler, TestOomHandlerSmallPageOom)
{
    int type = 1;
    std::string faultInfo = "1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:0,"
                            "timesec:1741057335,timeusec:469389}";
    MOCKER(ForwardOomEventToManager).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(0, OomHandler(type, faultInfo));
    MOCKER(ForwardOomEventToManager).reset();
}

TEST_F(TestUbseRasOomHandler, TestGetFreeMemInfo)
{
    GTEST_SKIP();
    uint64_t memFree;
    int16_t numaId = 1;
    EXPECT_EQ(0, GetFreeMemInfo(numaId, memFree));
}

TEST_F(TestUbseRasOomHandler, TestInitOomWaitTime)
{
    uint64_t memFree;
    int16_t numaId = 1;
    EXPECT_NO_THROW(InitOomWaitTime());
}

// InitOomWaitTime: GetFileValue 失败时，应返回默认值 100
TEST_F(TestUbseRasOomHandler, InitOomWaitTimeDefaultWhenGetFileValueFail)
{
    MOCKER(GetFileValue).stubs().will(returnValue(UBSE_ERROR));
    uint64_t result = InitOomWaitTime();
    ASSERT_EQ(result, 100u); // 默认等待时间 100ms
}

// ==================== SplitNids 测试 ====================

TEST_F(TestUbseRasOomHandler, SplitNidsSingleValue)
{
    auto result = SplitNids("0");
    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0], 0);
}

TEST_F(TestUbseRasOomHandler, SplitNidsMultipleValues)
{
    auto result = SplitNids("0,1,2,3");
    ASSERT_EQ(result.size(), 4u);
    ASSERT_EQ(result[0], 0);
    ASSERT_EQ(result[1], 1);
    ASSERT_EQ(result[2], 2);
    ASSERT_EQ(result[3], 3);
}

TEST_F(TestUbseRasOomHandler, SplitNidsWithNegative)
{
    auto result = SplitNids("0,-1,-1,-1");
    ASSERT_EQ(result.size(), 4u);
    ASSERT_EQ(result[0], 0);
    ASSERT_EQ(result[1], -1);
    ASSERT_EQ(result[3], -1);
}

TEST_F(TestUbseRasOomHandler, SplitNidsEmptyString)
{
    auto result = SplitNids("");
    ASSERT_TRUE(result.empty());
}

TEST_F(TestUbseRasOomHandler, SplitNidsInvalidValue)
{
    // Invalid values (non-numeric) cause exception catch, nids stays empty
    auto result = SplitNids("abc,def");
    ASSERT_TRUE(result.empty());
}

// ==================== ParseEventMessage 测试 ====================

TEST_F(TestUbseRasOomHandler, ParseEventMessageInvalidFormat)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    auto ret = ParseEventMessage("invalid_message", messageValue);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasOomHandler, ParseEventMessageMissingFields)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    // Missing the closing brace pattern
    auto ret = ParseEventMessage("1650_{nr_nid:1,nid:[0],sync:1,timeout:30000,reason:2", messageValue);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

// ==================== CheckHugePageOomParam 测试 ====================

TEST_F(TestUbseRasOomHandler, CheckHugePageOomParamInvalidNrNid)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["nr_nid"] = static_cast<int>(0);
    messageValue["nid"] = std::vector<int>{};
    messageValue["sync"] = static_cast<int>(0);
    auto ret = CheckHugePageOomParam(messageValue);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasOomHandler, CheckHugePageOomParamInvalidSync)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["nr_nid"] = static_cast<int>(1);
    messageValue["nid"] = std::vector<int>{0};
    messageValue["sync"] = static_cast<int>(0); // sync should be 1 for VM scenario
    auto ret = CheckHugePageOomParam(messageValue);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasOomHandler, CheckHugePageOomParamValid)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["nr_nid"] = static_cast<int>(1);
    messageValue["nid"] = std::vector<int>{0};
    messageValue["sync"] = static_cast<int>(1);
    auto ret = CheckHugePageOomParam(messageValue);
    ASSERT_EQ(ret, UBSE_OK);
}

// ==================== GetOomNumaId 测试 ====================

TEST_F(TestUbseRasOomHandler, GetOomNumaIdEmptyNids)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["nid"] = std::vector<int>{};
    int oomNumaId = 0;
    auto ret = GetOomNumaId(messageValue, oomNumaId);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasOomHandler, GetOomNumaIdNegative)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["nid"] = std::vector<int>{-1};
    int oomNumaId = 0;
    auto ret = GetOomNumaId(messageValue, oomNumaId);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasOomHandler, GetOomNumaIdValid)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["nid"] = std::vector<int>{3, -1, -1};
    int oomNumaId = 0;
    auto ret = GetOomNumaId(messageValue, oomNumaId);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(oomNumaId, 3);
}

// ==================== ProcessSmallpageOom 测试 ====================

TEST_F(TestUbseRasOomHandler, ProcessSmallpageOomReturnsOk)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    uint64_t nrFree = 0;
    auto ret = ProcessSmallpageOom(messageValue, nrFree);
    ASSERT_EQ(ret, static_cast<uint32_t>(UBSE_OK));
}

// ==================== GetLineInfo 测试 ====================

TEST_F(TestUbseRasOomHandler, GetLineInfoSingleToken)
{
    auto result = GetLineInfo("MemFree:");
    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0], "MemFree:");
}

TEST_F(TestUbseRasOomHandler, GetLineInfoMultipleTokens)
{
    auto result = GetLineInfo("MemFree:         12345 kB");
    ASSERT_EQ(result.size(), 3u);
    ASSERT_EQ(result[1], "12345");
}

TEST_F(TestUbseRasOomHandler, GetLineInfoEmpty)
{
    auto result = GetLineInfo("");
    ASSERT_TRUE(result.empty());
}

// ==================== GetFreeHugepages 测试 ====================

TEST_F(TestUbseRasOomHandler, GetFreeHugepagesWhenGetFileValueFail)
{
    MOCKER(GetFileValue).stubs().will(returnValue(UBSE_ERROR));
    uint64_t memFree = 0;
    auto ret = GetFreeHugepages(0, memFree);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseRasOomHandler, GetFreeHugepagesSuccess)
{
    MOCKER(GetFileValue).stubs().will(returnValue(UBSE_OK));
    uint64_t memFree = 0;
    auto ret = GetFreeHugepages(0, memFree);
    ASSERT_EQ(ret, UBSE_OK);
}

// ==================== SmapUrgentMigrateOut 测试 ====================

TEST_F(TestUbseRasOomHandler, SmapUrgentMigrateOutWhenFuncNull)
{
    // Reset the global function pointer to nullptr to test error path
    auto savedFunc = smapUrgentMigrateOutFunc;
    smapUrgentMigrateOutFunc = nullptr;
    // Mock UbseGetStr to return UBSE_OK (vm_code is set)
    MOCKER(UbseGetStr).stubs().will(returnValue(UBSE_OK));
    auto ret = SmapUrgentMigrateOut(128 * 1024 * 1024);
    ASSERT_EQ(ret, UBSE_ERROR);
    smapUrgentMigrateOutFunc = savedFunc;
}

// ==================== OomHandler 其他路径测试 ====================

TEST_F(TestUbseRasOomHandler, OomHandlerInvalidFormat)
{
    auto ret = OomHandler(ALARM_OOM_EVENT, "invalid_format");
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasOomHandler, OomHandlerInvalidReason)
{
    // reason=99 is not in oomReasons map
    std::string faultInfo = "1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:99,"
                            "timesec:1741057335,timeusec:469389}";
    auto ret = OomHandler(ALARM_OOM_EVENT, faultInfo);
    // CheckCommonParam finds invalid reason, returns UBSE_ERROR_INVAL, OomHandler returns UBSE_OK
    ASSERT_EQ(ret, UBSE_OK);
}

// ==================== GetRemoteFreeHugepages 测试 ====================

TEST_F(TestUbseRasOomHandler, GetRemoteFreeHugepagesWhenAllFreeFail)
{
    MOCKER(GetFileValue).stubs().will(returnValue(UBSE_ERROR));
    uint64_t memFree = 0;
    auto ret = GetRemoteFreeHugepages(memFree);
    ASSERT_EQ(ret, UBSE_ERROR);
}

// ==================== ForwardOomEventToManager 错误路径测试 ====================

TEST_F(TestUbseRasOomHandler, ForwardOomEventToManagerWhenGetCurrentFail)
{
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = ForwardOomEventToManager(0, TWO_MB_HUGE_PAGE);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseRasOomHandler, ForwardOomEventToManagerWhenGetMasterFail)
{
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = ForwardOomEventToManager(0, TWO_MB_HUGE_PAGE);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseRasOomHandler, ForwardOomEventToManagerWhenComModuleNull)
{
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseComModule>(nullptr)));
    auto ret = ForwardOomEventToManager(0, TWO_MB_HUGE_PAGE);
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

// ==================== SmapUrgentMigrateOut vm未启用测试 ====================

TEST_F(TestUbseRasOomHandler, SmapUrgentMigrateOutWhenVmNotEnabled)
{
    MOCKER(UbseGetStr).stubs().will(returnValue(UBSE_ERROR));
    auto ret = SmapUrgentMigrateOut(128 * 1024 * 1024);
    ASSERT_EQ(ret, UBSE_OK);
}

// ==================== IsRemoteNuma 测试 ====================

TEST_F(TestUbseRasOomHandler, IsRemoteNumaWhenGetFileValueFails)
{
    MOCKER(GetFileValue).stubs().will(returnValue(UBSE_ERROR));
    bool isRemote = true;
    auto ret = IsRemoteNuma(0, isRemote);
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseRasOomHandler, IsRemoteNumaWhenRemote)
{
    MOCKER(GetFileValue).stubs().will(returnValue(UBSE_OK));
    bool isRemote = false;
    auto ret = IsRemoteNuma(0, isRemote);
    ASSERT_EQ(ret, UBSE_OK);
}

// ==================== CheckCommonParam 额外路径测试 ====================

TEST_F(TestUbseRasOomHandler, CheckCommonParamNidsTooLarge)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["reason"] = static_cast<int>(0); // KSWAPD
    // nids size > MAX_NUMA_NUM (8)
    std::vector<int> largeNids(10, 0);
    messageValue["nid"] = largeNids;
    auto ret = CheckCommonParam(messageValue, "test event message");
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

// ==================== ForwardOomEventToManager response空指针测试 ====================

TEST_F(TestUbseRasOomHandler, ForwardOomEventToManagerWhenRpcSendFails)
{
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    auto func1 = &UbseComModule::RpcSend<UbseRasOomMessagePtr, UbseRasOomMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_ERROR));
    auto ret = ForwardOomEventToManager(0, TWO_MB_HUGE_PAGE);
    ASSERT_EQ(ret, UBSE_ERROR);
}

// ==================== ProcessHugepageOom 额外测试 ====================

TEST_F(TestUbseRasOomHandler, ProcessHugepageOomWhenCheckParamFails)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["nr_nid"] = static_cast<int>(0); // invalid nr_nid
    messageValue["nid"] = std::vector<int>{};
    messageValue["sync"] = static_cast<int>(0);
    uint64_t nrFree = 0;
    auto ret = ProcessHugepageOom(messageValue, nrFree);
    ASSERT_NE(ret, static_cast<uint32_t>(UBSE_OK));
}

TEST_F(TestUbseRasOomHandler, ProcessHugepageOomWhenGetOomNumaIdFails)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    messageValue["nr_nid"] = static_cast<int>(1);
    messageValue["nid"] = std::vector<int>{}; // empty nids
    messageValue["sync"] = static_cast<int>(1);
    uint64_t nrFree = 0;
    auto ret = ProcessHugepageOom(messageValue, nrFree);
    ASSERT_NE(ret, static_cast<uint32_t>(UBSE_OK));
}

TEST_F(TestUbseRasOomHandler, GetOomNumaIdWhenMissingNidKey)
{
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    // No "nid" key at all
    int oomNumaId = 0;
    auto ret = GetOomNumaId(messageValue, oomNumaId);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}
} // namespace ubse::ras::ut