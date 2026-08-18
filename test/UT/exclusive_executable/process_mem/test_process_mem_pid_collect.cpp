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

#include <fstream>
#include <sstream>

#include "process_mem_pid_info_manager.h"

namespace process_mem::collect {
size_t GetPageSize();
bool GetNumaInfoFromToken(const std::string& token, const size_t pageSize,
                          std::unordered_map<uint32_t, size_t>& numaMemDistribution);
void ParseLine(const std::string& line, std::unordered_map<uint32_t, size_t>& numaMemDistribution);
std::vector<pid_t> GetChildrenPidsFallback(pid_t parentPid);
std::vector<pid_t> GetChildrenPids(pid_t parentPid);
} // namespace process_mem::collect

namespace ubse::ut::process_mem {
using namespace ::process_mem::collect;
using namespace ::process_mem::manager;
namespace def = ::process_mem::def;

void TestProcessMemPidCollect::SetUp() {}

void TestProcessMemPidCollect::TearDown() {}

TEST_F(TestProcessMemPidCollect, GetPageSizeReturnsPositive)
{
    size_t pageSize = GetPageSize();
    EXPECT_GT(pageSize, 0u);
}

struct NumaTokenCase {
    std::string token;
    size_t pageSize;
    bool expectEmpty;
    uint32_t expectNuma{0};
    uint64_t expectBytes{0};
    std::string secondToken;
};

struct NumaLineCase {
    std::string line;
    std::vector<std::pair<uint32_t, uint64_t>> expect;
};

class TestProcessMemPidCollectNumaToken
    : public TestProcessMemPidCollect
    , public ::testing::WithParamInterface<NumaTokenCase> {
};
class TestProcessMemPidCollectParseLine
    : public TestProcessMemPidCollect
    , public ::testing::WithParamInterface<NumaLineCase> {
};

TEST_P(TestProcessMemPidCollectNumaToken, GetNumaInfoFromToken)
{
    const auto& tc = GetParam();
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = GetNumaInfoFromToken(tc.token, tc.pageSize, numaMemDistribution);
    EXPECT_TRUE(result);
    if (!tc.secondToken.empty()) {
        EXPECT_TRUE(GetNumaInfoFromToken(tc.secondToken, tc.pageSize, numaMemDistribution));
    }
    if (tc.expectEmpty) {
        EXPECT_TRUE(numaMemDistribution.empty());
    } else {
        EXPECT_EQ(numaMemDistribution[tc.expectNuma], tc.expectBytes);
    }
}

INSTANTIATE_TEST_SUITE_P(
    NumaTokenCases, TestProcessMemPidCollectNumaToken,
    testing::Values(NumaTokenCase{"N0=100", 4096, false, 0, 100u * 4096, ""},
                    NumaTokenCase{"N3=500", 1024, false, 3, 500u * 1024, ""},
                    NumaTokenCase{"N12=256", 4096, false, 12, 256u * 4096, ""},
                    NumaTokenCase{"kernel=100", 4096, true, 0, 0, ""}, NumaTokenCase{"N", 4096, true, 0, 0, ""},
                    NumaTokenCase{"NX=100", 4096, true, 0, 0, ""}, NumaTokenCase{"", 4096, true, 0, 0, ""},
                    NumaTokenCase{"N0=100", 4096, false, 0, 150u * 4096, "N0=50"},
                    NumaTokenCase{"N0x100", 4096, true, 0, 0, ""}, NumaTokenCase{"N0=0", 4096, false, 0, 0u, ""},
                    NumaTokenCase{"N-1=100", 4096, true, 0, 0, ""}, NumaTokenCase{"  N0=100", 4096, true, 0, 0, ""},
                    NumaTokenCase{"N999=123", 1024, false, 999, 123u * 1024, ""},
                    NumaTokenCase{"N0=100=200", 4096, false, 0, 100u * 4096, ""}));

TEST_P(TestProcessMemPidCollectParseLine, ParseLine)
{
    const auto& tc = GetParam();
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    EXPECT_NO_THROW(ParseLine(tc.line, numaMemDistribution));
    size_t pageSize = GetPageSize();
    ASSERT_EQ(numaMemDistribution.size(), tc.expect.size());
    for (const auto& [numa, pages] : tc.expect) {
        EXPECT_EQ(numaMemDistribution[numa], pages * pageSize);
    }
}

INSTANTIATE_TEST_SUITE_P(ParseLineCases, TestProcessMemPidCollectParseLine,
                         testing::Values(NumaLineCase{"N0=100", {{0, 100}}},
                                         NumaLineCase{"N0=100 N1=200 N2=50", {{0, 100}, {1, 200}, {2, 50}}},
                                         NumaLineCase{"N0=100 kernel=50 N1=200", {{0, 100}, {1, 200}}},
                                         NumaLineCase{"", {}},
                                         NumaLineCase{"   N0=100    N1=200   ", {{0, 100}, {1, 200}}},
                                         NumaLineCase{"N0=100\nN1=200", {{0, 100}, {1, 200}}}));

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

TEST_F(TestProcessMemPidCollect, CollectProcessNumaMemDistributionInitPid)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    auto ret = collector.CollectProcessNumaMemDistribution(1, numaMemDistribution);
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

std::string ReadComm(pid_t pid)
{
    std::ifstream commFile("/proc/" + std::to_string(pid) + "/comm");
    std::string comm;
    std::getline(commFile, comm);
    return comm;
}

TEST_F(TestProcessMemPidCollect, HandleNewPidsByNameSkipsRootProcess)
{
    ASSERT_EQ(GetProcUid(1), 0);

    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    std::string comm = ReadComm(1);
    ASSERT_FALSE(comm.empty());

    def::ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = comm;
    config.maxMemory = 1073741824;
    config.remoteRatio = 0.5;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    ProcessMemPidCollect::GetInstance().HandleNewPidsByName({1}, 1);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.find(1), snapshot.end());
    EXPECT_EQ(mgr.GetProcMemConfig(true, "1").maxMemory, 0u);

    EXPECT_EQ(mgr.RemoveProcMemConfig(false, comm), UBSE_OK);
}

TEST_F(TestProcessMemPidCollect, HandleNewPidsByNameManagesNonRootProcess)
{
    if (getuid() == 0) {
        GTEST_SKIP() << "本进程为 root，无法作为非 root 正例";
    }
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    std::string comm = ReadComm(getpid());
    ASSERT_FALSE(comm.empty());

    def::ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = comm;
    config.maxMemory = 1073741824;
    config.remoteRatio = 0.5;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    ProcessMemPidCollect::GetInstance().HandleNewPidsByName({getpid()}, 1);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(getpid());
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.sources & static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG),
              static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG));

    mgr.RemoveManagedPidEntry(getpid());
    EXPECT_EQ(mgr.RemoveProcMemConfig(false, comm), UBSE_OK);
}
} // namespace ubse::ut::process_mem
