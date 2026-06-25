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

#include "test_ubse_cert.h"
#include <linux/limits.h>
#include <sys/stat.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mockcpp/mockcpp.hpp>
#include <sstream>
#include <string>

#include "ubse_cert_cli_import.h"
#include "ubse_cert_file_utils.h"
#include "ubse_error.h"

namespace ubse::ut::cert {
using namespace ubse::cli::cert;
using namespace common::def;

class TempFile {
public:
    explicit TempFile(const std::filesystem::path& dir = std::filesystem::temp_directory_path())
        : path_(dir / RandomName())
    {
        std::ofstream(path_.native());
    }

    explicit TempFile(const std::filesystem::path& dir, const std::string& content) : path_(dir / RandomName())
    {
        std::ofstream f(path_.native());
        f << content;
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
        return "tmp_cert_" + std::to_string(time(nullptr));
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
        return "tmpdir_cert_" + std::to_string(time(nullptr));
    }
};

void TestUbseCert::SetUp()
{
    Test::SetUp();
}

void TestUbseCert::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST(TestUbseCertIsSymlink, ReturnsTrueForSymlink)
{
    auto tempFile = std::make_unique<TempFile>();
    std::filesystem::path symlink = "/tmp/cert_test_symlink";
    std::filesystem::create_symlink(tempFile->path(), symlink);
    EXPECT_TRUE(FileUtils::IsSymlink(symlink));
    std::filesystem::remove(tempFile->path());
    std::filesystem::remove(symlink);
}

TEST(TestUbseCertIsSymlink, ReturnsFalseForRegularFile)
{
    auto tempFile = std::make_unique<TempFile>();
    EXPECT_FALSE(FileUtils::IsSymlink(tempFile->path()));
}

TEST(TestUbseCertIsSymlink, ReturnsFalseForNonExistentPath)
{
    EXPECT_FALSE(FileUtils::IsSymlink("/nonexistent/path"));
}

TEST(TestUbseCertIsHardLink, RegularFileReturnsFalse)
{
    auto tempFile = std::make_unique<TempFile>();
    EXPECT_FALSE(FileUtils::IsHardLink(tempFile->path()));
}

TEST(TestUbseCertIsHardLink, NonExistentPathReturnsFalse)
{
    EXPECT_FALSE(FileUtils::IsHardLink("/nonexistent/path"));
}

TEST(TestUbseCertIsCanonicalPath, PathExceedsPathMaxReturnsFalse)
{
    std::string longPath(PATH_MAX + 1, 'a');
    std::string errMsg;
    EXPECT_FALSE(FileUtils::IsCanonicalPath(longPath, errMsg));
    EXPECT_FALSE(errMsg.empty());
}

TEST(TestUbseCertIsCanonicalPath, SymlinkReturnsFalse)
{
    auto tempFile = std::make_unique<TempFile>();
    std::filesystem::path symlink = "/tmp/cert_test_canonical_symlink";
    std::filesystem::create_symlink(tempFile->path(), symlink);
    std::string errMsg;
    EXPECT_FALSE(FileUtils::IsCanonicalPath(symlink, errMsg));
    EXPECT_FALSE(errMsg.empty());
    std::filesystem::remove(tempFile->path());
    std::filesystem::remove(symlink);
}

TEST(TestUbseCertIsCanonicalPath, NonExistentPathReturnsFalse)
{
    std::string errMsg;
    EXPECT_FALSE(FileUtils::IsCanonicalPath("/nonexistent/cert/test", errMsg));
    EXPECT_FALSE(errMsg.empty());
}

TEST(TestUbseCertIsCanonicalPath, ValidFileReturnsTrue)
{
    auto tempFile = std::make_unique<TempFile>();
    std::string errMsg;
    char resolvedPath[PATH_MAX + 1] = {};
    EXPECT_NE(realpath(tempFile->path().c_str(), resolvedPath), nullptr);
    EXPECT_TRUE(FileUtils::IsCanonicalPath(resolvedPath, errMsg));
}

TEST(TestUbseCertCheckFilePathValid, NonExistentFileReturnsFalse)
{
    std::string errMsg;
    EXPECT_FALSE(CheckFilePathValid("/nonexistent/cert/file.pem", true, errMsg));
    EXPECT_FALSE(errMsg.empty());
}

TEST(TestUbseCertCheckFilePathValid, DirectoryWhenExpectingFileReturnsFalse)
{
    TempDir tempDir;
    std::string errMsg;
    EXPECT_FALSE(CheckFilePathValid(tempDir.path(), true, errMsg));
}

TEST(TestUbseCertCheckFilePathValid, ValidRegularFileReturnsTrue)
{
    auto tempFile = std::make_unique<TempFile>();
    std::string errMsg;
    bool result = CheckFilePathValid(tempFile->path(), true, errMsg);
    EXPECT_TRUE(result);
}

TEST(TestUbseCertImportCertSet, WithoutCrlFailsWhenCertDirMissing)
{
    std::string errMsg{};
    std::string serverPath = "/nonexistent/server.pem";
    std::string trustPath = "/nonexistent/trust.pem";
    std::string serverKeyPath = "/nonexistent/server_key.pem";
    bool ret = ImportCertSet(serverPath, trustPath, serverKeyPath, "", errMsg);
    EXPECT_FALSE(ret);
    EXPECT_FALSE(errMsg.empty());
}

TEST(TestUbseCertDeleteCertSet, FailsWhenCertDirMissing)
{
    std::string errMsg{};
    UbseResult ret = DeleteCertSet(errMsg);
    EXPECT_EQ(ret, UBSE_ERROR_FILE_NOT_EXIST);
}
} // namespace ubse::ut::cert
