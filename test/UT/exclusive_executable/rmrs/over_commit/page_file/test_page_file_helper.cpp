/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_page_file_helper.h"

#include <gmock/gmock.h>
#include <filesystem>
#include <mockcpp/mockcpp.hpp>

#include "page_file_helper.h"
#include "ubse_file_util.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::ut::over_commit {
using namespace mempooling::over_commit;
using namespace ubse::utils;

void TestPageFileHelper::SetUp()
{
    Test::SetUp();
    const char *realPath = realpath("../testcase/over_commit/page_file/data/nr_hugepages", nullptr);
    if (realPath == nullptr) {
        std::cout << "[SetUp] current path: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::cout << "[SetUp] test data path: " << std::filesystem::current_path() << std::endl;
    constexpr uint64_t borrowSize = 1048576;
    PageFileHelper::RewriteHugePages(std::string(realPath), borrowSize);
}

void TestPageFileHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

const std::string remoteNumaId = "0";
const std::string filePathResult = "/sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages";
constexpr uint64_t originalHugePagesResult = 512;
constexpr uint64_t borrowSize = 2097152;
constexpr uint64_t reWriteHugePagesResult = 1024;

bool CanonicalPathSuccess(std::string &path)
{
    path = filePathResult;
    return true;
}

TEST_F(TestPageFileHelper, GetHugePageCanonicalPathSuccess)
{
    std::string filePath;
    MOCKER(&UbseFileUtil::CanonicalPath).stubs().will(invoke(CanonicalPathSuccess));
    const auto ret = PageFileHelper::GetHugePageCanonicalPath(remoteNumaId, filePath);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(filePath, filePathResult);
}

TEST_F(TestPageFileHelper, GetHugePageCanonicalPathFail)
{
    std::string filePath;
    MOCKER(&UbseFileUtil::CanonicalPath).stubs().will(returnValue(false));
    const auto ret = PageFileHelper::GetHugePageCanonicalPath(remoteNumaId, filePath);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestPageFileHelper, GetOriginalHugePagesSuccess)
{
    const char *realPath = realpath("../testcase/over_commit/page_file/data/nr_hugepages", nullptr);
    if (realPath == nullptr) {
        std::cout << "current path: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::cout << "test data path: " << std::filesystem::current_path() << std::endl;
    uint64_t originalHugePages;
    const auto ret = PageFileHelper::GetOriginalHugePages(std::string(realPath), originalHugePages);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_EQ(originalHugePages, originalHugePagesResult);
}

TEST_F(TestPageFileHelper, GetOriginalHugePagesFail1)
{
    const char *realPath = realpath("../testcase/over_commit/page_file/data/nr_hugepages_none", nullptr);
    if (realPath == nullptr) {
        std::cout << "current path: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::cout << "test data path: " << std::filesystem::current_path() << std::endl;
    uint64_t originalHugePages;
    const auto ret = PageFileHelper::GetOriginalHugePages(std::string(realPath), originalHugePages);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestPageFileHelper, GetOriginalHugePagesFail2)
{
    const std::string filePath = "unexist file";
    uint64_t originalHugePages;
    const auto ret = PageFileHelper::GetOriginalHugePages(filePath, originalHugePages);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestPageFileHelper, RewriteHugePagesSuccess)
{
    const char *realPath = realpath("../testcase/over_commit/page_file/data/nr_hugepages", nullptr);
    if (realPath == nullptr) {
        std::cout << "current path: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::cout << "test data path: " << std::filesystem::current_path() << std::endl;
    auto ret = PageFileHelper::RewriteHugePages(std::string(realPath), borrowSize);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    uint64_t resultHugePages;
    ret = PageFileHelper::GetOriginalHugePages(std::string(realPath), resultHugePages);
    EXPECT_EQ(resultHugePages, reWriteHugePagesResult);
}

TEST_F(TestPageFileHelper, RewriteHugePagesFail)
{
    const std::string filePath = "";
    const auto ret = PageFileHelper::RewriteHugePages(filePath, borrowSize);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestPageFileHelper, AllocateHugePagesSuccess)
{
    const std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs{
        {.srcNumaId = 0, .presentNumaId = 4, .borrowSize = 2097152}};
    MOCKER(PageFileHelper::GetHugePageCanonicalPath).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(PageFileHelper::GetOriginalHugePages).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(PageFileHelper::RewriteHugePagesWithRetry).stubs().will(returnValue(MEM_POOLING_OK));
    const auto ret = PageFileHelper::AllocateHugePages(memBorrowInfoWithSrcs);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestPageFileHelper, AllocateHugePagesFail1)
{
    const std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs{
            {.srcNumaId = 0, .presentNumaId = 4, .borrowSize = 2097152}};
    MOCKER(PageFileHelper::GetHugePageCanonicalPath).stubs().will(returnValue(MEM_POOLING_ERROR));
    MOCKER(PageFileHelper::GetOriginalHugePages).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(PageFileHelper::RewriteHugePagesWithRetry).stubs().will(returnValue(MEM_POOLING_OK));
    const auto ret = PageFileHelper::AllocateHugePages(memBorrowInfoWithSrcs);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestPageFileHelper, AllocateHugePagesFail2)
{
    const std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs{
                {.srcNumaId = 0, .presentNumaId = 4, .borrowSize = 2097152}};
    MOCKER(PageFileHelper::GetHugePageCanonicalPath).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(PageFileHelper::GetOriginalHugePages).stubs().will(returnValue(MEM_POOLING_ERROR));
    MOCKER(PageFileHelper::RewriteHugePagesWithRetry).stubs().will(returnValue(MEM_POOLING_OK));
    const auto ret = PageFileHelper::AllocateHugePages(memBorrowInfoWithSrcs);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestPageFileHelper, AllocateHugePagesFail3)
{
    const std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs{
                    {.srcNumaId = 0, .presentNumaId = 4, .borrowSize = 2097152}};
    MOCKER(PageFileHelper::GetHugePageCanonicalPath).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(PageFileHelper::GetOriginalHugePages).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(PageFileHelper::RewriteHugePagesWithRetry).stubs().will(returnValue(MEM_POOLING_ERROR));
    const auto ret = PageFileHelper::AllocateHugePages(memBorrowInfoWithSrcs);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestPageFileHelper, RewriteHugePagesWithRetrySuccess)
{
    const char *realPath = realpath("../testcase/over_commit/page_file/data/nr_hugepages", nullptr);
    if (realPath == nullptr) {
        std::cout << "current path: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::cout << "test data path: " << std::filesystem::current_path() << std::endl;
    const uint16_t remoteNumaId = 4;
    uint64_t hugePages = 0;
    const int retryCount = 3;
    auto ret = PageFileHelper::RewriteHugePagesWithRetry(std::string(realPath), hugePages,
                                                         remoteNumaId, borrowSize, retryCount);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestPageFileHelper, RewriteHugePagesWithRetryFail1)
{
    const char *realPath = realpath("../testcase/over_commit/page_file/data/nr_hugepages", nullptr);
    if (realPath == nullptr) {
        std::cout << "current path: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::cout << "test data path: " << std::filesystem::current_path() << std::endl;
    const uint16_t remoteNumaId = 4;
    uint64_t hugePages = 0;
    const int retryCount = 5;
    MOCKER(PageFileHelper::GetOriginalHugePages).stubs().will(returnValue(MEM_POOLING_ERROR));
    auto ret = PageFileHelper::RewriteHugePagesWithRetry(std::string(realPath), hugePages,
                                                         remoteNumaId, borrowSize, retryCount);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestPageFileHelper, RewriteHugePagesWithRetryFail2)
{
    const char *realPath = realpath("../testcase/over_commit/page_file/data/nr_hugepages", nullptr);
    if (realPath == nullptr) {
        std::cout << "current path: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::cout << "test data path: " << std::filesystem::current_path() << std::endl;
    const uint16_t remoteNumaId = 4;
    uint64_t hugePages = 0;
    const int retryCount = 5;
    MOCKER(PageFileHelper::RewriteHugePages).stubs().will(returnValue(MEM_POOLING_ERROR));
    auto ret = PageFileHelper::RewriteHugePagesWithRetry(std::string(realPath), hugePages,
                                                         remoteNumaId, borrowSize, retryCount);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

MpResult mockGetOriginalHugePages(const std::string &filePath, uint64_t &originalHugePages)
{
    originalHugePages = 0;
    return MEM_POOLING_OK;
}

TEST_F(TestPageFileHelper, RewriteHugePagesWithRetryFail3)
{
    const char *realPath = realpath("../testcase/over_commit/page_file/data/nr_hugepages", nullptr);
    if (realPath == nullptr) {
        std::cout << "current path: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::cout << "test data path: " << std::filesystem::current_path() << std::endl;
    const uint16_t remoteNumaId = 4;
    uint64_t hugePages = 0;
    const int retryCount = 1;
    MOCKER(PageFileHelper::GetOriginalHugePages).stubs().will(invoke(mockGetOriginalHugePages));
    auto ret = PageFileHelper::RewriteHugePagesWithRetry(std::string(realPath), hugePages,
                                                         remoteNumaId, borrowSize, retryCount);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
} // namespace mempooling::ut::over_commit