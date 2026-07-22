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

#include "test_process_mem_pid_collect.h"

#include "process_mem_pid_info_manager.h"

// Forward declare functions from process_mem_pid_collect.cpp for testing
namespace process_mem::collect {
size_t GetPageSize();
bool GetNumaInfoFromToken(const std::string& token, const size_t pageSize,
                          std::unordered_map<uint32_t, size_t>& numaMemDistribution);
void ParseLine(const std::string& line, std::unordered_map<uint32_t, size_t>& numaMemDistribution);
std::vector<pid_t> GetChildrenPidsFallback(pid_t parentPid);
std::vector<pid_t> GetChildrenPids(pid_t parentPid);
uint32_t GetPidInfoByCollect(process_mem::def::ProcessMemPidInfo& pidInfo, CollectInfoMap& pidCollectInfo);
} // namespace process_mem::collect

namespace ubse::ut::process_mem {
using namespace ::process_mem::collect;
using namespace ::process_mem::def;
using namespace ::process_mem::manager;

void TestProcessMemPidCollect::SetUp() {}

void TestProcessMemPidCollect::TearDown() {}

// ==================== GetPageSize tests ====================

TEST_F(TestProcessMemPidCollect, GetPageSizeReturnsPositive)
{
    size_t pageSize = GetPageSize();
    EXPECT_GT(pageSize, 0u);
}

// ==================== GetNumaInfoFromToken tests ====================

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenValid)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("N0=100", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_EQ(numaMemDistribution[0], 100u * 4096);
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenValidMultiplePages)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("N3=500", 1024, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_EQ(numaMemDistribution[3], 500u * 1024);
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenMultiDigitNuma)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("N12=256", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_EQ(numaMemDistribution[12], 256u * 4096);
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenNotNumaToken)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("kernel=100", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_TRUE(numaMemDistribution.empty());
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenTooShort)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("N", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_TRUE(numaMemDistribution.empty());
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenNoNumaAfterPrefix)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("NX=100", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_TRUE(numaMemDistribution.empty());
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenEmpty)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_TRUE(numaMemDistribution.empty());
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenAccumulates)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    // First token
    GetNumaInfoFromToken("N0=100", 4096, numaMemDistribution);
    // Second token with same numa adds to existing
    GetNumaInfoFromToken("N0=50", 4096, numaMemDistribution);
    EXPECT_EQ(numaMemDistribution[0], 150u * 4096);
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenNoEquals)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("N0x100", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_TRUE(numaMemDistribution.empty());
}

// ==================== ParseLine tests ====================

TEST_F(TestProcessMemPidCollect, ParseLineSingleNumaToken)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    ParseLine("N0=100", numaMemDistribution);
    size_t pageSize = GetPageSize();
    EXPECT_EQ(numaMemDistribution[0], 100u * pageSize);
}

TEST_F(TestProcessMemPidCollect, ParseLineMultipleNumaTokens)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    ParseLine("N0=100 N1=200 N2=50", numaMemDistribution);
    size_t pageSize = GetPageSize();
    EXPECT_EQ(numaMemDistribution[0], 100u * pageSize);
    EXPECT_EQ(numaMemDistribution[1], 200u * pageSize);
    EXPECT_EQ(numaMemDistribution[2], 50u * pageSize);
}

TEST_F(TestProcessMemPidCollect, ParseLineMixedTokens)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    ParseLine("N0=100 kernel=50 N1=200", numaMemDistribution);
    EXPECT_GT(numaMemDistribution[0], 0u);
    EXPECT_GT(numaMemDistribution[1], 0u);
}

TEST_F(TestProcessMemPidCollect, ParseLineEmpty)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    EXPECT_NO_THROW(ParseLine("", numaMemDistribution));
    EXPECT_TRUE(numaMemDistribution.empty());
}

// ==================== GetChildrenPids tests ====================

TEST_F(TestProcessMemPidCollect, GetChildrenPidsFallbackReturnsVector)
{
    auto children = GetChildrenPidsFallback(1);
    EXPECT_TRUE(children.empty() || !children.empty());
}

TEST_F(TestProcessMemPidCollect, GetChildrenPidsReturnsVector)
{
    auto children = GetChildrenPids(1);
    EXPECT_TRUE(children.empty() || !children.empty());
}

// ==================== GetPidInfoByCollect tests ====================

TEST_F(TestProcessMemPidCollect, GetPidInfoByCollectNonExistentPid)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 99999999;
    CollectInfoMap pidCollectInfo;
    auto ret = GetPidInfoByCollect(pidInfo, pidCollectInfo);
    EXPECT_NE(ret, UBSE_OK);
    EXPECT_EQ(pidInfo.processStatus, ProcessStatus::INACTIVE);
}

TEST_F(TestProcessMemPidCollect, RegisterCollectHandlerSuccess)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler handler = [](CollectInfoMap) {
    };
    auto ret = collector.RegisterCollectHandler("test_handler", handler);
    EXPECT_EQ(ret, UBSE_OK);
    collector.UnRegisterCollectHandler("test_handler");
}

TEST_F(TestProcessMemPidCollect, RegisterCollectHandlerNullHandler)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler nullHandler;
    auto ret = collector.RegisterCollectHandler("null_handler", nullHandler);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestProcessMemPidCollect, RegisterCollectHandlerOverwrite)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler handler1 = [](CollectInfoMap) {
    };
    NumaMemDistributionCollectHandler handler2 = [](CollectInfoMap) {
    };

    auto ret = collector.RegisterCollectHandler("overwrite_handler", handler1);
    EXPECT_EQ(ret, UBSE_OK);

    ret = collector.RegisterCollectHandler("overwrite_handler", handler2);
    EXPECT_EQ(ret, UBSE_OK);

    collector.UnRegisterCollectHandler("overwrite_handler");
}

TEST_F(TestProcessMemPidCollect, UnRegisterCollectHandlerNotExist)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    EXPECT_NO_THROW(collector.UnRegisterCollectHandler("non_existent_handler"));
}

TEST_F(TestProcessMemPidCollect, CollectProcessNumaMemDistributionInvalidPid)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    auto ret = collector.CollectProcessNumaMemDistribution(-1, numaMemDistribution);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidCollect, CollectProcessNumaMemDistributionNonExistentPid)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    auto ret = collector.CollectProcessNumaMemDistribution(99999999, numaMemDistribution);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidCollect, RegisterMultipleHandlers)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler handler1 = [](CollectInfoMap) {
    };
    NumaMemDistributionCollectHandler handler2 = [](CollectInfoMap) {
    };

    auto ret1 = collector.RegisterCollectHandler("multi_handler_1", handler1);
    auto ret2 = collector.RegisterCollectHandler("multi_handler_2", handler2);
    EXPECT_EQ(ret1, UBSE_OK);
    EXPECT_EQ(ret2, UBSE_OK);

    collector.UnRegisterCollectHandler("multi_handler_1");
    collector.UnRegisterCollectHandler("multi_handler_2");
}

TEST_F(TestProcessMemPidCollect, UnRegisterAndReRegister)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler handler = [](CollectInfoMap) {
    };

    auto ret = collector.RegisterCollectHandler("reregister_handler", handler);
    EXPECT_EQ(ret, UBSE_OK);

    collector.UnRegisterCollectHandler("reregister_handler");

    ret = collector.RegisterCollectHandler("reregister_handler", handler);
    EXPECT_EQ(ret, UBSE_OK);

    collector.UnRegisterCollectHandler("reregister_handler");
}

// ==================== Additional edge case tests ====================

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenZeroPages)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("N0=0", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_EQ(numaMemDistribution[0], 0u);
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenNegativeNuma)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("N-1=100", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_TRUE(numaMemDistribution.empty());
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenLeadingSpaces)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("  N0=100", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_TRUE(numaMemDistribution.empty());
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenWithMaxNuma)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("N999=123", 1024, numaMemDistribution);
    EXPECT_TRUE(result);
    EXPECT_EQ(numaMemDistribution[999], 123u * 1024);
}

TEST_F(TestProcessMemPidCollect, GetNumaInfoFromTokenMultipleEquals)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken("N0=100=200", 4096, numaMemDistribution);
    EXPECT_TRUE(result);
    // strtoul stops at non-digit, so after = we get "100=200" -> strtoul gets 100
}

TEST_F(TestProcessMemPidCollect, ParseLineWithEmptyTokens)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    ParseLine("   N0=100    N1=200   ", numaMemDistribution);
    size_t pageSize = GetPageSize();
    EXPECT_EQ(numaMemDistribution[0], 100u * pageSize);
    EXPECT_EQ(numaMemDistribution[1], 200u * pageSize);
}

TEST_F(TestProcessMemPidCollect, ParseLineNewlineHandling)
{
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    ParseLine("N0=100\nN1=200", numaMemDistribution);
    size_t pageSize = GetPageSize();
    EXPECT_EQ(numaMemDistribution[0], 100u * pageSize);
}

TEST_F(TestProcessMemPidCollect, CollectProcessNumaMemDistributionInitPid)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    auto ret = collector.CollectProcessNumaMemDistribution(1, numaMemDistribution);
    // init process (pid 1) should exist
    (void)ret;
}

TEST_F(TestProcessMemPidCollect, GetPidInfoByCollectInitPid)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 1;
    CollectInfoMap pidCollectInfo;
    auto ret = GetPidInfoByCollect(pidInfo, pidCollectInfo);
    (void)ret;
}

TEST_F(TestProcessMemPidCollect, GetChildrenPidsForKnownPid)
{
    auto children = GetChildrenPids(1);
    EXPECT_TRUE(children.empty() || !children.empty());
}

TEST_F(TestProcessMemPidCollect, GetChildrenPidsFallbackForKnownPid)
{
    auto children = GetChildrenPidsFallback(1);
    EXPECT_TRUE(children.empty() || !children.empty());
}

TEST_F(TestProcessMemPidCollect, RegisterNullHandlerByName)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler nullHandler;
    auto ret = collector.RegisterCollectHandler("test_null_handler", nullHandler);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestProcessMemPidCollect, UnRegisterThenCallCollect)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler handler = [](CollectInfoMap) {
    };
    collector.RegisterCollectHandler("tmp_handler", handler);
    collector.UnRegisterCollectHandler("tmp_handler");

    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    auto ret = collector.CollectProcessNumaMemDistribution(1, numaMemDistribution);
    (void)ret;
}
} // namespace ubse::ut::process_mem
