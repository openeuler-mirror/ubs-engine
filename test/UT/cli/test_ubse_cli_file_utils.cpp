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
#include "test_ubse_cli_file_utils.h"
#include <linux/limits.h>
#include <sys/stat.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include "ubse_cert_file_utils.h"

namespace ubse::ut::cli {
using namespace ubse::cli::cert;

void TestUbseCliFileUtils::SetUp() {}
void TestUbseCliFileUtils::TearDown() {}

class TempFile {
public:
    explicit TempFile(const std::filesystem::path& dir = std::filesystem::temp_directory_path())
        : path_(dir / RandomName())
    {
        std::ofstream(path_.native());
    }

    ~TempFile()
    {
        if (std::filesystem::exists(path_)) {
            std::filesystem::remove(path_);
        }
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
    static std::string RandomName()
    {
        return "tmp_mock_random_name";
    }
};

class TempDir {
public:
    explicit TempDir(const std::filesystem::path& base = std::filesystem::temp_directory_path())
        : path_(base / RandomName())
    {
        std::filesystem::create_directory(path_);
    }

    ~TempDir()
    {
        if (std::filesystem::exists(path_)) {
            std::filesystem::remove_all(path_);
        }
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
    static std::string RandomName()
    {
        return "tmpdir_mock_random_name";
    }
};

TEST(TestUbseCliFileUtils, IsSymlink)
{
    auto tempFile = std::make_unique<TempFile>();
    std::filesystem::path symlink = "/tmp/mock_symlink";
    std::filesystem::create_symlink(tempFile->path(), symlink);
    EXPECT_TRUE(FileUtils::IsSymlink(symlink));
    std::filesystem::remove(tempFile->path());
    std::filesystem::remove(symlink);
}

TEST(TestUbseCliFileUtils, FilePathPathExceedsPathMax)
{
    std::string longPath(PATH_MAX + 1, 'a');
    std::string errMsg;
    EXPECT_FALSE(FileUtils::IsCanonicalPath(longPath, errMsg));
    EXPECT_FALSE(errMsg.empty());
}

TEST(TestUbseCliFileUtils, FilePathIsSymlink)
{
    auto tempFile = std::make_unique<TempFile>();
    std::ofstream(tempFile->path()) << "test";
    std::filesystem::path symlink = "/tmp/mock_symlink";
    std::filesystem::create_symlink(tempFile->path(), symlink);
    std::string errMsg;
    EXPECT_FALSE(FileUtils::IsCanonicalPath(symlink, errMsg));
    EXPECT_FALSE(errMsg.empty());
    std::filesystem::remove(tempFile->path());
    std::filesystem::remove(symlink);
}
} // namespace ubse::ut::cli